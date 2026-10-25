# Corresponding-source build of the locked CyDAW v2 IPC-4 VST3 worker.
if(NOT EXISTS "${SF_JUCE_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Set SF_JUCE_SOURCE_DIR to the JUCE directory in the matching source archive")
endif()
add_subdirectory("${SF_JUCE_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/juce" EXCLUDE_FROM_ALL)
file(READ "${SF_JUCE_SOURCE_DIR}/CMakeLists.txt" _sf_juce_cmake)
if(NOT _sf_juce_cmake MATCHES "project[(]JUCE VERSION 8[.]0[.]15 ")
    message(FATAL_ERROR "This alpha worker requires JUCE 8.0.15")
endif()
juce_add_console_app(CyDAWPluginWorker PRODUCT_NAME "ShaderixFrame VST3 Worker"
    COMPANY_NAME "Cyberalien" VERSION "${PROJECT_VERSION}")
target_sources(CyDAWPluginWorker PRIVATE "${CMAKE_SOURCE_DIR}/vendor/cydaw2/src/plugin_worker/PluginWorkerMain.cpp")
target_include_directories(CyDAWPluginWorker PRIVATE "${CMAKE_SOURCE_DIR}/vendor/cydaw2/include")
target_compile_features(CyDAWPluginWorker PRIVATE cxx_std_20)
target_compile_definitions(CyDAWPluginWorker PRIVATE
    NOMINMAX WIN32_LEAN_AND_MEAN JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0
    JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_VST=0
    JUCE_PLUGINHOST_AU=0 JUCE_PLUGINHOST_LADSPA=0)
target_link_libraries(CyDAWPluginWorker PRIVATE juce::juce_audio_processors
    juce::juce_gui_basics juce::juce_recommended_config_flags)
set_target_properties(CyDAWPluginWorker PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
