// qml/MarkdownRender.qml
import QtQuick 2.15

Item {
    id: root
    property string markdownText: ""
    property color  textColor:    "#DBDEE1"
    property int    fontSize:     14

    implicitHeight: mdText.contentHeight
    height: implicitHeight

    // 直接用 TextEdit：兼容性更好，且支持选中/复制
    TextEdit {
        id: mdText
        anchors.left: parent.left
        anchors.right: parent.right
        text: root.markdownText
        textFormat: TextEdit.MarkdownText
        wrapMode: TextEdit.Wrap
        color: root.textColor
        font.pixelSize: root.fontSize
        readOnly: true
        selectByMouse: true
    }
}