"""Run with `python examples/python_modules.py`, or embed ModuleGallery in a host.

The host may supply ImageData and a CEFView; it owns textures and the CEF runtime.
"""
from pathlib import Path
import math
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'python'))
from cyui_modules.common import Range
from cyui_modules.canvas import Canvas, CanvasState
from cyui_modules.plots import Axis, PlotState, Series, plot, histogram, heatmap
from cyui_modules.curves import CurvePoint, CurveState, curve_editor
from cyui_modules.nodes import Node, Pin, Link, NodeState, node_editor
from cyui_modules.timeline import Lane, Clip, Marker, TimelineState, timeline
from cyui_modules.audioviews import make_peaks, WaveformState, waveform, spectrum, oscilloscope, stereo_scope
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


class ModuleGallery:
    def __init__(self, image=None, web=None):
        self.initialized = False
        self.image, self.web = image or ImageData(0, 256, 256), web
        self.canvas = CanvasState()
        self.plot = PlotState(Axis(Range(0, 10)), Axis(Range(-1.2, 1.2)))
        self.signal = [Series(1, [(n/30, math.sin(n/30)) for n in range(300)], 'Signal')]
        self.curves = CurveState(); self.curves.view.x.view = Range(0, 10)
        self.keys = [CurvePoint(1, 0, .2), CurvePoint(2, 3, .8, 'smooth'), CurvePoint(3, 8, .4)]
        self.nodes = [Node(1, (30, 40), 'Source', outputs=[Pin(11, 'Output')]), Node(2, (320, 60), 'Processor', [Pin(21, 'Input')], [Pin(22, 'Output')], 1)]
        self.links, self.node_state = [Link(1, 11, 21)], NodeState()
        self.lanes = [Lane(1, 'Video', 70), Lane(2, 'Audio', 80)]
        self.clips = [Clip(1, 1, 1, 3, 'Opening'), Clip(2, 2, .5, 7, 'Sound', 1)]
        self.timeline, self.wave = TimelineState(), WaveformState()
        self.peaks = make_peaks([math.sin(n*.04)*math.sin(n*.0001) for n in range(48000)], 1200)
        self.spectrum = PlotState(Axis(Range(20, 20000), True), Axis(Range(-90, 0)))
        self.spectrum_data = [Series(2, [(20*1000**(n/159), -60+35*math.exp(-math.log(20*1000**(n/159)/1000)**2)) for n in range(160)], 'Spectrum', color_index=1)]
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
        self.gain, self.db, self.meter = .5, -12., MeterState()

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
            event = node_editor('nodes', self.node_state, self.nodes, self.links)
            if event['moved'].committed:
                for node in self.nodes: node.position = event['moved'].value.get(node.id, node.position)
            if event['connect']:
                self.links.append(Link(max([link.id for link in self.links]+[0])+1, *event['connect']))
            removed = set(event['remove_nodes'])
            self.nodes = [node for node in self.nodes if node.id not in removed]
            pins = {pin.id for node in self.nodes for pin in node.inputs+node.outputs}
            self.links = [link for link in self.links if link.id != event['remove_link'] and link.source in pins and link.target in pins]
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
            document_view('document', self.document_state, self.document, (0, 300))
            transcript('chat', self.chat, [TranscriptMessage(1, 'User', 'Show the result.'), TranscriptMessage(2, 'Assistant', 'The **result** is ready.')], (0, 200))
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
        if i.begin_tab_bar('##modules', i.TabBarFlags_.fitting_policy_scroll):
            for name in ('Canvas', 'Plots', 'Curves', 'Nodes', 'Timeline', 'Audio', 'Audio views', 'Code', 'Diff', 'Inspector', 'Assets', 'Images', 'Viewport', 'Commands', 'Diagnostics', 'Documents', 'Web', 'Filtering'):
                shown, _ = i.begin_tab_item(c.tr_label(name))
                if shown:
                    self.draw_page(name)
                    i.end_tab_item()
            i.end_tab_bar()
        i.end()
        c.render_notifications()


if __name__ == '__main__':
    from imgui_bundle import immapp
    gallery = ModuleGallery()
    immapp.run(gui_function=gallery.draw, window_title='CyUI Python modules', window_size=(1200, 850))
