# Agents

This repository is **CyUI**, a themed widget library on top of Dear ImGui
(>= 1.92).

- **Using CyUI in another app:** read `docs/AI_GUIDE.md` first. It covers
  integration, the style rules, the ImGui → CyUI migration table, the API and
  recipes.
- **Working on CyUI itself:**
  - Keep every colour in `cyui::palette()` roles and every size through
    `cyui::dp()` or the style.
  - Add each new widget to `src/Gallery.cpp` and to `docs/AI_GUIDE.md`.
  - Build with `build_demo.bat` (MSVC 2022).
  - Check the look in every theme:
    `build\cyui_demo.exe --capture out.bmp --theme <key> --tab <n>`.
  - Do not edit `third_party/imgui`.
