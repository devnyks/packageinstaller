import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property string itemName: ""
    property string itemSummary: ""
    property string itemStatus: ""
    property string itemVersion: ""
    property string itemInstalledVersion: ""
    property string itemRepo: ""
    property string itemIcon: ""
    property string itemIconPath: ""
    property string itemIconUrl: ""
    property string itemSize: ""
    property bool itemSelected: false
    property bool itemInstalled: false
    property bool itemUpgradable: false
    property bool itemRuntime: false
    property color textColor: "#172033"
    property color mutedColor: "#667085"
    property color surfaceColor: "#ffffff"
    property color borderColor: "#dfe4ec"
    property color accentColor: "#2368d1"
    signal toggleRequested()
    signal detailsRequested()

    implicitHeight: 78

    Rectangle {
        anchors.fill: parent
        anchors.bottomMargin: 1
        color: root.itemSelected ? (root.surfaceColor === "#ffffff" ? "#f1f6ff" : "#202b3d") : root.surfaceColor
        border.color: root.borderColor
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 12
            spacing: 12

            CheckBox {
                checked: root.itemSelected
                onClicked: root.toggleRequested()
            }

            Rectangle {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                radius: 12
                color: root.itemRuntime ? "#f0eaff" : "#eef2f7"
                ToolButton {
                    anchors.centerIn: parent
                    icon.name: root.itemIcon
                    icon.width: 26
                    icon.height: 26
                    visible: root.itemIconPath.length === 0 && root.itemIcon.length > 0
                    enabled: false
                }
                Image {
                    id: iconImage
                    anchors.centerIn: parent
                    width: 30
                    height: 30
                    fillMode: Image.PreserveAspectFit
                    source: root.itemIconPath.length > 0 ? (root.itemIconPath.startsWith("/") ? "file://" + root.itemIconPath : root.itemIconPath) : root.itemIconUrl
                    visible: status === Image.Ready
                    asynchronous: true
                }
                Label {
                    anchors.centerIn: parent
                    text: root.itemName.length > 0 ? root.itemName.charAt(0).toUpperCase() : "?"
                    color: root.accentColor
                    font.bold: true
                    font.pixelSize: 17
                    visible: iconImage.status !== Image.Ready && (root.itemIconPath.length > 0 || root.itemIconUrl.length > 0 || root.itemIcon.length === 0)
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Label {
                    Layout.fillWidth: true
                    text: root.itemName
                    color: root.textColor
                    font.bold: true
                    font.pixelSize: 14
                    elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true
                    text: root.itemSummary
                    color: root.mutedColor
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }

            Label {
                text: root.itemRepo
                color: root.mutedColor
                font.pixelSize: 11
                Layout.preferredWidth: 120
                elide: Text.ElideRight
            }
            ColumnLayout {
                Layout.preferredWidth: 145
                spacing: 2
                Label {
                    text: root.itemUpgradable ? qsTr("Update available") : root.itemInstalled ? qsTr("Installed") : qsTr("Available")
                    color: root.itemUpgradable ? "#9a5b00" : root.itemInstalled ? "#087443" : root.mutedColor
                    font.pixelSize: 11
                    font.bold: true
                }
                Label {
                    text: root.itemUpgradable ? root.itemInstalledVersion + " -> " + root.itemVersion : root.itemVersion
                    color: root.mutedColor
                    font.pixelSize: 11
                }
            }
            Label {
                text: root.itemSize
                color: root.mutedColor
                font.pixelSize: 11
                Layout.preferredWidth: 80
                horizontalAlignment: Text.AlignRight
            }
            Button {
                text: qsTr("Info")
                flat: true
                onClicked: root.detailsRequested()
            }
        }
    }
}
