// qml/Main.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import QtWebEngine 1.15
import QtWebChannel 1.15

ApplicationWindow {
    id: appWindow
    width: 1060; height: 720
    minimumWidth: 800; minimumHeight: 560
    visible: true
    title: "ChatAgent — " + mainView.sessionName
    color: (typeof settings !== "undefined" && settings.theme === "light") ? "#F8FAFC" : "#0F172A"
    // ── 动态色板：Dark / Light 模式，舒适现代配色 ────────────────────────────────
    readonly property bool isLight:     (typeof settings !== "undefined" && settings.theme === "light")
    readonly property color cBg:        isLight ? "#F8FAFC" : "#0F172A"
    readonly property color cChatBg:    isLight ? "#FFFFFF" : "#0F172A"
    readonly property color cSidebar:   isLight ? "#F1F5F9" : "#1E293B"
    readonly property color cInput:     isLight ? "#FFFFFF" : "#1E293B"
    readonly property color cBorder:    isLight ? "#E2E8F0" : "#334155"
    readonly property color cHighlight: isLight ? "#E2E8F0" : "#334155"
    readonly property color cText:      isLight ? "#1E293B" : "#E2E8F0"
    readonly property color cMuted:     isLight ? "#64748B" : "#94A3B8"
    readonly property color cAccent:     isLight ? "#4F46E5" : "#6366F1"
    readonly property color cDivider:   isLight ? "#E2E8F0" : "#334155"
    readonly property color cSelectedBg: isLight ? "#EEF2FF" : "#312E81"
    readonly property color cScrollBar: isLight ? "#CBD5E1" : "#475569"
    readonly property color cScrollBarHover: isLight ? "#94A3B8" : "#64748B"
    readonly property color cPopupBg: isLight ? "#FFFFFF" : "#1E293B"

    // 左侧历史记录区高度（拖动分隔栏可调；允许为 0 以实现“折叠”效果）
    property real sidebarHistoryHeight: 260

    // 让拖动高度变化更“丝滑”：带速度的平滑动画（不会一帧一跳）
    Behavior on sidebarHistoryHeight {
        SmoothedAnimation {
            velocity: 1800
            easing.type: Easing.OutCubic
        }
    }

    // 关闭主窗口时直接退出应用，避免进程在后台残留
    onClosing: {
        Qt.quit()
    }

    // ── 弹窗（懒创建）────────────────────────────────────────────────────────
    property var settingsWin: null
    property var aboutWin:    null

    function openSettings() {
        if (!settingsWin) {
            var comp = Qt.createComponent("qrc:/src/qml/Settings.qml")
            if (comp.status === Component.Ready) {
                settingsWin = comp.createObject(appWindow, { "settings": settings })
            } else if (comp.status === Component.Error) {
                console.error("❌ Settings.qml 加载失败:", comp.errorString())
                return
            }
        }
        if (settingsWin) {
            settingsWin.show()
            settingsWin.raise()
            settingsWin.requestActivate()
        }
    }

    function openAbout() {
        if (!aboutWin) {
            var comp = Qt.createComponent("qrc:/src/qml/About.qml")
            if (comp.status === Component.Ready) {
                aboutWin = comp.createObject(appWindow)
            } else if (comp.status === Component.Error) {
                console.error("❌ About.qml 加载失败:", comp.errorString())
                return
            }
        }
        if (aboutWin) {
            aboutWin.show()
            aboutWin.raise()
            aboutWin.requestActivate()
        }
    }


    // ── 清空确认对话框 ────────────────────────────────────────────────────────
    Dialog {
        id: clearDialog
        title: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.clearChat : "Clear Chat"
        modal: true
        anchors.centerIn: parent
        width: 300

        background: Rectangle { color: cPopupBg; radius: 8; border.color: cBorder }

        contentItem: Column {
            spacing: 12; padding: 16
            Text {
                text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0 && localeBridge.t.clearChatConfirm) ? localeBridge.t.clearChatConfirm : "Are you sure to clear this chat? This cannot be undone."
                color: cText; font.pixelSize: 14; lineHeight: 1.5; wrapMode: Text.Wrap
                width: 260
            }
        }

        footer: Row {
            spacing: 8; padding: 12; layoutDirection: Qt.RightToLeft
            Button {
                text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0 && localeBridge.t.clear) ? localeBridge.t.clear : "Clear"; width: 80; height: 32
                contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 13;
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 5; color: "#ED4245" }
                onClicked: { mainView.clearMessages(); clearDialog.close() }
            }
            Button {
                text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.cancel : "Cancel"; width: 80; height: 32
                contentItem: Text { text: parent.text; color: cText; font.pixelSize: 13;
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 5; color: cInput; border.color: cBorder }
                onClicked: clearDialog.close()
            }
        }
    }

    // ── 删除确认对话框 ────────────────────────────────────────────────────────
    Dialog {
        id: deleteNodeDialog
        title: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.delete : "Delete"
        modal: true
        anchors.centerIn: parent
        width: 320

        property string targetId: ""
        property string targetName: ""
        property string targetType: ""

        background: Rectangle { color: cPopupBg; radius: 8; border.color: cBorder }

        contentItem: Column {
            spacing: 12; padding: 16
            Text {
                text: (localeBridge && localeBridge.lang) ? (deleteNodeDialog.targetType === "folder"
                       ? localeBridge.tr("deleteFolderConfirm", deleteNodeDialog.targetName)
                       : localeBridge.tr("deleteSessionConfirm", deleteNodeDialog.targetName)) : deleteNodeDialog.targetName
                color: cText; font.pixelSize: 14; lineHeight: 1.5; wrapMode: Text.Wrap
                width: 280
            }
        }

        footer: Row {
            spacing: 8; padding: 12; layoutDirection: Qt.RightToLeft
            Button {
                text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.delete : "Delete"; width: 80; height: 32
                contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 13;
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 5; color: "#ED4245" }
                onClicked: {
                    if (deleteNodeDialog.targetId !== "") {
                        var isCurrentSession = deleteNodeDialog.targetType === "session" && deleteNodeDialog.targetId === mainView.currentSession
                        history.deleteNode(deleteNodeDialog.targetId)
                        // 如果删除的是当前选中的会话，删除后创建新对话并清空聊天记录
                        if (isCurrentSession) {
                            mainView.newSession()
                        }
                    }
                    deleteNodeDialog.close()
                }
            }
            Button {
                text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.cancel : "Cancel"; width: 80; height: 32
                contentItem: Text { text: parent.text; color: cText; font.pixelSize: 13;
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 5; color: cInput; border.color: cBorder }
                onClicked: deleteNodeDialog.close()
            }
        }
    }

    // ── 重命名输入框 ──────────────────────────────────────────────────────────
    Dialog {
        id: renameDialog
        title: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.rename : "Rename"
        modal: true
        anchors.centerIn: parent
        width: 320

        background: Rectangle { color: cPopupBg; radius: 8; border.color: cBorder }

        contentItem: Column {
            spacing: 10; padding: 16
            Text { text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0 && localeBridge.t.enterNewName) ? localeBridge.t.enterNewName : "Enter new name:"; color: cText; font.pixelSize: 13 }
            TextField {
                id: renameField
                width: 280; height: 36
                color: cText; font.pixelSize: 13
                background: Rectangle { radius: 5; color: cInput; border.color: cBorder }
                leftPadding: 10
                selectByMouse: true
                Keys.onReturnPressed: { mainView.renameSession(text); renameDialog.close() }
            }
        }

        footer: Row {
            spacing: 8; padding: 12; layoutDirection: Qt.RightToLeft
            Button {
                text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.ok : "OK"; width: 80; height: 32
                contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 13;
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 5; color: cAccent }
                onClicked: { mainView.renameSession(renameField.text); renameDialog.close() }
            }
            Button {
                text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.cancel : "Cancel"; width: 80; height: 32
                contentItem: Text { text: parent.text; color: cText; font.pixelSize: 13;
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 5; color: cInput; border.color: cBorder }
                onClicked: renameDialog.close()
            }
        }

        onAboutToShow: {
            renameField.text = mainView.sessionName
            renameField.forceActiveFocus()
            renameField.selectAll()
        }
    }

    // ── 导出文件选择对话框 ────────────────────────────────────────────────────
    FileDialog {
        id: exportFileDialog
        title: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.exportChat : "Export Chat"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Markdown (*.md)"]
        defaultSuffix: "md"
        acceptLabel: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.save : "Save"
        onAccepted: {
            var path = selectedFile.toString()
            var ok = mainView.exportCurrentChat(path)
            exportResultDialog.isSuccess = ok
            exportResultDialog.open()
        }
    }

    // ── 导出结果提示对话框 ────────────────────────────────────────────────────
    Dialog {
        id: exportResultDialog
        title: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? (exportResultDialog.isSuccess ? localeBridge.t.exportSuccess : localeBridge.t.exportFailed) : (exportResultDialog.isSuccess ? "Export successful" : "Export failed")
        modal: true
        anchors.centerIn: parent
        width: 300

        property bool isSuccess: true

        background: Rectangle { color: cPopupBg; radius: 8; border.color: cBorder }

        contentItem: Column {
            spacing: 12; padding: 16
            Text {
                text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? (exportResultDialog.isSuccess ? localeBridge.t.exportSuccessMsg : localeBridge.t.exportFailedMsg) : (exportResultDialog.isSuccess ? "Chat exported as Markdown file." : "Cannot write file. Please check path or permissions.")
                color: cText; font.pixelSize: 14; lineHeight: 1.5; wrapMode: Text.Wrap
                width: 260
            }
        }

        footer: Row {
            spacing: 8; padding: 12; layoutDirection: Qt.RightToLeft
            Button {
                text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.ok : "OK"; width: 80; height: 32
                contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 13;
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 5; color: cAccent }
                onClicked: exportResultDialog.close()
            }
        }
    }

    // ── 新建文件夹对话框 ──────────────────────────────────────────────────────
    Dialog {
        id: folderDialog
        title: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.newFolder : "New Folder"
        modal: true
        anchors.centerIn: parent
        width: 320

        property string parentId: ""

        background: Rectangle { color: cPopupBg; radius: 8; border.color: cBorder }

        contentItem: Column {
            spacing: 10; padding: 16
            Text { text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0 && localeBridge.t.folderName) ? localeBridge.t.folderName : "Folder name:"; color: cText; font.pixelSize: 13 }
            TextField {
                id: folderNameField
                width: 280; height: 36
                color: cText; font.pixelSize: 13
                background: Rectangle { radius: 5; color: cInput; border.color: cBorder }
                leftPadding: 10
                selectByMouse: true
                Keys.onReturnPressed: {
                    if (text.trim() !== "") {
                        history.createFolder(text.trim(), folderDialog.parentId)
                        folderDialog.close()
                    }
                }
            }
        }

        footer: Row {
            spacing: 8; padding: 12; layoutDirection: Qt.RightToLeft
            Button {
                text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.create : "Create"; width: 80; height: 32
                contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 13;
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 5; color: cAccent }
                onClicked: {
                    if (folderNameField.text.trim() !== "") {
                        history.createFolder(folderNameField.text.trim(), folderDialog.parentId)
                        folderDialog.close()
                    }
                }
            }
            Button {
                text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.cancel : "Cancel"; width: 80; height: 32
                contentItem: Text { text: parent.text; color: cText; font.pixelSize: 13;
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 5; color: cInput; border.color: cBorder }
                onClicked: folderDialog.close()
            }
        }

        onAboutToShow: { folderNameField.text = ""; folderNameField.forceActiveFocus() }
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //                           主布局
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ═══════════════════════════ 左侧：侧边栏 ════════════════════════════
        Rectangle {
            Layout.preferredWidth: 248
            Layout.fillHeight: true
            color: cSidebar

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // App 标题
                Rectangle {
                    Layout.fillWidth: true; height: 58
                    color: "transparent"
                    RowLayout {
                        anchors { fill: parent; leftMargin: 14; rightMargin: 14 }
                        spacing: 10
                        Rectangle {
                            width: 32; height: 32; radius: 8; color: "#F9A825"
                            Text { anchors.centerIn: parent; text: "💬"; font.pixelSize: 18 }
                        }
                        Column {
                            spacing: 1; Layout.fillWidth: true
                            Text { text: "ChatAgent"; color: cText; font.bold: true; font.pixelSize: 15 }
                            Text { text: "Community Edition"; color: cMuted; font.pixelSize: 10 }
                        }
                    }
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: cDivider }
                }

                // 新建按钮行
                Rectangle {
                    Layout.fillWidth: true; height: 42
                    color: "transparent"

                    RowLayout {
                        anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                        spacing: 6

                        // 新对话
                        Rectangle {
                            Layout.fillWidth: true; height: 30; radius: 5
                            color: newChatHover.hovered ? cHighlight : cDivider
                            Row {
                                anchors.centerIn: parent; spacing: 6
                                Text { text: "✏️"; font.pixelSize: 13 }
                                Text { text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.newChat : "New Chat"; color: cText; font.pixelSize: 12 }
                            }
                            HoverHandler { id: newChatHover }
                            MouseArea {
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: mainView.newSession()
                            }
                        }

                        // 新建文件夹
                        Rectangle {
                            width: 30; height: 30; radius: 5
                            color: folderHover.hovered ? cHighlight : cDivider
                            Text { anchors.centerIn: parent; text: "📁"; font.pixelSize: 15 }
                            ToolTip.text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.newFolderInMenu : "New Folder"
                            ToolTip.visible: folderHover.hovered
                            ToolTip.delay: 600
                            HoverHandler { id: folderHover }
                            MouseArea {
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    folderDialog.parentId = ""
                                    folderDialog.open()
                                }
                            }
                        }
                    }
                }

                // 分隔线
                Rectangle { Layout.fillWidth: true; height: 1; color: cDivider }

                // ── 历史树列表（可拖动分割线调整高度）───────────────────────────
                ColumnLayout {
                    id: historyColumn
                    Layout.fillWidth: true
                    Layout.preferredHeight: sidebarHistoryHeight
                    Layout.minimumHeight: 0
                    spacing: 0

                    // 历史记录标题栏
                    Rectangle {
                        Layout.fillWidth: true
                        height: 36
                        color: "transparent"

                        RowLayout {
                            anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                            spacing: 6

                            Text {
                                text: "📜 " + ((localeBridge && localeBridge.t && localeBridge.tVersion >= 0 && localeBridge.t.history) ? localeBridge.t.history : "历史记录")
                                color: cText
                                font.pixelSize: 13
                                font.bold: true
                            }
                            Item { Layout.fillWidth: true }
                            Rectangle {
                                width: 22; height: 18; radius: 9
                                color: cAccent
                                visible: history && history.flatModel && history.flatModel.count > 0
                                Text {
                                    anchors.centerIn: parent
                                    text: history && history.flatModel ? history.flatModel.count : "0"
                                    color: "white"
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }
                        }
                    }

                    ListView {
                        id: historyList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: history.flatModel
                        spacing: 1
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            contentItem: Rectangle {
                                implicitWidth: 6
                                radius: 3
                                color: parent.pressed ? cScrollBarHover : (parent.hovered ? cScrollBarHover : cScrollBar)
                            }
                            background: Rectangle {
                                implicitWidth: 6
                                color: cDivider
                                radius: 3
                            }
                        }

                        delegate: Item {
                        id: delegateRoot
                        width: historyList.width
                        height: 36

                        property bool held: false  // 长按后为 true，用于启动拖拽
                        property bool validDropTarget: false  // 拖拽悬停时，是否为有效放置目标（同层兄弟）

                        readonly property string nodeId:   model.nodeId || ""
                        readonly property string nodeType: model.nodeType || ""
                        readonly property string nodeName: model.nodeName || ""
                        readonly property string nodeParentId: model.nodeParentId || ""
                        readonly property int    nodeDepth: model.nodeDepth || 0
                        readonly property bool   nodeExpanded: model.nodeExpanded !== false
                        readonly property bool   isSession: nodeType === "session"
                        readonly property bool   isFolder:   nodeType === "folder"
                        readonly property bool   isCurrent: isSession && nodeId === mainView.currentSession

                        Drag.active: held
                        Drag.hotSpot.x: width / 2
                        Drag.hotSpot.y: height / 2
                        Drag.source: delegateRoot
                        Drag.keys: ["history-node"]

                        Rectangle {
                            id: rowRect
                            anchors { fill: parent; leftMargin: 6; rightMargin: 6 }
                            radius: 5
                            color: {
                                if (dropArea.containsDrag && validDropTarget) return cAccent + "40"
                                if (isCurrent) return cSelectedBg
                                if (itemHover.hovered) return cHighlight
                                return "transparent"
                            }
                            opacity: Drag.active ? 0.6 : 1.0

                            RowLayout {
                                anchors { fill: parent; leftMargin: nodeDepth * 14 + 8; rightMargin: 8 }
                                spacing: 6

                                // 拖拽手柄（长按后拖动可调整上下顺序）
                                Text {
                                    text: "⋮⋮"
                                    font.pixelSize: 12
                                    color: cMuted
                                    opacity: itemHover.hovered ? 1 : (isLight ? 0.7 : 0.4)
                                    Layout.preferredWidth: 16
                                    horizontalAlignment: Text.AlignHCenter
                                    MouseArea {
                                        id: dragHandle
                                        anchors.fill: parent
                                        cursorShape: held ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                        pressAndHoldInterval: 400  // 长按 400ms 后启动拖拽
                                        drag.target: held ? rowRect : undefined
                                        drag.axis: Drag.YAxis
                                        drag.smoothed: false
                                        onPressAndHold: delegateRoot.held = true
                                        onReleased: delegateRoot.held = false
                                    }
                                }

                                Text {
                                    font.pixelSize: 13
                                    text: isFolder
                                        ? (nodeExpanded ? "📂" : "📁")
                                        : (isCurrent ? "🗨️" : "💬")
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: nodeName
                                    color: isCurrent ? (isLight ? cText : "white") : cText
                                    font.pixelSize: 12; font.bold: isCurrent
                                    elide: Text.ElideRight
                                }

                                Row {
                                    spacing: 2
                                    visible: itemHover.hovered

                                    Text {
                                        text: "🗑"; font.pixelSize: 12
                                        color: delHover.hovered ? "#ED4245" : cMuted
                                        HoverHandler { id: delHover }
                                        MouseArea {
                                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                deleteNodeDialog.targetId = nodeId
                                                deleteNodeDialog.targetName = nodeName
                                                deleteNodeDialog.targetType = nodeType
                                                deleteNodeDialog.open()
                                            }
                                        }
                                    }
                                }
                            }

                            HoverHandler { id: itemHover }
                            MouseArea {
                                id: mainMouseArea
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                acceptedButtons: Qt.RightButton
                                onClicked: function(mouse) {
                                    if (mouse.button === Qt.RightButton) {
                                        if (historyContextMenu.visible)
                                            return
                                        historyContextMenu.targetNode = {
                                            id: nodeId, type: nodeType, name: nodeName,
                                            depth: nodeDepth, expanded: nodeExpanded,
                                            parentId: model.nodeParentId || ""
                                        }
                                        var target = Overlay.overlay || appWindow.contentItem
                                        var pos = mainMouseArea.mapToItem(target, mouse.x, mouse.y)
                                        historyContextMenu.openAt(pos.x, pos.y)
                                    }
                                }
                            }

                            MouseArea {
                                id: mainClickArea
                                anchors { fill: parent; leftMargin: nodeDepth * 14 + 8 + 16 + 6 }
                                cursorShape: Qt.PointingHandCursor
                                acceptedButtons: Qt.LeftButton
                                onClicked: {
                                    if (isFolder)
                                        history.toggleExpand(nodeId)
                                    else if (nodeId)
                                        mainView.loadSession(nodeId)
                                }
                            }
                        }

                        DropArea {
                            id: dropArea
                            anchors.fill: parent
                            keys: ["history-node"]
                            onEntered: {
                                // 只有同层兄弟节点才能作为有效放置目标，用于调整顺序
                                validDropTarget = (drag.source && drag.source !== delegateRoot
                                    && drag.source.nodeId && drag.source.nodeId !== nodeId
                                    && String(drag.source.nodeParentId || "") === String(nodeParentId || ""))
                            }
                            onExited: validDropTarget = false
                            onDropped: {
                                if (validDropTarget && drag.source && drag.source.nodeId && drag.source.nodeId !== nodeId) {
                                    history.reorderNode(drag.source.nodeId, nodeId)
                                }
                                validDropTarget = false
                            }
                        }
                    }
                }
                }  // 关闭历史记录ColumnLayout

                // 可拖动分隔栏
                Rectangle {
                    id: skillsSplitter
                    Layout.fillWidth: true
                    height: 14
                    color: "transparent"

                    Rectangle {
                        anchors.centerIn: parent
                        width: 40
                        height: 4
                        radius: 2
                        color: splitterHover.containsMouse ? cAccent : cMuted
                    }

                    // 只负责提供“↑↓”光标（DragHandler 本身不控制 cursorShape）
                    MouseArea {
                        id: splitterHover
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.NoButton
                        cursorShape: Qt.SizeVerCursor
                    }

                    // 用 DragHandler 替代 MouseArea：更丝滑，且拖动过程中不易“丢失”鼠标
                    DragHandler {
                        id: splitterDrag
                        target: null
                        yAxis.enabled: true
                        xAxis.enabled: false
                        grabPermissions: PointerHandler.CanTakeOverFromAnything

                        property real startHistoryHeight: 0

                        onActiveChanged: {
                            if (active) startHistoryHeight = sidebarHistoryHeight
                        }

                        onTranslationChanged: {
                            if (!active) return

                            var delta = translation.y
                            // 允许拖到 0 形成“折叠”效果；上限需要给技能区与底部按钮留空间
                            var parentH = historyColumn.parent.height
                            var minSkills = skillsColumn.Layout.minimumHeight
                            var minBottom = bottomButtons.implicitHeight
                            var maxHistory = Math.max(0, parentH - minSkills - skillsSplitter.height - minBottom)

                            sidebarHistoryHeight = Math.max(0, Math.min(maxHistory, startHistoryHeight + delta))
                        }
                    }
                }

                // ── 技能面板（可拖动分隔调整高度）─────────────────────────────
                ColumnLayout {
                    id: skillsColumn
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 0
                    spacing: 0

                    // 技能标题栏
                    Rectangle {
                        Layout.fillWidth: true
                        height: 36
                        color: "transparent"

                        RowLayout {
                            anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                            spacing: 6

                            Text {
                                text: "🛠 " + ((localeBridge && localeBridge.t && localeBridge.tVersion >= 0 && localeBridge.t.skills) ? localeBridge.t.skills : "Skills")
                                color: cText
                                font.pixelSize: 13
                                font.bold: true
                            }
                            Item { Layout.fillWidth: true }
                            Rectangle {
                                width: 22; height: 18; radius: 9
                                color: cAccent
                                visible: typeof skillManager !== "undefined" && skillManager && skillManager.skills.length > 0
                                Text {
                                    anchors.centerIn: parent
                                    text: (typeof skillManager !== "undefined" && skillManager) ? skillManager.skills.length : "0"
                                    color: "white"
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }
                        }
                    }

                    ListView {
                        id: skillsListView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 4
                        model: (typeof skillManager !== "undefined" && skillManager) ? skillManager.skills : []

                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            contentItem: Rectangle {
                                implicitWidth: 6
                                radius: 3
                                color: parent.pressed ? cAccent : (parent.hovered ? cAccent : cMuted)
                            }
                        }

                        delegate: Rectangle {
                            id: skillCard
                            width: skillsListView.width - 16
                            x: 8
                            height: 56
                            radius: 8
                            color: skillCardHover.hovered ? cHighlight : (isLight ? "#F1F5F9" : "#1E293B")
                            border.color: skillCardHover.hovered ? cAccent : cBorder
                            border.width: skillCardHover.hovered ? 1 : 0

                            property string skillId: modelData.id || ""

                            RowLayout {
                                anchors { fill: parent; margins: 8 }
                                spacing: 8

                                Rectangle {
                                    width: 36; height: 36; radius: 8
                                    color: isLight ? "#EEF2FF" : "#312E81"
                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.icon || "🔧"
                                        font.pixelSize: 18
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text {
                                        text: modelData.title || ""
                                        color: cText
                                        font.pixelSize: 12
                                        font.bold: true
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        property string ct: modelData.content ? String(modelData.content) : ""
                                        text: ct.length > 30 ? ct.substring(0, 30) + "…" : ct
                                        color: cMuted
                                        font.pixelSize: 10
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }

                            }

                            HoverHandler { id: skillCardHover }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                onClicked: function(mouse) {
                                    if (mouse.button === Qt.RightButton) {
                                        skillContextMenu.targetId      = skillCard.skillId
                                        skillContextMenu.targetTitle   = modelData.title   || ""
                                        skillContextMenu.targetIcon    = modelData.icon    || "🔧"
                                        skillContextMenu.targetContent = modelData.content || ""
                                        var target = Overlay.overlay || appWindow.contentItem
                                        var pos = skillCard.mapToItem(target, mouse.x, mouse.y)
                                        skillContextMenu.openAt(pos.x, pos.y)
                                    } else {
                                        skillDetailPopup.skillTitle   = modelData.title   || ""
                                        skillDetailPopup.skillIcon    = modelData.icon    || "🔧"
                                        skillDetailPopup.skillContent = modelData.content || ""
                                        skillDetailPopup.open()
                                    }
                                }
                            }
                        }
                    }
                }

                // ── 底部按钮 ──────────────────────────────────────────────────
                RowLayout {
                    id: bottomButtons
                    Layout.fillWidth: true
                    Layout.bottomMargin: 8
                    Layout.leftMargin: 8
                    Layout.rightMargin: 8
                    spacing: 6

                    // 新建技能按钮
                    Rectangle {
                        Layout.fillWidth: true
                        height: 36
                        radius: 5
                        color: newSkillBtnHover.hovered ? cAccent : cDivider
                        Row {
                            anchors.centerIn: parent
                            spacing: 6
                            Text { text: "➕"; color: newSkillBtnHover.hovered ? "white" : cMuted; font.pixelSize: 12 }
                            Text { text: "新建技能"; color: newSkillBtnHover.hovered ? "white" : cText; font.pixelSize: 12 }
                        }
                        HoverHandler { id: newSkillBtnHover }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                editSkillDialog.targetId = ""
                                editSkillDialog.isNewMode = true
                                editSkillIconField.text = "🔧"
                                editSkillTitleField.text = ""
                                editSkillContentArea.text = ""
                                editSkillDialog.open()
                            }
                        }
                    }

                    // 加载技能按钮
                    Rectangle {
                        Layout.fillWidth: true
                        height: 36
                        radius: 5
                        color: loadSkillBtnHover.hovered ? cAccent : cDivider
                        Row {
                            anchors.centerIn: parent
                            spacing: 6
                            Text { text: "📂"; color: loadSkillBtnHover.hovered ? "white" : cMuted; font.pixelSize: 12 }
                            Text { text: "新建文件夹"; color: loadSkillBtnHover.hovered ? "white" : cText; font.pixelSize: 12 }
                        }
                        HoverHandler { id: loadSkillBtnHover }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: importSkillDialog.openForLoad()
                        }
                    }
                }

                // 设置和关于按钮
                RowLayout {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 8
                    Layout.leftMargin: 8
                    Layout.rightMargin: 8
                    spacing: 6

                    Rectangle {
                        Layout.fillWidth: true
                        height: 36
                        radius: 5
                        color: settingsHover.hovered ? cHighlight : "transparent"
                        Row {
                            anchors.centerIn: parent
                            spacing: 6
                            Text { text: "⚙️"; color: cMuted; font.pixelSize: 13 }
                            Text { text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0 && localeBridge.t.settings) ? localeBridge.t.settings : "设置"; color: cText; font.pixelSize: 12 }
                        }
                        HoverHandler { id: settingsHover }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: openSettings()
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 36
                        radius: 5
                        color: aboutHover.hovered ? cHighlight : "transparent"
                        Row {
                            anchors.centerIn: parent
                            spacing: 6
                            Text { text: "ℹ️"; color: cMuted; font.pixelSize: 13 }
                            Text { text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0 && localeBridge.t.about) ? localeBridge.t.about : "关于"; color: cText; font.pixelSize: 12 }
                        }
                        HoverHandler { id: aboutHover }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: openAbout()
                        }
                    }
                }
            }
        }

        // ═══════════════════════════ 中间：聊天区 ════════════════════════════
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // 聊天区域背景（与侧边栏区分的柔和底色）
            Rectangle {
                anchors { top: parent.top; bottom: parent.bottom; left: parent.left; right: parent.right }
                color: cChatBg
            }

            // ── 顶部 Header ───────────────────────────────────────────────────
            Rectangle {
                id: chatHeader
                anchors { top: parent.top; left: parent.left; right: parent.right }
                height: 56
                color: "transparent"

                RowLayout {
                    anchors { fill: parent; leftMargin: 20; rightMargin: 16 }
                    spacing: 12

                    // 会话名（双击重命名）
                    Text {
                        id: sessionNameLabel
                        text: mainView.sessionName
                        color: cText; font.pixelSize: 15; font.bold: true
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onDoubleClicked: renameDialog.open()
                        }
                        ToolTip.text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.doubleClickRename : "Double-click to rename"
                        ToolTip.visible: nameTipHover.hovered
                        ToolTip.delay: 800
                        HoverHandler { id: nameTipHover }
                    }

                    // 停止生成
                    Rectangle {
                        height: 30; radius: 5
                        width: stopRow.implicitWidth + 24
                        Layout.minimumWidth: stopRow.implicitWidth + 24
                        visible: mainView.isStreaming
                        color: "#ED4245"
                        Row {
                            id: stopRow
                            anchors.centerIn: parent
                            spacing: 5
                            Text { text: "⏹"; font.pixelSize: 12 }
                            Text { text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.stop : "Stop"; color: "white"; font.pixelSize: 12 }
                        }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: mainView.stopGeneration()
                        }
                    }

                    // 清空对话
                    Rectangle {
                        height: 30; radius: 5
                        width: clearRow.implicitWidth + 24
                        Layout.minimumWidth: clearRow.implicitWidth + 24
                        color: clearHover.hovered ? "#ED4245" : cHighlight
                        Row {
                            id: clearRow
                            anchors.centerIn: parent
                            spacing: 5
                            Text { text: "🗑️"; font.pixelSize: 12 }
                            Text { text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.clear : "Clear"; color: cText; font.pixelSize: 12 }
                        }
                        HoverHandler { id: clearHover }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: clearDialog.open()
                        }
                    }

                    // 导出对话（右上角）
                    Rectangle {
                        height: 30; radius: 5
                        width: exportRow.implicitWidth + 24
                        Layout.minimumWidth: exportRow.implicitWidth + 24
                        color: exportHover.hovered ? Qt.lighter(cHighlight, 1.12) : cHighlight
                        Row {
                            id: exportRow
                            anchors.centerIn: parent
                            spacing: 5
                            Text { text: "📤"; font.pixelSize: 12 }
                            Text { text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.export : "Export"; color: cText; font.pixelSize: 12 }
                        }
                        ToolTip.text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.exportChat : "Export Chat"
                        ToolTip.visible: exportHover.hovered
                        ToolTip.delay: 600
                        HoverHandler { id: exportHover }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: exportFileDialog.open()
                        }
                    }
                }

                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: cDivider }
            }

            // ── 输入区 ────────────────────────────────────────────────────────
            Rectangle {
                id: inputPanel
                anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                height: 130
                color: cSidebar

                Rectangle {
                    anchors { fill: parent; margins: 16; topMargin: 10 }
                    color: cInput
                    radius: 12
                    border.width: 1
                    border.color: inputArea.activeFocus ? cAccent : cBorder

                    ColumnLayout {
                        anchors { fill: parent; margins: 10 }
                        spacing: 0

                        ScrollView {
                            Layout.fillWidth: true; Layout.fillHeight: true
                            clip: true
                            TextArea {
                                id: inputArea
                                placeholderText: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0 && localeBridge.t.inputPlaceholder) ? localeBridge.t.inputPlaceholder : "Type message… (Enter to send, Shift+Enter for new line)"
                                placeholderTextColor: cMuted
                                color: cText; font.pixelSize: 14
                                wrapMode: TextArea.Wrap
                                background: null
                                enabled: typeof mainView !== "undefined" ? !mainView.isStreaming : true
                                // 明确支持多行与 IME，便于 Windows/Linux 下中文等输入法候选框正常显示
                                inputMethodHints: Qt.ImhMultiLine

                                Keys.onPressed: (event) => {
                                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                        if (event.modifiers & Qt.ShiftModifier) {
                                        } else {
                                            event.accepted = true
                                            _sendMsg()
                                        }
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            // 左侧：模式选择按钮组（图标 + 文字）
                            Row {
                                id: modeButtonRow
                                spacing: 4

                                readonly property var modes: ["chat", "agent", "planning"]
                                readonly property var labels: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0 && localeBridge.t.modeChat) ? [localeBridge.t.modeChat, localeBridge.t.modeAgent, localeBridge.t.modeResearch] : ["对话", "智能体", "研究"]
                                readonly property var iconNorm: ["qrc:/src/icons/icon_chat.svg", "qrc:/src/icons/icon_agent.svg", "qrc:/src/icons/icon_planning.svg"]
                                readonly property var iconActive: ["qrc:/src/icons/icon_chat_active.svg", "qrc:/src/icons/icon_agent_active.svg", "qrc:/src/icons/icon_planning_active.svg"]

                                function currentIndex() {
                                    var m = mainView.chatMode
                                    if (m === "chat") return 0
                                    if (m === "agent") return 1
                                    if (m === "planning") return 2
                                    return 0
                                }

                                Repeater {
                                    model: 3

                                    Rectangle {
                                        id: modeBtn
                                        height: 32
                                        width: modeBtnRow.implicitWidth + 16
                                        radius: 8
                                        property bool selected: modeButtonRow.currentIndex() === index
                                        property bool hovered: btnHover.hovered

                                        color: {
                                            if (selected) return cAccent
                                            if (hovered) return cHighlight
                                            return cInput
                                        }
                                        border.color: selected ? cAccent : (hovered ? cBorder : "transparent")
                                        border.width: selected ? 0 : 1

                                        Row {
                                            id: modeBtnRow
                                            anchors.centerIn: parent
                                            spacing: 6

                                            Image {
                                                width: 16
                                                height: 16
                                                anchors.verticalCenter: parent.verticalCenter
                                                source: modeBtn.selected ? modeButtonRow.iconActive[index] : modeButtonRow.iconNorm[index]
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                mipmap: true
                                            }
                                            Text {
                                                text: modeButtonRow.labels[index]
                                                color: modeBtn.selected ? "white" : cText
                                                font.pixelSize: 12
                                                font.weight: modeBtn.selected ? Font.DemiBold : Font.Normal
                                                anchors.verticalCenter: parent.verticalCenter
                                            }
                                        }

                                        HoverHandler { id: btnHover }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (mainView.chatMode !== modeButtonRow.modes[index])
                                                    mainView.chatMode = modeButtonRow.modes[index]
                                            }
                                        }
                                    }
                                }
                            }

                            // 对话模式下的联网切换按钮（一个按钮实现联网/不联网两种状态）
                            Rectangle {
                                id: onlineToggleBtn
                                visible: mainView.chatMode === "chat"
                                height: 32
                                width: onlineToggleRow.implicitWidth + 16
                                radius: 8
                                property bool hovered: onlineToggleHover.hovered
                                property bool isOnline: typeof settings !== "undefined" && settings.chatOnline
                                color: {
                                    if (isOnline) return cAccent
                                    if (hovered) return cHighlight
                                    return cInput
                                }
                                border.color: isOnline ? cAccent : (hovered ? cBorder : "transparent")
                                border.width: isOnline ? 0 : 1
                                Row {
                                    id: onlineToggleRow
                                    anchors.centerIn: parent
                                    spacing: 6
                                    Text { text: "🌐"; font.pixelSize: 14; anchors.verticalCenter: parent.verticalCenter }
                                    Text {
                                        text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0)
                                            ? (onlineToggleBtn.isOnline ? localeBridge.t.chatOnline : localeBridge.t.chatOffline)
                                            : (onlineToggleBtn.isOnline ? "联网" : "不联网")
                                        color: onlineToggleBtn.isOnline ? "white" : cText
                                        font.pixelSize: 12
                                        font.weight: onlineToggleBtn.isOnline ? Font.DemiBold : Font.Normal
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                                HoverHandler { id: onlineToggleHover }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (typeof settings !== "undefined")
                                            settings.chatOnline = !settings.chatOnline
                                    }
                                }
                                ToolTip.text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0)
                                    ? localeBridge.t.chatOnlineTip : "点击切换：联网时使用 RAG 检索回答"
                                ToolTip.visible: onlineToggleBtn.hovered
                                ToolTip.delay: 600
                            }

                            // 字数提示
                            Text {
                                text: inputArea.text.length > 0 && localeBridge && localeBridge.t && localeBridge.tVersion >= 0
                                    ? inputArea.text.length + " " + localeBridge.t.chars
                                    : (inputArea.text.length > 0 ? inputArea.text.length + " chars" : "")
                                color: cMuted; font.pixelSize: 11
                                Layout.fillWidth: true
                            }

                            // 常用模型选择器（ICON+文字，与对话/智能体/规划风格一致）
                            Rectangle {
                                id: modelSelectorBtn
                                width: 100
                                height: 32
                                radius: 8
                                property bool hovered: modelBtnHover.hovered
                                color: hovered ? cHighlight : cInput
                                border.color: hovered ? cBorder : "transparent"
                                border.width: 1

                                Row {
                                    anchors.centerIn: parent
                                    spacing: 6

                                    Image {
                                        width: 16
                                        height: 16
                                        anchors.verticalCenter: parent.verticalCenter
                                        source: "qrc:/src/icons/icon_model.svg"
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        mipmap: true
                                    }
                                    Text {
                                        text: (typeof settings !== "undefined" && settings.modelName)
                                            ? settings.modelName : ((localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.model : "Model")
                                        color: cText
                                        font.pixelSize: 12
                                        font.weight: Font.Normal
                                        anchors.verticalCenter: parent.verticalCenter
                                        elide: Text.ElideRight
                                        maximumLineCount: 1
                                        width: 62
                                    }
                                }

                                HoverHandler { id: modelBtnHover }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        var overlay = Overlay.overlay
                                        if (!overlay) return
                                        var pos = modelSelectorBtn.mapToItem(overlay, 0, 0)
                                        // 在按钮上方打开，避免被输入区遮挡
                                        modelSelectPopup.x = Math.min(pos.x, overlay.width - modelSelectPopup.width - 6)
                                        modelSelectPopup.y = Math.max(6, pos.y - modelSelectPopup.height - 4)
                                        modelSelectPopup.open()
                                    }
                                }
                                ToolTip.text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.modelSwitchTip : ""
                                ToolTip.visible: modelBtnHover.hovered
                                ToolTip.delay: 600
                            }

                            // 发送按钮
                            Rectangle {
                                width: 36; height: 32; radius: 6
                                color: (inputArea.text.trim().length > 0 && !mainView.isStreaming)
                                    ? cAccent : cHighlight

                                Text {
                                    anchors.centerIn: parent; text: "➤"; font.pixelSize: 17
                                    color: (inputArea.text.trim().length > 0 && !mainView.isStreaming)
                                        ? "white" : cMuted
                                }
                                MouseArea {
                                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                    onClicked: _sendMsg()
                                }
                            }
                        }
                    }
                }
            }

            // ── 消息列表：完全交给 WebEngine + HTML/JS ─────────────────────────
            WebChannel {
                id: chatChannel
            }

            WebEngineView {
                id: chatWebView
                anchors {
                    top:    chatHeader.bottom
                    bottom: inputPanel.top
                    left:   parent.left
                    right:  parent.right
                }
                backgroundColor: "transparent"
                settings.javascriptEnabled: true
                settings.localContentCanAccessFileUrls: true
                settings.localContentCanAccessRemoteUrls: true

                // 同步当前 QML 中的主题到 HTML 内部，通过 appWindow.isLight 判断，
                // 避免 WebEngineView.settings 遮蔽全局 Settings 对象。
                function syncChatTheme() {
                    if (chatWebView.loading)
                        return;
                    // NOTE: 不能直接用 settings.theme，因为 WebEngineView 自带的
                    // settings 属性（WebEngineSettings）会遮蔽 C++ 注入的全局 Settings。
                    // 改用 ApplicationWindow 根节点上的 isLight 属性。
                    var light = appWindow.isLight;
                    var js =
                        "document.body.classList.remove('theme-dark','theme-light');" +
                        "document.body.classList.add('" + (light ? "theme-light" : "theme-dark") + "');" +
                        "document.body.style.setProperty('--bubble-ai','" + (light ? "#F0F2F5" : "#2A2F3A") + "');" +
                        "document.body.style.setProperty('--bubble-user','" + (light ? "#E8ECF0" : "#323842") + "');" +
                        "document.body.style.setProperty('--border-subtle','" + (light ? "#DDE2E8" : "#3E4553") + "');" +
                        "document.body.style.setProperty('--text','" + (light ? "#1A202C" : "#F1F5F9") + "');" +
                        "document.body.style.setProperty('--text-muted','" + (light ? "#6B7280" : "#9CA3AF") + "');" +
                        "window.isLightTheme = " + (light ? "true" : "false") + ";" +
                        "lastMessagesKey = '';";
                    chatWebView.runJavaScript(js);
                }

                Component.onCompleted: {
                    // 将核心对象暴露给 JS，由单页 Web 聊天 UI 完成头像/气泡/滚动等全部渲染
                    chatChannel.registerObject("mainView", mainView)
                    chatChannel.registerObject("settings", settings)
                    chatChannel.registerObject("history", history)
                    chatChannel.registerObject("clipboard", clipboardBridge)
                    webChannel = chatChannel
                    url = "qrc:/src/qml/chat_render.html"
                }

                // 页面加载完一次后，同步一次主题，保证初次打开时左右两侧配色一致
                onLoadingChanged: function(loadRequest) {
                    if (!loadRequest.isLoading) {
                        syncChatTheme();
                    }
                }

                // 拦截所有导航请求：非 qrc:// 内部资源一律用系统浏览器打开，
                // 防止链接在 WebEngineView 内部跳转导致聊天界面消失。
                onNavigationRequested: function(request) {
                    var url = request.url.toString()
                    if (!url.startsWith("qrc:") && url !== "about:blank") {
                        Qt.openUrlExternally(request.url)
                        request.reject()
                    }
                }

                // 当用户在“系统设置 → 主题设置”中切换 Dark / Light 时，
                // 主 QML 已经立刻更新自身配色，这里直接把最新 theme 推送给 HTML。
                Connections {
                    target: appWindow
                    function onIsLightChanged() {
                        chatWebView.syncChatTheme();
                    }
                }
            }
        }
    }

    // 文件夹重命名对话框（用于历史树右键）
    Dialog {
        id: folderRenameDialog
        title: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.renameFolder : "Rename Folder"
        modal: true
        anchors.centerIn: parent
        width: 320

        property string targetId: ""

        background: Rectangle { color: cPopupBg; radius: 8; border.color: cBorder }

        contentItem: Column {
            spacing: 10; padding: 16
            Text { text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0 && localeBridge.t.enterNewName) ? localeBridge.t.enterNewName : "Enter new name:"; color: cText; font.pixelSize: 13 }
            TextField {
                id: folderRenameField
                width: 280; height: 36
                color: cText; font.pixelSize: 13
                background: Rectangle { radius: 5; color: cInput; border.color: cBorder }
                leftPadding: 10
                selectByMouse: true
                Keys.onReturnPressed: {
                    if (folderRenameDialog.targetId !== "") {
                        history.renameNode(folderRenameDialog.targetId, text)
                    }
                    folderRenameDialog.close()
                }
            }
        }

        footer: Row {
            spacing: 8; padding: 12; layoutDirection: Qt.RightToLeft
            Button {
                text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.ok : "OK"; width: 80; height: 32
                contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 13;
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 5; color: cAccent }
                onClicked: {
                    if (folderRenameDialog.targetId !== "") {
                        history.renameNode(folderRenameDialog.targetId, folderRenameField.text)
                    }
                    folderRenameDialog.close()
                }
            }
            Button {
                text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.cancel : "Cancel"; width: 80; height: 32
                contentItem: Text { text: parent.text; color: cText; font.pixelSize: 13;
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 5; color: cInput; border.color: cBorder }
                onClicked: folderRenameDialog.close()
            }
        }

        onAboutToShow: {
            folderRenameField.forceActiveFocus()
            folderRenameField.selectAll()
        }
    }

    // ── 历史项右键菜单（自定义漂亮样式，跟随鼠标）────────────────────────────────
    Popup {
        id: historyContextMenu
        parent: Overlay.overlay
        property var targetNode: null
        property var folderOptions: []
        // 过滤后的“添加到”选项：已在根目录时隐藏“(空)”
        property var filteredFolderOptions: {
            var opts = historyContextMenu.folderOptions
            var node = historyContextMenu.targetNode
            if (!node || !opts || opts.length === 0) return []
            var alreadyAtRoot = !node.parentId || node.parentId === ""
            if (!alreadyAtRoot) return opts
            var out = []
            for (var i = 0; i < opts.length; i++) {
                if (opts[i].id) out.push(opts[i])
            }
            return out
        }
        padding: 0
        margins: 4
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        property real _pendingX: 0
        property real _pendingY: 0

        function openAt(px, py) {
            if (targetNode)
                folderOptions = history.getFolderOptions(targetNode.id)
            _pendingX = px
            _pendingY = py
            x = px
            y = py
            open()
        }

        onOpened: {
            x = Math.min(Math.max(6, _pendingX), appWindow.width - width - 6)
            y = Math.min(Math.max(6, _pendingY), appWindow.height - height - 6)
        }

        background: Rectangle {
            color: cPopupBg
            radius: 10
            border.width: 1
            border.color: cBorder
        }

        contentItem: Column {
            width: 188
            spacing: 0
            padding: 6

            // 重命名
            ContextMenuItem {
                icon: "✏️"
                label: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.rename : "Rename"
                onTriggered: {
                    if (!historyContextMenu.targetNode) return
                    var n = historyContextMenu.targetNode
                    if (n.type === "session") {
                        if (n.id !== mainView.currentSession)
                            mainView.loadSession(n.id)
                        renameDialog.open()
                    } else if (n.type === "folder") {
                        folderRenameDialog.targetId = n.id
                        folderRenameField.text = n.name
                        folderRenameDialog.open()
                    }
                    historyContextMenu.close()
                }
            }

            // 删除
            ContextMenuItem {
                icon: "🗑"
                label: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.delete : "Delete"
                accent: true
                onTriggered: {
                    if (!historyContextMenu.targetNode) return
                    var n = historyContextMenu.targetNode
                    deleteNodeDialog.targetId = n.id
                    deleteNodeDialog.targetName = n.name
                    deleteNodeDialog.targetType = n.type
                    deleteNodeDialog.open()
                    historyContextMenu.close()
                }
            }

            Rectangle {
                width: parent.width - 12
                height: 1
                color: cBorder
                visible: historyContextMenu.filteredFolderOptions.length > 0
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Repeater {
                model: historyContextMenu.filteredFolderOptions
                delegate: ContextMenuItem {
                    icon: "📁"
                    label: (localeBridge && localeBridge.lang) ? localeBridge.tr("addToFolder", modelData.name) : ("Add to: " + modelData.name)
                    property string _folderId: modelData.id || ""
                    onTriggered: {
                        if (historyContextMenu.targetNode)
                            history.moveNode(historyContextMenu.targetNode.id, _folderId)
                        historyContextMenu.close()
                    }
                }
            }

            Rectangle {
                width: parent.width - 12
                height: 1
                color: cBorder
                visible: historyContextMenu.filteredFolderOptions.length > 0
                anchors.horizontalCenter: parent.horizontalCenter
            }

            // 新建文件夹
            ContextMenuItem {
                icon: "➕"
                label: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.newFolderInMenu : "New Folder"
                onTriggered: {
                    if (!historyContextMenu.targetNode) return
                    var n = historyContextMenu.targetNode
                    var pid = n.type === "folder" ? n.id : (n.parentId || "")
                    folderDialog.parentId = pid
                    folderDialog.open()
                    historyContextMenu.close()
                }
            }
        }
    }

    // 可复用的菜单项组件（用 signal 避免 binding loop）
    component ContextMenuItem: Rectangle {
        id: ctxItem
        width: 176
        height: 34
        radius: 6
        color: ctxHover.hovered ? (ctxItem.accent ? "#4A2226" : cHighlight) : "transparent"

        property string icon: "📄"
        property string label: ""
        property bool accent: false
        signal triggered()

        Row {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 10
            leftPadding: 4

            Text {
                text: ctxItem.icon
                font.pixelSize: 14
                color: ctxItem.accent ? "#ED4245" : cMuted
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: ctxItem.label
                font.pixelSize: 13
                color: ctxItem.accent ? "#ED4245" : cText
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        HoverHandler { id: ctxHover }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            acceptedButtons: Qt.LeftButton
            onClicked: function(mouse) {
                if (mouse.button === Qt.LeftButton)
                    ctxItem.triggered()
            }
        }
    }

    // ── 常用模型选择弹窗 ───────────────────────────────────────────────────────
    Popup {
        id: modelSelectPopup
        parent: Overlay.overlay
        width: 180
        padding: 6
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: cPopupBg
            radius: 8
            border.width: 1
            border.color: cBorder
        }

        contentItem: ListView {
            clip: true
            // 引用外层 Popup，方便在 delegate 中关闭弹窗
            property var popup: modelSelectPopup
            model: {
                if (typeof settings === "undefined" || !settings.modelList)
                    return []
                // 将模型列表视为循环列表：当前模型在最上面，当前模型之前的部分拼接到末尾
                var raw = settings.modelList
                var current = settings.modelName
                var idx = raw.indexOf(current)
                if (idx <= 0)
                    return raw
                var head = raw.slice(idx)      // 从当前模型开始到结尾
                var tail = raw.slice(0, idx)   // 当前模型之前的部分
                return head.concat(tail)
            }
            spacing: 2
            implicitHeight: Math.min(contentHeight, 240)

            delegate: Rectangle {
                width: ListView.view.width - 12
                height: 34
                radius: 6
                color: modelItemHover.hovered ? cHighlight : "transparent"

                Row {
                    anchors { fill: parent; leftMargin: 10 }
                    spacing: 8
                    Text {
                        text: "✓"
                        color: (typeof settings !== "undefined" && settings.modelName === modelData)
                            ? cAccent : "transparent"
                        font.pixelSize: 12
                        anchors.verticalCenter: parent.verticalCenter
                        width: 14
                    }
                    Text {
                        text: modelData
                        color: cText
                        font.pixelSize: 12
                        anchors.verticalCenter: parent.verticalCenter
                        elide: Text.ElideRight
                        width: 130
                    }
                }

                HoverHandler { id: modelItemHover }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof settings !== "undefined")
                            settings.modelName = modelData
                        if (ListView.view && ListView.view.popup)
                            ListView.view.popup.close()
                    }
                }
            }
        }
    }

    // ── 删除技能确认对话框 ───────────────────────────────────────────────────
    Dialog {
        id: deleteSkillDialog
        title: "删除技能"
        modal: true
        anchors.centerIn: parent
        width: 320

        property string targetId: ""
        property string targetTitle: ""

        background: Rectangle { color: cPopupBg; radius: 8; border.color: cBorder }

        contentItem: Column {
            spacing: 12; padding: 16
            Text {
                text: "确定要删除技能「" + deleteSkillDialog.targetTitle + "」吗？此操作无法撤销。"
                color: cText; font.pixelSize: 14; lineHeight: 1.5; wrapMode: Text.Wrap
                width: 280
            }
        }

        footer: Row {
            spacing: 8; padding: 12; layoutDirection: Qt.RightToLeft
            Button {
                text: "删除"; width: 80; height: 32
                contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 13;
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 5; color: "#ED4245" }
                onClicked: {
                    if (deleteSkillDialog.targetId !== "" && typeof skillManager !== "undefined" && skillManager)
                        skillManager.removeSkill(deleteSkillDialog.targetId)
                    deleteSkillDialog.close()
                }
            }
            Button {
                text: "取消"; width: 80; height: 32
                contentItem: Text { text: parent.text; color: cText; font.pixelSize: 13;
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 5; color: cInput; border.color: cBorder }
                onClicked: deleteSkillDialog.close()
            }
        }
    }

    // ── 导入技能文件对话框（选择 .md 文件）──────────────────────────────────────
    FileDialog {
        id: importSkillFileDialog
        title: "选择技能文件 (.md)"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Markdown 文件 (*.md)", "所有文件 (*)"]
        onAccepted: {
            var url = selectedFile.toString()
            var xhr = new XMLHttpRequest()
            xhr.open("GET", url, false)
            xhr.send()
            var content = xhr.responseText
            if (!content || !content.trim()) return
            // 从文件名提取标题（去掉扩展名和路径）
            var parts = url.split("/")
            var filename = parts[parts.length - 1]
            var title = decodeURIComponent(filename.replace(/\.md$/i, ""))
            if (title && typeof skillManager !== "undefined" && skillManager)
                skillManager.addSkill(title, content.trim(), "📄", "custom")
        }
    }

    // ── 技能卡片右键菜单 ──────────────────────────────────────────────────────
    Popup {
        id: skillContextMenu
        parent: Overlay.overlay
        padding: 0
        margins: 4
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        property string targetId: ""
        property string targetTitle: ""
        property string targetIcon: "🔧"
        property string targetContent: ""

        property real _px: 0
        property real _py: 0

        function openAt(px, py) {
            _px = px; _py = py
            x = px; y = py
            open()
        }

        onOpened: {
            x = Math.min(Math.max(6, _px), appWindow.width  - width  - 6)
            y = Math.min(Math.max(6, _py), appWindow.height - height - 6)
        }

        background: Rectangle {
            color: cPopupBg; radius: 10
            border.width: 1; border.color: cBorder
        }

        contentItem: Column {
            width: 160; spacing: 0; padding: 6

            ContextMenuItem {
                icon: "✏️"
                label: "编辑"
                onTriggered: {
                    editSkillDialog.targetId = skillContextMenu.targetId
                    editSkillIconField.text  = skillContextMenu.targetIcon
                    editSkillTitleField.text = skillContextMenu.targetTitle
                    editSkillContentArea.text = skillContextMenu.targetContent
                    editSkillDialog.open()
                    skillContextMenu.close()
                }
            }

            ContextMenuItem {
                icon: "🗑"
                label: "删除"
                accent: true
                onTriggered: {
                    deleteSkillDialog.targetId    = skillContextMenu.targetId
                    deleteSkillDialog.targetTitle = skillContextMenu.targetTitle
                    deleteSkillDialog.open()
                    skillContextMenu.close()
                }
            }
        }
    }

    // ── 编辑技能对话框 ────────────────────────────────────────────────────────
    Dialog {
        id: editSkillDialog
        modal: true
        anchors.centerIn: parent
        width: 480
        height: 400

        // 不要在 contentItem 里 anchors.fill（会覆盖 header 区域导致重叠）
        padding: 20
        topPadding: 12

        property string targetId: ""
        property bool isNewMode: false

        background: Rectangle { color: cPopupBg; radius: 12; border.color: cBorder; border.width: 1 }

        header: Rectangle {
            width: parent ? parent.width : 0
            height: 52
            color: "transparent"
            radius: 12

            RowLayout {
                anchors { fill: parent; leftMargin: 20; rightMargin: 16 }
                spacing: 10

                Text { text: "✏️"; font.pixelSize: 18 }
                Text {
                    text: "编辑技能"
                    color: cText; font.pixelSize: 15; font.bold: true
                    Layout.fillWidth: true
                }
                Text {
                    text: "✕"
                    color: editSkillCloseHover.hovered ? "#ED4245" : cMuted
                    font.pixelSize: 16
                    HoverHandler { id: editSkillCloseHover }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: editSkillDialog.close()
                    }
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width; height: 1; color: cBorder
            }
        }

        contentItem: ColumnLayout {
            spacing: 14
            Layout.fillWidth: true

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                ColumnLayout {
                    spacing: 4
                    Text { text: "图标"; color: cMuted; font.pixelSize: 12 }
                    Rectangle {
                        width: 56; height: 36; radius: 6
                        color: cInput; border.color: cBorder; border.width: 1
                        TextField {
                            id: editSkillIconField
                            anchors.fill: parent
                            color: cText; font.pixelSize: 16
                            horizontalAlignment: TextInput.AlignHCenter
                            background: null; selectByMouse: true
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text { text: "技能名称"; color: cMuted; font.pixelSize: 12 }
                    Rectangle {
                        Layout.fillWidth: true; height: 36; radius: 6
                        color: cInput
                        border.color: editSkillTitleField.activeFocus ? cAccent : cBorder
                        border.width: 1
                        TextField {
                            id: editSkillTitleField
                            anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                            placeholderText: "技能名称…"
                            placeholderTextColor: cMuted
                            color: cText; font.pixelSize: 13
                            background: null; selectByMouse: true
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 4

                Text { text: "技能内容"; color: cMuted; font.pixelSize: 12 }
                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    radius: 6; color: cInput
                    border.color: editSkillContentArea.activeFocus ? cAccent : cBorder
                    border.width: 1
                    ScrollView {
                        anchors { fill: parent; margins: 8 }
                        clip: true
                        TextArea {
                            id: editSkillContentArea
                            placeholderText: "技能描述 / SOP 内容…"
                            placeholderTextColor: cMuted
                            color: cText; font.pixelSize: 13
                            wrapMode: TextArea.Wrap
                            background: null; selectByMouse: true
                        }
                    }
                }
            }
        }

        footer: Row {
            spacing: 8; padding: 14; layoutDirection: Qt.RightToLeft
            Button {
                text: "保存"; width: 90; height: 34
                contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 13;
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle {
                    radius: 6
                    color: (editSkillTitleField.text.trim().length > 0 && editSkillContentArea.text.trim().length > 0)
                        ? cAccent : Qt.darker(cAccent, 1.5)
                }
                onClicked: {
                    var title   = editSkillTitleField.text.trim()
                    var content = editSkillContentArea.text.trim()
                    if (title.length > 0 && content.length > 0
                            && typeof skillManager !== "undefined" && skillManager) {
                        if (editSkillDialog.isNewMode) {
                            skillManager.addSkill(title, content, editSkillIconField.text || "🔧", "custom")
                        } else if (editSkillDialog.targetId !== "") {
                            skillManager.updateSkill(editSkillDialog.targetId, title, content)
                        }
                        editSkillDialog.close()
                    }
                }
            }
            Button {
                text: "取消"; width: 90; height: 34
                contentItem: Text { text: parent.text; color: cText; font.pixelSize: 13;
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 6; color: cInput; border.color: cBorder }
                onClicked: editSkillDialog.close()
            }
        }

        onAboutToShow: { editSkillTitleField.forceActiveFocus() }
    }

    // ── 技能详情弹窗 ────────────────────────────────────────────────────────
    Dialog {
        id: skillDetailPopup
        modal: true
        anchors.centerIn: parent
        width: Math.min(appWindow.width * 0.6, 520)
        height: Math.min(appWindow.height * 0.65, 480)

        property string skillTitle: ""
        property string skillIcon: "🔧"
        property string skillContent: ""

        background: Rectangle { color: cPopupBg; radius: 12; border.color: cBorder; border.width: 1 }

        header: Rectangle {
            width: parent ? parent.width : 0
            height: 56
            color: "transparent"
            radius: 12

            RowLayout {
                anchors { fill: parent; leftMargin: 20; rightMargin: 16 }
                spacing: 12

                Rectangle {
                    width: 36; height: 36; radius: 8
                    color: isLight ? "#EEF2FF" : "#312E81"
                    Text {
                        anchors.centerIn: parent
                        text: skillDetailPopup.skillIcon
                        font.pixelSize: 18
                    }
                }

                Text {
                    text: skillDetailPopup.skillTitle
                    color: cText
                    font.pixelSize: 16
                    font.bold: true
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Text {
                    text: "✕"
                    color: skillCloseHover.hovered ? "#ED4245" : cMuted
                    font.pixelSize: 16
                    HoverHandler { id: skillCloseHover }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: skillDetailPopup.close()
                    }
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: cBorder
            }
        }

        contentItem: Flickable {
            clip: true
            contentWidth: width
            contentHeight: skillContentText.implicitHeight + 32
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Text {
                id: skillContentText
                width: parent.width - 32
                x: 16
                y: 16
                text: skillDetailPopup.skillContent
                color: cText
                font.pixelSize: 13
                lineHeight: 1.6
                wrapMode: Text.Wrap
            }
        }

        footer: Item { height: 8 }
    }

    // ── 发送函数 ──────────────────────────────────────────────────────────────
    function _sendMsg() {
        let t = inputArea.text.trim()
        if (t !== "" && !mainView.isStreaming) {
            mainView.sendMessage(t)
            inputArea.text = ""
        }
    }
}
