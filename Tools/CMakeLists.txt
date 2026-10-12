# CLIの引数解釈はアプリ側。Engineは制作コマンドを参照しない。
add_library(ForgeTools STATIC AssetCommands.cpp AssetCommands.h ModelWorkflowCommands.cpp ModelWorkflowCommands.h)
target_link_libraries(ForgeTools PUBLIC KT::Engine PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(ForgeTools PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(ForgeTools PRIVATE /W4 /utf-8)
