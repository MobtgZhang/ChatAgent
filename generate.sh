#!/bin/bash
# =============================================================================
# generate.sh - 生成自包含 deb 安装包（仅限 Linux）
# 说明：使用 Qt 部署 API 将 Qt 库打包进应用，无需单独安装 Qt 依赖
#       仅包含二进制与资源，不含任何源代码
# =============================================================================

set -e

# 仅支持 Linux（deb 打包）
if [ "$(uname -s)" != "Linux" ]; then
    echo "❌ generate.sh 仅支持 Linux 系统（用于生成 .deb 包）。"
    echo "   其他系统请使用 run.sh (Linux/macOS) 或 run.ps1 (Windows) 构建运行。"
    exit 1
fi

# ==========================================
# 1. 变量配置
# ==========================================
# Qt 安装根目录默认 /home/mobtgzhang/Qt；套件路径可用 QT_INSTALL_DIR 直接指定
QT_ROOT="${QT_ROOT:-/home/mobtgzhang/Qt}"
QT_INSTALL_DIR="${QT_INSTALL_DIR:-${QT_ROOT}/6.10.2/gcc_64}"
QT6_CMAKE_DIR="${QT_INSTALL_DIR}/lib/cmake/Qt6"
BUILD_DIR="build"
INSTALL_PREFIX="/opt/appchatagent"
PKG_NAME="appchatagent"
APP_NAME="ChatAgent"
VERSION="1.2.0"
ARCH="amd64"
EXECUTABLE_NAME="appChatAgent"
STAGING_DIR="deb_staging"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "========================================"
echo "📦 生成自包含 deb 安装包"
echo "   项目: ${APP_NAME} v${VERSION}"
echo "   Qt 根目录: ${QT_ROOT}"
echo "   Qt 套件: ${QT_INSTALL_DIR}"
echo "   安装路径: ${INSTALL_PREFIX}"
echo "   （内置 Qt 库，无需额外依赖）"
echo "========================================"

# ==========================================
# 2. 构建项目
# ==========================================
cd "$PROJECT_ROOT"
if [ ! -d "$BUILD_DIR" ]; then
    mkdir -p "$BUILD_DIR"
fi
cd "$BUILD_DIR"

if [ ! -f "${QT6_CMAKE_DIR}/Qt6Config.cmake" ]; then
    echo "❌ 未找到 Qt6: ${QT6_CMAKE_DIR}/Qt6Config.cmake"
    exit 1
fi
if [ -f CMakeCache.txt ] && grep -qE '^Qt6_DIR:PATH=/usr/' CMakeCache.txt 2>/dev/null; then
    echo "⚠️  清除指向系统 Qt 的 CMake 缓存，改用: ${QT_INSTALL_DIR}"
    rm -f CMakeCache.txt
    rm -rf CMakeFiles
fi

echo ""
echo "⚙️  配置 CMake..."
cmake -DQt6_DIR="${QT6_CMAKE_DIR}" \
      -DCMAKE_PREFIX_PATH="${QT_INSTALL_DIR}" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
      ..

echo ""
echo "🔨 编译项目..."
cmake --build . --parallel $(nproc) --target appChatAgent

# ==========================================
# 3. 安装到临时目录（含 Qt 库部署）
# ==========================================
cd "$PROJECT_ROOT"
echo ""
echo "📦 部署 Qt 库与插件（将 Qt 打包进应用）..."
rm -rf "$STAGING_DIR"
mkdir -p "$STAGING_DIR"
DESTDIR="$PROJECT_ROOT/$STAGING_DIR" cmake --install "$BUILD_DIR"

