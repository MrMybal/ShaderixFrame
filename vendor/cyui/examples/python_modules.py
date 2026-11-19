"""Run with `python examples/python_modules.py`, or embed ModuleGallery in a host.

The host may supply ImageData and a CEFView; it owns textures and the CEF runtime.
"""
from pathlib import Path
import math
import itertools
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'python'))
from cyui_modules.common import Range
from cyui_modules.canvas import Canvas, CanvasState
from cyui_modules.plots import Axis, PlotState, Series, ColorRamp, plot, histogram, heatmap
from cyui_modules.curves import CurvePoint, CurveState, curve_editor
from cyui_modules.nodes import Node, Pin, Link, NodeState, NodeGroup, NodeReroute, node_editor
from cyui_modules.timeline import Lane, Clip, Marker, TimelineState, timeline
from cyui_modules.audioviews import make_peaks, WaveformState, waveform, spectrum, oscilloscope, stereo_scope
from cyui_modules.audioviews import (SpectrogramData, spectrogram, LoudnessData, loudness,
    TruePeakChannel, true_peak, correlation, ResponseState, ResponsePoint, filter_response, DynamicsData, dynamics)
from cyui_modules.audioviews import (SpectrogramHistory, SpectrogramState, SpectrumBand, band_spectrum,
    PersistenceOptions, StereoDensityState, stereo_density, SpatialSample, SpatialScopeState, spatial_scope)
from cyui_modules.audio import knob, fader, meter, MeterState
from cyui_modules.code import CodeDocument, CodeViewState, CodeEditor, code_viewer
from cyui_modules.diff import DiffDocument, DiffState, diff_viewer
from cyui_modules.inspector import Property, InspectorState, inspector
from cyui_modules.assets import Asset, AssetState, asset_browser
from cyui_modules.images import ImageData, ImageState, GradientStop, GradientState, image_viewport, gradient_editor
from cyui_modules.viewport import ViewportState, TransformState, Transform2D, viewport, transform_gizmo
from cyui_modules.commands import Command, CommandState, command_palette
from cyui_modules.diagnostics import LogBuffer, LogState, Diagnostic, log_view, diagnostic_list
from cyui_modules.documents import Document, DocumentState, TranscriptMessage, TranscriptState, document_view, transcript
from cyui_modules.web import web_panel, theme_message
from cyui_modules.documenteditor import DocumentEditor
from cyui_modules.media import MediaSnapshot, MediaPlayerState, media_player
from cyui_modules.documents import DocumentServices, DocumentOptions


