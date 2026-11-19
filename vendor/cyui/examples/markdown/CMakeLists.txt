# SDL is an example dependency only. Core/modules never fetch or link it.
find_package(SDL3 3.4 QUIET CONFIG)
if(NOT TARGET SDL3::SDL3)
    option(CYUI_FETCH_DEMO_DEPENDENCIES "Fetch pinned SDL3 for the native examples" ON)
    if(NOT CYUI_FETCH_DEMO_DEPENDENCIES)
        message(FATAL_ERROR "Install SDL3 >= 3.4 or enable CYUI_FETCH_DEMO_DEPENDENCIES")
    endif()
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(cyui_sdl3
        URL https://codeload.github.com/libsdl-org/SDL/tar.gz/fa2c02bb6e21974a89ea9824bc53c9932abe5f9c
        URL_HASH SHA256=d246967187a4f37cc850608dbaf5cff9d1df4d731259903d25539755cdfcc657)
    FetchContent_MakeAvailable(cyui_sdl3)
endif()
set(backends ${PROJECT_SOURCE_DIR}/third_party/imgui-sdl3)
add_library(cyui_example_host STATIC Platform.cpp
    ${backends}/imgui_impl_sdl3.cpp ${backends}/imgui_impl_sdlrenderer3.cpp)
target_include_directories(cyui_example_host PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${backends})
target_link_libraries(cyui_example_host PUBLIC CyUI::Core SDL3::SDL3)
cyui_configure_target(cyui_example_host)
if(WIN32)
    target_sources(cyui_example_host PRIVATE Clipboard_win32.cpp)
    target_link_libraries(cyui_example_host PRIVATE shell32 user32)
elseif(APPLE)
    enable_language(OBJCXX)
    target_sources(cyui_example_host PRIVATE Clipboard_macos.mm)
    set_target_properties(cyui_example_host PROPERTIES OBJCXX_STANDARD 17 OBJCXX_STANDARD_REQUIRED YES)
    set_source_files_properties(Clipboard_macos.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_link_libraries(cyui_example_host PRIVATE "-framework AppKit")
else()
    target_sources(cyui_example_host PRIVATE Clipboard_posix.cpp)
endif()

function(cyui_example target)
    cyui_configure_target(${target})
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    target_link_libraries(${target} PRIVATE cyui_example_host)
    if(WIN32)
        set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/third_party/jetbrains-mono/OFL.txt"
            "$<TARGET_FILE_DIR:${target}>/licenses/JetBrainsMono-OFL.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/third_party/imgui-sdl3/LICENSE.txt"
            "$<TARGET_FILE_DIR:${target}>/licenses/DearImGui-MIT.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/third_party/stb/LICENSE"
            "$<TARGET_FILE_DIR:${target}>/licenses/stb.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/SDL-LICENSE.txt"
            "$<TARGET_FILE_DIR:${target}>/licenses/SDL-zlib.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/LICENSE"
            "$<TARGET_FILE_DIR:${target}>/licenses/CyUI-MIT.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/third_party/texteditor/LICENSE"
            "$<TARGET_FILE_DIR:${target}>/licenses/TextEditor-MIT.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/third_party/md4c/LICENSE.md"
            "$<TARGET_FILE_DIR:${target}>/licenses/MD4C-MIT.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/third_party/miniz/LICENSE"
            "$<TARGET_FILE_DIR:${target}>/licenses/miniz-MIT.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/third_party/lexilla/License.txt"
            "$<TARGET_FILE_DIR:${target}>/licenses/Lexilla.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/third_party/scintilla/License.txt"
            "$<TARGET_FILE_DIR:${target}>/licenses/Scintilla.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/third_party/SciTE-Properties-License.txt"
            "$<TARGET_FILE_DIR:${target}>/licenses/SciTE-Properties.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/third_party/pugixml/LICENSE.md"
            "$<TARGET_FILE_DIR:${target}>/licenses/pugixml-MIT.txt")
    get_target_property(sdl_type SDL3::SDL3 TYPE)
    if(WIN32 AND sdl_type STREQUAL "SHARED_LIBRARY")
        add_custom_command(TARGET ${target} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3>" "$<TARGET_FILE_DIR:${target}>")
    endif()
endfunction()
if(CYUI_BUILD_MARKDOWN_DEMO)
    add_executable(cyui_markdown_test main.cpp Raster.cpp)
    target_include_directories(cyui_markdown_test PRIVATE ${PROJECT_SOURCE_DIR}/third_party/stb)
    target_link_libraries(cyui_markdown_test PRIVATE CyUI::DocumentEditor CyUI::DocumentIO)
    if(MSVC)
        set_source_files_properties(Raster.cpp PROPERTIES COMPILE_OPTIONS /w)
    else()
        set_source_files_properties(Raster.cpp PROPERTIES COMPILE_OPTIONS -w)
    endif()
    cyui_example(cyui_markdown_test)
    if(CYUI_BUILD_TESTS)
        enable_testing()
        add_test(NAME cyui_markdown_playground COMMAND cyui_markdown_test --self-test)
        add_test(NAME cyui_markdown_video COMMAND cyui_markdown_test --scenario document-video --frames 64 --capture markdown-video.bmp)
        add_test(NAME cyui_markdown_million COMMAND cyui_markdown_test --scenario million-chat --capture markdown-million.bmp)
        set_tests_properties(cyui_markdown_playground cyui_markdown_video cyui_markdown_million PROPERTIES
            TIMEOUT 45 ENVIRONMENT "SDL_RENDER_DRIVER=software")
    endif()
endif()
if(CYUI_BUILD_PORTABLE_DEMO)
    add_executable(cyui_demo_sdl3 ../gallery_sdl3/main.cpp)
    target_link_libraries(cyui_demo_sdl3 PRIVATE CyUI::CyUI)
    cyui_example(cyui_demo_sdl3)
    if(CYUI_BUILD_TESTS)
        enable_testing()
        add_test(NAME cyui_portable_gallery COMMAND cyui_demo_sdl3 --capture gallery.bmp --tab 18)
        set_tests_properties(cyui_portable_gallery PROPERTIES TIMEOUT 45 ENVIRONMENT "SDL_RENDER_DRIVER=software")
    endif()
endif()
