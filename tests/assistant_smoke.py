"""Local end-to-end assistant transport fixtures; no credentials or online generation."""
import http.server
import json
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.request
import uuid
from pathlib import Path

exe, cli, native = [Path(value).resolve() for value in sys.argv[1:4]]
work = exe.parent / 'test-output' / 'assistant'
work.mkdir(parents=True, exist_ok=True)
flags = subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0

class Fixture(http.server.BaseHTTPRequestHandler):
    projected_user_counts = []
    def log_message(self, *args):
        pass

    def do_GET(self):
        data = json.dumps({'data': [{'id': 'fixture-model'}]}).encode()
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self):
        body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        messages = body['messages']
        user = next((str(m.get('content', '')) for m in reversed(messages) if m['role'] == 'user'), '')
        if 'long fixture' in user:
            self.projected_user_counts.append(sum(m['role'] == 'user' for m in messages))
        if 'error fixture' in user:
            data = b'{"error":{"message":"Deliberate local generation failure"}}'
            self.send_response(503)
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data)
            return
        self.send_response(200)
        self.send_header('Content-Type', 'text/event-stream')
        self.send_header('Connection', 'close')
        self.end_headers()
        def event(delta, finish=None):
            self.wfile.write(('data: ' + json.dumps({'choices': [{'index': 0, 'delta': delta, 'finish_reason': finish}]}) + '\n\n').encode())
            self.wfile.flush()
        try:
            if 'cancel fixture' in user:
                event({'content': 'Starting'})
                for _ in range(30):
                    time.sleep(.1)
                    event({'content': '.'})
                event({}, 'stop')
            elif 'long fixture' in user:
                event({'content': 'Context retained.'})
                event({}, 'stop')
            elif 'remote fixture' in user:
                call_id = str(messages[-1].get('tool_call_id', ''))
                if messages[-1]['role'] != 'tool':
                    event({'tool_calls': [{'index': 0, 'id': 'fixture-remote-echo', 'type': 'function', 'function': {'name': 'cy_tools_call', 'arguments': '{"name":"remote.fixture.echo","arguments":{"value":"chat-remote-value"}}'}}]})
                    event({}, 'tool_calls')
                elif call_id == 'fixture-remote-echo':
                    assert 'chat-remote-value' in messages[-1]['content']
                    event({'tool_calls': [{'index': 0, 'id': 'fixture-remote-fail', 'type': 'function', 'function': {'name': 'cy_tools_call', 'arguments': '{"name":"remote.fixture.fail","arguments":{}}'}}]})
                    event({}, 'tool_calls')
                else:
                    assert 'RemoteToolError' in messages[-1]['content']
                    event({'content': 'Remote output chat-remote-value; RemoteToolError handled.'})
                    event({}, 'stop')
            elif messages[-1]['role'] == 'tool' and str(messages[-1].get('tool_call_id', '')).startswith('fixture-search'):
                event({'tool_calls': [{'index': 0, 'id': 'fixture-describe', 'type': 'function', 'function': {'name': 'cy_tools_describe', 'arguments': '{"name":"set_sequence"}'}}]})
                event({}, 'tool_calls')
            elif messages[-1]['role'] == 'tool' and str(messages[-1].get('tool_call_id', '')).startswith('fixture-describe'):
                event({'tool_calls': [{'index': 0, 'id': 'fixture-edit', 'type': 'function', 'function': {'name': 'cy_tools_call', 'arguments': '{"name":"set_sequence","arguments":{"name":"Assistant fixture edit"}}'}}]})
                event({}, 'tool_calls')
            elif messages[-1]['role'] == 'tool':
                event({'content': 'Updated'})
                time.sleep(.12)
                event({'content': ' sequence.'})
                time.sleep(.12)
                event({}, 'stop')
            else:
                event({'tool_calls': [{'index': 0, 'id': 'fixture-search', 'type': 'function', 'function': {'name': 'cy_tools_search', 'arguments': '{"query":"sequence"}'}}]})
                event({}, 'tool_calls')
            self.wfile.write(b'data: [DONE]\n\n')
            self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass

fixture = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Fixture)
thread = threading.Thread(target=fixture.serve_forever, daemon=True)
thread.start()

def run(arguments, expected=0):
    result = subprocess.run([str(value) for value in arguments], capture_output=True, text=True,
                            encoding='utf-8', timeout=70, creationflags=flags, cwd=work)
    assert result.returncode == expected, (result.returncode, result.stdout[:1000], result.stderr[:1000])
    data = json.loads(result.stdout)
    if expected:
        assert result.stderr and not data['ok']
    return data

