"""Drive real hidden GUI controls through the application's authenticated MCP API."""
import json
import http.server
import os
import subprocess
import sys
import time
import threading
import urllib.request
import uuid
from pathlib import Path

exe, cli = (Path(p).resolve() for p in sys.argv[1:3])
work = exe.parent/'test-output/ui-automation'
work.mkdir(parents=True, exist_ok=True)
flags = subprocess.CREATE_NO_WINDOW

class Provider(http.server.BaseHTTPRequestHandler):
    gui_tool_completed = False
    def log_message(self, *args):
        pass
    def do_GET(self):
        data = b'{"data":[{"id":"fixture-model"}]}'
        self.send_response(200)
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)
    def do_POST(self):
        assert not self.headers.get('Authorization'), 'Synthetic provider must not receive a real credential'
        body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        assert any(t['function']['name']=='cy_tools_call' for t in body['tools'])
        self.send_response(200)
        self.send_header('Content-Type','text/event-stream')
        self.send_header('Connection','close')
        self.end_headers()
        def event(delta, finish=None):
            self.wfile.write(('data: '+json.dumps({'choices':[{'index':0,'delta':delta,'finish_reason':finish}]})+'\n\n').encode())
            self.wfile.flush()
        if body['messages'][-1]['role']=='tool':
            assert 'error' not in json.loads(body['messages'][-1]['content'])
            Provider.gui_tool_completed = True
            event({'content':'Editor switched through the real assistant.'})
            event({}, 'stop')
        else:
            event({'tool_calls':[{'index':0,'id':'ui-fixture','type':'function','function':{'name':'cy_tools_call','arguments':'{"name":"ui_workspace","arguments":{"mode":"color"}}'}}]})
            event({}, 'tool_calls')
        self.wfile.write(b'data: [DONE]\n\n')

provider = http.server.ThreadingHTTPServer(('127.0.0.1',0), Provider)
threading.Thread(target=provider.serve_forever,daemon=True).start()

class App:
    def __init__(self, writes):
        self.descriptor = work/('instance-'+uuid.uuid4().hex+'.json')
        environment = os.environ.copy()
        environment.pop('OPENAI_API_KEY', None)
        self.process = subprocess.Popen([str(exe), '--hidden', '--warp', '--assistant', '--mcp-http',
            '--mcp-info='+str(self.descriptor), '--ui-snapshot='+str(work/'ui.png'), '--snapshot-frames=2500',
            *(['--allow-changes'] if writes else [])], cwd=work,
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, creationflags=flags, env=environment)
        until = time.monotonic()+20
        while not self.descriptor.is_file() and self.process.poll() is None and time.monotonic()<until:
            time.sleep(.02)
        assert self.descriptor.is_file(), 'GUI descriptor not created'
        self.connection = json.loads(self.descriptor.read_text())
        self.counter = 0
        self.session = None
        self.rpc('initialize', {'protocolVersion':'2025-06-18','capabilities':{},'clientInfo':{'name':'ui-fixture','version':'1'}})

    def rpc(self, method, params=None):
        self.counter += 1
        body = {'jsonrpc':'2.0','id':self.counter,'method':method}
        if params is not None:
            body['params'] = params
        request = urllib.request.Request(self.connection['url']+'/mcp', data=json.dumps(body).encode(),
            headers={'Authorization':'Bearer '+self.connection['token'],'Content-Type':'application/json'})
        if self.session:
            request.add_header('Mcp-Session-Id', self.session)
        with urllib.request.urlopen(request, timeout=20) as response:
            self.session = response.headers.get('Mcp-Session-Id', self.session)
            data = json.load(response)
        assert 'error' not in data, data
        return data['result']

    def call(self, name, arguments=None, error=None):
        result = self.rpc('tools/call', {'name':name,'arguments':arguments or {}})
        if error:
            assert result.get('isError') and json.loads(result['content'][0]['text'])['code']==error, result
            return result
        assert not result.get('isError'), result
        return result['structuredContent']

    def wait(self, predicate, seconds=10):
        until = time.monotonic()+seconds
        while time.monotonic()<until:
            value = predicate()
            if value:
                return value
            time.sleep(.025)
        raise AssertionError('UI state did not reach the requested result')

    def input(self, arguments):
        job = self.call('ui_input', arguments)['job']
        result = self.wait(lambda: (r if (r:=self.call('get_ui_input',{'job':job}))['state'] in ('completed','failed','cancelled') else None))
        assert result['state']=='completed', result
        return result

    def close(self):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait(timeout=10)
        if self.descriptor.exists():
            assert self.descriptor.resolve().is_relative_to(work.resolve())
            self.descriptor.unlink()

