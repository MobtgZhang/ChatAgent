// qml/MarkdownMathRender.qml
// 使用 WebEngineView + marked + KaTeX 渲染 Markdown 与 LaTeX 数学公式
import QtQuick 2.15
import QtWebEngine
import QtWebChannel

Item {
    id: root
    property string markdownText: ""
    property color  textColor:    "#DBDEE1"
    property int    fontSize:     14
    property bool   isLight:      (typeof settings !== "undefined" && settings.theme === "light")

    implicitHeight: 100
    height: implicitHeight

    onMarkdownTextChanged: Qt.callLater(renderContent)
    Connections {
        target: typeof settings !== "undefined" ? settings : null
        function onThemeChanged() { Qt.callLater(renderContent) }
    }

    function toHexColor(c) {
        if (!c) return "#DBDEE1"
        if (typeof c === "string" && c.charAt(0) === "#") return c
        var r = Math.min(255, Math.round((c.r !== undefined ? c.r : 0.86) * 255))
        var g = Math.min(255, Math.round((c.g !== undefined ? c.g : 0.87) * 255))
        var b = Math.min(255, Math.round((c.b !== undefined ? c.b : 0.88) * 255))
        var rh = r.toString(16); if (rh.length === 1) rh = "0" + rh
        var gh = g.toString(16); if (gh.length === 1) gh = "0" + gh
        var bh = b.toString(16); if (bh.length === 1) bh = "0" + bh
        return "#" + rh + gh + bh
    }

    WebChannel {
        id: webChannel
    }

    Component.onCompleted: {
        if (typeof clipboardBridge !== "undefined")
            webChannel.registerObject("clipboard", clipboardBridge)
        webView.webChannel = webChannel
        webView.url = "qrc:/src/qml/md_render.html"
    }

    function renderContent() {
        try {
            if (!webView || !webView.loadProgress || webView.loadProgress < 100)
                return
            // 内联转换颜色，避免 Qt.callLater 延迟执行时 root 已失效导致 toHexColor 无法调用
            var c = textColor
            var hex = "#DBDEE1"
            if (c && typeof c === "string" && c.charAt(0) === "#") {
                hex = c
            } else if (c && (c.r !== undefined || c.g !== undefined || c.b !== undefined)) {
                var r = Math.min(255, Math.round((c.r !== undefined ? c.r : 0.86) * 255))
                var g = Math.min(255, Math.round((c.g !== undefined ? c.g : 0.87) * 255))
                var b = Math.min(255, Math.round((c.b !== undefined ? c.b : 0.88) * 255))
                var rh = r.toString(16); if (rh.length === 1) rh = "0" + rh
                var gh = g.toString(16); if (gh.length === 1) gh = "0" + gh
                var bh = b.toString(16); if (bh.length === 1) bh = "0" + bh
                hex = "#" + rh + gh + bh
            }
            var txt = markdownText
            var fs = fontSize
            var light = isLight
            var js = "renderContent(" + JSON.stringify(txt) + ","
                    + JSON.stringify(hex) + ","
                    + fs + ","
                    + (light ? "true" : "false") + "); "
                    + "Math.max(20, document.getElementById('content').scrollHeight)"
            webView.runJavaScript(js, function(h) {
                try {
                    if (root && h !== undefined && h !== null && !isNaN(h))
                        root.implicitHeight = Number(h) + 16
                } catch (_) {}
            })
        } catch (_) {}
    }

    WebEngineView {
        id: webView
        anchors.fill: parent
        backgroundColor: "transparent"
        settings.pluginsEnabled: false
        settings.javascriptEnabled: true
        settings.localContentCanAccessFileUrls: true
        settings.localContentCanAccessRemoteUrls: true

        onLoadProgressChanged: {
            if (loadProgress === 100)
                renderContent()
        }
    }
}
