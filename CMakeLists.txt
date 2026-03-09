cmake_minimum_required(VERSION 3.16)
project(appAIChat VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ── 依赖查找 ────────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Quick QuickControls2 Network Sql WebEngineQuick WebChannel
)

# ── 可执行文件 ──────────────────────────────────────────────────────────────
qt_add_executable(appAIChat
    src/main.cpp
    src/about.h   src/about.cpp
    src/locale_bridge.h src/locale_bridge.cpp
    src/settings.h src/settings.cpp
    src/history.h  src/history.cpp
    src/historylistmodel.h src/historylistmodel.cpp
    src/messagemodel.h src/messagemodel.cpp
    src/mainview.h src/mainview.cpp
    # Agent 模块
    src/agent_core.h src/agent_core.cpp
    src/llm_backend.h src/llm_backend.cpp
    src/memory_module.h src/memory_module.cpp
    src/tool_registry.h src/tool_registry.cpp
    src/tools/base_tool.h src/tools/base_tool.cpp
    src/tools/file_tool.h src/tools/file_tool.cpp
    src/tools/shell_tool.h src/tools/shell_tool.cpp
    src/tools/websearch_tool.h src/tools/websearch_tool.cpp
    src/tools/keyboard_tool.h src/tools/keyboard_tool.cpp
    src/tools/ocr_tool.h src/tools/ocr_tool.cpp
    src/tools/window_tool.h src/tools/window_tool.cpp
    src/tools/clipboard_tool.h src/tools/clipboard_tool.cpp
    src/tools/wait_tool.h src/tools/wait_tool.cpp
    src/tools/image_match_tool.h src/tools/image_match_tool.cpp
    src/clipboard_bridge.h src/clipboard_bridge.cpp
)

# ── QML 资源打包 ─────────────────────────────────────────────────────────────
# 3. 配置 QML 模块 (Qt 6 推荐做法)
qt_add_resources(appAIChat "qml_res"
    PREFIX "/"          # 对应 qrc:/
    BASE_DIR "src"      # 关键：告诉 CMake 忽略物理路径中的 "src/" 文件夹
    FILES
        src/qml/Main.qml
        src/qml/ChatMessage.qml
        src/qml/MarkdownRender.qml
        src/qml/MarkdownMathRender.qml
        src/qml/md_render.html
        src/icons/copy.svg
        src/icons/copy_light.svg
        src/icons/pencil.svg
        src/icons/pencil_light.svg
        src/icons/regenerate.svg
        src/icons/regenerate_light.svg
        src/icons/icon_chat.svg
        src/icons/icon_agent.svg
        src/icons/icon_planning.svg
        src/icons/icon_chat_active.svg
        src/icons/icon_agent_active.svg
        src/icons/icon_planning_active.svg
        src/icons/icon_model.svg
        src/qml/Settings.qml
        src/qml/About.qml
        src/translations/zh.json
        src/translations/en.json
        src/translations/ru.json
        src/translations/fr.json
)

# ── 链接 ─────────────────────────────────────────────────────────────────────
target_link_libraries(appAIChat PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    Qt6::Sql
    Qt6::WebEngineQuick
    Qt6::WebChannel
)

# ── Windows 隐藏控制台窗口 ──────────────────────────────────────────────────
if(WIN32)
    set_target_properties(appAIChat PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# ── macOS bundle ─────────────────────────────────────────────────────────────
if(APPLE)
    set_target_properties(appAIChat PROPERTIES MACOSX_BUNDLE TRUE)
endif()

