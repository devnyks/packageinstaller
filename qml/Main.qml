import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    visible: true
    width: controller.windowWidth
    height: controller.windowHeight
    minimumWidth: 920
    minimumHeight: 620
    title: qsTr("CachyOS Software Catalog")
    color: theme.background
    palette: Palette {
        window: theme.background
        windowText: theme.text
        base: theme.surface
        alternateBase: theme.surfaceMuted
        text: theme.text
        button: theme.surfaceMuted
        buttonText: theme.text
        highlight: theme.accent
        highlightedText: "white"
        placeholderText: theme.textMuted
    }

    property bool darkMode: controller.theme === "Dark" || (controller.theme === "System" && Application.styleHints.colorScheme === Qt.ColorScheme.Dark)
    property bool consoleOpen: false
    property string selectedDetailsName: ""
    property string selectedDetailsSummary: ""
    property string selectedDetailsDescription: ""
    property string selectedDetailsVersion: ""
    property string selectedDetailsStatus: ""
    property string selectedDetailsSource: ""
    property string selectedDetailsRepo: ""
    property string selectedDetailsTargets: ""
    property string selectedDetailsHomepage: ""
    property string selectedDetailsLicense: ""
    property string selectedDetailsIcon: ""
    property string selectedDetailsScreenshot: ""

    Theme {
        id: theme
        dark: window.darkMode
    }

    function openDetails(name, summary, description, version, status, source, repo, targets, homepage, license, icon, screenshot) {
        selectedDetailsName = name
        selectedDetailsSummary = summary
        selectedDetailsDescription = description
        selectedDetailsVersion = version
        selectedDetailsStatus = status
        selectedDetailsSource = source
        selectedDetailsRepo = repo
        selectedDetailsTargets = targets
        selectedDetailsHomepage = homepage
        selectedDetailsLicense = license
        selectedDetailsIcon = icon
        selectedDetailsScreenshot = screenshot
        detailDialog.open()
    }

    function applySearch() {
        if (controller.page === "home") {
            controller.popularModel.search = globalSearch.text
        } else if (controller.page === "native") {
            controller.repositoryModel.search = globalSearch.text
        } else if (controller.page === "flatpak") {
            controller.flatpakModel.search = globalSearch.text
        }
    }

    onClosing: function(close) {
        controller.saveWindowSize(width, height)
        if (controller.operationRunning) {
            close.accepted = false
            quitDialog.open()
        }
    }

    header: ToolBar {
        height: 72
        background: Rectangle {
            color: theme.surface
            border.color: theme.border
            border.width: 1
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 18

            Rectangle {
                Layout.preferredWidth: 38
                Layout.preferredHeight: 38
                radius: 12
                color: theme.accent
                Label {
                    anchors.centerIn: parent
                    text: "C"
                    color: "white"
                    font.pixelSize: 22
                    font.bold: true
                }
            }

            ColumnLayout {
                spacing: 0
                Layout.preferredWidth: window.width < 1100 ? 0 : 245
                visible: window.width >= 1100
                Label {
                    text: qsTr("CachyOS Software Catalog")
                    color: theme.text
                    font.pixelSize: 16
                    font.bold: true
                }
                Label {
                    text: controller.statusMessage.length > 0 ? controller.statusMessage : qsTr("Native packages and Flatpaks")
                    color: theme.textMuted
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            TextField {
                id: globalSearch
                Layout.fillWidth: true
                Layout.maximumWidth: 620
                placeholderText: controller.page === "home" ? qsTr("Search curated applications") : qsTr("Search packages, descriptions and refs")
                leftPadding: 42
                rightPadding: 38
                onTextChanged: window.applySearch()
                background: Rectangle {
                    radius: 12
                    color: theme.surfaceMuted
                    border.color: globalSearch.activeFocus ? theme.accent : theme.border
                    border.width: globalSearch.activeFocus ? 2 : 1
                }
                Label {
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    text: "?"
                    color: theme.textMuted
                    font.pixelSize: 16
                }
                Label {
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Ctrl+K"
                    color: theme.textMuted
                    font.pixelSize: 10
                    visible: !globalSearch.activeFocus && globalSearch.text.length === 0
                }
            }

            Item { Layout.fillWidth: true }

            ToolButton {
                text: qsTr("Console")
                icon.name: "utilities-terminal"
                onClicked: window.consoleOpen = !window.consoleOpen
            }
            ToolButton {
                text: qsTr("Settings")
                icon.name: "settings-configure"
                onClicked: controller.page = "settings"
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: window.width < 1100 ? 72 : 224
            Layout.fillHeight: true
            color: theme.surface
            border.color: theme.border
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: window.width < 1100 ? 8 : 14
                spacing: 8

                Label {
                    text: qsTr("Explore")
                    color: theme.textMuted
                    font.pixelSize: 11
                    font.bold: true
                    leftPadding: 10
                    topPadding: 12
                    bottomPadding: 4
                    visible: window.width >= 1100
                }
                Button {
                    Layout.fillWidth: true
                    text: window.width < 1100 ? "H" : qsTr("Home")
                    icon.name: "go-home"
                    highlighted: controller.page === "home"
                    onClicked: controller.page = "home"
                    ToolTip.visible: window.width < 1100 && hovered
                    ToolTip.text: qsTr("Home")
                }
                Button {
                    Layout.fillWidth: true
                    text: window.width < 1100 ? "N" : qsTr("Native repositories")
                    icon.name: "package-x-generic"
                    highlighted: controller.page === "native"
                    onClicked: controller.page = "native"
                    ToolTip.visible: window.width < 1100 && hovered
                    ToolTip.text: qsTr("Native repositories")
                }
                Button {
                    Layout.fillWidth: true
                    text: window.width < 1100 ? "F" : qsTr("Flatpak")
                    icon.name: "application-x-flatpak"
                    highlighted: controller.page === "flatpak"
                    visible: controller.showFlatpak
                    onClicked: {
                        controller.page = "flatpak"
                        controller.loadFlatpaks()
                    }
                    ToolTip.visible: window.width < 1100 && hovered
                    ToolTip.text: qsTr("Flatpak")
                }

                Item { Layout.fillHeight: true }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: theme.border
                }
                Button {
                    Layout.fillWidth: true
                    text: window.width < 1100 ? "S" : qsTr("Settings")
                    icon.name: "settings-configure"
                    highlighted: controller.page === "settings"
                    onClicked: controller.page = "settings"
                    ToolTip.visible: window.width < 1100 && hovered
                    ToolTip.text: qsTr("Settings")
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("CachyOS Package Installer")
                    color: theme.textMuted
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                    leftPadding: 10
                    bottomPadding: 8
                    visible: window.width >= 1100
                }
            }
        }

        StackLayout {
            id: pages
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: controller.page === "native" ? 1 : controller.page === "flatpak" ? 2 : controller.page === "settings" ? 3 : 0

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 28
                    spacing: 22

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 20
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Label {
                                text: qsTr("Find your next favorite app")
                                color: theme.text
                                font.pixelSize: 29
                                font.bold: true
                            }
                            Label {
                                text: qsTr("Curated software from the CachyOS repositories, with native package safety built in.")
                                color: theme.textMuted
                                font.pixelSize: 13
                            }
                        }
                        Button {
                            text: qsTr("Refresh")
                            icon.name: "view-refresh"
                            enabled: !controller.loading && !controller.operationRunning
                            onClicked: controller.refreshRepositories()
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        Repeater {
                            model: ["All", "Audio", "Browsers", "Development", "Graphics", "Games", "Multimedia", "Office", "Video"]
                            delegate: Button {
                                text: qsTr(modelData)
                                flat: true
                                highlighted: modelData === "All" ? controller.popularModel.search.length === 0 : controller.popularModel.search === modelData
                                onClicked: controller.popularModel.search = modelData === "All" ? "" : modelData
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 18
                        color: theme.surface
                        border.color: theme.border
                        border.width: 1

                        GridView {
                            id: popularGrid
                            anchors.fill: parent
                            anchors.margins: 16
                            clip: true
                            cellWidth: Math.max(260, Math.floor(width / Math.max(1, Math.floor(width / 290))))
                            cellHeight: 190
                            model: controller.popularModel
                            delegate: AppCard {
                                width: popularGrid.cellWidth - 12
                                height: popularGrid.cellHeight - 12
                                itemName: name
                                itemSummary: summary
                                itemCategory: category
                                itemStatus: status
                                itemVersion: version
                                itemIcon: iconName
                                itemIconPath: iconPath
                                itemIconUrl: iconUrl
                                itemSelected: selected
                                itemInstalled: installed
                                itemUpgradable: upgradable
                                textColor: theme.text
                                mutedColor: theme.textMuted
                                surfaceColor: theme.surfaceRaised
                                borderColor: theme.border
                                accentColor: theme.accent
                                onToggleRequested: controller.popularModel.toggle(index)
                                onDetailsRequested: window.openDetails(name, summary, description, version, status, source, repo, targets.join(" "), homepage, license, iconName, screenshotUrl)
                            }
                            ScrollBar.vertical: ScrollBar { }
                        }

                        BusyIndicator {
                            anchors.centerIn: parent
                            running: controller.loading && controller.popularModel.totalCount === 0
                            visible: running
                        }
                        Label {
                            anchors.centerIn: parent
                            text: controller.fatalError.length > 0 ? controller.fatalError : qsTr("No curated applications match your search")
                            color: theme.textMuted
                            visible: !controller.loading && controller.popularModel.count === 0
                            horizontalAlignment: Text.AlignHCenter
                            width: Math.min(parent.width - 48, 430)
                            wrapMode: Text.WordWrap
                        }
                    }

                    ActionBar {
                        Layout.fillWidth: true
                        visible: controller.popularModel.selectedCount > 0
                        selectedCount: controller.popularModel.selectedCount
                        textColor: theme.text
                        mutedColor: theme.textMuted
                        surfaceColor: theme.accentSoft
                        accentColor: theme.accent
                        installText: qsTr("Install selected")
                        onInstallClicked: controller.installSelected("curated")
                        onRemoveClicked: controller.removeSelected("curated")
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 28
                    spacing: 16
                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Label { text: qsTr("Native repositories"); color: theme.text; font.pixelSize: 27; font.bold: true }
                            Label { text: qsTr("Browse the configured pacman databases without synchronizing them."); color: theme.textMuted; font.pixelSize: 13 }
                        }
                        Button { text: qsTr("Refresh list"); icon.name: "view-refresh"; enabled: !controller.loading && !controller.operationRunning; onClicked: controller.refreshRepositories() }
                        Button { text: qsTr("Upgrade selected"); icon.name: "system-upgrade"; enabled: controller.ready && !controller.loading && controller.repositoryModel.selectedCount > 0 && !controller.operationRunning; onClicked: controller.upgradeSelected() }
                        Button { text: qsTr("Upgrade all"); icon.name: "system-upgrade"; enabled: controller.ready && !controller.operationRunning; onClicked: controller.upgradeAll() }
                        Button { text: qsTr("Orphans"); icon.name: "edit-delete"; enabled: controller.ready && !controller.loading && !controller.operationRunning; onClicked: orphanDialog.open() }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        ComboBox {
                            id: repoFilter
                            model: [qsTr("All packages"), qsTr("Installed"), qsTr("Upgradable"), qsTr("Not installed")]
                            onActivated: controller.repositoryModel.statusFilter = ["all", "installed", "upgradable", "available"][currentIndex]
                        }
                        CheckBox {
                            text: qsTr("Hide library and developer packages")
                            checked: controller.hideLibraries
                            onClicked: controller.hideLibraries = checked
                        }
                        Label { Layout.fillWidth: true; text: qsTr("%1 visible of %2 packages").arg(controller.repositoryModel.count).arg(controller.repositoryModel.totalCount); color: theme.textMuted; horizontalAlignment: Text.AlignRight }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 18
                        color: theme.surface
                        border.color: theme.border
                        border.width: 1
                        ListView {
                            id: repoList
                            anchors.fill: parent
                            anchors.margins: 10
                            clip: true
                            model: controller.repositoryModel
                            delegate: CatalogRow {
                                width: repoList.width
                                itemName: name
                                itemSummary: summary
                                itemStatus: status
                                itemVersion: version
                                itemInstalledVersion: installedVersion
                                itemRepo: repo
                                itemIcon: iconName
                                itemIconPath: iconPath
                                itemIconUrl: iconUrl
                                itemSize: sizeText
                                itemSelected: selected
                                itemInstalled: installed
                                itemUpgradable: upgradable
                                textColor: theme.text
                                mutedColor: theme.textMuted
                                surfaceColor: theme.surfaceRaised
                                borderColor: theme.border
                                accentColor: theme.accent
                                onToggleRequested: controller.repositoryModel.toggle(index)
                                onDetailsRequested: window.openDetails(name, summary, description, version, status, source, repo, targets.join(" "), homepage, license, iconName, screenshotUrl)
                            }
                            ScrollBar.vertical: ScrollBar { }
                        }
                        BusyIndicator { anchors.centerIn: parent; running: controller.loading; visible: running }
                    }
                    ActionBar {
                        Layout.fillWidth: true
                        visible: controller.repositoryModel.selectedCount > 0
                        selectedCount: controller.repositoryModel.selectedCount
                        textColor: theme.text
                        mutedColor: theme.textMuted
                        surfaceColor: theme.accentSoft
                        accentColor: theme.accent
                        installText: qsTr("Install / upgrade")
                        onInstallClicked: controller.installSelected("native")
                        onRemoveClicked: controller.removeSelected("native")
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 28
                    spacing: 16
                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Label { text: qsTr("Flatpak"); color: theme.text; font.pixelSize: 27; font.bold: true }
                            Label { text: qsTr("Discover applications and runtimes from a selected remote and scope."); color: theme.textMuted; font.pixelSize: 13 }
                        }
                        Button { text: qsTr("Manage remotes"); icon.name: "network-server"; enabled: controller.flatpakAvailable && !controller.operationRunning; onClicked: remoteDialog.open() }
                        Button { text: qsTr("Flatpakref"); icon.name: "document-open"; enabled: controller.flatpakAvailable && !controller.operationRunning; onClicked: flatpakrefDialog.open() }
                        Button { text: qsTr("Refresh metadata"); icon.name: "view-refresh"; enabled: controller.flatpakAvailable && !controller.operationRunning; onClicked: controller.refreshFlatpakMetadata() }
                        Button { text: qsTr("Refresh"); icon.name: "view-refresh"; enabled: controller.flatpakAvailable && !controller.operationRunning; onClicked: controller.loadFlatpaks() }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        ComboBox {
                            id: scopeCombo
                            model: [qsTr("System"), qsTr("User")]
                            enabled: !controller.operationRunning && !controller.flatpakLoading
                            currentIndex: controller.flatpakScope === "user" ? 1 : 0
                            onActivated: controller.flatpakScope = currentIndex === 1 ? "user" : "system"
                        }
                        ComboBox {
                            id: remoteCombo
                            Layout.preferredWidth: 190
                            enabled: !controller.operationRunning && !controller.flatpakLoading
                            model: controller.remoteNames
                            currentIndex: Math.max(0, controller.remoteNames.indexOf(controller.selectedRemote))
                            onActivated: controller.selectedRemote = currentText
                        }
                        Label { Layout.fillWidth: true; text: controller.selectedRemoteUrl; color: theme.textMuted; elide: Text.ElideMiddle; visible: text.length > 0 }
                        ComboBox {
                            id: flatpakFilter
                            enabled: !controller.operationRunning && !controller.flatpakLoading
                            model: [qsTr("All"), qsTr("Apps"), qsTr("Runtimes"), qsTr("Updates"), qsTr("Installed"), qsTr("Not installed")]
                            onActivated: {
                                const values = ["all", "apps", "runtimes", "upgradable", "installed", "available"]
                                controller.flatpakModel.statusFilter = values[currentIndex]
                            }
                        }
                        Button { text: qsTr("Update all"); icon.name: "system-software-update"; enabled: controller.flatpakAvailable && !controller.operationRunning; onClicked: controller.updateFlatpaks() }
                        Button { text: qsTr("Unused runtimes"); icon.name: "edit-delete"; enabled: controller.flatpakAvailable && !controller.operationRunning; onClicked: controller.removeUnusedFlatpaks() }
                        Item { Layout.fillWidth: true }
                        Label { text: qsTr("%1 entries").arg(controller.flatpakModel.count); color: theme.textMuted }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 18
                        color: theme.surface
                        border.color: theme.border
                        border.width: 1
                        ListView {
                            id: flatpakList
                            anchors.fill: parent
                            anchors.margins: 10
                            clip: true
                            model: controller.flatpakModel
                            delegate: CatalogRow {
                                width: flatpakList.width
                                itemName: name
                                itemSummary: summary
                                itemStatus: status
                                itemVersion: version
                                itemRepo: runtime ? qsTr("Runtime") : controller.selectedRemote
                                itemIcon: iconName
                                itemIconPath: iconPath
                                itemIconUrl: iconUrl
                                itemSize: sizeText
                                itemSelected: selected
                                itemInstalled: installed
                                itemUpgradable: upgradable
                                itemRuntime: runtime
                                textColor: theme.text
                                mutedColor: theme.textMuted
                                surfaceColor: theme.surfaceRaised
                                borderColor: theme.border
                                accentColor: theme.accent
                                onToggleRequested: controller.flatpakModel.toggle(index)
                                onDetailsRequested: window.openDetails(name, summary, description, version, status, source, controller.selectedRemote, ref, homepage, license, iconName, screenshotUrl)
                            }
                            ScrollBar.vertical: ScrollBar { }
                        }
                        BusyIndicator { anchors.centerIn: parent; running: controller.flatpakLoading; visible: running }
                        Column {
                            anchors.centerIn: parent
                            spacing: 12
                            visible: !controller.flatpakLoading && (!controller.flatpakAvailable || controller.flatpakModel.count === 0)
                            width: Math.min(parent.width - 48, 420)
                            Label { anchors.horizontalCenter: parent.horizontalCenter; text: controller.flatpakAvailable ? qsTr("No Flatpak entries are available for this scope and remote.") : qsTr("Flatpak is not installed on this system."); color: theme.textMuted; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; width: parent.width }
                            Button { anchors.horizontalCenter: parent.horizontalCenter; text: controller.flatpakAvailable ? qsTr("Manage remotes") : qsTr("Install Flatpak support"); onClicked: controller.flatpakAvailable ? remoteDialog.open() : controller.installFlatpakSupport() }
                        }
                    }
                    ActionBar {
                        Layout.fillWidth: true
                        visible: controller.flatpakModel.selectedCount > 0
                        selectedCount: controller.flatpakModel.selectedCount
                        textColor: theme.text
                        mutedColor: theme.textMuted
                        surfaceColor: theme.accentSoft
                        accentColor: theme.accent
                        installText: qsTr("Install selected")
                        showUpdate: true
                        updateText: qsTr("Update selected")
                        onInstallClicked: controller.installSelected("flatpak")
                        onUpdateClicked: controller.updateSelectedFlatpaks()
                        onRemoveClicked: controller.removeSelected("flatpak")
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 36
                    spacing: 20
                    Label { text: qsTr("Settings"); color: theme.text; font.pixelSize: 29; font.bold: true }
                    Label { text: qsTr("Tune the catalog without changing pacman or Flatpak configuration implicitly."); color: theme.textMuted; font.pixelSize: 13 }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 720
                        Layout.preferredHeight: 260
                        radius: 18
                        color: theme.surface
                        border.color: theme.border
                        border.width: 1
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 24
                            spacing: 18
                            RowLayout {
                                Layout.fillWidth: true
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Label { text: qsTr("Appearance"); color: theme.text; font.bold: true; font.pixelSize: 15 }
                                    Label { text: qsTr("Choose how the catalog follows your desktop."); color: theme.textMuted; font.pixelSize: 12 }
                                }
                                ComboBox {
                                    model: [qsTr("System"), qsTr("Light"), qsTr("Dark")]
                                    currentIndex: controller.theme === "Dark" ? 2 : controller.theme === "Light" ? 1 : 0
                                    onActivated: controller.theme = ["System", "Light", "Dark"][currentIndex]
                                }
                            }
                            CheckBox { text: qsTr("Show Flatpak catalog"); checked: controller.showFlatpak; onClicked: controller.showFlatpak = checked }
                            Label { Layout.fillWidth: true; text: qsTr("Native package changes always go through pacman and Polkit. Preview and console output remain available before and during every transaction."); color: theme.textMuted; font.pixelSize: 12; wrapMode: Text.WordWrap }
                            RowLayout {
                                Layout.fillWidth: true
                                Item { Layout.fillWidth: true }
                                Button { text: qsTr("Help"); icon.name: "help-browser"; onClicked: controller.openHelp() }
                                Button { text: qsTr("About"); icon.name: "help-about"; onClicked: aboutDialog.open() }
                            }
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }
    }

    Rectangle {
        id: systemErrorOverlay
        anchors.fill: parent
        z: 100
        visible: !controller.systemReady && !controller.loading
        color: theme.background
        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 520)
            spacing: 14
            Label { Layout.fillWidth: true; text: qsTr("Package database unavailable"); color: theme.text; font.pixelSize: 24; font.bold: true; horizontalAlignment: Text.AlignHCenter }
            Label { Layout.fillWidth: true; text: controller.fatalError; color: theme.textMuted; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter }
            Button { Layout.alignment: Qt.AlignHCenter; text: qsTr("Retry"); highlighted: true; onClicked: controller.retry() }
        }
    }

    component ActionBar: Rectangle {
        property int selectedCount: 0
        property string installText: qsTr("Install selected")
        property string updateText: qsTr("Update")
        property bool showUpdate: false
        property color textColor: "#172033"
        property color mutedColor: "#667085"
        property color surfaceColor: "#e6efff"
        property color accentColor: "#2368d1"
        signal installClicked()
        signal updateClicked()
        signal removeClicked()
        implicitHeight: 58
        radius: 14
        color: surfaceColor
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 12
            spacing: 12
            Label { text: qsTr("%1 selected").arg(parent.parent.selectedCount); color: parent.parent.textColor; font.bold: true }
            Item { Layout.fillWidth: true }
            Button { text: parent.parent.installText; icon.name: "list-add"; onClicked: parent.parent.installClicked() }
            Button { text: parent.parent.updateText; icon.name: "view-refresh"; visible: parent.parent.showUpdate; onClicked: parent.parent.updateClicked() }
            Button { text: qsTr("Remove"); icon.name: "list-remove"; flat: true; onClicked: parent.parent.removeClicked() }
        }
    }

    Dialog {
        id: resultDialog
        modal: true
        width: 500
        title: controller.resultSuccess ? qsTr("Operation complete") : qsTr("Operation failed")
        visible: controller.resultVisible
        contentItem: ColumnLayout {
            spacing: 12
            Label { Layout.fillWidth: true; text: controller.resultMessage; color: controller.resultSuccess ? theme.success : theme.danger; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button { text: qsTr("Open console"); flat: true; onClicked: { controller.dismissResult(); window.consoleOpen = true } }
                Button { text: qsTr("Close"); highlighted: true; onClicked: controller.dismissResult() }
            }
        }
    }

    Dialog {
        id: aboutDialog
        modal: true
        width: 520
        title: qsTr("About CachyOS Software Catalog")
        standardButtons: Dialog.Close
        contentItem: ColumnLayout {
            spacing: 12
            Label { Layout.fillWidth: true; text: qsTr("CachyOS Software Catalog 2.0.0"); color: theme.text; font.pixelSize: 20; font.bold: true }
            Label { Layout.fillWidth: true; text: qsTr("A modern catalog for native CachyOS packages and Flatpak applications. Native changes are previewed with libalpm and executed by pacman through Polkit."); color: theme.textMuted; wrapMode: Text.WordWrap }
            Label { Layout.fillWidth: true; text: qsTr("Licensed under the GNU General Public License, version 3 or later."); color: theme.textMuted; wrapMode: Text.WordWrap }
            Button { Layout.alignment: Qt.AlignRight; text: qsTr("Project website"); onClicked: Qt.openUrlExternally("https://github.com/CachyOS/packageinstaller") }
        }
    }

    Dialog {
        id: orphanDialog
        modal: true
        width: 540
        title: qsTr("Remove orphan packages")
        standardButtons: Dialog.Cancel
        contentItem: ColumnLayout {
            spacing: 12
            Label { Layout.fillWidth: true; text: qsTr("Orphan packages may still be useful to another workflow. Review the pacman preview carefully before confirming removal."); color: theme.text; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button { text: qsTr("Continue"); highlighted: true; onClicked: { orphanDialog.close(); controller.removeOrphans() } }
            }
        }
    }

    Dialog {
        id: flatpakWarningDialog
        modal: true
        width: 520
        title: qsTr("About Flatpak applications")
        visible: controller.flatpakWarningVisible
        contentItem: ColumnLayout {
            spacing: 12
            Label { Layout.fillWidth: true; text: qsTr("Flatpak applications are provided by their upstream projects and remote maintainers. CachyOS provides this catalog for convenience and cannot guarantee the behavior or content of individual applications."); color: theme.text; wrapMode: Text.WordWrap }
            CheckBox { id: disableFlatpakWarning; text: qsTr("Do not show this message again") }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button { text: qsTr("Continue"); highlighted: true; onClicked: controller.dismissFlatpakWarning(disableFlatpakWarning.checked) }
            }
        }
    }

    Dialog {
        id: detailDialog
        modal: true
        width: Math.min(window.width - 80, 720)
        title: qsTr("Package details")
        standardButtons: Dialog.Close
        contentItem: ColumnLayout {
            spacing: 14
            RowLayout {
                Layout.fillWidth: true
                Rectangle {
                    Layout.preferredWidth: 62
                    Layout.preferredHeight: 62
                    radius: 16
                    color: theme.accentSoft
                    ToolButton { anchors.centerIn: parent; icon.name: detailDialog.selectedDetailsIcon; icon.width: 42; icon.height: 42; visible: detailDialog.selectedDetailsIcon.length > 0; enabled: false }
                    Label { anchors.centerIn: parent; text: detailDialog.selectedDetailsName.length > 0 ? detailDialog.selectedDetailsName.charAt(0).toUpperCase() : "?"; color: theme.accent; font.pixelSize: 26; font.bold: true; visible: detailDialog.selectedDetailsIcon.length === 0 }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Label { text: detailDialog.selectedDetailsName; color: theme.text; font.pixelSize: 20; font.bold: true; elide: Text.ElideRight }
                    Label { text: detailDialog.selectedDetailsSummary; color: theme.textMuted; font.pixelSize: 13; wrapMode: Text.WordWrap }
                }
            }
            Label { Layout.fillWidth: true; text: detailDialog.selectedDetailsDescription.length > 0 ? detailDialog.selectedDetailsDescription : qsTr("No extended description is available."); color: theme.text; font.pixelSize: 13; wrapMode: Text.WordWrap; maximumLineCount: 8; elide: Text.ElideRight }
            Image { Layout.fillWidth: true; Layout.preferredHeight: 180; source: detailDialog.selectedDetailsScreenshot; fillMode: Image.PreserveAspectFit; asynchronous: true; visible: source.length > 0 }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Homepage"); color: theme.textMuted; visible: detailDialog.selectedDetailsHomepage.length > 0 }
                Button { text: detailDialog.selectedDetailsHomepage; flat: true; visible: detailDialog.selectedDetailsHomepage.length > 0; onClicked: Qt.openUrlExternally(detailDialog.selectedDetailsHomepage) }
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                Label { text: qsTr("Status"); color: theme.textMuted }
                Label { text: detailDialog.selectedDetailsStatus; color: theme.text; font.bold: true }
                Label { text: qsTr("Version"); color: theme.textMuted }
                Label { text: detailDialog.selectedDetailsVersion; color: theme.text }
                Label { text: qsTr("Source"); color: theme.textMuted }
                Label { text: detailDialog.selectedDetailsSource + (detailDialog.selectedDetailsRepo.length > 0 ? " / " + detailDialog.selectedDetailsRepo : ""); color: theme.text }
                Label { text: qsTr("Targets"); color: theme.textMuted }
                Label { Layout.fillWidth: true; text: detailDialog.selectedDetailsTargets; color: theme.text; wrapMode: Text.WordWrap }
                Label { text: qsTr("License"); color: theme.textMuted; visible: detailDialog.selectedDetailsLicense.length > 0 }
                Label { text: detailDialog.selectedDetailsLicense; color: theme.text; visible: detailDialog.selectedDetailsLicense.length > 0 }
            }
        }
        property string selectedDetailsName: window.selectedDetailsName
        property string selectedDetailsSummary: window.selectedDetailsSummary
        property string selectedDetailsDescription: window.selectedDetailsDescription
        property string selectedDetailsVersion: window.selectedDetailsVersion
        property string selectedDetailsStatus: window.selectedDetailsStatus
        property string selectedDetailsSource: window.selectedDetailsSource
        property string selectedDetailsRepo: window.selectedDetailsRepo
        property string selectedDetailsTargets: window.selectedDetailsTargets
        property string selectedDetailsLicense: window.selectedDetailsLicense
        property string selectedDetailsScreenshot: window.selectedDetailsScreenshot
        property string selectedDetailsHomepage: window.selectedDetailsHomepage
        property string selectedDetailsIcon: window.selectedDetailsIcon
    }

    Dialog {
        id: transactionDialog
        modal: true
        width: Math.min(window.width - 100, 760)
        title: qsTr("Review package changes")
        visible: controller.transactionVisible
        onClosed: controller.dismissTransaction()
        contentItem: ColumnLayout {
            spacing: 12
            Label { Layout.fillWidth: true; text: controller.transactionPreview.summary || qsTr("Review the selected changes before continuing."); color: theme.text; wrapMode: Text.WordWrap }
            Label { Layout.fillWidth: true; text: controller.transactionPreview.error || ""; color: theme.danger; wrapMode: Text.WordWrap; visible: text.length > 0 }
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 190
                GroupBox {
                    title: qsTr("Install / update")
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ScrollView { anchors.fill: parent; clip: true; Column { width: parent.width; Repeater { model: controller.transactionPreview.additions || []; delegate: Label { text: "+ " + modelData; color: theme.success; width: parent.width; wrapMode: Text.WordWrap; padding: 3 } } } }
                }
                GroupBox {
                    title: qsTr("Remove")
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ScrollView { anchors.fill: parent; clip: true; Column { width: parent.width; Repeater { model: controller.transactionPreview.removals || []; delegate: Label { text: "- " + modelData; color: theme.danger; width: parent.width; wrapMode: Text.WordWrap; padding: 3 } } } }
                }
            }
            Label { Layout.fillWidth: true; text: (controller.transactionPreview.conflicts || []).join("\n"); color: theme.warning; wrapMode: Text.WordWrap; visible: text.length > 0 }
            RowLayout {
                Layout.fillWidth: true
                Button { text: qsTr("Open console"); flat: true; onClicked: window.consoleOpen = true }
                Item { Layout.fillWidth: true }
                Button { text: qsTr("Cancel"); onClicked: controller.dismissTransaction() }
                Button { text: qsTr("Confirm"); highlighted: true; enabled: controller.transactionPreview.success === true; onClicked: controller.confirmTransaction() }
            }
        }
    }

    Dialog {
        id: flatpakrefDialog
        modal: true
        title: qsTr("Install Flatpakref")
        standardButtons: Dialog.Cancel
        contentItem: ColumnLayout {
            spacing: 12
            Label { Layout.fillWidth: true; text: qsTr("Use a local .flatpakref file or an HTTPS URL. The selected Flatpak scope will be used."); color: theme.textMuted; wrapMode: Text.WordWrap }
            TextField { id: flatpakrefInput; Layout.fillWidth: true; placeholderText: qsTr("https://example.org/app.flatpakref or /path/app.flatpakref") }
            Button { Layout.alignment: Qt.AlignRight; text: qsTr("Continue"); highlighted: true; enabled: flatpakrefInput.text.length > 0; onClicked: { controller.installFlatpakref(flatpakrefInput.text); flatpakrefInput.clear(); flatpakrefDialog.close() } }
        }
    }

    Dialog {
        id: remoteDialog
        modal: true
        title: qsTr("Manage Flatpak remotes")
        standardButtons: Dialog.Close
        contentItem: ColumnLayout {
            spacing: 12
            Label { Layout.fillWidth: true; text: qsTr("Remote changes are scoped to the current Flatpak installation."); color: theme.textMuted; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true
                TextField { id: remoteName; Layout.preferredWidth: 150; placeholderText: qsTr("Name") }
                TextField { id: remoteUrl; Layout.fillWidth: true; placeholderText: qsTr("https://remote.example/repo.flatpakrepo") }
                Button { text: qsTr("Add"); onClicked: { controller.addRemote(remoteName.text, remoteUrl.text); remoteName.clear(); remoteUrl.clear() } }
            }
            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: 180
                model: controller.remoteNames
                delegate: RowLayout {
                    width: parent.width
                    Label { Layout.fillWidth: true; text: modelData; color: theme.text }
                    Button { text: qsTr("Remove"); flat: true; enabled: modelData !== "flathub"; onClicked: controller.removeRemote(modelData) }
                }
            }
        }
    }

    Drawer {
        id: consoleDrawer
        edge: Qt.BottomEdge
        width: window.width
        height: Math.min(window.height * 0.44, 360)
        visible: window.consoleOpen || controller.operationRunning
        modal: false
        background: Rectangle { color: theme.surface; border.color: theme.border; border.width: 1 }
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Label { text: controller.operationRunning ? controller.operationTitle : qsTr("Console output"); color: theme.text; font.bold: true; font.pixelSize: 14 }
                Item { Layout.fillWidth: true }
                Button { text: qsTr("Cancel"); visible: controller.operationRunning; onClicked: controller.cancelTransaction() }
                Button { text: qsTr("Close"); onClicked: window.consoleOpen = false }
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                TextArea { id: consoleArea; width: parent.width; text: controller.consoleText; readOnly: true; wrapMode: TextEdit.Wrap; color: theme.text; background: Rectangle { color: theme.background; radius: 10; border.color: theme.border } }
            }
            RowLayout {
                Layout.fillWidth: true
                TextField { id: consoleInput; Layout.fillWidth: true; placeholderText: qsTr("Send a response to the running process..."); enabled: controller.operationRunning; onAccepted: { controller.sendConsoleInput(text); clear() } }
                Button { text: qsTr("Send"); enabled: controller.operationRunning; onClicked: { controller.sendConsoleInput(consoleInput.text); consoleInput.clear() } }
            }
        }
    }

    Dialog {
        id: quitDialog
        modal: true
        width: 460
        title: qsTr("Operation in progress")
        standardButtons: Dialog.Cancel
        contentItem: ColumnLayout {
            spacing: 12
            Label { Layout.fillWidth: true; text: qsTr("The package process is still running. Cancelling it may leave a transaction incomplete. Quit anyway?"); color: theme.text; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button { text: qsTr("Quit"); onClicked: { controller.cancelTransaction(); Qt.quit() } }
            }
        }
    }

    Shortcut { sequence: "Ctrl+K"; onActivated: globalSearch.forceActiveFocus() }
    Shortcut { sequence: "Escape"; onActivated: controller.transactionVisible ? controller.dismissTransaction() : window.consoleOpen = false }

    Connections {
        target: controller
        function onPageChanged() {
            if (controller.page === "flatpak") {
                controller.loadFlatpaks()
            }
        }
    }

    Component.onCompleted: {
        if (controller.page === "flatpak") {
            controller.loadFlatpaks()
        }
    }
}
