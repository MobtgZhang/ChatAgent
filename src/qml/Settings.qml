// qml/Settings.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Dialogs

Window {
    id: settingsWin
    property var settings: null
    title: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.settingsTitle : "Settings"
    width: 680; height: 720
    minimumWidth: 600; minimumHeight: 640
    color: (typeof settings !== "undefined" && settings.theme === "light") ? "#FAFAFA" : "#0D0D0F"
    flags: Qt.Dialog
    modality: Qt.ApplicationModal

    // ── 动态色板：Dark 黑底白字 / Light 白底黑字，高对比度 ──────────────────────
    readonly property bool isLight: (typeof settings !== "undefined" && settings.theme === "light")
    readonly property color cBg:     isLight ? "#FAFAFA" : "#0D0D0F"
    readonly property color cPanel:  isLight ? "#F0F0F2" : "#111113"
    readonly property color cInput:  isLight ? "#FFFFFF" : "#1C1C1F"
    readonly property color cBorder: isLight ? "#D8D8DC" : "#2D2D32"
    readonly property color cText:   isLight ? "#0D0D0F" : "#F2F2F4"
    readonly property color cMuted:  isLight ? "#3D3F47" : "#8E9099"
    readonly property color cAccent:  "#5865F2"
    readonly property color cHighlight: isLight ? "#E8E8EC" : "#252528"
    readonly property color cPopupBg: isLight ? "#FFFFFF" : "#111113"

    // ── 标签选项卡 ────────────────────────────────────────────────────────────
    property int currentTab: 0
    readonly property var tabs: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? ["🔑 " + localeBridge.t.tabApiConfigEmoji, "⚙️ " + localeBridge.t.tabParamsEmoji, "📝 " + localeBridge.t.tabSystemPromptEmoji, "🤖 " + localeBridge.t.tabAgentEmoji, "⚙️ " + localeBridge.t.tabSystemSettingsEmoji] : ["API Config", "Parameters", "System Prompt", "Agent", "System"]

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

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
            Flickable {
                id: apiFlick
                anchors.fill: parent
                clip: true
                contentWidth: width
                contentHeight: apiCol.implicitHeight + 40
                boundsBehavior: Flickable.StopAtBounds
                visible: settingsWin.currentTab === 0
                enabled: visible
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                ColumnLayout {
                    id: apiCol
                    width: parent.width - 40
                    x: 20
                    y: 20
                    spacing: 18

                    // API Key
                    SectionBox {
                        label: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.apiKey : "API Key"
                        hint:  (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.apiKeyHint : ""
                        FieldInput {
                            id: apiKeyField
                            Layout.fillWidth: true
                            text: settings.apiKey
                            echoMode: TextInput.Password
                            onEditingFinished: settings.apiKey = text
                        }
                        Button {
                            text: apiKeyField.echoMode === TextInput.Password ? ("👁 " + (localeBridge && localeBridge.t && localeBridge.tVersion >= 0 ? localeBridge.t.show : "Show")) : ("🔒 " + (localeBridge && localeBridge.t && localeBridge.tVersion >= 0 ? localeBridge.t.hide : "Hide"))
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
                        label: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.apiUrl : "API URL"
                        hint:  (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.apiUrlHint : ""
                        FieldInput {
                            Layout.fillWidth: true
                            text: settings.apiUrl
                            onEditingFinished: settings.apiUrl = text
                        }
                    }

                    // 模型选择
                    SectionBox {
                        label: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.model : "Model"
                        hint:  (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.modelHint : ""

                        ComboBox {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
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
                            popup.background: Rectangle { color: cPopupBg; radius: 5; border.color: cBorder }
                            delegate: ItemDelegate {
                                width: parent ? parent.width - 20 : 200
                                hoverEnabled: true
                                contentItem: Text {
                                    text: modelData
                                    color: cText
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: (parent.highlighted || parent.hovered) ? cHighlight : "transparent"
                                    radius: 4
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Button {
                                Layout.alignment: Qt.AlignLeft
                                text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.refreshModels : "Refresh models"
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
            Flickable {
                id: paramsFlick
                anchors.fill: parent
                clip: true
                contentWidth: width
                contentHeight: paramsCol.implicitHeight + 40
                boundsBehavior: Flickable.StopAtBounds
                visible: settingsWin.currentTab === 1
                enabled: visible
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                ColumnLayout {
                    id: paramsCol
                    width: parent.width - 40
                    x: 20
                    y: 20
                    spacing: 18

                    SliderSection {
                        label: "Temperature"
                        hint:  (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.temperatureHint : ""
                        value: settings.temperature
                        from: 0.0; to: 1.0; stepSize: 0.05
                        scrollView: paramsFlick
                        onMoved: function(value) { settings.temperature = value }
                    }
                    SliderSection {
                        label: "Top-P"
                        hint:  (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.topPHint : ""
                        value: settings.topP
                        from: 0.0; to: 1.0; stepSize: 0.01
                        scrollView: paramsFlick
                        onMoved: function(value) { settings.topP = value }
                    }
                    SliderSection {
                        label: "Top-K"
                        hint:  (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.topKHint : ""
                        value: settings.topK
                        from: 1; to: 200; stepSize: 1
                        scrollView: paramsFlick
                        onMoved: function(value) { settings.topK = value }
                    }
                    SliderSection {
                        label: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.maxTokens : "Max Tokens"
                        hint:  (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.maxTokensHint : ""
                        value: settings.maxTokens
                        from: 256; to: 32768; stepSize: 256
                        scrollView: paramsFlick
                        onMoved: function(value) { settings.maxTokens = value }
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
                    text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.systemPrompt : "System Prompt"
                    color: cText; font.pixelSize: 13; font.bold: true
                }
                Text {
                    text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.systemPromptDesc : ""
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
                Text {
                    Layout.fillWidth: true
                    text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.systemPromptEmptyHint : ""
                    color: cMuted; font.pixelSize: 11; wrapMode: Text.Wrap
                }
            }

            // ── Tab 3: Agent 设置 ────────────────────────────────────────────────
            Flickable {
                id: agentFlick
                anchors.fill: parent
                clip: true
                contentWidth: width
                contentHeight: agentCol.implicitHeight + 40
                boundsBehavior: Flickable.StopAtBounds
                visible: settingsWin.currentTab === 3
                enabled: visible
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                ColumnLayout {
                    id: agentCol
                    width: parent.width - 40
                    x: 20
                    y: 20
                    spacing: 18

                    SliderSection {
                        label: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.maxToolRounds : "Max tool rounds"
                        hint:  (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.maxToolRoundsHint : ""
                        value: settings.maxToolRounds
                        from: 5; to: 40; stepSize: 1
                        scrollView: agentFlick
                        onMoved: function(v) { settings.maxToolRounds = Math.round(v) }
                    }

                    SectionBox {
                        label: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.actionPrinciple : "Action Principle"
                        hint:  (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.actionPrincipleHint : ""
                    }

                    Item { height: 20 }
                }
            }

            // ── Tab 4: 系统设置 ────────────────────────────────────────────────────
            Flickable {
                id: systemFlick
                anchors.fill: parent
                clip: true
                contentWidth: width
                contentHeight: systemCol.implicitHeight + 40
                boundsBehavior: Flickable.StopAtBounds
                visible: settingsWin.currentTab === 4
                enabled: visible
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                ColumnLayout {
                    id: systemCol
                    width: parent.width - 40
                    x: 20
                    y: 20
                    spacing: 18

                    // 主题设置（Dark / Light，Radio 按钮）
                    SectionBox {
                        label: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.themeSettings : "Theme"
                        hint:  (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.themeSettingsHint : ""
                        Row {
                            spacing: 16
                            RadioButton {
                                id: themeDark
                                text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.themeDark : "Dark"
                                checked: (typeof settings !== "undefined" && settings.theme !== "light")
                                onCheckedChanged: if (checked && typeof settings !== "undefined") settings.theme = "dark"
                                contentItem: Text {
                                    text: themeDark.text
                                    color: cText
                                    font.pixelSize: 13
                                    leftPadding: themeDark.indicator.width + themeDark.spacing
                                    verticalAlignment: Text.AlignVCenter
                                }
                                indicator: Rectangle {
                                    implicitWidth: 18
                                    implicitHeight: 18
                                    radius: 9
                                    border.color: themeDark.checked ? cAccent : cBorder
                                    border.width: 2
                                    color: themeDark.checked ? cAccent : "transparent"
                                    Rectangle {
                                        width: 8
                                        height: 8
                                        radius: 4
                                        anchors.centerIn: parent
                                        color: themeDark.checked ? "white" : "transparent"
                                    }
                                }
                            }
                            RadioButton {
                                id: themeLight
                                text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.themeLight : "Light"
                                checked: (typeof settings !== "undefined" && settings.theme === "light")
                                onCheckedChanged: if (checked && typeof settings !== "undefined") settings.theme = "light"
                                contentItem: Text {
                                    text: themeLight.text
                                    color: cText
                                    font.pixelSize: 13
                                    leftPadding: themeLight.indicator.width + themeLight.spacing
                                    verticalAlignment: Text.AlignVCenter
                                }
                                indicator: Rectangle {
                                    implicitWidth: 18
                                    implicitHeight: 18
                                    radius: 9
                                    border.color: themeLight.checked ? cAccent : cBorder
                                    border.width: 2
                                    color: themeLight.checked ? cAccent : "transparent"
                                    Rectangle {
                                        width: 8
                                        height: 8
                                        radius: 4
                                        anchors.centerIn: parent
                                        color: themeLight.checked ? "white" : "transparent"
                                    }
                                }
                            }
                        }
                    }

                    // 语言设置（动态探测翻译文件夹，英文排最前；无配置时仅显示英语）
                    SectionBox {
                        label: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.languageSettings : "Language"
                        hint:  (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.languageSettingsHint : ""
                        ComboBox {
                            id: langCombo
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            readonly property var langList: (localeBridge && settings) ? (settings.hasConfigFile, localeBridge.availableLanguageList()) : []
                            model: langList
                            property int _sel: {
                                if (!model || model.length === 0) return 0
                                var lang = (typeof settings !== "undefined") ? settings.language : "en"
                                for (var i = 0; i < model.length; i++)
                                    if (model[i] && model[i].code === lang) return i
                                return 0
                            }
                            currentIndex: (model && model.length > 0) ? Math.min(_sel, model.length - 1) : 0
                            onActivated: function(index) {
                                if (typeof settings !== "undefined" && settings.applyLanguage && model && model[index])
                                    settings.applyLanguage(model[index].code)
                            }
                            contentItem: Text {
                                leftPadding: 10
                                text: (langCombo.model && langCombo.currentIndex >= 0 && langCombo.currentIndex < langCombo.model.length && langCombo.model[langCombo.currentIndex])
                                      ? langCombo.model[langCombo.currentIndex].name : ""
                                color: cText
                                font.pixelSize: 13
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle { radius: 5; color: cInput; border.color: cBorder }
                            popup.background: Rectangle { color: cPopupBg; radius: 5; border.color: cBorder }
                            delegate: ItemDelegate {
                                width: parent ? parent.width - 20 : 200
                                hoverEnabled: true
                                contentItem: Text {
                                    text: modelData ? modelData.name : ""
                                    color: cText
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: (parent.highlighted || parent.hovered) ? cHighlight : "transparent"
                                    radius: 4
                                }
                            }
                        }
                    }

                    // 搜索引擎选择（联网搜索用）
                    SectionBox {
                        label: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.searchEngineSettings : "Search Engine"
                        hint:  (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.searchEngineHint : ""
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8
                        ComboBox {
                            id: searchEngineCombo
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            readonly property var engineList: ["duckduckgo", "baidu", "bing", "brave", "google", "tencent"]
                            readonly property var engineNames: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0)
                                ? [localeBridge.t.searchEngineDuckDuckGo, localeBridge.t.searchEngineBaidu, localeBridge.t.searchEngineBing, localeBridge.t.searchEngineBrave, localeBridge.t.searchEngineGoogle, localeBridge.t.searchEngineTencent]
                                : ["DuckDuckGo", "Baidu", "Bing Search", "Brave Search", "Google", "Tencent Search"]
                            model: engineList
                            currentIndex: {
                                var idx = engineList.indexOf(settings.searchEngine)
                                return idx >= 0 ? idx : 0
                            }
                            onActivated: settings.searchEngine = engineList[currentIndex]
                            contentItem: Text {
                                leftPadding: 10
                                text: (searchEngineCombo.currentIndex >= 0 && searchEngineCombo.currentIndex < searchEngineCombo.engineNames.length)
                                      ? searchEngineCombo.engineNames[searchEngineCombo.currentIndex] : ""
                                color: cText; font.pixelSize: 13
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle { radius: 5; color: cInput; border.color: cBorder }
                            popup.background: Rectangle { color: cPopupBg; radius: 5; border.color: cBorder }
                            delegate: ItemDelegate {
                                width: parent ? parent.width - 20 : 200
                                hoverEnabled: true
                                property string engineName: (searchEngineCombo.engineNames && index >= 0 && index < searchEngineCombo.engineNames.length) ? searchEngineCombo.engineNames[index] : modelData
                                contentItem: Text {
                                    text: parent.engineName
                                    color: cText
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: (parent.highlighted || parent.hovered) ? cHighlight : "transparent"
                                    radius: 4
                                }
                            }
                        }
                        // Bing/Brave/Google API Key（选 bing=2, brave=3, google=4 时显示）
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            visible: searchEngineCombo.currentIndex >= 2 && searchEngineCombo.currentIndex <= 4
                            FieldInput {
                                id: webSearchApiKeyField
                                Layout.fillWidth: true
                                text: settings.webSearchApiKey
                                echoMode: TextInput.Password
                                placeholderText: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.webSearchApiKeyHint : "Bing/Brave/Google API Key"
                                onEditingFinished: settings.webSearchApiKey = text
                            }
                            Button {
                                height: 36
                                text: webSearchApiKeyField.echoMode === TextInput.Password ? ("👁 " + (localeBridge && localeBridge.t && localeBridge.tVersion >= 0 ? localeBridge.t.show : "Show")) : ("🔒 " + (localeBridge && localeBridge.t && localeBridge.tVersion >= 0 ? localeBridge.t.hide : "Hide"))
                                contentItem: Text { text: parent.text; color: cMuted; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                background: Rectangle { radius: 5; color: cInput; border.color: cBorder }
                                onClicked: webSearchApiKeyField.echoMode = (webSearchApiKeyField.echoMode === TextInput.Password) ? TextInput.Normal : TextInput.Password
                            }
                        }
                        // 腾讯 SecretId + SecretKey（选 tencent=5 时显示）
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            visible: searchEngineCombo.currentIndex === 5
                            FieldInput {
                                id: tencentSecretIdField
                                Layout.fillWidth: true
                                text: settings.tencentSecretId
                                placeholderText: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.tencentSecretIdHint : "SecretId"
                                onEditingFinished: settings.tencentSecretId = text
                            }
                            FieldInput {
                                id: tencentSecretKeyField
                                Layout.fillWidth: true
                                text: settings.tencentSecretKey
                                echoMode: TextInput.Password
                                placeholderText: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.tencentSecretKeyHint : "SecretKey"
                                onEditingFinished: settings.tencentSecretKey = text
                            }
                        }
                        }
                    }

                    // 代理设置（不显示长说明，仅保留选项）
                    SectionBox {
                        label: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.proxySettings : "Proxy"
                        hint:  ""
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            ComboBox {
                                id: proxyModeCombo
                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                                readonly property var proxyModeList: ["off", "system", "manual"]
                                readonly property var proxyModeNames: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0)
                                    ? [localeBridge.t.proxyModeOff, localeBridge.t.proxyModeSystem, localeBridge.t.proxyModeManual]
                                    : ["Off", "System", "Manual"]
                                model: proxyModeList
                                currentIndex: {
                                    var idx = proxyModeList.indexOf(settings.proxyMode)
                                    return idx >= 0 ? idx : 0
                                }
                                onActivated: settings.proxyMode = proxyModeList[currentIndex]
                                contentItem: Text {
                                    leftPadding: 10
                                    text: (proxyModeCombo.currentIndex >= 0 && proxyModeCombo.currentIndex < proxyModeCombo.proxyModeNames.length)
                                          ? proxyModeCombo.proxyModeNames[proxyModeCombo.currentIndex] : ""
                                    color: cText; font.pixelSize: 13
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle { radius: 5; color: cInput; border.color: cBorder }
                                popup.background: Rectangle { color: cPopupBg; radius: 5; border.color: cBorder }
                                delegate: ItemDelegate {
                                    width: parent ? parent.width - 20 : 200
                                    hoverEnabled: true
                                    property string modeName: (proxyModeCombo.proxyModeNames && index >= 0 && index < proxyModeCombo.proxyModeNames.length) ? proxyModeCombo.proxyModeNames[index] : modelData
                                    contentItem: Text {
                                        text: parent.modeName
                                        color: cText
                                        font.pixelSize: 13
                                        elide: Text.ElideRight
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle {
                                        color: (parent.highlighted || parent.hovered) ? cHighlight : "transparent"
                                        radius: 4
                                    }
                                }
                            }
                            FieldInput {
                                Layout.fillWidth: true
                                visible: settings.proxyMode === "manual"
                                text: settings.proxyUrl
                                placeholderText: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.proxyPlaceholder : ""
                                onEditingFinished: settings.proxyUrl = text
                            }
                        }
                    }

                    // 缓存目录设置（历史记录与会话配置存储位置）
                    SectionBox {
                        label: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.cacheDirSettings : "Cache Directory"
                        hint:  (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.cacheDirHint : ""
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            FieldInput {
                                id: cacheDirField
                                Layout.fillWidth: true
                                text: (typeof settings !== "undefined") ? settings.cacheDirectory : ""
                                placeholderText: (typeof settings !== "undefined") ? settings.defaultCacheDirectory() : ""
                                onEditingFinished: {
                                    if (typeof settings !== "undefined")
                                        settings.cacheDirectory = text.trim()
                                }
                            }
                            Button {
                                text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.cacheDirBrowse : "Browse"
                                height: 36
                                contentItem: Text {
                                    text: parent.text
                                    color: cMuted
                                    font.pixelSize: 12
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle { radius: 5; color: cInput; border.color: cBorder }
                                onClicked: cacheFolderDialog.open()
                            }
                        }
                        Text {
                            text: ((localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.cacheDirDefault : "Default") + ": " + (typeof settings !== "undefined" ? settings.defaultCacheDirectory() : "")
                            color: cMuted
                            font.pixelSize: 11
                            wrapMode: Text.Wrap
                        }
                    }

                    Item { height: 20 }
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
                    text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.resetDefault : "Reset"; height: 34
                    contentItem: Text {
                        text: parent.text; color: cMuted; font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle { radius: 5; color: cInput; border.color: cBorder }
                    onClicked: {
                        settings.temperature = 0.7
                        settings.topP        = 0.9
                        settings.topK        = 50
                        settings.maxTokens   = 4096
                        settings.apiUrl      = "https://dashscope.aliyuncs.com/compatible-mode/v1"
                        settings.modelName   = "qwen3.5-plus"
                        settings.systemPrompt  = ""
                        settings.maxToolRounds = 40
                        settings.showThinking  = false
                        if (settings.applyLanguage) settings.applyLanguage("en")
                        else { settings.language = "en"; settings.save() }
                        settings.theme         = "dark"
                        settings.cacheDirectory = ""
                        settings.searchEngine  = "duckduckgo"
                        settings.webSearchApiKey = ""
                        settings.tencentSecretId = ""
                        settings.tencentSecretKey = ""
                        settings.proxyMode     = "off"
                        settings.proxyUrl      = ""
                        settings.save()
                    }
                }
                Button {
                    text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.saveClose : "Save & Close"; height: 34
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

    // 缓存目录选择对话框
    FolderDialog {
        id: cacheFolderDialog
        title: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.selectCacheDir : "Select cache directory"
        currentFolder: (typeof settings !== "undefined" && settings.cacheDirectory)
            ? "file://" + settings.cacheDirectory
            : "file://" + (typeof settings !== "undefined" ? settings.defaultCacheDirectory() : "/")
        onAccepted: {
            if (typeof settings !== "undefined" && selectedFolder) {
                var path = settings.urlToLocalPath(selectedFolder.toString())
                settings.cacheDirectory = path
                cacheDirField.text = path
            }
        }
    }

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

    component FieldInput: TextField {
        Layout.fillWidth: true
        height: 36
        color: cText; font.pixelSize: 13
        placeholderTextColor: cMuted
        background: Rectangle { radius: 5; color: cInput; border.color: cBorder }
        leftPadding: 10
        selectByMouse: true
    }

    component SliderSection: ColumnLayout {
        property string label:    ""
        property string hint:     ""
        property real   value:    0
        property real   from:     0
        property real   to:       1
        property real   stepSize: 0.01
        property Item   scrollView: null
        signal moved(real value)

        property real displayValue: value
        property bool isInt: stepSize >= 1

        Layout.fillWidth: true
        spacing: 4

        onValueChanged: {
            if (!sliderControl.pressed && !valueEdit.activeFocus) {
                sliderControl.value = value
                displayValue = value
                valueEdit.text = formatVal(value)
            }
        }

        function formatVal(v) { return isInt ? Math.round(v).toString() : v.toFixed(2) }
        function clampVal(v) { return Math.max(from, Math.min(to, v)) }

        Component.onCompleted: {
            sliderControl.value = value
            displayValue = value
            valueEdit.text = formatVal(value)
        }

        Text {
            text: parent.label
            color: cText; font.pixelSize: 13; font.bold: true
        }
        Text { text: parent.hint; color: cMuted; font.pixelSize: 11 }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Slider {
                id: sliderControl
                Layout.fillWidth: true
                Layout.minimumHeight: 28
                from: parent.parent.from
                to: parent.parent.to
                stepSize: parent.parent.stepSize
                value: 0

                onPressedChanged: {
                    if (pressed) sliderControl.forceActiveFocus()
                    if (scrollView) {
                        if (scrollView.contentItem)
                            scrollView.contentItem.interactive = !pressed
                        else
                            scrollView.interactive = !pressed
                    }
                }
                onMoved: {
                    parent.parent.displayValue = value
                    parent.parent.moved(value)
                    valueEdit.text = parent.parent.formatVal(value)
                }
                onValueChanged: {
                    if (pressed) {
                        parent.parent.displayValue = value
                        parent.parent.moved(value)
                        valueEdit.text = parent.parent.formatVal(value)
                    }
                }

                palette.accent: cAccent
                palette.base: cInput
                palette.mid: cBorder
            }
            TextField {
                id: valueEdit
                Layout.preferredWidth: 72
                Layout.alignment: Qt.AlignVCenter
                horizontalAlignment: TextInput.AlignRight
                color: cText; font.pixelSize: 13
                selectByMouse: true
                background: Rectangle { radius: 5; color: cInput; border.color: cBorder }
                leftPadding: 8; rightPadding: 8

                onEditingFinished: {
                    var ss = parent.parent
                    var v = parseFloat(text)
                    if (!isNaN(v)) {
                        if (ss.isInt) v = Math.round(v)
                        v = ss.clampVal(v)
                        ss.displayValue = v
                        ss.moved(v)
                        sliderControl.value = v
                        text = ss.formatVal(v)
                    } else {
                        text = ss.formatVal(ss.displayValue)
                    }
                }
                onTextChanged: {
                    if (!activeFocus) return
                    var v = parseFloat(text)
                    if (!isNaN(v)) parent.parent.displayValue = parent.parent.clampVal(v)
                }
            }
        }
    }
}
