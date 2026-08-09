import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property string itemName: ""
    property string itemSummary: ""
    property string itemCategory: ""
    property string itemStatus: ""
    property string itemVersion: ""
    property string itemIcon: ""
    property string itemIconPath: ""
    property string itemIconUrl: ""
    property bool itemSelected: false
    property bool itemInstalled: false
    property bool itemUpgradable: false
    property color textColor: "#172033"
    property color mutedColor: "#667085"
    property color surfaceColor: "#ffffff"
    property color borderColor: "#dfe4ec"
    property color accentColor: "#2368d1"
    signal toggleRequested()
    signal detailsRequested()

    implicitWidth: 270
    implicitHeight: 176

    Rectangle {
        anchors.fill: parent
        radius: 16
        color: root.surfaceColor
        border.width: root.itemSelected ? 2 : 1
        border.color: root.itemSelected ? root.accentColor : root.borderColor

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 50
                    Layout.preferredHeight: 50
                    radius: 14
                    color: root.itemSelected ? "#e6efff" : "#eef2f7"

                    ToolButton {
                        anchors.centerIn: parent
                        width: 42
                        height: 42
                        icon.name: root.itemIcon
                        icon.width: 32
                        icon.height: 32
                        visible: root.itemIconPath.length === 0 && root.itemIconUrl.length === 0 && root.itemIcon.length > 0
                        enabled: false
                    }
                    Image {
                        anchors.centerIn: parent
                        width: 36
                        height: 36
                        id: iconImage
                        fillMode: Image.PreserveAspectFit
                        source: root.itemIconPath.length > 0 ? (root.itemIconPath.startsWith("/") ? "file://" + root.itemIconPath : root.itemIconPath) : root.itemIconUrl
                        visible: status === Image.Ready
                        asynchronous: true
                    }
                    Label {
                        anchors.centerIn: parent
                        text: root.itemName.length > 0 ? root.itemName.charAt(0).toUpperCase() : "?"
                        color: root.accentColor
                        font.pixelSize: 22
                        font.bold: true
                        visible: iconImage.status !== Image.Ready && (root.itemIcon.length === 0 || root.itemIconPath.length > 0 || root.itemIconUrl.length > 0)
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label {
                        Layout.fillWidth: true
                        text: root.itemName
                        color: root.textColor
                        font.pixelSize: 15
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        text: root.itemCategory
                        color: root.mutedColor
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        visible: text.length > 0
                    }
                }

                CheckBox {
                    checked: root.itemSelected
                    onClicked: root.toggleRequested()
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: root.itemSummary.length > 0 ? root.itemSummary : qsTr("No description available")
                color: root.mutedColor
                font.pixelSize: 12
                maximumLineCount: 2
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    text: root.itemUpgradable ? qsTr("Update available") : root.itemInstalled ? qsTr("Installed") : root.itemStatus === "unavailable" ? qsTr("Not in sync database") : qsTr("Available")
                    color: root.itemUpgradable ? "#9a5b00" : root.itemInstalled ? "#087443" : root.mutedColor
                    font.pixelSize: 11
                    font.bold: true
                }
                Label {
                    text: root.itemVersion
                    color: root.mutedColor
                    font.pixelSize: 11
                    visible: text.length > 0
                }
                Button {
                    text: qsTr("Details")
                    flat: true
                    onClicked: root.detailsRequested()
                }
            }
        }
    }
}
