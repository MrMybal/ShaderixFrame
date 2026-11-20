"""Exercise access-profile migration and tokenless calls against real editor processes."""
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import urllib.error
import urllib.request
import uuid

exe, cli = (Path(p).resolve() for p in sys.argv[1:3])
work = exe.parent / 'test-output/access-profiles' / uuid.uuid4().hex
work.mkdir(parents=True)
flags = subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0
environment = dict(os.environ, LOCALAPPDATA=str(work / 'local'))


def run(arguments, expected=0):
    result = subprocess.run([str(a) for a in arguments], capture_output=True, text=True,
                            encoding='utf-8', cwd=work, env=environment, creationflags=flags, timeout=30)
    assert result.returncode == expected, (result.returncode, result.stdout, result.stderr)
    return json.loads(result.stdout)


class Editor:
    def __init__(self, config, writes=False, default=False):
        self.descriptor = work / (uuid.uuid4().hex + '-instance.json')
        self.config = work / (uuid.uuid4().hex + '-config.json')
        self.config.write_text(json.dumps(config), encoding='utf-8')
        self.process = subprocess.Popen([str(exe), '--hidden', '--warp', '--mcp', '--mcp-http',
            '--mcp-info=' + str(self.descriptor),
            *([] if default else ['--automation-config=' + str(self.config)]),
            *(['--allow-changes'] if writes else [])], cwd=work, env=environment,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, encoding='utf-8', creationflags=flags)
        try:
            until = time.monotonic() + 20
            while not self.descriptor.is_file() and self.process.poll() is None and time.monotonic() < until:
                time.sleep(.02)
            assert self.descriptor.is_file(), 'Real application did not publish its endpoint'
            self.connection = json.loads(self.descriptor.read_text('utf-8'))
            self.base = self.connection['url']
            assert self.base.startswith('http://127.0.0.1:')
            self.session = None
            self.counter = 0
        except BaseException:
            self.close()
            raise

    def post(self, path, body, token=None, status=200, extra=None):
        headers = {'Content-Type': 'application/json', **(extra or {})}
        if token is not None:
            headers['Authorization'] = 'Bearer ' + token
        if self.session:
            headers['Mcp-Session-Id'] = self.session
        request = urllib.request.Request(self.base + path, data=json.dumps(body).encode(), headers=headers)
        try:
            with urllib.request.urlopen(request, timeout=10) as response:
                assert status == response.status
                self.session = response.headers.get('Mcp-Session-Id', self.session)
                return json.load(response)
        except urllib.error.HTTPError as error:
            assert error.code == status, (error.code, status)
            return None

    def rpc(self, method, params=None, token=None, status=200):
        self.counter += 1
        body = {'jsonrpc': '2.0', 'id': self.counter, 'method': method}
        if params is not None:
            body['params'] = params
        result = self.post('/mcp', body, token, status)
        if status != 200:
            return
        assert 'error' not in result, result
        return result['result']

    def call(self, name, args=None, token=None, error=None):
        result = self.rpc('tools/call', {'name': name, 'arguments': args or {}}, token)
        if error:
            assert result.get('isError'), result
            detail = json.loads(result['content'][0]['text'])
            assert detail['code'] == error, detail
            return detail
        assert not result.get('isError'), result
        return result['structuredContent']

    def close(self):
        if self.process.poll() is None:
            self.process.stdin.close()
            try:
                assert self.process.wait(timeout=12) == 0
            finally:
                if self.process.poll() is None:
                    self.process.kill()
                    self.process.wait()
        if hasattr(self, 'connection'):
            assert not self.descriptor.exists(), 'Descriptor must be removed on normal shutdown'


initialize = {'protocolVersion': '2025-06-18', 'capabilities': {},
              'clientInfo': {'name': 'access-profile-test', 'version': '1'}}

