#!/bin/bash
# =============================================================================
# run.sh - 构建并运行 ChatAgent（支持 Linux 与 macOS）
# Windows 请使用 run.ps1
# =============================================================================

set -e

# ==========================================
# 1. 检测操作系统与变量配置
# ==========================================
UNAME="$(uname -s)"
case "${UNAME}" in
    Linux*)
        PLATFORM="linux"
        # Qt 安装根目录（本机默认 /home/mobtgzhang/Qt；可用 QT_ROOT 或 QT_INSTALL_DIR 覆盖）
        QT_ROOT="${QT_ROOT:-/home/mobtgzhang/Qt}"
        QT_INSTALL_DIR="${QT_INSTALL_DIR:-${QT_ROOT}/6.10.2/gcc_64}"
        PARALLEL_JOBS="$(nproc 2>/dev/null || echo 4)"
        ;;
    Darwin*)
        PLATFORM="macos"
        QT_INSTALL_DIR="${QT_INSTALL_DIR:-$HOME/Qt/6.10.2/macos}"
        PARALLEL_JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
        ;;
    *)
        echo "❌ 不支持的操作系统: ${UNAME}"
        echo "   Linux/macOS 请使用本脚本；Windows 请使用 run.ps1"
        exit 1
        ;;
esac

BUILD_DIR="build"
EXECUTABLE_NAME="appChatAgent"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "========================================"
echo "🚀 开始构建项目 (${PLATFORM})..."
if [ "$PLATFORM" = "linux" ]; then
    echo "   Qt 根目录: ${QT_ROOT}"
    echo "   Qt 套件 (CMAKE_PREFIX_PATH): ${QT_INSTALL_DIR}"
else
    echo "   Qt 路径: ${QT_INSTALL_DIR}"
fi
echo "========================================"

# ==========================================
# 2. 准备构建目录
# ==========================================
cd "$PROJECT_ROOT"
if [ ! -d "$BUILD_DIR" ]; then
    mkdir -p "$BUILD_DIR"
fi
cd "$BUILD_DIR"

# 强制使用套件内的 Qt6（避免沿用旧缓存里的 /usr/lib/... 系统 Qt，导致找不到 WebEngine）
QT6_CMAKE_DIR="${QT_INSTALL_DIR}/lib/cmake/Qt6"
if [ ! -f "${QT6_CMAKE_DIR}/Qt6Config.cmake" ]; then
    echo "❌ 未找到 Qt6: ${QT6_CMAKE_DIR}/Qt6Config.cmake"
    echo "   请确认 QT_INSTALL_DIR 指向完整套件目录（例如 …/6.10.2/gcc_64）"
    exit 1
fi
if [ ! -f "${QT_INSTALL_DIR}/lib/cmake/Qt6WebEngineQuick/Qt6WebEngineQuickConfig.cmake" ]; then
    echo "❌ 当前 Qt 套件未安装 Qt WebEngine（Qt6WebEngineQuick）"
    echo "   请在 Qt Maintenance Tool 中为该套件勾选「Qt WebEngine」后重试"
    exit 1
fi
if [ -f CMakeCache.txt ]; then
    if grep -qE '^Qt6_DIR:PATH=/usr/' CMakeCache.txt 2>/dev/null; then
        echo "⚠️  检测到 build 缓存指向系统 Qt (/usr)，已清除 CMakeCache，改用: ${QT_INSTALL_DIR}"
        rm -f CMakeCache.txt
        rm -rf CMakeFiles
    fi
fi

# ==========================================
# 3. 执行 CMake 配置
# ==========================================
echo "⚙️  正在配置 CMake..."
cmake -DQt6_DIR="${QT6_CMAKE_DIR}" \
      -DCMAKE_PREFIX_PATH="${QT_INSTALL_DIR}" \
      -DCMAKE_BUILD_TYPE=Release \
      ..

# ==========================================
# 4. 编译项目
# ==========================================
echo "🔨 正在编译..."
cmake --build . --parallel "${PARALLEL_JOBS}"

# ==========================================
# 5. 运行程序
# ==========================================
echo "✅ 编译成功！准备运行..."
echo "========================================"

# 设置环境变量：确保运行时加载安装的 Qt 动态库
if [ "$PLATFORM" = "linux" ]; then
    export LD_LIBRARY_PATH="${QT_INSTALL_DIR}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    # 屏蔽 Qt 字体 OpenType 警告（Monaco 在 Linux 上缺少部分脚本支持）
    export QT_LOGGING_RULES="qt.text.font.db=false"
    # 输入法：让 Qt 找到系统 fcitx5 插件；自定义 Qt 的 plugins 放最前
    _im_plugin_paths=""
    for d in /usr/lib/x86_64-linux-gnu/qt6/plugins /usr/lib64/qt6/plugins /usr/lib/qt6/plugins; do
        [ -d "$d" ] && _im_plugin_paths="${_im_plugin_paths}${_im_plugin_paths:+:}$d"
    done
    [ -n "$_im_plugin_paths" ] && export QT_PLUGIN_PATH="${QT_INSTALL_DIR}/plugins:${_im_plugin_paths}${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
    export QT_IM_MODULES="fcitx5;fcitx;wayland;ibus"
    export QT_IM_MODULE=fcitx5
    export XMODIFIERS=@im=fcitx
elif [ "$PLATFORM" = "macos" ]; then
    export DYLD_LIBRARY_PATH="${QT_INSTALL_DIR}/lib${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
    export QT_PLUGIN_PATH="${QT_INSTALL_DIR}/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
fi

export QML2_IMPORT_PATH="${QT_INSTALL_DIR}/qml${QML2_IMPORT_PATH:+:$QML2_IMPORT_PATH}"

# macOS 为 .app bundle（直接运行 bundle 内二进制以继承环境变量），Linux 为普通可执行文件
if [ "$PLATFORM" = "macos" ] && [ -d "${EXECUTABLE_NAME}.app" ]; then
    exec "./${EXECUTABLE_NAME}.app/Contents/MacOS/${EXECUTABLE_NAME}" "$@"
else
    exec ./${EXECUTABLE_NAME} "$@"
fi
