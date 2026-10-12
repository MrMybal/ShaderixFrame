// Generated from Shaderixel NodeEditor.cpp; D3D11 thumbnail bridge adapted.
    auto linkPin = [&](const Node& src, int ch) { return src.kind == NodeKind::Split && ch > 0 ? ch - 1 : 0; };
    // Links (colored by the type they carry)
    auto drawLink = [&](ImVec2 a, ImVec2 b, ImU32 col, float thick) {
        float dx = std::max(std::abs(b.x - a.x) * 0.5f, 40.0f * ed.zoom);
        dl->AddBezierCubic(a, ImVec2(a.x + dx, a.y), ImVec2(b.x - dx, b.y), b, col, thick * ed.zoom);
    };
    for (const Node& n : g.nodes) {
        for (int i = 0; i < (int)n.inputs.size() && i < NodeInputCount(n); ++i) {
            const Node* src = n.inputs[i] ? g.Find(n.inputs[i]) : nullptr;
            if (!src) continue;
            int ch = InputChannel(n, i);
            bool sel = ed.selection.count(src->id) || ed.selection.count(n.id);
            drawLink(outPin(*src, linkPin(*src, ch)), inPin(n, i), sel ? ui::col::Selection : TypeColor(ch ? PinType::Channel : PinType::RGBA, 200), sel ? 3.0f : 2.0f);
        }
    }
    if (ed.draggingLink) {
        if (ed.linkFrom) { if (const Node* s = g.Find(ed.linkFrom)) drawLink(outPin(*s, linkPin(*s, ed.linkChannel)), mouse, TypeColor(ed.linkChannel ? PinType::Channel : PinType::RGBA), 2.5f); }
        else if (const Node* t = g.Find(ed.linkTo)) drawLink(mouse, inPin(*t, ed.linkInput), ui::col::Accent, 2.5f);
    }

    // Nodes
    ImFont* font = ImGui::GetFont();
    float fs = ImGui::GetFontSize() * ed.zoom;
    bool drawText = ed.zoom > 0.45f;
    for (const Node& n : g.nodes) {
        Layout L = NodeLayout(n, aspect);
        ImVec2 a = toScreen(n.pos), b = toScreen(n.pos + float2{kNodeW, L.h});
        if (b.x < origin.x || b.y < origin.y || a.x > origin.x + size.x || a.y > origin.y + size.y) continue;
        float r = 6 * ed.zoom;
        bool selected = ed.selection.count(n.id) || n.id == this->selected;
        bool failedNode = false;
        bool isPaint = false;
        dl->AddRectFilled(ImVec2(a.x + 3, a.y + 4), ImVec2(b.x + 3, b.y + 4), IM_COL32(0, 0, 0, 70), r);
        dl->AddRectFilled(a, b, n.bypass ? IM_COL32(36, 37, 42, 255) : ui::col::PanelLight, r);
        ImU32 hc = HeaderColor(n);
        if (n.bypass) hc = (hc & 0x00FFFFFF) | 0x60000000;
        dl->AddRectFilled(a, ImVec2(b.x, a.y + kHeader * ed.zoom), hc, r, ImDrawFlags_RoundCornersTop);
        ImU32 border = failedNode ? ui::col::Error : selected ? ui::col::Selection : (n.id == hoverNode ? IM_COL32(110, 110, 130, 255) : ui::col::Border);
        if (selected) dl->AddRect(ImVec2(a.x - 3, a.y - 3), ImVec2(b.x + 3, b.y + 3), IM_COL32(255, 140, 0, 90), r + 3, 0, 4.0f);
        dl->AddRect(a, b, border, r, 0, selected ? 3.0f : failedNode ? 2.0f : 1.0f);
        if (drawText) {
            std::string title = n.name;
            if (isPaint) title = "\xE2\x9C\x8E " + title;  // pencil mark on the paint target
            dl->PushClipRect(a, ImVec2(b.x - 4, a.y + kHeader * ed.zoom), true);
            dl->AddText(font, fs, ImVec2(a.x + 8 * ed.zoom, a.y + (kHeader * ed.zoom - fs) * 0.5f), IM_COL32_WHITE, title.c_str());
            dl->PopClipRect();
        }
        // Inputs (filled with the color of the linked channel)
        for (int i = 0; i < L.inputs; ++i) {
            ImVec2 p = inPin(n, i);
            bool connected = i < (int)n.inputs.size() && n.inputs[i];
            bool hot = hoverInNode == n.id && hoverIn == i;
            float pr = pinR * (hot ? 1.4f : 1.0f);
            ImU32 tc = TypeColor(NodeInputType(n, i));
            if (connected) {
                dl->AddCircleFilled(p, pr, tc);
                dl->AddCircle(p, pr, IM_COL32(20, 20, 24, 255), 0, 1.5f);
            } else {
                dl->AddCircleFilled(p, pr, IM_COL32(40, 42, 50, 255));
                dl->AddCircle(p, pr, tc, 0, 2.0f * ed.zoom);
            }
            if (drawText) dl->AddText(font, fs * 0.9f, ImVec2(p.x + 10 * ed.zoom, p.y - fs * 0.45f), ui::col::TextDim, NodeInputName(n, i).c_str());
        }
        // Outputs
        for (int o = 0; o < L.outputs; ++o) {
            ImVec2 p = outPin(n, o);
            bool hot = hoverOut == n.id && hoverOutCh == o;
            float pr = pinR * (hot ? 1.4f : 1.0f);
            dl->AddCircleFilled(p, pr, TypeColor(NodeOutputType(n, o)));
            dl->AddCircle(p, pr, IM_COL32(20, 20, 24, 255), 0, 1.5f);
            if (drawText) {
                std::string lbl = NodeOutputName(n, o);
                ImVec2 ts = font->CalcTextSizeA(fs * 0.85f, FLT_MAX, 0, lbl.c_str());
                dl->AddText(font, fs * 0.85f, ImVec2(p.x - 10 * ed.zoom - ts.x, p.y - fs * 0.43f), LabelColor(n, o), lbl.c_str());
            }
        }
        if(auto *out=renderer.layerNodeView(n.id)) {
            ImVec2 ta=toScreen(n.pos+float2{8,kHeader+L.rows*kRow+4});
            ImVec2 tb(ta.x+L.thumbW*ed.zoom,ta.y+L.thumbH*ed.zoom);
            dl->AddImageRounded((ImTextureID)out,ta,tb,ImVec2(0,0),ImVec2(1,1),IM_COL32_WHITE,3*ed.zoom);
        }
    }