class ModuleGallery:
    def __init__(self, image=None, web=None):
        self.active_page = 'Canvas'
        self.initialized = False
        self.image, self.web = image or ImageData(0, 256, 256), web
        self.canvas = CanvasState()
        self.plot = PlotState(Axis(Range(0, 10)), Axis(Range(-1.2, 1.2)))
        self.signal = [Series(1, [(n/30, math.sin(n/30)) for n in range(300)], 'Signal')]
        self.curves = CurveState(); self.curves.view.x.view = Range(0, 10)
        self.keys = [CurvePoint(1, 0, .2), CurvePoint(2, 3, .8, 'smooth'), CurvePoint(3, 8, .4)]
        self.nodes = [Node(1, (30, 40), 'Source', outputs=[Pin(11, 'Output')]), Node(2, (320, 60), 'Processor', [Pin(21, 'Input')], [Pin(22, 'Output')], 1)]
        self.links, self.node_state = [Link(1, 11, 21)], NodeState()
        self.node_ids = itertools.count(100)
        self.node_groups = [NodeGroup(50, (10, 5), (530, 180), 'Signal'), NodeGroup(51, (20, 30), (200, 140), 'Source')]
        self.node_routes = [NodeReroute(52, 1, (280, 100))]
        self.lanes = [Lane(1, 'Video', 70), Lane(2, 'Audio', 80)]
        self.clips = [Clip(1, 1, 1, 3, 'Opening'), Clip(2, 2, .5, 7, 'Sound', 1)]
        self.timeline, self.wave = TimelineState(), WaveformState()
        self.peaks = make_peaks([math.sin(n*.04)*math.sin(n*.0001) for n in range(48000)], 1200)
        self.spectrum = PlotState(Axis(Range(20, 20000), True), Axis(Range(-90, 0)))
        self.spectrum_data = [Series(2, [(20*1000**(n/159), -60+35*math.exp(-math.log(20*1000**(n/159)/1000)**2)) for n in range(160)], 'Spectrum', color_index=1)]
        self.spectrogram = SpectrogramData([
            max([-92+5*math.sin(t*.3+f*.71)]+[-15-h*3-60*(math.log((20*1000**((f+.5)/48))/((180+100*math.sin(t*.08))*h))/.13)**2 for h in range(1, 13)])
            for t in range(96) for f in range(48)], 96, 48, Range(0, 8), logarithmic_bins=True)
        self.spectrogram_view = PlotState(Axis(Range(0, 8)), Axis(Range(20, 20000), True))
        self.loudness_view = PlotState(Axis(Range(0, 120)), Axis(Range(-60, 0)))
        self.loudness_data = LoudnessData(-16.2, -18.1, -19.3, -25, -17, [Series(1, [(n, -19+4*math.sin(n*.12)) for n in range(121)], 'Momentary', color_index=1)])
        self.peak_channels = [TruePeakChannel('L', -1.3, .4, 3), TruePeakChannel('R', -2.1, -.6)]
        self.response = ResponseState(); self.bands = [ResponsePoint(1, 120, 4), ResponsePoint(2, 1800, -7), ResponsePoint(3, 8000, 3)]
        self.dynamics_view = PlotState(Axis(Range(-60, 0)), Axis(Range(-60, 0)))
        self.transfer = [Series(1, [(x, x if x < -18 else -18+(x+18)/4) for x in range(-60, 1)], 'Output')]
        self.correlation_view = PlotState(Axis(Range(0, 120)), Axis(Range(-1, 1)))
        self.coherence = [Series(1, [(n, .55+.3*math.sin(n*.08)) for n in range(121)], 'Correlation', color_index=2)]
        self.source, self.code_state = CodeDocument('// Shared code\nfloat gain = 0.5;\n', 'cpp'), CodeViewState()
        self.editor = CodeEditor('// Edit with native ImGui undo\nfloat value = 42;\n', 'cpp')
        self.diff, self.diff_state = DiffDocument(), DiffState()
        self.diff.set_texts('float gain = 0.5;\n', 'float gain = 0.8;\nfloat pan = 0;\n', 'cpp')
        self.properties = [Property(1, 'Enabled', True), Property(2, 'Exact integer', 9007199254740993), Property(3, 'Gain', .5, minimum=0, maximum=1), Property(4, 'Name', 'Example'), Property(5, 'Mode', 'Preview', choices=('Preview', 'Final'))]
        self.inspector, self.assets = InspectorState(), AssetState()
        from cyui_modules.filters import TextFilter, FilterMenuItem
        self.property_filter, self.menu_filter = TextFilter(), TextFilter('export')
        self.menu_items = [FilterMenuItem(label='File', children=[FilterMenuItem(1, 'Save', 'Ctrl+S'),
                           FilterMenuItem(label='Export', children=[FilterMenuItem(2, 'Export PNG'), FilterMenuItem(3, 'Export HDR', enabled=False)])])]
        self.asset_items = [Asset(1, 'Images', folder=True), Asset(2, 'Sounds', folder=True), Asset(3, 'Landscape', 1, 'Image')]
        self.image_state, self.gradient_state, self.stops = ImageState(), GradientState(), []
        self.viewport, self.transform_state = ViewportState(), TransformState()
        self.transform = Transform2D((150, 90), (150, 70), .2)
        self.commands, self.command_state, self.command_open = [Command(1, 'Open document', 'Ctrl+O', 'File'), Command(2, 'Save document', 'Ctrl+S', 'File')], CommandState(), False
        self.log, self.log_state = LogBuffer(), LogState()
        self.log.append('info', 'Application initialized'); self.log.append('warning', 'Example warning')
        self.document = Document('# Shared documents\nA paragraph with **emphasis**, `code` and [a link](https://example.com).\n| A | B |\n|---|---|\n| 1 | 2 |\n```cpp\nfloat gain = 0.5;\n```')
        self.document_state, self.chat = DocumentState(), TranscriptState()
        self.document.set_markdown('# Native Markdown\n\n**Bold**, *italic*, <u>underline</u>, ~~strike~~ and ||spoilers||.\n\n![Native image|320](gallery/image.png)\n\n- [x] Native textures\n- [ ] Review notes\n\n| Element | Display |\n|:---|---:|\n| Code | Syntax colors |\n| Media | Optional backend |\n')
        self.document_editor = DocumentEditor(self.document.text)
        self.media_state = MediaPlayerState()
        class ExampleMedia:
            def __init__(self): self.value = MediaSnapshot(duration=120, ready=True, seekable=True, title='Media player')
            def snapshot(self): return self.value
            def play(self, value): self.value.playing = value
            def seek(self, value): self.value.position = value
            def volume(self, value): self.value.volume = value
            def mute(self, value): self.value.muted = value
        self.media_backend = ExampleMedia()
        self.gain, self.db, self.meter = .5, -12., MeterState()
        self.spectral_ready = False

    def draw_spectral(self):
        from imgui_bundle import imgui as i
        import cyui as c
        if not self.spectral_ready:
            self.stream = SpectrogramHistory(); self.stream.reset(96, 96, 1/12, Range(20, 20000), True)
            self.stream_time = 0.
            self.spectral_horizontal, self.spectral_vertical = SpectrogramState(), SpectrogramState()
            self.spectral_color = 0
            self.filled_view = PlotState(Axis(Range(20, 20000), True), Axis(Range(-90, 0)))
            self.band_view = PlotState(Axis(Range(20, 20000), True), Axis(Range(-90, 0)))
            self.spatial = SpatialScopeState(); self.densities = [StereoDensityState() for _ in range(3)]
            self.spectral_ready = True
            for _ in range(96): self.append_spectral()
        self.append_spectral()
        _, self.spectral_color = c.combo(c.tr_label('Color map'), self.spectral_color, ['Intensity', 'Frequency', 'Monochrome', 'Theme colors'], c.dp(150))
        i.same_line()
        changed, self.spectral_horizontal.follow_latest = i.checkbox(c.tr_label('Follow latest'), self.spectral_horizontal.follow_latest)
        if changed: self.spectral_vertical.follow_latest = self.spectral_horizontal.follow_latest
        i.same_line()
        if i.small_button(c.tr_label('Clear persistence')):
            self.spatial.density.clear()
            for state in self.densities: state.density.clear()
        ramp = ColorRamp(['intensity', 'frequency', 'monochrome', 'themed'][self.spectral_color])
        def level(hz): return -72+36*math.exp(-(math.log(hz/150)/1.1)**2)+29*math.exp(-(math.log(hz/2200)/1.8)**2)
        left, right = [], []
        for n in range(240):
            hz = 20*1000**(n/239)
            left.append((hz, level(hz)+3*math.sin(n*.7))); right.append((hz, level(hz)-7+2*math.cos(n*.4)))
        if i.begin_table('spectral', 2, i.TableFlags_.sizing_stretch_same):
            i.table_next_column(); c.section_title(c.tr('Filled spectra and gradients'))
            spectrum('filled', self.filled_view, [Series(1, left, 'L', 'area', baseline=-90, fill_alpha=.65, gradient=True, color_by_x=True, ramp=ColorRamp('frequency')), Series(2, right, 'R', 'area', 1, baseline=-90, fill_alpha=.65, gradient=True)], (0, 150))
            i.table_next_column(); c.section_title(c.tr('Band spectrum and FFT overlay'))
            bands = [SpectrumBand(Range(20*1000**(n/30), 20*1000**((n+1)/30)), level(20*1000**((n+.5)/30))-5) for n in range(30)]
            band_spectrum('bands', self.band_view, bands, [Series(2, left, 'FFT', color_index=1)], (0, 150), label='RTA')
            data = self.stream.view()
            i.table_next_column(); c.section_title(c.tr('Vertical scrolling spectrogram'))
            spectrogram('vertical', self.spectral_vertical, data, (0, 164), orientation='time_vertical', reverse_time=True, legend=True, ramp=ramp, color_by_frequency=self.spectral_color == 1)
            i.table_next_column(); c.section_title(c.tr('Horizontal scrolling spectrogram'))
            spectrogram('horizontal', self.spectral_horizontal, data, (0, 164), legend=True, ramp=ramp, color_by_frequency=self.spectral_color == 1)
            i.table_next_column(); c.section_title(c.tr('Spatial stereo spectrogram'))
            phase = i.get_time()
            cloud = [SpatialSample(20*1000**(f/95), .15*math.sin(f*.08+phase)+(.025+.6*f/95)*math.sin(n*1.7+f*.3+phase), -12-f*.6+6*math.sin(f*.2+n)) for f in range(96) for n in range(8)]
            spatial_scope('spatial', self.spatial, cloud, i.get_frame_count(), (0, 154), persistence=PersistenceOptions(resolution=128, exposure=.06, point_radius=3))
            i.table_next_column(); c.section_title(c.tr('Persistent vectorscope'))
            for k, name in enumerate(['Stereo', 'Mono', 'Out of phase']):
                i.push_id(k); i.begin_group()
                trace = []
                for n in range(720):
                    left_sample = .85*math.sin(n*.08+phase)
                    right_sample = left_sample if k == 1 else (-left_sample if k == 2 else .75*math.sin(n*.08+phase+.75))
                    trace.append((left_sample, right_sample))
                stereo_density('density', self.densities[k], trace, i.get_frame_count(), (145, 134), persistence=PersistenceOptions(axes=False))
                c.text_dim(c.tr(name)); i.end_group(); i.pop_id()
                if k != 2: i.same_line(0, c.dp(12))
            i.end_table()

    def append_spectral(self):
        fundamental = 180+70*math.sin(self.stream_time*.65)
        column = []
        for f in range(96):
            hz = 20*1000**((f+.5)/96)
            column.append(max([-98+3*math.sin(f*.8+self.stream_time)]+[-12-h*3-65*(math.log(hz/(fundamental*h))/.16)**2 for h in range(1, 16)]))
        self.stream.append(column); self.stream_time += 1/12

    def draw_page(self, name):
        from imgui_bundle import imgui as i
        import cyui as c
        if name == 'Canvas':
            with Canvas('canvas', self.canvas, (0, 320)) as frame:
                a, b = self.canvas.view.screen((40, 40), frame.top), self.canvas.view.screen((220, 160), frame.top)
                frame.draw.add_rect_filled(a, b, c.mix(c.palette().bg0, c.palette().accent, .4))
        elif name == 'Plots':
            plot('plot', self.plot, self.signal)
            heatmap('heat', list(range(128)), 16, 8, Range(0, 127), (0, 120))
        elif name == 'Curves':
            edit = curve_editor('curves', self.curves, self.keys, snap_step=.25)
            if edit.committed: self.keys = edit.value
        elif name == 'Nodes':
            c.text_dim(c.tr('Copy/paste nodes, groups, links and reroutes: Ctrl+C/V/D or right-click. Paste at pointer.'))
            event = node_editor('nodes', self.node_state, self.nodes, self.links,
                groups=self.node_groups, reroutes=self.node_routes, allocate_id=self.node_ids.__next__)
            if event['groups'].committed: self.node_groups = event['groups'].value
            if event['reroutes'].committed: self.node_routes = event['reroutes'].value
            created = event['created']; self.nodes.extend(created.nodes); self.links.extend(created.links)
            self.node_groups.extend(created.groups); self.node_routes.extend(created.reroutes)
            if event['moved'].committed:
                for node in self.nodes: node.position = event['moved'].value.get(node.id, node.position)
            if event['connect']:
                self.links.append(Link(next(self.node_ids), *event['connect']))
            removed = set(event['remove_nodes'])
            self.nodes = [node for node in self.nodes if node.id not in removed]
            pins = {pin.id for node in self.nodes for pin in node.inputs+node.outputs}
            self.links = [link for link in self.links if link.id != event['remove_link'] and link.id not in event['remove_links'] and link.source in pins and link.target in pins]
            self.node_groups = [g for g in self.node_groups if g.id not in event['remove_groups']]
            self.node_routes = [r for r in self.node_routes if r.id not in event['remove_reroutes'] and any(l.id == r.link for l in self.links)]
        elif name == 'Timeline':
            event = timeline('timeline', self.timeline, self.lanes, self.clips, [Marker(1, 4, 'Cue')], snap_step=.25)
            if event['clips'].committed: self.clips = event['clips'].value
            self.clips = [clip for clip in self.clips if clip.id not in event['remove']]
        elif name == 'Audio':
            _, self.gain = knob('gain', self.gain, 0, 1, .5, label='Gain')
            i.same_line(); _, self.db = fader('fader', self.db)
            i.same_line(); meter('meter', [.5, .7], [.3, .4], state=self.meter)
        elif name == 'Audio views':
            waveform('waveform', self.wave, [self.peaks, self.peaks], Range(0, 1), .4, (0, 140))
            spectrum('spectrum', self.spectrum, self.spectrum_data, (0, 130))
            oscilloscope('scope', self.plot, self.signal, (0, 100))
        elif name == 'Audio analysis':
            if i.begin_table('analysis', 2, i.TableFlags_.sizing_stretch_same):
                i.table_next_column(); c.section_title(c.tr('Spectrogram'))
                spectrogram('spectrogram', self.spectrogram_view, self.spectrogram, (0, 192))
                i.table_next_column(); c.section_title(c.tr('EQ and phase response'))
                gain, phase = [], []
                for n in range(240):
                    hz = 20*1000**(n/239)
                    x = [(math.log(hz/b.frequency)/.5, b.gain) for b in (self.response.preview if self.response.dragging else self.bands)]
                    gain.append((hz, sum(g*math.exp(-a*a) for a, g in x)))
                    phase.append((hz, sum(g*a*math.exp(-a*a)*5 for a, g in x)))
                result = filter_response('response', self.response, [Series(1, gain, c.tr('Gain'))], [Series(2, phase, c.tr('Phase'), color_index=1)], self.bands, (0, 192))
                if result['points'].committed: self.bands = result['points'].value
                i.table_next_column(); c.section_title(c.tr('Loudness'))
                self.loudness_data.history[0].label = c.tr('Momentary')
                if loudness('loudness', self.loudness_view, self.loudness_data, (0, 190))['reset_requested']:
                    self.loudness_data.integrated = math.nan
                i.table_next_column(); c.section_title(c.tr('Dynamics transfer'))
                self.transfer[0].label = c.tr('Output')
                dynamics('dynamics', self.dynamics_view, DynamicsData(self.transfer, -9, -15.75, 6.75), (0, 190))
                i.table_next_column(); c.section_title(c.tr('True peak'))
                if true_peak('truePeak', self.peak_channels, (0, 126))['reset_requested']:
                    for channel in self.peak_channels: channel.held, channel.overs = channel.level, 0
                i.table_next_column(); c.section_title(c.tr('Correlation and phase'))
                self.coherence[0].label = c.tr('Correlation')
                correlation('correlation', self.correlation_view, .72, self.coherence, (0, 126))
                i.end_table()
        elif name == 'Spectral displays': self.draw_spectral()
        elif name == 'Code':
            code_viewer('code', self.code_state, self.source, (0, 180))
            self.editor.draw('editor', (0, 300))
        elif name == 'Diff': diff_viewer('diff', self.diff_state, self.diff)
        elif name == 'Inspector':
            c.filter_box('##propertiesFilter', self.property_filter, c.tr('Filter properties...'))
            for key, edit in inspector('inspector', self.inspector, self.properties, filter_state=self.property_filter):
                if edit.committed:
                    next(p for p in self.properties if p.id == key).value = edit.value
        elif name == 'Assets': asset_browser('assets', self.assets, self.asset_items)
        elif name == 'Images':
            if not self.stops:
                self.stops = [GradientStop(n+1, n/2, tuple(c.vec(color))) for n, color in enumerate((c.palette().info, c.palette().accent, c.palette().success))]
            edit = gradient_editor('gradient', self.gradient_state, self.stops)
            if edit.committed: self.stops = edit.value
            image_viewport('image', self.image_state, self.image, tool='crop')
        elif name == 'Viewport':
            viewport('viewport', self.viewport, self.image, (0, 180))
            with Canvas('transform', CanvasState(), (0, 200), navigation=False) as frame:
                edit = transform_gizmo('gizmo', self.transform_state, self.transform, project=lambda p: (frame.top[0]+p[0], frame.top[1]+p[1]), unproject=lambda p: (p[0]-frame.top[0], p[1]-frame.top[1]))
                if edit.committed: self.transform = edit.value
        elif name == 'Commands':
            if c.primary_button(c.tr_label('Open command palette')): self.command_open = True
            self.command_open, request = command_palette('Commands', self.command_open, self.command_state, self.commands)
            if request: c.notify(f'Command requested: {request}')
        elif name == 'Diagnostics':
            log_view('log', self.log_state, self.log, (0, 220))
            diagnostic_list('diagnostics', [Diagnostic(1, 'Example warning', 'example.cpp', 8, 4, 'warning')])
        elif name == 'Documents':
            services = DocumentServices(image=lambda source: self.image)
            result = document_view('document', self.document_state, self.document, (0, 400), DocumentOptions(services))
            if result['task_offset'] is not None:
                raw = bytearray(self.document.text.encode('utf8')); raw[result['task_offset']] = ord('x' if result['task_checked'] else ' '); self.document.set_markdown(raw.decode('utf8'))
            transcript('chat', self.chat, [TranscriptMessage(1, 'User', 'Show the result.'), TranscriptMessage(2, 'Assistant', 'The **result** is ready.')], (0, 200))
        elif name == 'Document editor':
            from cyui_modules.documenteditor import DocumentEditorOptions
            self.document_editor.draw('document-editor', (0, 550), DocumentEditorOptions(DocumentOptions(DocumentServices(image=lambda source: self.image))))
        elif name == 'Media player':
            c.text_dim(c.tr('Native playback controls with a simulated host backend; no codec is loaded by the gallery.'))
            self.media_backend.value.frame = self.image
            media_player('media', self.media_state, self.media_backend, (0, 480))
        elif name == 'Web':
            if self.web:
                for event in web_panel('web', self.web):
                    if event['kind'] == 'navigated': self.web.post_json(theme_message())
            else: c.callout('info', c.tr('Supply a CEFView; the host owns its runtime and texture upload.'))
        elif name == 'Filtering':
            if c.filter_menu('##menuExample', self.menu_filter, self.menu_items, reset_on_open=False, focus_on_open=False):
                c.notify(c.tr('Menu action requested'))

    def draw(self):
        from imgui_bundle import imgui as i
        import cyui as c
        if not self.initialized:
            c.apply(); self.initialized = True
        i.set_next_window_size((1000, 740), i.Cond_.first_use_ever)
        i.begin(c.tr_label('CyUI Python modules'))
        themes = ['mid-grey', 'light-grey', 'amber-night', 'violet', 'navy']
        changed, selected = i.combo(c.tr_label('Theme'), themes.index(c.current_theme()), themes)
        if changed: c.set_theme(themes[selected])
        languages = c.available_locales()
        changed, selected = i.combo(c.tr_label('Language'), [code for code, _ in languages].index(c.locale()), [name for _, name in languages])
        if changed: c.set_locale(languages[selected][0])
        i.same_line()
        help_requested = c.button(c.tr_label('Documentation'))
        help_requested = (i.is_window_focused(i.FocusedFlags_.root_and_child_windows)
                          and i.is_key_pressed(i.Key.f1, False)) or help_requested
        if i.begin_tab_bar('##modules', i.TabBarFlags_.fitting_policy_scroll):
            for name in ('Canvas', 'Plots', 'Curves', 'Nodes', 'Timeline', 'Audio', 'Audio views', 'Audio analysis', 'Spectral displays', 'Code', 'Diff', 'Inspector', 'Assets', 'Images', 'Viewport', 'Commands', 'Diagnostics', 'Documents', 'Document editor', 'Media player', 'Web', 'Filtering'):
                shown, _ = i.begin_tab_item(c.tr_label(name))
                if shown:
                    self.active_page = name
                    self.draw_page(name)
                    i.end_tab_item()
            i.end_tab_bar()
        if help_requested:
            from cyui_modules.help import open_documentation
            pages = {'Code': ('code', ''), 'Diff': ('diff', ''), 'Filtering': ('filtering', ''),
                     'Documents': ('native-markdown', 'reader'), 'Document editor': ('native-markdown', 'editor'), 'Media player': ('native-markdown', 'resources'), 'Web': ('web', ''), 'Canvas': ('module-recipes', 'canvas-and-numeric-plots'),
                     'Plots': ('module-recipes', 'canvas-and-numeric-plots'),
                     'Curves': ('module-recipes', 'curves-graphs-and-timeline'),
                     'Nodes': ('module-recipes', 'curves-graphs-and-timeline'),
                     'Timeline': ('module-recipes', 'curves-graphs-and-timeline'),
                     'Audio': ('module-recipes', 'audio-controls-and-prepared-signal-views'),
                     'Audio views': ('module-recipes', 'audio-controls-and-prepared-signal-views'),
                     'Audio analysis': ('module-recipes', 'audio-analysis-displays'),
                     'Spectral displays': ('module-recipes', 'spectral-and-spatial-displays'),
                     'Inspector': ('module-recipes', 'inspector-and-assets'),
                     'Assets': ('module-recipes', 'inspector-and-assets'),
                     'Images': ('module-recipes', 'images-viewport-and-optional-3d-gizmo'),
                     'Viewport': ('module-recipes', 'images-viewport-and-optional-3d-gizmo')}
            page, section = pages.get(self.active_page, ('module-recipes', 'commands-logs-and-documents'))
            try:
                if not open_documentation(page, section, locale=c.locale()):
                    c.notify(c.tr('Unable to open local documentation. Keep the Documentation folder beside the demo.'), tone='error')
            except (OSError, ValueError):
                c.notify(c.tr('Unable to open local documentation. Keep the Documentation folder beside the demo.'), tone='error')
        i.end()
        c.render_notifications()


if __name__ == '__main__':
    from imgui_bundle import immapp
    gallery = ModuleGallery()
    immapp.run(gui_function=gallery.draw, window_title='CyUI Python modules', window_size=(1200, 850))