# ==========================================
# 3.1 创建启动器包装脚本（解决 RPATH 被清空导致找不到 Qt 库的问题）
# ==========================================
echo ""
echo "📝 创建启动器包装脚本..."
_APP_BIN="${STAGING_DIR}${INSTALL_PREFIX}/bin/${EXECUTABLE_NAME}"
_APP_LIB="${STAGING_DIR}${INSTALL_PREFIX}/lib"
if [ -f "$_APP_BIN" ]; then
    mv "$_APP_BIN" "${_APP_BIN}.bin"
    cat > "$_APP_BIN" << 'WRAPPER'
#!/bin/sh
INSTALL_PREFIX="/opt/appchatagent"
export LD_LIBRARY_PATH="${INSTALL_PREFIX}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QTWEBENGINEPROCESS_PATH="${INSTALL_PREFIX}/libexec/QtWebEngineProcess"
export QTWEBENGINE_RESOURCES_PATH="${INSTALL_PREFIX}/resources"
export QTWEBENGINE_LOCALES_PATH="${INSTALL_PREFIX}/translations/qtwebengine_locales"

# 输入法：只使用打包内置的 fcitx5 Qt 插件，避免加载系统 ibus 插件导致崩溃
export QT_PLUGIN_PATH="${INSTALL_PREFIX}/plugins"
export QT_IM_MODULES="fcitx5;fcitx"
export QT_IM_MODULE=fcitx5
export XMODIFIERS=@im=fcitx

exec "${INSTALL_PREFIX}/bin/appChatAgent.bin" "$@"
WRAPPER
    chmod 755 "$_APP_BIN"
fi

# ==========================================
# 3.2 部署 QML 模块所需库（qt_deploy 未包含，因主程序未直接链接）
#     QuickTemplates2/QuickControls2/QuickDialogs2/QuickLayouts 等
# ==========================================
echo ""
echo "📦 部署 QML 插件所需库..."
_APP_LIB="${STAGING_DIR}${INSTALL_PREFIX}/lib"
_QT_LIB="${QT_INSTALL_DIR}/lib"
# 直接按模式整体复制 Quick/QML 相关库，避免漏掉 Fusion/Material/Universal 等样式库
for _pattern in \
    "libQt6QuickTemplates2.so*" \
    "libQt6QuickControls2*.so*" \
    "libQt6QuickDialogs2*.so*" \
    "libQt6QuickLayouts.so*"; do
    for _f in "${_QT_LIB}"/${_pattern}; do
        [ -e "$_f" ] || continue
        cp -a "$_f" "$_APP_LIB/" 2>/dev/null && echo "   ✓ $(basename "$_f")"
    done
done

# 额外复制 QtWebEngine 相关库（QML 插件依赖）
for _pattern in \
    "libQt6WebEngineQuick.so*" \
    "libQt6WebEngineCore.so*"; do
    for _f in "${_QT_LIB}"/${_pattern}; do
        [ -e "$_f" ] || continue
        cp -a "$_f" "$_APP_LIB/" 2>/dev/null && echo "   ✓ $(basename "$_f")"
    done
done

# ==========================================
# 3.3 部署 QML 模块（QtQuick.Controls/Layouts/Dialogs/Window 等）
# ==========================================
echo ""
echo "📦 部署 QML 模块..."
_QML_DEST="${STAGING_DIR}${INSTALL_PREFIX}/qml"
mkdir -p "$_QML_DEST"
_QT_QML="${QT_INSTALL_DIR}/qml"
for _mod in QtQuick QtQuick/Controls QtQuick/Controls/Imagine QtQuick/Layouts QtQuick/Dialogs QtQuick/Window QtWebEngine; do
    if [ -d "${_QT_QML}/${_mod}" ]; then
        mkdir -p "${_QML_DEST}/$(dirname "$_mod")"
        cp -r "${_QT_QML}/${_mod}" "${_QML_DEST}/$(dirname "$_mod")/"
        echo "   ✓ ${_mod}"
    fi
done

