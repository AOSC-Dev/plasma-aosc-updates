/***************************************************************************
 *   Copyright (C) 2024 by Amo Updates contributors                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.extras as PlasmaExtras
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami
import org.kde.plasma.amo 1.0

Item {
    id: fullRepresentation

    property bool anySelected: false
    property bool allSelected: false
    property bool populatePreSelected: true

    width: Kirigami.Units.gridUnit * 20
    height: Kirigami.Units.gridUnit * 20

    Binding {
        target: timestampLabel
        property: "text"
        value: AmoUpdates.timestamp
    }

    Connections {
        target: AmoUpdates
        function onUpdatesChanged() { populateModel() }
        function onUpdatesInstalled() { plasmoid.expanded = false }
    }

    Component.onCompleted: populateModel()

    ListModel {
        id: updatesModel
    }

    ColumnLayout {
        id: statusbar

        anchors.fill: parent

        spacing: Kirigami.Units.smallSpacing

        PlasmaExtras.Heading {
            Layout.fillWidth: true
            level: 4
            wrapMode: Text.WordWrap
            text: AmoUpdates.message
        }

        PlasmaComponents3.Label {
            visible: AmoUpdates.isActive || AmoUpdates.count === 0
            font.pointSize: Kirigami.Theme.smallFont.pointSize;
            opacity: 0.6;
            text: {
                if (AmoUpdates.isActive)
                    return AmoUpdates.statusMessage
                else
                    return ""
            }
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        PlasmaComponents3.Label {
            id: timestampLabel
            Layout.fillWidth: true
            visible: !AmoUpdates.isActive
            wrapMode: Text.WordWrap
            font.italic: true
            font.pointSize: Kirigami.Theme.smallFont.pointSize;
            opacity: 0.6;
            text: AmoUpdates.timestamp
        }

        PlasmaComponents3.ProgressBar {
            Layout.fillWidth: true
            visible: AmoUpdates.isActive
            from: 0
            to: 101 // BUG workaround a bug in ProgressBar! if the value is > max, it's set to max and never changes below
            value: AmoUpdates.percentage
            indeterminate: AmoUpdates.percentage > 100
        }

        PlasmaComponents3.ScrollView {
            id: listViewScrollArea

            Layout.fillWidth: true
            Layout.fillHeight: true

            visible: AmoUpdates.count && !AmoUpdates.isActive

            ListView {
                id: updatesView

                reuseItems: true
                clip: true
                model: updatesModel
                anchors.fill: parent
                currentIndex: -1
                property int lastIndex: -1
                boundsBehavior: Flickable.StopAtBounds
                delegate: PackageDelegate {
                    onClicked: {
                        if (updatesView.lastIndex == updatesView.currentIndex) {
                            // Unselect as current
                            updatesView.currentIndex = -1
                        } else {
                            // Expand
                            updatesView.currentIndex = index
                        }
                        updatesView.lastIndex = updatesView.currentIndex
                    }
                    onCheckStateChanged: updateSelectionState();
                }
            }
        }

        // Container for other items that can be shown when the main scroll
        // view is not visible
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            visible: !listViewScrollArea.visible

            PlasmaComponents3.BusyIndicator {
                running: AmoUpdates.isActive && AmoUpdates.count == 0
                visible: running
                anchors.centerIn: parent
            }

            PlasmaExtras.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - (Kirigami.Units.largeSpacing * 4)

                visible: AmoUpdates.count === 0 && !AmoUpdates.isActive

                text: AmoUpdates.lastCheckSuccessful ? i18n("No updates available") : ""

                helpfulAction: QQC2.Action {
                    icon.name: "view-refresh"
                    text: i18n("Check for Updates")
                    onTriggered: {
                        AmoUpdates.checkUpdates(true /* manual */) // circumvent the checks, the user knows what they're doing ;)
                    }
                }
            }
        }

        PlasmaComponents3.CheckBox {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing

            visible: AmoUpdates.count !== 0 && !AmoUpdates.isActive

            tristate: true

            checkState: fullRepresentation.allSelected ? Qt.Checked :
                        (fullRepresentation.anySelected ? Qt.PartiallyChecked
                                                        : Qt.Unchecked)

            text: i18n("Select all packages")

            onClicked: {
                populatePreSelected = !fullRepresentation.anySelected;
                populateModel();
            }
        }

        PlasmaComponents3.Button {
            visible: AmoUpdates.count !== 0 && !AmoUpdates.isActive
            icon.name: "install"
            enabled: fullRepresentation.anySelected
            Layout.alignment: Qt.AlignHCenter
            text: i18n("Install Updates")
            onClicked: AmoUpdates.installUpdates(selectedPackages())

            PlasmaComponents3.ToolTip {
                text: i18n("Performs the software update")
            }
        }
    }

    function updateSelectionState() {
        var anySelected = false;
        var allSelected = true;
        for (var i = 0; i < updatesModel.count; i++) {
            var pkg = updatesModel.get(i)
            if (pkg.selected)
                anySelected = true;
            else
                allSelected = false;

            if (anySelected && !allSelected)
                break; // Can't change anymore
        }
        fullRepresentation.anySelected = anySelected;
        fullRepresentation.allSelected = allSelected;
    }

    function selectedPackages() {
        var result = []
        for (var i = 0; i < updatesModel.count; i++) {
            var pkg = updatesModel.get(i)
            if (pkg.selected) {
                result.push(pkg.id)
            }
        }
        return result
    }

    function populateModel() {
        updatesModel.clear()
        var packages = AmoUpdates.packages
        for (var i = 0; i < packages.length; i++) {
            var id = packages[i]
            var desc = AmoUpdates.packageDescription(id)
            updatesModel.append({"selected": populatePreSelected, "id": id, "name": AmoUpdates.packageName(id), "desc": desc, "version": AmoUpdates.packageVersion(id)})
        }
        updateSelectionState();
    }
}