# No configuration and legacy configurations with no explicit choice both migrate to Free.
for default, config in ((True, {}), (False, {'compact': False}),
                        (False, {'access_profile': 'free'}), (False, {'access_profile': 'token_only'}),
                        (False, {'access_profile': 'local_controlled'}), (False, {'access_profile': 'protected'})):
    app = Editor(config, default=default)
    try:
        profile = config.get('access_profile', 'free')
        assert app.connection['access_profile'] == profile
        secured = profile in ('token_only', 'protected')
        controlled = profile in ('local_controlled', 'protected')
        token = app.connection.get('token')
        assert bool(token) == secured
        if secured:
            app.rpc('initialize', initialize, status=401)
            app.rpc('initialize', initialize, token='invalid-test-token', status=401)
            app.post('/v1', {'method': 'describe'}, status=401)
            run([cli, 'invoke', 'status', '--connect=' + app.base], 4)
        app.rpc('initialize', initialize, token)
        tools = app.rpc('tools/list', token=token)['tools']
        assert len(tools) == 186
        assert app.call('status', token=token)['ok']
        assert len(run([cli, 'list', '--connect=' + str(app.descriptor)])['operations']) == 186
        if controlled:
            app.call('set_sequence', {'name': 'Forbidden'}, token, error='AccessDenied')
            app.call('ui_workspace', {'mode': 'audio'}, token, error='AccessDenied')
            run([cli, 'invoke', 'set_sequence', '--json={"name":"Forbidden"}',
                 '--connect=' + str(app.descriptor)], 3)
        else:
            # Neither a write flag nor a second confirmation is supplied.
            app.call('set_sequence', {'name': 'Address-only edit'}, token)
            assert app.call('status', token=token)['project']['name'] == 'Address-only edit'
            app.call('set_sequence', {'width': 'invalid'}, token, error='InvalidParameters')
            assert app.call('status', token=token)['project']['name'] == 'Address-only edit'
            app.call('new_project', {'name': 'No extra confirmation'}, token)
            assert app.call('status', token=token)['project']['name'] == 'No extra confirmation'
            app.call('ui_workspace', {'mode': 'audio'}, token)
            assert app.call('get_ui_state', token=token)['mode'] == 'audio'
            if default:
                clip = app.call('add_generator', {'duration': 2}, token)['clip']
                app.call('ui_clip_editor', {'action': 'open', 'clip': clip}, token)
                app.call('ui_clip_invoke', {'workspace': [clip], 'operation': 'set_sequence',
                         'arguments': {'name': 'Free nested draft'}}, token)
                app.call('ui_clip_invoke', {'workspace': [clip], 'operation': 'new_project',
                         'arguments': {'name': 'Free nested replacement'}}, token)
                assert app.call('get_ui_clip_project', {'workspace': [clip]}, token)['project']['name'] == 'Free nested replacement'
                app.call('ui_clip_editor', {'action': 'reload'}, token)
            target = str(app.descriptor) if secured else app.base + '/mcp'
            run([cli, 'invoke', 'set_sequence', '--json={"name":"CLI address edit"}', '--connect=' + target])
            assert app.call('status', token=token)['project']['name'] == 'CLI address edit'
            # The simultaneous stdio client sees the same live state.
            for request in ({'jsonrpc':'2.0','id':91,'method':'initialize','params':initialize},
                            {'jsonrpc':'2.0','id':92,'method':'tools/call','params':{'name':'status','arguments':{}}}):
                app.process.stdin.write(json.dumps(request) + '\n')
                app.process.stdin.flush()
                response = json.loads(app.process.stdout.readline())
            assert response['result']['structuredContent']['project']['name'] == 'CLI address edit'
            app.post('/v1', {'method': 'describe'}, token, status=403, extra={'Origin': 'https://untrusted.example'})
    finally:
        app.close()

# An explicit Protected selection still admits writes when the instance owner enables them.
app = Editor({'access_profile': 'protected'}, writes=True)
try:
    token = app.connection['token']
    app.rpc('initialize', initialize, token)
    app.call('set_sequence', {'name': 'Protected authorized edit'}, token)
    app.call('new_project', token=token, error='ConfirmationRequired')
finally:
    app.close()

# A profile saved in the default per-user configuration is also an explicit choice.
settings = work / 'local/ShaderixFrame/Assistant/automation.json'
settings.parent.mkdir(parents=True, exist_ok=True)
settings.write_text('{"access_profile":"protected"}', encoding='utf-8')
app = Editor({}, default=True)
try:
    assert app.connection['access_profile'] == 'protected'
    app.rpc('initialize', initialize, status=401)
    token = app.connection['token']
    app.rpc('initialize', initialize, token)
    app.call('set_sequence', {'name': 'Denied'}, token, error='AccessDenied')
finally:
    app.close()
    settings.unlink()

# Both standalone CLI entry points share Free, while explicit controlled profiles retain refusals.
saved = work / 'free-cli.sframe'
run([cli, 'invoke', 'set_sequence', '--json={"name":"Free standalone"}', '--save=' + str(saved), '--warp'])
assert json.loads(saved.read_text('utf-8'))['name'] == 'Free standalone'
run([exe, '--cli', 'invoke', 'set_sequence', '--json={"name":"GUI CLI Free"}', '--warp'])
for profile in ('local_controlled', 'protected'):
    run([cli, 'invoke', 'set_sequence', '--json={"name":"Denied"}', '--mcp-profile=' + profile, '--warp'], 3)
run([cli, 'invoke', 'set_sequence', '--json={"width":"invalid"}', '--warp'], 2)
run([cli, 'list', '--mcp-profile=typo'], 2)
print('Real editor: all four profiles, Free defaults/migration, address-only HTTP/CLI, shared stdio state, validation, controlled permissions and Protected authentication passed')