# ==========================================
# 3.4 部署 QtWebEngine 运行时（Process/资源/本地化）
# ==========================================
echo ""
echo "📦 部署 QtWebEngine 运行时..."
_QT_LIBEXEC="${QT_INSTALL_DIR}/libexec"
_APP_LIBEXEC="${STAGING_DIR}${INSTALL_PREFIX}/libexec"
if [ -x "${_QT_LIBEXEC}/QtWebEngineProcess" ]; then
    mkdir -p "${_APP_LIBEXEC}"
    cp -a "${_QT_LIBEXEC}/QtWebEngineProcess" "${_APP_LIBEXEC}/"
    echo "   ✓ QtWebEngineProcess"
else
    echo "   ⚠️ 未找到 QtWebEngineProcess，可检查 Qt 安装目录: ${_QT_LIBEXEC}"
fi

# QtWebEngine 资源文件
_QT_RES="${QT_INSTALL_DIR}/resources"
_APP_RES="${STAGING_DIR}${INSTALL_PREFIX}/resources"
mkdir -p "${_APP_RES}"
for _res in \
    "qtwebengine_resources.pak" \
    "qtwebengine_resources_100p.pak" \
    "qtwebengine_resources_200p.pak" \
    "qtwebengine_devtools_resources.pak"; do
    if [ -f "${_QT_RES}/${_res}" ]; then
        cp -a "${_QT_RES}/${_res}" "${_APP_RES}/"
        echo "   ✓ ${_res}"
    fi
done

# QtWebEngine 本地化
_QT_LOCALES="${QT_INSTALL_DIR}/translations/qtwebengine_locales"
_APP_LOCALES="${STAGING_DIR}${INSTALL_PREFIX}/translations/qtwebengine_locales"
if [ -d "${_QT_LOCALES}" ]; then
    mkdir -p "$(dirname "${_APP_LOCALES}")"
    cp -a "${_QT_LOCALES}" "${_APP_LOCALES%/..}/"
    echo "   ✓ qtwebengine_locales"
fi

# ==========================================
# 3.5 手动部署 Qt 输入法插件（仅 fcitx5，避免 ibus 崩溃）
# ==========================================
echo ""
echo "📦 部署 Qt 输入法插件 (fcitx5)..."
_QT_PlatformInput="${QT_INSTALL_DIR}/plugins/platforminputcontexts"
_APP_PlatformInput="${STAGING_DIR}${INSTALL_PREFIX}/plugins/platforminputcontexts"
mkdir -p "${_APP_PlatformInput}"
for _im in \
    "libfcitx5platforminputcontextplugin.so"; do
    if [ -f "${_QT_PlatformInput}/${_im}" ]; then
        cp -a "${_QT_PlatformInput}/${_im}" "${_APP_PlatformInput}/"
        echo "   ✓ ${_im}"
    else
        echo "   ⚠️ 未找到 ${_im}，请检查 Qt 安装是否包含 fcitx5 Qt6 插件 (fcitx5-frontend-qt6)"
    fi
done

# ==========================================
# 4. 添加桌面项与图标
# ==========================================
echo ""
echo "📁 添加桌面项与图标..."
mkdir -p "${STAGING_DIR}/usr/share/applications"
mkdir -p "${STAGING_DIR}/usr/share/icons/hicolor/48x48/apps"
mkdir -p "${STAGING_DIR}/usr/share/icons/hicolor/128x128/apps"
mkdir -p "${STAGING_DIR}/usr/share/icons/hicolor/256x256/apps"

ICON_SRC="${PROJECT_ROOT}/src/icons/app_icon.png"
if [ -f "$ICON_SRC" ]; then
    cp "$ICON_SRC" "${STAGING_DIR}/usr/share/icons/hicolor/256x256/apps/${PKG_NAME}.png"
    if command -v convert &>/dev/null; then
        convert "$ICON_SRC" -resize 48x48 "${STAGING_DIR}/usr/share/icons/hicolor/48x48/apps/${PKG_NAME}.png"
        convert "$ICON_SRC" -resize 128x128 "${STAGING_DIR}/usr/share/icons/hicolor/128x128/apps/${PKG_NAME}.png"
    else
        cp "$ICON_SRC" "${STAGING_DIR}/usr/share/icons/hicolor/48x48/apps/${PKG_NAME}.png"
        cp "$ICON_SRC" "${STAGING_DIR}/usr/share/icons/hicolor/128x128/apps/${PKG_NAME}.png"
    fi
