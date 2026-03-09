cmake_minimum_required(VERSION 3.16)
project(appAIChat VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ── 依赖查找 ────────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Quick QuickControls2 Network
)

# ── 可执行文件 ──────────────────────────────────────────────────────────────
qt_add_executable(appAIChat
    src/main.cpp
    src/about.h   src/about.cpp
    src/settings.h src/settings.cpp
    src/history.h  src/history.cpp
    src/messagemodel.h src/messagemodel.cpp
    src/mainview.h src/mainview.cpp
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
        src/qml/Settings.qml
        src/qml/About.qml
)

# ── 链接 ─────────────────────────────────────────────────────────────────────
target_link_libraries(appAIChat PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
)

# ── Windows 隐藏控制台窗口 ──────────────────────────────────────────────────
if(WIN32)
    set_target_properties(appAIChat PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# ── macOS bundle ─────────────────────────────────────────────────────────────
if(APPLE)
    set_target_properties(appAIChat PROPERTIES MACOSX_BUNDLE TRUE)
endif()