readonly = App(False)
try:
    readonly.call('get_ui_state')
    readonly.call('ui_command', {'id':'marker.add'}, error='AccessDenied')
    readonly.call('ui_panel', {'panel':'Assistant','visible':False}, error='AccessDenied')
    readonly.call('ui_input', {'action':'click','item':'12345678'}, error='AccessDenied')
    readonly.call('get_ui_native_controls', {'window':'0:1'}, error='AccessDenied')
finally:
    readonly.close()

app = App(True)
try:
    names = {tool['name'] for tool in app.rpc('tools/list')['tools']}
    assert {'ui_input','ui_clip_invoke','ui_native_control','ui_panel'}.issubset(names)
    initial = app.call('get_ui_state')
    assert len(initial['panels']) >= 25
    app.call('ui_workspace', {'mode':'audio'})
    assert app.call('get_ui_state')['mode']=='audio'
    app.call('ui_workspace', {'mode':'edit'})
    app.call('ui_panel', {'panel':'Source','visible':True})
    assert next(p for p in app.call('get_ui_state')['panels'] if p['panel']=='Source')['visible']
    app.call('ui_panel', {'panel':'Source','visible':False})
    app.call('ui_panel', {'panel':'Timeline','dock_to':'Composition'})
    app.wait(lambda: next(p for p in app.call('get_ui_state')['panels'] if p['panel']=='Timeline').get('dock') ==
                    next(p for p in app.call('get_ui_state')['panels'] if p['panel']=='Composition').get('dock'))
    app.call('ui_workspace', {'reset_layout':True})
    app.call('ui_view', {'view':'timeline','zoom':143,'first_visible':1.25,'snap':False})
    assert app.call('get_ui_state')['timeline']['pixels_per_second']==143
    before = app.call('get_ui_state')['timeline']['loop']
    app.call('ui_command', {'id':'loop.toggle'})
    assert app.call('get_ui_state')['timeline']['loop'] != before
    commands = app.call('list_ui_commands')['commands']
    assert any(c['id']=='marker.add' for c in commands)
    app.call('ui_command', {'id':'command/Open Project...'}, error='PathRequired')
    app.call('set_sequence', {'name':'Preserve dirty work'})
    app.call('ui_command', {'id':'command/New project'}, error='ConfirmationRequired')
    assert app.call('status')['project']['name']=='Preserve dirty work'
    app.call('ui_command', {'id':'command/Save Project','file_path':str(work/'saved.sframe')})
    assert (work/'saved.sframe').is_file()
    app.call('new_project', {'name':'UI automation','width':320,'height':180,'duration':3})
    clip = app.call('add_generator', {'duration':3})['clip']
    app.call('ui_focus', {'clip':clip})
    app.call('ui_clip_editor', {'action':'open','clip':clip})
    app.call('ui_clip_invoke', {'workspace':[clip],'operation':'set_sequence','arguments':{'name':'Nested real draft'}})
    assert app.call('get_ui_clip_project', {'workspace':[clip]})['project']['name']=='Nested real draft'
    app.call('ui_clip_editor', {'action':'reload'}, error='ConfirmationRequired')
    app.call('ui_clip_editor', {'action':'apply'})
    assert app.call('get_clip_composition', {'clip':clip})['project']['name']=='Nested real draft'
    app.call('ui_panel', {'panel':'Clip Editor','visible':False})
    app.call('ui_panel', {'panel':'Assistant','visible':True,'focus':True})
    item = app.wait(lambda: next((r for r in app.call('get_ui_items',{'window':'Assistant','query':'AI may use'})['items'] if r['checkable']),None))
    checked = item['checked']
    app.input({'action':'click','item':item['id']})
    app.wait(lambda: any(r['checked']!=checked for r in app.call('get_ui_items',{'query':'AI may use'})['items']))
    model = app.wait(lambda: next((r for r in app.call('get_ui_items',{'window':'Assistant','query':'##model'})['items'] if r['inputable'] and r['label']=='##model'),None))
    app.input({'action':'text','item':model['id'],'text':'fixture-model'})
    app.wait(lambda: any(r.get('active_text')=='fixture-model' for r in app.call('get_ui_items',{'window':'Assistant','query':'##model','include_active_text':True})['items']))
    cancel = app.call('ui_input', {'action':'drag','item':model['id'],'to_x':model['rect'][2]-2,'to_y':model['rect'][1]+4,'frames':60})
    cancelled = app.call('ui_cancel_input', {'job':cancel['job']})
    assert cancelled['state']=='cancelled' and not cancelled['held_keys'] and not cancelled['held_buttons']
    app.input({'action':'scroll','window':model['window'],'wheel_y':-20})
    permissions = app.wait(lambda: next((r for r in app.call('get_ui_items',{'window':'Assistant','query':'Permissions'})['items']),None))
    app.input({'action':'click','item':permissions['id']})
    app.input({'action':'scroll','window':model['window'],'wheel_y':-20})
    discard = app.wait(lambda: next((r for r in app.call('get_ui_items',{'window':'Assistant','query':'Allow replacing once'})['items']),None))
    job = app.call('ui_input',{'action':'click','item':discard['id']})['job']
    refused = app.wait(lambda: (r if (r:=app.call('get_ui_input',{'job':job}))['state']=='failed' else None))
    assert refused['error_code']=='ConfirmationRequired'
    app.call('ui_command', {'id':'command/New project'}, error='ConfirmationRequired')
    app.input({'action':'scroll','window':model['window'],'wheel_y':20})
    app.call('ui_preferences', {'locale':'fr'})
    assert app.call('get_ui_state')['locale']=='fr'
    assert any(c['id']=='marker.add' for c in app.call('list_ui_commands')['commands'])
    app.call('ui_preferences', {'locale':'en'})
    app.call('ui_shortcut', {'id':'marker.add','key':'G','ctrl':True})
    assert next(k for k in app.call('list_ui_shortcuts')['shortcuts'] if k['id']=='marker.add')['chord']=='Ctrl+G'
    app.call('ui_shortcut', {'reset':True})
    assert len(app.call('ui_file_list', {'path':str(work),'limit':1})['entries'])==1
    app.call('get_ui_native_windows')
    app.call('ui_input', {'action':'click','item':'ffffffff'}, error='NotReady')
    # A live connected CLI discovers these same editor extensions.
    result = subprocess.run([str(cli),'describe','ui_panel','--connect='+str(app.descriptor)],capture_output=True,text=True,creationflags=flags,timeout=15)
    assert result.returncode==0 and json.loads(result.stdout)['operation']['name']=='ui_panel'
    result = subprocess.run([str(cli),'invoke','ui_view','--json={"view":"timeline","zoom":155}','--connect='+str(app.descriptor)],capture_output=True,text=True,creationflags=flags,timeout=15)
    assert result.returncode==0 and app.call('get_ui_state')['timeline']['pixels_per_second']==155
    # Configure and use the actual integrated chat entirely through its real controls.
    def item(query, exact=None):
        return app.wait(lambda: next((r for r in app.call('get_ui_items',{'window':'Assistant','query':query})['items'] if exact is None or r['label']==exact),None))
    tools = item('AI may use application tools')
    if not tools['checked']:
        app.input({'action':'click','item':tools['id']})
    base = item('##baseurl', '##baseurl')
    app.input({'action':'text','item':base['id'],'text':f'http://127.0.0.1:{provider.server_port}/v1'})
    advanced = item('Advanced connection')
    app.input({'action':'click','item':advanced['id']})
    app.input({'action':'scroll','window':advanced['window'],'wheel_y':-10})
    provider_id = item('##provider-id','##provider-id')
    app.input({'action':'text','item':provider_id['id'],'text':'openai-compatible'})
    app.input({'action':'scroll','window':advanced['window'],'wheel_y':20})
    connect = item('Connect / New chat')
    app.input({'action':'click','item':connect['id']})
    settings = item('Assistant settings')
    app.input({'action':'click','item':settings['id']})
    composer = app.wait(lambda: next((r for r in app.call('get_ui_items',{'window':'Assistant','query':'##input'})['items'] if r['inputable'] and not r['disabled']),None))
    app.input({'action':'text','item':composer['id'],'text':'Switch the editor to color grading.','submit':True})
    app.wait(lambda: Provider.gui_tool_completed and app.call('get_ui_state')['mode']=='color', seconds=20)
    print('Real GUI UI automation: read-only denial, shared discovery, modes, panels/docking, navigation, commands, path injection, dirty-project confirmation, nested drafts/apply, actual checkbox click, text input, locales, keymap and connected CLI passed')
    print('Actual integrated GUI chat configured through real controls: compact API tool changed the live workspace and returned its result')
finally:
    app.close()
    provider.shutdown()