else
    echo "⚠️  未找到图标: $ICON_SRC，桌面图标可能无法正常显示"
fi

cat > "${STAGING_DIR}/usr/share/applications/${PKG_NAME}.desktop" << DESKTOP
[Desktop Entry]
Version=1.0
Type=Application
Name=ChatAgent
Comment=支持 OpenAI 兼容接口的本地 AI 聊天客户端
Exec=${INSTALL_PREFIX}/bin/${EXECUTABLE_NAME}
Icon=${PKG_NAME}
Categories=Utility;Network;Chat;
Terminal=false
StartupNotify=true
Keywords=chat;AI;LLM;agent;
DESKTOP

# 确认桌面文件与图标已包含
echo "   ✓ .desktop: usr/share/applications/${PKG_NAME}.desktop"
echo "   ✓ 图标: usr/share/icons/hicolor/48x48,128x128,256x256/apps/${PKG_NAME}.png"

# 创建 /usr/bin 快捷方式（可选，便于命令行启动）
mkdir -p "${STAGING_DIR}/usr/bin"
ln -sf "${INSTALL_PREFIX}/bin/${EXECUTABLE_NAME}" "${STAGING_DIR}/usr/bin/${EXECUTABLE_NAME}" 2>/dev/null || true

# ==========================================
# 5. DEBIAN/control（仅依赖系统基础库）
# ==========================================
mkdir -p "${STAGING_DIR}/DEBIAN"
cat > "${STAGING_DIR}/DEBIAN/control" << CONTROL
Package: ${PKG_NAME}
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: ${ARCH}
Depends: libc6 (>= 2.34), libstdc++6 (>= 12), libgcc-s1 (>= 4.2)
Maintainer: User <user@localhost>
Description: 支持 OpenAI 兼容接口的本地 AI 聊天客户端
 支持流式输出、Markdown 渲染与数学公式显示。
 内置 Qt 运行时，无需单独安装 Qt 依赖。
CONTROL

# ==========================================
# 6. postinst：更新桌面与图标缓存
# ==========================================
cat > "${STAGING_DIR}/DEBIAN/postinst" << 'POSTINST'
#!/bin/sh
set -e
if [ -n "$DESTDIR" ]; then
    exit 0
fi
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /usr/share/applications 2>/dev/null || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f /usr/share/icons/hicolor 2>/dev/null || true
fi
exit 0
POSTINST
chmod 755 "${STAGING_DIR}/DEBIAN/postinst"

# ==========================================
# 7. 打包生成 .deb
# ==========================================
OUTPUT_DEB="${PROJECT_ROOT}/${PKG_NAME}_${VERSION}_${ARCH}.deb"
echo ""
echo "📦 生成 .deb 包..."
if command -v fakeroot &>/dev/null; then
    fakeroot dpkg-deb --build "$STAGING_DIR" "$OUTPUT_DEB"
else
    echo "⚠️  未找到 fakeroot，使用 dpkg-deb（若遇权限问题请安装: sudo apt install fakeroot）"
    dpkg-deb --build "$STAGING_DIR" "$OUTPUT_DEB"
fi

# 清理临时目录
rm -rf "$STAGING_DIR"

echo ""
echo "========================================"
echo "✅ 完成！自包含 deb 包已生成："
echo "   ${OUTPUT_DEB}"
echo ""
echo "安装方式（无需额外依赖）："
echo "   sudo dpkg -i ${PKG_NAME}_${VERSION}_${ARCH}.deb"
echo "========================================"
