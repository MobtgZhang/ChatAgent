// qml/Settings.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

Window {
    id: settingsWin
    title: "设置"
    width: 640; height: 520
    minimumWidth: 560; minimumHeight: 460
    color: "#2B2D31"
    flags: Qt.Dialog
    modality: Qt.ApplicationModal

    // ── 色板 ─────────────────────────────────────────────────────────────────
    readonly property color cBg:     "#2B2D31"
    readonly property color cPanel:  "#1E1F22"
    readonly property color cInput:  "#383A40"
    readonly property color cBorder: "#404249"
    readonly property color cText:   "#DBDEE1"
    readonly property color cMuted:  "#949BA4"
    readonly property color cAccent: "#5865F2"

    // ── 标签选项卡 ────────────────────────────────────────────────────────────
    property int currentTab: 0
    readonly property var tabs: ["🔑 API 配置", "⚙️ 参数调节", "📝 系统提示词"]

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 标题栏
        Rectangle {
            Layout.fillWidth: true; height: 48
            color: cPanel
            Text {
                anchors.centerIn: parent
                text: "设置"; color: cText; font.pixelSize: 16; font.bold: true
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: cBorder }
        }

        // Tab 行
        Rectangle {
            Layout.fillWidth: true; height: 44
            color: cPanel

            Row {
                anchors.fill: parent; anchors.leftMargin: 12; spacing: 4

                Repeater {
                    model: settingsWin.tabs
                    delegate: Rectangle {
                        width: tabLabel.implicitWidth + 24; height: 44
                        color: "transparent"

                        Text {
                            id: tabLabel
                            anchors.centerIn: parent
                            text: modelData
                            color: index === settingsWin.currentTab ? cText : cMuted
                            font.pixelSize: 13; font.bold: index === settingsWin.currentTab
                        }

                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width; height: 2
                            color: index === settingsWin.currentTab ? cAccent : "transparent"
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: settingsWin.currentTab = index
                        }
                    }
                }
            }

            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: cBorder }
        }

        // ── 内容区 ────────────────────────────────────────────────────────────
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // ── Tab 0: API 配置 ───────────────────────────────────────────────
            ScrollView {
                id: paramsScrollView
                anchors.fill: parent
                clip: true
                // 关键：固定内容宽度，避免水平滚动条出现及布局溢出
                contentWidth: availableWidth
                visible: settingsWin.currentTab === 0
                enabled: visible

                ColumnLayout {
                    // 不使用 anchors，直接通过 width + x/y 定位
                    width: parent.width - 40
                    x: 20
                    y: 20
                    spacing: 18

                    // API Key
                    SectionBox {
                        label: "API Key"
                        hint:  "用于鉴权的 Bearer token"
                        FieldInput {
                            id: apiKeyField
                            Layout.fillWidth: true
                            text: settings.apiKey
                            echoMode: TextInput.Password
                            onEditingFinished: settings.apiKey = text
                        }
                        Button {
                            text: apiKeyField.echoMode === TextInput.Password ? "👁 显示" : "🔒 隐藏"
                            height: 30
                            contentItem: Text {
                                text: parent.text; color: cMuted; font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle { radius: 5; color: cInput; border.color: cBorder }
                            onClicked: apiKeyField.echoMode =
                                (apiKeyField.echoMode === TextInput.Password)
                                ? TextInput.Normal : TextInput.Password
                        }
                    }

                    // API URL
                    SectionBox {
                        label: "API 地址"
                        hint:  "兼容 OpenAI 格式的接口地址"
                        FieldInput {
                            Layout.fillWidth: true
                            text: settings.apiUrl
                            onEditingFinished: settings.apiUrl = text
                        }
                    }

                    // 模型选择
                    SectionBox {
                        label: "模型"
                        hint:  "当前使用的模型名称"

                        ComboBox {
                            Layout.fillWidth: true
                            model: settings.modelList
                            currentIndex: {
                                var idx = settings.modelList.indexOf(settings.modelName)
                                return idx >= 0 ? idx : 0
                            }
                            onActivated: settings.modelName = settings.modelList[currentIndex]
                            contentItem: Text {
                                leftPadding: 10
                                text: parent.displayText
                                color: cText; font.pixelSize: 13
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle { radius: 5; color: cInput; border.color: cBorder }
                            popup.background: Rectangle { color: cPanel; radius: 5; border.color: cBorder }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Button {
                                Layout.alignment: Qt.AlignLeft
                                text: "从 API 刷新模型列表"
                                height: 30
                                enabled: !settings.modelsRefreshing
                                contentItem: Text {
                                    text: parent.text; color: cMuted; font.pixelSize: 12
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle { radius: 5; color: cInput; border.color: cBorder }
                                onClicked: settings.refreshModels()
                            }
                            Text {
                                Layout.fillWidth: true
                                text: settings.modelsStatus
                                color: cMuted
                                font.pixelSize: 11
                                wrapMode: Text.Wrap
                                visible: settings.modelsStatus !== ""
                            }
                        }
                    }

                    Item { height: 20 }
                }
            }

            // ── Tab 1: 参数调节 ───────────────────────────────────────────────
            ScrollView {
            id: paramsTuningScroll
                anchors.fill: parent
                clip: true
                // 关键：固定内容宽度，防止溢出；禁用水平滚动
                contentWidth: availableWidth
                visible: settingsWin.currentTab === 1
                enabled: visible

                ColumnLayout {
                    width: parent.width - 40
                    x: 20
                    y: 20
                    spacing: 18

                    SliderSection {
                        label: "Temperature"
                        hint:  "控制随机性，越大越有创意 (0.0 – 2.0)"
                        value: settings.temperature
                        from: 0.0; to: 2.0; stepSize: 0.05
                        scrollView: paramsTuningScroll
                        onMoved: settings.temperature = value
                    }
                    SliderSection {
                        label: "Top-P"
                        hint:  "核采样阈值 (0.0 – 1.0)"
                        value: settings.topP
                        from: 0.0; to: 1.0; stepSize: 0.01
                        scrollView: paramsTuningScroll
                        onMoved: settings.topP = value
                    }
                    SliderSection {
                        label: "Top-K"
                        hint:  "每步候选 token 数量 (1 – 200)"
                        value: settings.topK
                        from: 1; to: 200; stepSize: 1
                        scrollView: paramsTuningScroll
                        onMoved: settings.topK = value
                    }
                    SliderSection {
                        label: "Max Tokens"
                        hint:  "最大生成 token 数 (256 – 32768)"
                        value: settings.maxTokens
                        from: 256; to: 32768; stepSize: 256
                        scrollView: paramsTuningScroll
                        onMoved: settings.maxTokens = value
                    }

                    Item { height: 20 }
                }
            }

            // ── Tab 2: 系统提示词 ─────────────────────────────────────────────
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                visible: settingsWin.currentTab === 2
                enabled: visible
                spacing: 12

                Text {
                    text: "System Prompt"
                    color: cText; font.pixelSize: 13; font.bold: true
                }
                Text {
                    text: "对话开始前注入的角色设定，对所有新对话生效"
                    color: cMuted; font.pixelSize: 12
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: cInput; radius: 6
                    border.color: cBorder

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 4
                        contentWidth: availableWidth

                        TextArea {
                            id: systemPromptArea
                            width: parent.width
                            text: settings.systemPrompt
                            color: cText; font.pixelSize: 13
                            wrapMode: TextArea.Wrap
                            background: null
                            onTextChanged: settings.systemPrompt = text
                        }
                    }
                }
            }
        }

        // ── 底部按钮 ──────────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true; height: 56
            color: cPanel
            Rectangle { width: parent.width; height: 1; color: cBorder }

            Row {
                anchors {
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    rightMargin: 16
                }
                spacing: 10

                Button {
                    text: "重置默认"; height: 34
                    contentItem: Text {
                        text: parent.text; color: cMuted; font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle { radius: 5; color: cInput; border.color: cBorder }
                    onClicked: {
                        // 参数默认值
                        settings.temperature = 0.7
                        settings.topP        = 0.9
                        settings.topK        = 50
                        settings.maxTokens   = 4096
                        // API 默认值
                        settings.apiUrl      = "https://dashscope.aliyuncs.com/compatible-mode/v1"
                        settings.modelName   = "qwen3-plus"
                    }
                }
                Button {
                    text: "保存关闭"; height: 34
                    contentItem: Text {
                        text: parent.text; color: "white"; font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle { radius: 5; color: cAccent }
                    onClicked: { settings.save(); settingsWin.close() }
                }
            }
        }
    }

    // ── 内部组件：带标签的分组 ───────────────────────────────────────────────
    component SectionBox: ColumnLayout {
        property string label: ""
        property string hint:  ""
        Layout.fillWidth: true
        spacing: 6
        Text { text: parent.label; color: cText; font.pixelSize: 13; font.bold: true }
        Text {
            text: parent.hint; color: cMuted; font.pixelSize: 11
            visible: parent.hint !== ""
        }
    }

    // ── 内部组件：输入框 ─────────────────────────────────────────────────────
    component FieldInput: TextField {
        Layout.fillWidth: true
        height: 36
        color: cText; font.pixelSize: 13
        placeholderTextColor: cMuted
        background: Rectangle { radius: 5; color: cInput; border.color: cBorder }
        leftPadding: 10
        selectByMouse: true
    }

    // ── 内部组件：滑块分组 ───────────────────────────────────────────────────
    component SliderSection: ColumnLayout {
        property string label:    ""
        property string hint:     ""
        property real   value:    0
        property real   from:     0
        property real   to:       1
        property real   stepSize: 0.01
        // 所在的 ScrollView（用于按下时临时禁用滚动，避免抢事件）
        property Item   scrollView: null
        signal moved(real value)

        Layout.fillWidth: true
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: parent.parent.label
                color: cText; font.pixelSize: 13; font.bold: true
                Layout.fillWidth: true
            }
            Text {
                text: parent.parent.value.toFixed(parent.parent.stepSize < 1 ? 2 : 0)
                color: cAccent; font.pixelSize: 13; font.bold: true
            }
        }
        Text { text: parent.hint; color: cMuted; font.pixelSize: 11 }

        Slider {
            id: sliderControl
            Layout.fillWidth: true
            from: parent.from; to: parent.to
            stepSize: parent.stepSize
            value: parent.value

            // 关键：按下时让所属 ScrollView 暂停滚动，避免抢事件
            onPressedChanged: {
                if (pressed) {
                    sliderControl.forceActiveFocus()
                }
                if (scrollView && scrollView.contentItem) {
                    scrollView.contentItem.interactive = !pressed
                }
            }
            onMoved: parent.moved(value)
            onValueChanged: {
                // 支持：点击轨道跳转、键盘调整等也能更新设置值
                if (pressed) parent.moved(value)
            }

            background: Rectangle {
                x: sliderControl.leftPadding
                y: sliderControl.topPadding + sliderControl.availableHeight / 2 - height / 2
                width: sliderControl.availableWidth; height: 4; radius: 2; color: cBorder

                Rectangle {
                    width: sliderControl.visualPosition * parent.width
                    height: 4; radius: 2; color: cAccent
                }
            }
            handle: Rectangle {
                x: sliderControl.leftPadding +
                   sliderControl.visualPosition * (sliderControl.availableWidth - width)
                y: sliderControl.topPadding + sliderControl.availableHeight / 2 - height / 2
                width: 16; height: 16; radius: 8
                color: sliderControl.pressed ? Qt.lighter(cAccent, 1.2) : cAccent
            }

            // 支持鼠标滚轮调节参数
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton      // 只接收滚轮事件，不影响点击和拖动
                onWheel: function(wheel) {
                    var step = parent.stepSize > 0 ? parent.stepSize : 0.01
                    var dir = wheel.angleDelta.y > 0 ? 1 : -1
                    var newVal = sliderControl.value + dir * step
                    newVal = Math.max(sliderControl.from, Math.min(sliderControl.to, newVal))
                    if (newVal !== sliderControl.value) {
                        sliderControl.value = newVal
                        parent.moved(sliderControl.value)
                    }
                    wheel.accepted = true
                }
            }
        }
    }
}

