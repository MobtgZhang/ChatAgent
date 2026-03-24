cmake_minimum_required(VERSION 3.16)
project(appChatAgent VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ── 依赖查找 ────────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Quick QuickControls2 Network Sql WebEngineQuick WebChannel Concurrent
)

# ── 可执行文件 ──────────────────────────────────────────────────────────────
qt_add_executable(appChatAgent
    src/main.cpp
    src/about.h   src/about.cpp
    src/locale_bridge.h src/locale_bridge.cpp
    src/settings.h src/settings.cpp
    src/history.h  src/history.cpp
    src/historylistmodel.h src/historylistmodel.cpp
    src/messagemodel.h src/messagemodel.cpp
    src/mainview.h src/mainview.cpp
    src/web_search_service.h src/web_search_service.cpp
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
    src/tools/window_tool.h src/tools/window_tool.cpp
    src/tools/clipboard_tool.h src/tools/clipboard_tool.cpp
    src/tools/wait_tool.h src/tools/wait_tool.cpp
    src/tools/python_tool.h src/tools/python_tool.cpp
    src/tools/memory_tool.h src/tools/memory_tool.cpp
    src/clipboard_bridge.h src/clipboard_bridge.cpp
    src/skill_manager.h src/skill_manager.cpp
)

# ── QML 资源打包 ─────────────────────────────────────────────────────────────
# 3. 配置 QML 模块 (Qt 6 推荐做法)
qt_add_resources(appChatAgent "qml_res"
    PREFIX "/"          # 对应 qrc:/
    BASE_DIR "src"      # 关键：告诉 CMake 忽略物理路径中的 "src/" 文件夹
    FILES
        src/qml/Main.qml
        src/qml/chat_render.html
        # Markdown + KaTeX 资源（供 chat_render.html 使用）
        src/qml/md_libs/marked.min.js
        src/qml/md_libs/katex.min.css
        src/qml/md_libs/katex.min.js
        src/qml/md_libs/contrib/auto-render.min.js
        src/qml/md_libs/fonts/KaTeX_AMS-Regular.woff2
        src/qml/md_libs/fonts/KaTeX_Caligraphic-Bold.woff2
        src/qml/md_libs/fonts/KaTeX_Caligraphic-Regular.woff2
        src/qml/md_libs/fonts/KaTeX_Fraktur-Bold.woff2
        src/qml/md_libs/fonts/KaTeX_Fraktur-Regular.woff2
        src/qml/md_libs/fonts/KaTeX_Main-Bold.woff2
        src/qml/md_libs/fonts/KaTeX_Main-BoldItalic.woff2
        src/qml/md_libs/fonts/KaTeX_Main-Italic.woff2
        src/qml/md_libs/fonts/KaTeX_Main-Regular.woff2
        src/qml/md_libs/fonts/KaTeX_Math-BoldItalic.woff2
        src/qml/md_libs/fonts/KaTeX_Math-Italic.woff2
        src/qml/md_libs/fonts/KaTeX_SansSerif-Bold.woff2
        src/qml/md_libs/fonts/KaTeX_SansSerif-Italic.woff2
        src/qml/md_libs/fonts/KaTeX_SansSerif-Regular.woff2
        src/qml/md_libs/fonts/KaTeX_Script-Regular.woff2
        src/qml/md_libs/fonts/KaTeX_Size1-Regular.woff2
        src/qml/md_libs/fonts/KaTeX_Size2-Regular.woff2
        src/qml/md_libs/fonts/KaTeX_Size3-Regular.woff2
        src/qml/md_libs/fonts/KaTeX_Size4-Regular.woff2
        src/qml/md_libs/fonts/KaTeX_Typewriter-Regular.woff2
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
        src/icons/icon_debug.svg
        src/icons/icon_debug_active.svg
        src/icons/icon_model.svg
        src/icons/app_icon.png
        src/qml/Settings.qml
        src/qml/About.qml
        src/translations/zh.json
        src/translations/en.json
        src/translations/ru.json
        src/translations/fr.json
        src/translations/ja.json
        src/translations/ko.json
        src/translations/pt.json
        src/translations/es.json
)

# ── 包含路径 ──────────────────────────────────────────────────────────────────
target_include_directories(appChatAgent PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ── 链接 ─────────────────────────────────────────────────────────────────────
target_link_libraries(appChatAgent PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    Qt6::Sql
    Qt6::WebEngineQuick
    Qt6::WebChannel
    Qt6::Concurrent
)

# ── Windows 隐藏控制台窗口 ──────────────────────────────────────────────────
if(WIN32)
    set_target_properties(appChatAgent PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# ── macOS bundle ─────────────────────────────────────────────────────────────
if(APPLE)
    set_target_properties(appChatAgent PROPERTIES MACOSX_BUNDLE TRUE)
endif()

# ── Linux 安装与自包含部署（用于 deb 打包）────────────────────────────────────────
if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS appChatAgent
        BUNDLE  DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    # 排除 platforminputcontexts：fcitx5 插件 RPATH 设置会失败，排除后使用系统输入法
    qt_generate_deploy_app_script(
        TARGET appChatAgent
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
        EXCLUDE_PLUGIN_TYPES platforminputcontexts
    )
    install(SCRIPT ${deploy_script})
endif()

# ── 历史消息调试（验证 editHistory/branchTails 逻辑）──────────────────────────
qt_add_executable(test_history_debug
    src/test/test_history_debug.cpp
    src/messagemodel.cpp
)
target_link_libraries(test_history_debug PRIVATE Qt6::Core)
target_include_directories(test_history_debug PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ── 工具功能测试（逐个验证 FileTool / ShellTool / WaitTool / PythonTool）──────
qt_add_executable(test_tools
    src/test/test_tools.cpp
    src/settings.h src/settings.cpp
    src/locale_bridge.h src/locale_bridge.cpp
    src/tools/base_tool.cpp
    src/tools/file_tool.h src/tools/file_tool.cpp
    src/tools/shell_tool.h src/tools/shell_tool.cpp
    src/tools/wait_tool.h src/tools/wait_tool.cpp
    src/tools/python_tool.h src/tools/python_tool.cpp
)
target_include_directories(test_tools PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_tools PRIVATE Qt6::Core Qt6::Network)

# ── 搜索调试 CLI（diagnose DuckDuckGo search/proxy）──────────────────────────
qt_add_executable(test_search_cli
    src/test/test_search_cli.cpp
    src/web_search_service.cpp
)
target_include_directories(test_search_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_search_cli PRIVATE Qt6::Core Qt6::Network)

