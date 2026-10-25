cmake_minimum_required(VERSION 3.24)
project(ShaderixFrame VERSION 0.13.0 LANGUAGES CXX)
set(SF_VERSION_SUFFIX "alpha.1")
set(SF_APPLICATION_VERSION "${PROJECT_VERSION}-${SF_VERSION_SUFFIX}")
# CMake decodes French cl.exe diagnostics through the OEM codepage on this machine.
# Ninja consumes the compiler's UTF-8 output directly. Keep the dependency prefix exact.
if(CMAKE_CXX_CL_SHOWINCLUDES_PREFIX MATCHES "^Remarque")
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "Remarque : inclusion du fichier : ")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(NOT WIN32)
    message(FATAL_ERROR "This first native backend requires Windows 10/11.")
endif()
add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE IMGUI_DEFINE_MATH_OPERATORS SF_SHARED_GRAPH_ABI=2)
if(MSVC)
    add_compile_options(/utf-8 /EHsc /MP /W3)
endif()
set(IMGUI ${CMAKE_SOURCE_DIR}/vendor/imgui)
add_library(imgui STATIC ${IMGUI}/imgui.cpp ${IMGUI}/imgui_draw.cpp
    ${IMGUI}/imgui_widgets.cpp ${IMGUI}/imgui_tables.cpp
    ${IMGUI}/backends/imgui_impl_win32.cpp ${IMGUI}/backends/imgui_impl_dx11.cpp
    ${IMGUI}/misc/cpp/imgui_stdlib.cpp)
target_include_directories(imgui PUBLIC ${IMGUI} ${IMGUI}/backends ${IMGUI}/misc/cpp)
add_library(shared_components STATIC
    vendor/shaderixel/src/core/Json.cpp vendor/shaderixel/src/doc/EffectGraph.cpp
    vendor/shaderixel/src/ui/Theme.cpp src/adapters/ShaderixelPlatform.cpp
    src/adapters/ShaderixelComposition.cpp src/adapters/CyDawWidgets.cpp
    vendor/cydaw2/src/graph/GraphDefinition.cpp
    vendor/cydaw2/src/engine/EffectProcessors.cpp
    vendor/cydaw2/src/engine/AudioGraph.cpp vendor/cydaw2/src/engine/AudioRenderPlan.cpp
    vendor/cydaw2/src/engine/AudioBuffer.cpp vendor/cydaw2/src/engine/AudioAnalysis.cpp)
target_include_directories(shared_components PUBLIC vendor/shaderixel/src vendor/cydaw2/include src)
target_link_libraries(shared_components PUBLIC imgui)
add_library(frame_engine STATIC src/core/Project.cpp src/core/Motion.cpp src/core/Recovery.cpp src/media/Media.cpp src/media/MediaBackground.cpp src/media/ExportJobs.cpp
    src/adapters/CyDawAudio.cpp
    src/adapters/DspGraph.cpp
    src/adapters/LayerGraph.cpp
    src/plugins/IsolatedAudio.cpp
    src/plugins/PluginEditors.cpp
    src/plugins/IsolatedOfx.cpp
    src/media/Loudness.cpp
    src/media/Tracking.cpp
    src/media/Avi.cpp
    src/media/PrecisionImages.cpp src/render/Color.cpp src/render/Grading.cpp src/render/GradeShader.cpp src/render/LutImages.cpp
    src/render/Renderer.cpp src/render/Titles.cpp src/plugins/Effects.cpp src/mcp/Protocol.cpp)