try:
    result = subprocess.run([str(native), f'http://127.0.0.1:{fixture.server_port}/v1', str(work / 'private')],
                            capture_output=True, text=True, encoding='utf-8', timeout=100, creationflags=flags, cwd=work)
    assert result.returncode == 0, (result.stdout, result.stderr)
    metric_files = list((work/'private').glob('performance-*.jsonl'))
    assert metric_files, 'Performance JSONL must be generated only when enabled'
    for metric_file in metric_files:
        metadata = metric_file.read_text('utf-8')
        assert 'Sensitive body sentinel' not in metadata and 'fixture-owner' not in metadata
        for line in metadata.splitlines():
            row = json.loads(line)
            assert not set(row).intersection({'messages','arguments','result','token','principal'})
    assert Fixture.projected_user_counts and max(Fixture.projected_user_counts) < 8, 'API context must be reduced while the app retains full history'
    print(result.stdout.strip())
    assert run([cli, 'help'])['ok']
    assert len(run([cli, 'list'])['operations']) == 163
    assert run([cli, 'describe', 'set_sequence'])['operation']['inputSchema']['type'] == 'object'
    assert len(run([exe, '--cli', 'list'])['operations']) == 163
    run([cli, 'invoke', 'set_sequence', '--json={"name":"Denied"}', '--warp'], 3)
    run([cli, 'invoke', 'status', '--json={bad'], 2)
    run([cli, 'serve', '--port=70000'], 2)
    run([cli, 'serve', '--port=invalid'], 2)
    run([cli, 'list', 'unexpected'], 2)
    run([cli, 'invoke', 'status', '--connect='], 2)
    saved = work / 'cli.sframe'
    run([cli, 'invoke', 'set_sequence', '--json={"name":"CLI saved edit"}', '--allow-changes', '--save='+str(saved), '--warp'])
    assert json.loads(saved.read_text('utf-8'))['name'] == 'CLI saved edit'
    for version in ('2024-11-05', '2025-03-26'):
        legacy = subprocess.Popen([str(exe), '--hidden', '--warp', '--mcp'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding='utf-8', cwd=work, creationflags=flags)
        try:
            legacy.stdin.write(json.dumps({'jsonrpc':'2.0','id':1,'method':'initialize','params':{'protocolVersion':version,'capabilities':{},'clientInfo':{'name':'legacy-fixture','version':'1'}}})+'\n')
            legacy.stdin.flush()
            assert json.loads(legacy.stdout.readline())['result']['protocolVersion'] == version
            legacy.stdin.write('{"jsonrpc":"2.0","id":2,"method":"tools/list"}\n')
            legacy.stdin.flush()
            assert len(json.loads(legacy.stdout.readline())['result']['tools']) >= 163
            legacy.stdin.close()
            assert legacy.wait(timeout=10) == 0
        finally:
            if legacy.poll() is None:
                legacy.kill()
                legacy.wait()
    descriptor = work / ('instance-'+uuid.uuid4().hex+'.json')
    process = subprocess.Popen([str(exe), '--hidden', '--warp', '--mcp', '--mcp-http', '--mcp-info='+str(descriptor)],
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               text=True, encoding='utf-8', cwd=work, creationflags=flags)
    try:
        until = time.monotonic() + 20
        while not descriptor.is_file() and process.poll() is None and time.monotonic() < until:
            time.sleep(.03)
        assert descriptor.is_file(), ('No GUI instance descriptor', process.poll())
        connection = json.loads(descriptor.read_text())
        process.stdin.write('{"jsonrpc":"2.0","id":0,"method":"initialize","params":{"protocolVersion":"2025-06-18","clientInfo":{"name":"assistant-fixture","version":"1"},"capabilities":{}}}\n')
        process.stdin.flush()
        assert 'result' in json.loads(process.stdout.readline())
        request = urllib.request.Request(connection['url']+'/v1', data=b'{"method":"describe"}', headers={'Content-Type':'application/json'})
        try:
            urllib.request.urlopen(request, timeout=3)
            raise AssertionError('Unauthenticated access accepted')
        except urllib.error.HTTPError as error:
            assert error.code == 401
        bad_descriptor = work / ('bad-'+uuid.uuid4().hex+'.json')
        bad_descriptor.write_text(json.dumps(dict(connection, token='deliberately-invalid-fixture-token')), encoding='utf-8')
        try:
            run([cli, 'invoke', 'status', '--connect='+str(bad_descriptor)], 4)
        finally:
            bad_descriptor.unlink()
        run([cli, 'invoke', 'set_sequence', '--json={"name":"Connected CLI edit"}', '--connect='+str(descriptor)])
        process.stdin.write('{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"status","arguments":{}}}\n')
        process.stdin.flush()
        response = json.loads(process.stdout.readline())
        assert response['result']['structuredContent']['project']['name'] == 'Connected CLI edit', response
        process.stdin.write('{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"undo","arguments":{}}}\n')
        process.stdin.flush()
        response = json.loads(process.stdout.readline())
        assert not response['result'].get('isError'), response
        assert run([cli, 'invoke', 'status', '--connect='+str(descriptor)])['result']['project']['name'] != 'Connected CLI edit'
        process.stdin.close()
        assert process.wait(timeout=15) == 0
        assert not descriptor.exists(), 'Owned connection descriptor must be removed on normal shutdown'
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
    # A second real GUI instance defaults to read-only automation.
    readonly = subprocess.Popen([str(exe), '--hidden', '--warp', '--mcp-http', '--mcp-info='+str(descriptor), '--ui-snapshot='+str(work/'readonly.png'), '--snapshot-frames=900'],
                                stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, cwd=work, creationflags=flags)
    try:
        until = time.monotonic()+20
        while not descriptor.is_file() and readonly.poll() is None and time.monotonic()<until:
            time.sleep(.03)
        assert descriptor.is_file()
        run([cli, 'invoke', 'status', '--connect='+str(descriptor)])
        run([cli, 'invoke', 'set_sequence', '--json={"name":"Denied from CLI"}', '--connect='+str(descriptor)], 3)
    finally:
        if readonly.poll() is None:
            readonly.kill()
            readonly.wait()
        if descriptor.exists():
            assert descriptor.resolve().is_relative_to(work.resolve())
            descriptor.unlink()
    print('Real CLI/GUI/MCP: discovery, schemas, JSON, exit codes, headless save, same-instance editing/undo, authorization and refusal passed')
finally:
    fixture.shutdown()
    fixture.server_close()
