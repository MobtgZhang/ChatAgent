// qml/Main.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs

ApplicationWindow {
    id: appWindow
    width: 1060; height: 720
    minimumWidth: 800; minimumHeight: 560
    visible: true
    title: "ChatAgent — " + mainView.sessionName
    color: (typeof settings !== "undefined" && settings.theme === "light") ? "#FAFAFA" : "#0D0D0F"
    // ── 动态色板：Dark 黑底白字 / Light 白底黑字，高对比度 ──────────────────────
    readonly property bool isLight:     (typeof settings !== "undefined" && settings.theme === "light")
    readonly property color cBg:        isLight ? "#FAFAFA" : "#0D0D0F"
    readonly property color cSidebar:   isLight ? "#F0F0F2" : "#111113"
    readonly property color cInput:     isLight ? "#FFFFFF" : "#1C1C1F"
    readonly property color cBorder:    isLight ? "#D8D8DC" : "#2D2D32"
    readonly property color cHighlight: isLight ? "#E8E8EC" : "#252528"
    readonly property color cText:      isLight ? "#0D0D0F" : "#F2F2F4"
    readonly property color cMuted:     isLight ? "#3D3F47" : "#8E9099"
    readonly property color cAccent:    "#5865F2"
    readonly property color cDivider:   isLight ? "#E0E0E4" : "#2E3035"
    readonly property color cSelectedBg: isLight ? "#E0E4F8" : "#3D4270"
    readonly property color cScrollBar: isLight ? "#C0C2C8" : "#484B52"
    readonly property color cScrollBarHover: isLight ? "#A0A2A8" : "#60636A"
    // 弹窗/菜单背景：Light 模式下用纯白以提升对比度
    readonly property color cPopupBg: isLight ? "#FFFFFF" : "#111113"

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
                    if (deleteNodeDialog.targetId !== "")
                        history.deleteNode(deleteNodeDialog.targetId)
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

                // ── 历史树列表（可上下滚动）────────────────────────────────────
                ListView {
                    id: historyList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 80
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

                Rectangle { Layout.fillWidth: true; height: 1; color: cDivider }

                // ── Agent Memory（自动总结的记忆，可删除）───────────────────────────
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    spacing: 6

                    Text {
                        text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? ("🧠 " + localeBridge.t.agentMemory) : "Agent Memory"
                        color: cText
                        font.pixelSize: 13
                        font.bold: true
                        Layout.leftMargin: 10
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 120
                        clip: true
                        ScrollBar.vertical.policy: ScrollBar.AsNeeded

                        ListView {
                            model: typeof agentMemory !== "undefined" && agentMemory ? agentMemory.longTermFacts : []
                            spacing: 2
                            delegate: Rectangle {
                                width: ListView.view.width - 4
                                height: 32
                                radius: 4
                                color: memHover.hovered ? cHighlight : "transparent"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    spacing: 6
                                    Text {
                                        text: modelData.key + ":"
                                        color: cAccent
                                        font.pixelSize: 11
                                        font.bold: true
                                        Layout.preferredWidth: 60
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        text: modelData.value
                                        color: cText
                                        font.pixelSize: 11
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        text: "×"
                                        color: memDelHover.hovered ? "#ED4245" : cMuted
                                        font.pixelSize: 12
                                        HoverHandler { id: memDelHover }
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (agentMemory)
                                                    agentMemory.removeFact(modelData.key)
                                            }
                                        }
                                    }
                                }
                                HoverHandler { id: memHover }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.leftMargin: 10
                        Layout.rightMargin: 10
                        height: 28
                        visible: agentMemory && agentMemory.longTermFacts.length > 0
                        color: memClearHover.hovered ? "#5C2E2E" : "transparent"
                        radius: 4
                        border.color: "#ED4245"
                        border.width: 1
                        Row {
                            anchors.centerIn: parent
                            spacing: 4
                            Text { text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.clearAllMemory : ""; color: "#ED4245"; font.pixelSize: 11 }
                        }
                        HoverHandler { id: memClearHover }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: memClearDialog.open()
                        }
                    }
                }

                // 清空记忆确认对话框
                Dialog {
                    id: memClearDialog
                    title: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.clearAgentMemory : "Clear Agent Memory"
                    modal: true
                    anchors.centerIn: parent
                    width: 300
                    background: Rectangle { color: cPopupBg; radius: 8; border.color: cBorder }
                    contentItem: Text {
                        text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.clearMemoryConfirm : ""
                        color: cText
                        font.pixelSize: 13
                    }
                    footer: Row {
                        spacing: 8
                        layoutDirection: Qt.RightToLeft
                        Button {
                            text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.clear : "Clear"
                            width: 80
                            height: 32
                            contentItem: Text {
                                text: parent.text
                                color: "white"
                                font.pixelSize: 13
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle { radius: 5; color: "#ED4245" }
                            onClicked: {
                                if (agentMemory)
                                    agentMemory.clearLongTerm()
                                memClearDialog.close()
                            }
                        }
                        Button {
                            text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.cancel : "Cancel"
                            width: 80
                            height: 32
                            contentItem: Text {
                                text: parent.text
                                color: cText
                                font.pixelSize: 13
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle { radius: 5; color: cInput; border.color: cBorder }
                            onClicked: memClearDialog.close()
                        }
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: cDivider }

                // ── 底部按钮 ──────────────────────────────────────────────────
                Column {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 8
                    spacing: 2
                    padding: 8

                    Repeater {
                        model: [
                            { icon: "⚙️", key: "settings", action: "settings" },
                            { icon: "ℹ️", key: "about", action: "about" }
                        ]
                        delegate: Rectangle {
                            width: parent.width - 16; height: 36; radius: 5
                            color: sideHover.hovered ? cHighlight : "transparent"
                            Row {
                                anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 10 }
                                spacing: 10
                                Text { text: modelData.icon;  color: cMuted; font.pixelSize: 15 }
                                Text { text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0 && localeBridge.t[modelData.key]) ? localeBridge.t[modelData.key] : modelData.key; color: cText;  font.pixelSize: 12 }
                            }
                            HoverHandler { id: sideHover }
                            MouseArea {
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (modelData.action === "settings") openSettings()
                                    else openAbout()
                                }
                            }
                        }
                    }
                }
            }
        }

        // ═══════════════════════════ 中间：聊天区 ════════════════════════════
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

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

                    // 显示/隐藏思考过程
                    Rectangle {
                        height: 30; radius: 5
                        width: thinkRow.implicitWidth + 24
                        Layout.minimumWidth: thinkRow.implicitWidth + 24
                        color: thinkToggleHover.hovered
                            ? Qt.lighter(cHighlight, 1.12)
                            : (settings.showThinking ? cSelectedBg : cHighlight)
                        border.color: settings.showThinking ? cAccent : "transparent"
                        Row {
                            id: thinkRow
                            anchors.centerIn: parent
                            spacing: 6
                            Text { text: "🧠"; font.pixelSize: 12 }
                            Text {
                                text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? (settings.showThinking ? localeBridge.t.thinkingOn : localeBridge.t.thinkingOff) : (settings.showThinking ? "Thinking: On" : "Thinking: Off")
                                color: cText
                                font.pixelSize: 12
                            }
                        }
                        ToolTip.text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? (settings.showThinking ? localeBridge.t.thinkingTooltipOn : localeBridge.t.thinkingTooltipOff) : (settings.showThinking ? "Click to hide thinking" : "Click to show thinking")
                        ToolTip.visible: thinkToggleHover.hovered
                        ToolTip.delay: 600
                        HoverHandler { id: thinkToggleHover }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: settings.showThinking = !settings.showThinking
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

                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: cBorder }
            }

            // ── 输入区 ────────────────────────────────────────────────────────
            Rectangle {
                id: inputPanel
                anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                height: 130
                color: cSidebar

                Rectangle {
                    anchors { fill: parent; margins: 16; topMargin: 10 }
                    color: cInput; radius: 8
                    border.color: inputArea.activeFocus ? "#5865F2" : cBorder

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
                                readonly property var labels: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0 && localeBridge.t.modeChat) ? [localeBridge.t.modeChat, localeBridge.t.modeAgent, localeBridge.t.modePlanning] : ["Chat", "Agent", "Planning"]
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

            // ── 消息列表 ──────────────────────────────────────────────────────
            ListView {
                id: chatListView
                anchors {
                    top:    chatHeader.bottom
                    bottom: inputPanel.top
                    left:   parent.left
                    right:  parent.right
                }
                clip: true
                spacing: 8
                topMargin: 16; bottomMargin: 8
                cacheBuffer: 3000
                reuseItems: true

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

                model: mainView.messagesModel

                delegate: ChatMessage {
                    width: chatListView.width
                    role: (model && model.role) ? model.role : "user"
                    msgContent: (model && model.content) ? model.content : ""
                    thinkingContent: (model && model.thinking) ? model.thinking : ""
                    isThinking: (model && model.isThinking) ? true : false
                    messageIndex:     index
                }

                onCountChanged: Qt.callLater(function() {
                    if (typeof mainView !== "undefined" && (mainView.isStreaming || atYEnd))
                        positionViewAtEnd()
                })
                onContentHeightChanged: Qt.callLater(function() {
                    if (typeof mainView !== "undefined" && (mainView.isStreaming || atYEnd))
                        positionViewAtEnd()
                })
            }

            // 覆盖层：鼠标置于对话区域时，滚轮可上下滚动对话列表（acceptedButtons: NoButton 不拦截点击）
            MouseArea {
                anchors {
                    top: chatHeader.bottom
                    bottom: inputPanel.top
                    left: parent.left
                    right: parent.right
                }
                z: 10
                acceptedButtons: Qt.NoButton
                onWheel: function(wheel) {
                    // 这里是调节鼠标滑动速度，现在是2倍速度
                    var dy = wheel.angleDelta.y * 2
                    if (chatListView.verticalOvershoot !== 0.0 ||
                        (dy > 0 && chatListView.verticalVelocity <= 0) ||
                        (dy < 0 && chatListView.verticalVelocity >= 0)) {
                        chatListView.flick(0, dy - chatListView.verticalVelocity)
                    } else {
                        chatListView.cancelFlick()
                    }
                    wheel.accepted = true
                }
            }

            Timer {
                interval: 200
                repeat: true
                running: typeof mainView !== "undefined" && mainView.isStreaming
                onTriggered: {
                    if (mainView.isStreaming && chatListView.atYEnd)
                        chatListView.positionViewAtEnd()
                }
            }

            // ── 右下角：对话进度 ────────────────────────────────────────────────
            Text {
                anchors {
                    right: parent.right
                    bottom: inputPanel.top
                    rightMargin: 14
                    bottomMargin: 4
                }
                text: chatListView.count > 0
                      ? (chatListView.count + " / " + chatListView.count)
                      : "0 / 0"
                color: cMuted
                font.pixelSize: 11
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
            model: typeof settings !== "undefined" ? settings.modelList : []
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
                        modelSelectPopup.close()
                    }
                }
            }
        }
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