configure_file(src/core/Version.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/core/Version.h @ONLY)
target_include_directories(frame_engine PUBLIC src ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(frame_engine PUBLIC shared_components d3d11 dxgi d3dcompiler
    mfplat mfreadwrite mfuuid ole32 windowscodecs winmm d2d1 dwrite crypt32)
add_executable(ShaderixFrame WIN32 src/main.cpp src/ui/Editor.cpp src/ui/GradingPanel.cpp src/ui/DspCanvas.cpp src/ui/LayerCanvas.cpp src/ui/ClipPreviews.cpp src/ui/ClipWorkspace.cpp src/ui/MotionPanel.cpp src/ui/ShaderixelLibrary.cpp src/ui/Timeline.cpp src/ui/Workspaces.cpp src/ui/FrameStyle.cpp src/ui/CurveEditor.cpp src/ui/SourceMonitor.cpp src/ui/SequencePanels.cpp src/ui/RenderCache.cpp src/ui/Keymap.cpp src/ui/CommandPalette.cpp)
target_link_libraries(ShaderixFrame PRIVATE frame_engine comdlg32 shell32 dwmapi)
target_link_options(ShaderixFrame PRIVATE /ENTRY:mainCRTStartup)
add_executable(ShaderixFramePluginWorker src/plugins/NativeWorker.cpp)
add_executable(ShaderixFrameOfxWorker src/plugins/OfxWorker.cpp)
target_include_directories(ShaderixFrameOfxWorker PRIVATE vendor/openfx/include)
target_link_libraries(ShaderixFrameOfxWorker PRIVATE shared_components)
target_include_directories(ShaderixFramePluginWorker PRIVATE sdk vendor/cydaw2/include)
add_library(native_gain SHARED sdk/examples/Gain.cpp)
target_include_directories(native_gain PRIVATE sdk)
add_library(native_latency SHARED sdk/examples/Latency.cpp)
add_library(ofx_gain SHARED sdk/examples/OfxGain.cpp)
target_include_directories(ofx_gain PRIVATE vendor/openfx/include)
set_target_properties(ofx_gain PROPERTIES SUFFIX ".ofx")
add_library(ofx_basic SHARED vendor/openfx/Examples/Basic/basic.cpp)
target_include_directories(ofx_basic PRIVATE vendor/openfx/include)
set_target_properties(ofx_basic PROPERTIES SUFFIX ".ofx")
target_include_directories(native_latency PRIVATE sdk)
set(SF_CYDAW_WORKER "${CMAKE_SOURCE_DIR}/vendor/cydaw2/build/desktop-asio/CyDAWPluginWorker_artefacts/Release/CyDAWPluginWorker.exe")
option(SF_BUILD_VST3_WORKER "Build the VST3 worker from the corresponding JUCE source" OFF)
set(SF_JUCE_SOURCE_DIR "" CACHE PATH "JUCE 8.0.15 source directory")
if(SF_BUILD_VST3_WORKER)
    enable_language(C)
    add_subdirectory(tools/vst3_worker)
    add_dependencies(ShaderixFrame CyDAWPluginWorker)
elseif(EXISTS "${SF_CYDAW_WORKER}")
    configure_file("${SF_CYDAW_WORKER}" ${CMAKE_CURRENT_BINARY_DIR}/CyDAWPluginWorker.exe COPYONLY)
else()
    message(STATUS "Optional VST3 worker not installed; see docs/BUILDING.md")
endif()
add_dependencies(ShaderixFrame ShaderixFramePluginWorker ShaderixFrameOfxWorker native_gain native_latency)
# Shaders, plugins and examples are refreshed on every build, not only when the executable relinks:
# a regenerated Shaderixel library must reach the build directory too.
set(SF_ASSET_COPY
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:ShaderixFrame>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/plugins $<TARGET_FILE_DIR:ShaderixFrame>/plugins)
if(EXISTS ${CMAKE_SOURCE_DIR}/examples)
    list(APPEND SF_ASSET_COPY
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples $<TARGET_FILE_DIR:ShaderixFrame>/examples)
endif()
add_custom_target(frame_assets ALL ${SF_ASSET_COPY} VERBATIM)
add_dependencies(ShaderixFrame frame_assets)
enable_testing()
add_executable(advanced_tests tests/AdvancedTests.cpp)
target_link_libraries(advanced_tests PRIVATE frame_engine)
add_test(NAME advanced_workflows COMMAND advanced_tests ${CMAKE_SOURCE_DIR})
set_tests_properties(advanced_workflows PROPERTIES TIMEOUT 60)
add_executable(frame_tests tests/CoreTests.cpp)
target_link_libraries(frame_tests PRIVATE frame_engine)
add_test(NAME core_and_gpu COMMAND frame_tests ${CMAKE_SOURCE_DIR})
add_executable(editor_tests tests/EditorTests.cpp src/ui/Editor.cpp src/ui/GradingPanel.cpp src/ui/DspCanvas.cpp src/ui/LayerCanvas.cpp src/ui/ClipPreviews.cpp src/ui/ClipWorkspace.cpp src/ui/MotionPanel.cpp src/ui/ShaderixelLibrary.cpp src/ui/Timeline.cpp src/ui/Workspaces.cpp src/ui/FrameStyle.cpp src/ui/CurveEditor.cpp src/ui/SourceMonitor.cpp src/ui/SequencePanels.cpp src/ui/RenderCache.cpp src/ui/Keymap.cpp src/ui/CommandPalette.cpp)
target_link_libraries(editor_tests PRIVATE frame_engine comdlg32 shell32 dwmapi)
add_test(NAME editor_interactions COMMAND editor_tests ${CMAKE_SOURCE_DIR})
add_executable(export_tests tests/ExportTests.cpp)
target_link_libraries(export_tests PRIVATE frame_engine)
add_test(NAME background_exports COMMAND export_tests ${CMAKE_SOURCE_DIR})
set_tests_properties(background_exports PROPERTIES TIMEOUT 120)
add_executable(transition_tests tests/TransitionTests.cpp)
target_link_libraries(transition_tests PRIVATE frame_engine)
add_test(NAME transitions COMMAND transition_tests ${CMAKE_SOURCE_DIR})
set_tests_properties(transitions PROPERTIES TIMEOUT 60)
add_executable(mixer_tests tests/MixerTests.cpp)
target_link_libraries(mixer_tests PRIVATE frame_engine)
add_test(NAME audio_mixer COMMAND mixer_tests ${CMAKE_SOURCE_DIR})
set_tests_properties(audio_mixer PROPERTIES TIMEOUT 60)
find_package(Python3 COMPONENTS Interpreter)
add_executable(production_tests tests/ProductionTests.cpp src/ui/DspCanvas.cpp src/ui/ClipPreviews.cpp)
target_link_libraries(production_tests PRIVATE frame_engine)
add_dependencies(production_tests ShaderixFrameOfxWorker ofx_gain ofx_basic)
add_test(NAME production_features COMMAND production_tests ${CMAKE_SOURCE_DIR})
set_tests_properties(production_features PROPERTIES TIMEOUT 90)
option(SF_BUILD_AUDIO_QUALITY_TESTS "Build the optional standalone audio quality executable" OFF)
# Kaspersky removed the Debug executable during linking on the development host.
# Keep this optional target out of normal builds until that detection is resolved.
if(SF_BUILD_AUDIO_QUALITY_TESTS)
    add_executable(audio_quality_tests tests/AudioQualityTests.cpp)
    target_link_libraries(audio_quality_tests PRIVATE frame_engine)
    add_test(NAME audio_quality COMMAND audio_quality_tests ${CMAKE_SOURCE_DIR})
    set_tests_properties(audio_quality PROPERTIES TIMEOUT 90)
endif()
if(Python3_FOUND)
    add_test(NAME mcp_stdio COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/mcp_smoke.py $<TARGET_FILE:ShaderixFrame>)
    set_tests_properties(mcp_stdio PROPERTIES TIMEOUT 60)
    add_test(NAME workspace_modes COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/mode_smoke.py $<TARGET_FILE:ShaderixFrame>)
    set_tests_properties(workspace_modes PROPERTIES TIMEOUT 150)
endif()
