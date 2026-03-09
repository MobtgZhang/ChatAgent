// qml/About.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

Window {
    id: aboutWin
    title: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.about : "About"
    width: 380; height: 300
    minimumWidth: 360; minimumHeight: 280
    color: (typeof settings !== "undefined" && settings.theme === "light") ? "#FAFAFA" : "#0D0D0F"
    flags: Qt.Dialog
    modality: Qt.ApplicationModal

    readonly property bool isLight: (typeof settings !== "undefined" && settings.theme === "light")
    readonly property color cText:  isLight ? "#0D0D0F" : "#F2F2F4"
    readonly property color cMuted: isLight ? "#4A4C54" : "#8E9099"
    readonly property color cBorder: isLight ? "#D8D8DC" : "#2D2D32"

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 14
        width: parent.width - 60

        // Logo + 名称
        Row {
            Layout.alignment: Qt.AlignHCenter
            spacing: 14
            Rectangle {
                width: 54; height: 54; radius: 12; color: "#F9A825"
                Text { anchors.centerIn: parent; text: "💬"; font.pixelSize: 28 }
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                Text { text: about.appName;  color: aboutWin.cText; font.pixelSize: 20; font.bold: true }
                Text { text: "v" + about.version; color: aboutWin.cMuted; font.pixelSize: 13 }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: aboutWin.cBorder }

        Text {
            Layout.fillWidth: true
            text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0 && localeBridge.t.aboutDescription) ? localeBridge.t.aboutDescription : about.description
            color: aboutWin.cMuted; font.pixelSize: 13
            wrapMode: Text.Wrap
            lineHeight: 1.5
            horizontalAlignment: Text.AlignHCenter
        }

        Grid {
            Layout.alignment: Qt.AlignHCenter
            columns: 2; rowSpacing: 6; columnSpacing: 20
            Repeater {
                model: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? [
                    [localeBridge.t.aboutLicense, about.license],
                    [localeBridge.t.aboutQtVersion, about.qtVersion],
                    [localeBridge.t.aboutBuildDate, about.buildDate]
                ] : [
                    ["License", about.license],
                    ["Qt Version", about.qtVersion],
                    ["Build Date", about.buildDate]
                ]
                delegate: Row {
                    spacing: 6
                    Text { text: modelData[0] + "："; color: aboutWin.cMuted; font.pixelSize: 12 }
                    Text { text: modelData[1];         color: aboutWin.cText; font.pixelSize: 12 }
                }
            }
        }

        Button {
            Layout.alignment: Qt.AlignHCenter
            text: (localeBridge && localeBridge.t && localeBridge.tVersion >= 0) ? localeBridge.t.close : "Close"; width: 100; height: 32
            contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 13;
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            background: Rectangle { radius: 6; color: "#5865F2" }
            onClicked: aboutWin.close()
        }
    }
}
