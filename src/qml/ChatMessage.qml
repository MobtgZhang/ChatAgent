// qml/ChatMessage.qml — ChatGPT 风格
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    width: ListView.view ? ListView.view.width : 700
    // 高度根据内容列自适应，保证 Markdown / 数学公式完全显示
    height: bubbleColumn.implicitHeight + 28
    property string role:           "user"
    property string msgContent:      ""
    property string thinkingContent:""
    property bool   isThinking:     false
    property int    messageIndex:   -1

    // 版本导航（预留扩展：当前为 1/1，后续可接入多版本）
    property int    currentVersion: 1
    property int    totalVersions:  1

    property bool isAI: role === "ai" || role === "assistant"

    function hasMath(s) {
        if (!s || typeof s !== "string") return false
        return /\$\$|\$[^\s$]|\\[\[\()\]]/.test(s)
    }
    function hasCodeBlocks(s) {
        if (!s || typeof s !== "string") return false
        return /```[\s\S]*?```/.test(s)
    }

    Component {
        id: mdRenderComp
        MarkdownRender { }
    }
    Component {
        id: mathRenderComp
        MarkdownMathRender { }
    }

    property bool editing:        false
    property bool thinkExpanded:  false

    onIsThinkingChanged: {
        if (isThinking && settings.showThinking && isAI) {
            thinkExpanded = true
        }
    }
    // 用户从关→开切换思考按钮时，若有已记录的思考内容则自动展开显示
    Connections {
        target: settings
        function onShowThinkingChanged() {
            if (settings.showThinking && isAI && thinkingContent !== "")
                thinkExpanded = true
        }
    }
    // 历史加载后：与思考按钮切换时保持一致的展开状态
    Component.onCompleted: {
        if (settings.showThinking && isAI && thinkingContent !== "")
            thinkExpanded = true
    }

    property real thinkTime: 0.0
    Timer {
        id: thinkTimer
        interval: 100; running: root.isThinking; repeat: true
        onTriggered: root.thinkTime += 0.1
    }

    // ChatGPT 风格：用户右对齐，AI 左对齐
    Row {
        id: outerRow
        anchors.top: parent.top
        anchors.topMargin: 12
        width: parent.width - 40
        spacing: 12
        layoutDirection: isAI ? Qt.LeftToRight : Qt.RightToLeft

        // 头像
        Rectangle {
            width: 28; height: 28; radius: 6
            color: isAI ? "#F9A825" : "#5865F2"
            Text {
                anchors.centerIn: parent
                text: isAI ? "🤖" : "Me"
                color: "white"
                font.bold: true
                font.pixelSize: 11
            }
        }

        // 气泡 + 内容
        Column {
            id: bubbleColumn
            width: Math.min(outerRow.width - 28 - 12, 640)
            spacing: 0

            // 思考秒数（仅 AI，无论思考按钮开/关都显示）
            Item {
                width: parent.width
                height: 20
                visible: isAI && (isThinking || thinkTime > 0)
                Row {
                    spacing: 6
                    anchors.verticalCenter: parent.verticalCenter
                    Text { text: "🧠"; font.pixelSize: 12 }
                    Text {
                        text: isThinking
                            ? "思考中... " + thinkTime.toFixed(1) + "s"
                            : "思考完成 (" + thinkTime.toFixed(1) + "s)" +
                            (settings.showThinking && thinkingContent !== "" ? (thinkExpanded ? "  ▲" : "  ▼") : "")
                        color: "#949BA4"
                        font.pixelSize: 11
                        font.italic: true
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: if (settings.showThinking && thinkingContent !== "") root.thinkExpanded = !root.thinkExpanded
                }
            }

            // 思考过程展开内容（仅当思考按钮打开时显示）
            Column {
                width: parent.width
                spacing: 4
                visible: settings.showThinking && isAI

                Rectangle {
                    id: thinkBox
                    width: parent.width
                    visible: thinkExpanded && thinkingContent !== ""
                    height: visible && thinkLoader.item ? (thinkLoader.item.implicitHeight || thinkLoader.item.height) + 10 : 0
                    color: "#1A1B1E"
                    radius: 8
                    border.color: "#2E3035"

                    Loader {
                        id: thinkLoader
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 8 }
                        sourceComponent: (root.hasMath(root.thinkingContent) || root.hasCodeBlocks(root.thinkingContent)) ? mathRenderComp : mdRenderComp
                        onLoaded: {
                            item.markdownText = root.thinkingContent
                            item.textColor = "#C9CDD4"
                            item.fontSize = 12
                        }
                    }
                }
            }

            // 思考占位
            Text {
                width: parent.width
                visible: settings.showThinking && isAI && root.msgContent === "" && root.isThinking
                text: "▍"
                color: "#949BA4"
                font.pixelSize: 14
            }

            // 气泡主体（ChatGPT 风格：AI 浅灰，用户蓝色）
            Rectangle {
                id: bubble
                width: parent.width
                implicitHeight: contentCol.implicitHeight + 16
                color: isAI ? "#404249" : "#5865F2"
                radius: 12

                Column {
                    id: contentCol
                    width: parent.width - 24
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    topPadding: 8
                    bottomPadding: 8
                    spacing: 8

                    // 正文
                    Loader {
                        id: contentLoader
                        width: contentCol.width
                        visible: root.msgContent !== "" && !root.editing
                        sourceComponent: (root.hasMath(root.msgContent) || root.hasCodeBlocks(root.msgContent)) ? mathRenderComp : mdRenderComp
                        onLoaded: {
                            item.markdownText = root.msgContent
                            item.textColor = isAI ? "#DBDEE1" : "white"
                        }
                    }

                    // 编辑模式
                    Column {
                        width: parent.width
                        spacing: 8
                        visible: root.editing

                        TextArea {
                            id: editArea
                            width: parent.width
                            height: Math.max(60, implicitHeight)
                            text: root.msgContent
                            color: "#DBDEE1"
                            font.pixelSize: 14
                            wrapMode: TextArea.Wrap
                            background: Rectangle {
                                color: "#2E3035"
                                radius: 6
                                border.color: "#5865F2"
                                border.width: 1
                            }
                            padding: 10
                        }
                    }
                }
            }

            // 气泡外边右下角：复制、修改、修改记录
            Row {
                width: parent.width
                layoutDirection: Qt.RightToLeft
                spacing: 4
                topPadding: 4

                // 非编辑模式：复制、修改、修改记录
                // 编辑模式：保存、取消

                // 修改记录（版本导航）
                Row {
                    spacing: 2
                    height: 24
                    // 仅对用户气泡显示修改记录（AI 回复不需要）
                    visible: !root.editing && !root.isAI

                    Rectangle {
                        width: 22
                        height: 22
                        radius: 4
                        color: prevHover.hovered ? "#5C5F66" : "transparent"
                        opacity: currentVersion > 1 ? 1 : 0.4
                        Text {
                            anchors.centerIn: parent
                            text: "◀"
                            color: "#B5BAC1"
                            font.pixelSize: 10
                        }
                        HoverHandler { id: prevHover }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (currentVersion > 1)
                                    root.currentVersion = currentVersion - 1
                            }
                        }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: currentVersion + "/" + totalVersions
                        color: "#B5BAC1"
                        font.pixelSize: 11
                    }

                    Rectangle {
                        width: 22
                        height: 22
                        radius: 4
                        color: nextHover.hovered ? "#5C5F66" : "transparent"
                        opacity: currentVersion < totalVersions ? 1 : 0.4
                        Text {
                            anchors.centerIn: parent
                            text: "▶"
                            color: "#B5BAC1"
                            font.pixelSize: 10
                        }
                        HoverHandler { id: nextHover }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (currentVersion < totalVersions)
                                    root.currentVersion = currentVersion + 1
                            }
                        }
                    }
                }

                // 编辑模式：保存、取消
                Row {
                    spacing: 4
                    height: 24
                    visible: root.editing

                    Rectangle {
                        width: 44
                        height: 24
                        radius: 4
                        color: cancelHover.hovered ? "#5C5F66" : "#3F4147"
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("取消")
                            color: "#B5BAC1"
                            font.pixelSize: 12
                        }
                        HoverHandler { id: cancelHover }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.editing = false
                        }
                    }

                    Rectangle {
                        width: 44
                        height: 24
                        radius: 4
                        color: saveHover.hovered ? "#6B7AF2" : "#5865F2"
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("保存")
                            color: "white"
                            font.pixelSize: 12
                        }
                        HoverHandler { id: saveHover }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof mainView !== "undefined") {
                                    mainView.editAndRegenerate(root.messageIndex, editArea.text)
                                    root.editing = false
                                }
                            }
                        }
                    }
                }

                // 修改（铅笔图标）
                Rectangle {
                    width: 24
                    height: 24
                    radius: 4
                    color: editHover.hovered ? "#5C5F66" : "transparent"
                    // 仅用户消息支持“修改”
                    visible: !root.editing && !root.isAI
                    Image {
                        anchors.centerIn: parent
                        source: "qrc:/src/qml/pencil.svg"
                        sourceSize: Qt.size(14, 14)
                        opacity: 0.85
                    }
                    HoverHandler { id: editHover }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.editing = true
                            editArea.text = root.msgContent
                            editArea.forceActiveFocus()
                        }
                    }
                }

                // 复制（与代码块复制按钮一致）
                Rectangle {
                    width: 24
                    height: 24
                    radius: 4
                    color: copyHover.hovered ? "#5C5F66" : "transparent"
                    visible: !root.editing
                    Image {
                        anchors.centerIn: parent
                        source: "qrc:/src/qml/copy.svg"
                        sourceSize: Qt.size(14, 14)
                        opacity: 0.85
                    }
                    HoverHandler { id: copyHover }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof mainView === "undefined")
                                return

                            // 用户消息：永远只复制正文
                            if (!root.isAI) {
                                if (root.msgContent)
                                    mainView.copyToClipboard(root.msgContent)
                                return
                            }

                            // AI 消息：
                            // - 思考过程开：复制「思考过程 + 两个换行 + 正文」
                            // - 思考过程关：只复制正文
                            var text = ""
                            if (settings.showThinking && root.thinkingContent) {
                                text = root.thinkingContent
                                if (root.msgContent)
                                    text += "\n\n" + root.msgContent
                            } else {
                                text = root.msgContent
                            }
                            if (text)
                                mainView.copyToClipboard(text)
                        }
                    }
                }
            }
        }
    }

    // 更多菜单：删除（用户）、重新生成（AI）
    Menu {
        id: moreMenu
        width: 140
        background: Rectangle {
            color: "#2B2D31"
            border.color: "#3F4147"
            radius: 6
        }

        MenuItem {
            text: "删除消息"
            visible: !root.isAI
            onTriggered: mainView.deleteMessage(root.messageIndex)
        }
        MenuItem {
            text: "重新生成"
            visible: root.isAI
            onTriggered: mainView.resendFrom(root.messageIndex)
        }
    }

    onMsgContentChanged: {
        if (contentLoader.item) {
            contentLoader.item.markdownText = root.msgContent
            contentLoader.item.textColor = isAI ? "#DBDEE1" : "white"
        }
    }
    onThinkingContentChanged: {
        if (!isThinking && settings.showThinking && isAI && thinkingContent !== "")
            thinkExpanded = true
        if (thinkLoader.item)
            thinkLoader.item.markdownText = root.thinkingContent
    }
}
