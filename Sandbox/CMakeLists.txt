include(Font.cmake)
add_executable(Sandbox main.cpp StartupScene.cpp StartupScene.h
    PhysicsDemo.cpp PhysicsDemo.h AnimationDemo.cpp AnimationDemo.h
    ToonDemo.cpp ToonDemo.h EffectDemo.cpp EffectDemo.h UiDemo.cpp UiDemo.h
    SandboxUi.cpp SandboxUi.h)
target_link_libraries(Sandbox PRIVATE KT::Engine ForgeTools)
target_include_directories(Sandbox PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(Sandbox PRIVATE /W4 /utf-8)
set_target_properties(Sandbox PROPERTIES CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Sandbox>")
kt_forge_runtime_assets(Sandbox "${CMAKE_CURRENT_SOURCE_DIR}/Assets")
add_custom_target(Sandbox_Font
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Sandbox>/ProjectAssets/UI"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${KT_FORGE_UI_FONT}" "$<TARGET_FILE_DIR:Sandbox>/ProjectAssets/UI/NotoSansCJKjp-Regular.otf"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/RmlUi" "$<TARGET_FILE_DIR:Sandbox>/ProjectAssets/UI/Licenses"
    VERBATIM)
add_dependencies(Sandbox_Font Sandbox_Assets)
add_dependencies(Sandbox Sandbox_Font)
get_target_property(_sandbox_sources Sandbox SOURCES)
source_group("Sandbox" FILES ${_sandbox_sources})
