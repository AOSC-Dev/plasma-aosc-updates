/***************************************************************************
 *   Copyright (C) 2024 by AOSC Updates contributors                       *
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
import io.aosc.plasmaaoscupdates 1.0

Item {
    id: fullRepresentation

    property bool checkDaily: false
    property bool checkWeekly: false
    property bool checkMonthly: false

    width: Kirigami.Units.gridUnit * 20
    height: Kirigami.Units.gridUnit * 20

    Connections {
        target: AmoUpdates
        function onUpdatesChanged() { populateModel() }
        function onUpdatesInstalled() { plasmoid.expanded = false }
    }

    Component.onCompleted: populateModel()

    ListModel {
        id: updatesModel
    }

    ListModel {
        id: topicUpdatesModel
    }

    ColumnLayout {
        id: statusbar

        anchors.fill: parent

        spacing: Kirigami.Units.smallSpacing

        PlasmaExtras.Heading {
            Layout.fillWidth: true
            level: 4
            wrapMode: Text.WordWrap
            text: AmoUpdates.isNetworkOnline ? AmoUpdates.message : i18n("Network is offline")
        }

        PlasmaComponents3.Label {
            visible: AmoUpdates.isActive || AmoUpdates.count === 0
            font.pointSize: Kirigami.Theme.smallFont.pointSize;
            opacity: 0.6;
            text: {
                if (AmoUpdates.isActive)
                    return AmoUpdates.statusMessage
                else if (!AmoUpdates.isNetworkOnline)
                    return ""
                else if (AmoUpdates.count === 0 && AmoUpdates.lastCheckSuccessful)
                    return i18n("Updates are automatically checked %1.", updateInterval())
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
            text: i18n("Last check: %1 ago", formatDuration(AmoUpdates.lastRefreshTimestamp()))
        }

        PlasmaComponents3.Label {
            Layout.fillWidth: true
            visible: !AmoUpdates.isActive && AmoUpdates.errorMessage !== ""
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.negativeTextColor
            text: i18n("Error: %1", AmoUpdates.errorMessage)
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: AmoUpdates.topicUpdateCount > 0 && !AmoUpdates.isActive
            type: AmoUpdates.hasSecurityUpdates
                ? Kirigami.MessageType.Warning
                : Kirigami.MessageType.Information
            text: AmoUpdates.hasSecurityUpdates
                ? i18n("Important security updates are available.")
                : i18n("Important updates are available.")
            actions: [
                Kirigami.Action {
                    icon.name: "documentinfo"
                    text: i18n("View Details")
                    onTriggered: topicDetailsDialog.open()
                }
            ]
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

            contentItem: ListView {
                id: updatesView

                reuseItems: true
                clip: true
                model: updatesModel
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

                text: AmoUpdates.lastCheckSuccessful
                    ? i18n("No updates available")
                    : (AmoUpdates.errorMessage !== ""
                        ? i18n("The update check failed")
                        : i18n("Update check has not completed"))

                helpfulAction: QQC2.Action {
                    icon.name: "view-refresh"
                    text: AmoUpdates.errorMessage !== "" ? i18n("Retry") : i18n("Check for Updates")
                    onTriggered: {
                        AmoUpdates.checkUpdates(true /* manual */) // circumvent the checks, the user knows what they're doing ;)
                    }
                }
            }
        }

        PlasmaComponents3.Label {
            visible: AmoUpdates.count !== 0 && !AmoUpdates.isActive
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            opacity: 0.6
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            text: i18n("Download size: %1 · Disk space: %2",
                       formatBytes(AmoUpdates.totalDownloadSize),
                       formatBytes(AmoUpdates.diskSizeDelta, true))
        }

        PlasmaComponents3.Button {
            visible: AmoUpdates.count !== 0 && !AmoUpdates.isActive
            icon.name: "install"
            Layout.alignment: Qt.AlignHCenter
            text: i18n("Install Updates")
            onClicked: installDialog.open()

            PlasmaComponents3.ToolTip {
                text: i18n("Performs the software update")
            }
        }
    }

    QQC2.Dialog {
        id: topicDetailsDialog

        title: i18n("Update Details")
        modal: true
        standardButtons: QQC2.Dialog.Close

        contentItem: PlasmaComponents3.ScrollView {
            implicitWidth: Kirigami.Units.gridUnit * 20
            implicitHeight: Math.min(topicUpdatesView.contentHeight,
                                     Kirigami.Units.gridUnit * 22)

            contentItem: ListView {
                id: topicUpdatesView

                clip: true
                spacing: Kirigami.Units.smallSpacing
                model: topicUpdatesModel
                boundsBehavior: Flickable.StopAtBounds

                delegate: Kirigami.AbstractCard {
                    required property string topicId
                    required property string displayName
                    required property string caution
                    required property bool isSecurity
                    required property int packageCount
                    required property string packages
                    required property string childTopics

                    width: ListView.view.width

                    contentItem: ColumnLayout {
                        spacing: Kirigami.Units.smallSpacing

                        PlasmaExtras.Heading {
                            Layout.fillWidth: true
                            level: 4
                            wrapMode: Text.WordWrap
                            text: displayName !== "" ? displayName : topicId
                        }

                        PlasmaComponents3.Label {
                            Layout.fillWidth: true
                            font.weight: Font.DemiBold
                            color: isSecurity
                                ? Kirigami.Theme.negativeTextColor
                                : Kirigami.Theme.highlightColor
                            text: isSecurity ? i18n("Security update") : i18n("Important update")
                        }

                        PlasmaComponents3.Label {
                            Layout.fillWidth: true
                            visible: caution !== ""
                            wrapMode: Text.WordWrap
                            text: caution
                        }

                        PlasmaComponents3.Label {
                            Layout.fillWidth: true
                            opacity: 0.6
                            text: i18np("Affects %1 package",
                                       "Affects %1 packages",
                                       packageCount)
                        }

                        PlasmaComponents3.Label {
                            Layout.fillWidth: true
                            visible: packages !== ""
                            wrapMode: Text.WrapAnywhere
                            opacity: 0.6
                            text: i18n("Affected packages: %1", packages)
                        }

                        PlasmaComponents3.Label {
                            Layout.fillWidth: true
                            visible: childTopics !== ""
                            wrapMode: Text.WrapAnywhere
                            opacity: 0.6
                            text: i18n("Included topics: %1", childTopics)
                        }

                        PlasmaComponents3.Label {
                            Layout.fillWidth: true
                            opacity: 0.6
                            elide: Text.ElideRight
                            text: i18n("Topic ID: %1", topicId)
                        }
                    }
                }
            }
        }
    }

    QQC2.Dialog {
        id: installDialog

        title: i18n("Install updates?")
        modal: true
        standardButtons: QQC2.Dialog.Ok | QQC2.Dialog.Cancel

        contentItem: PlasmaComponents3.Label {
            width: Kirigami.Units.gridUnit * 16
            wrapMode: Text.WordWrap
            text: i18n("Install all available updates?")
        }

        onAccepted: AmoUpdates.installAllUpdates()
    }

    function updateInterval() {
        if (checkDaily)
            return i18n("daily")
        else if (checkWeekly)
            return i18n("weekly")
        else if (checkMonthly)
            return i18n("monthly")
        return i18n("never")
    }

    function formatDuration(timestamp) {
        if (timestamp <= 0)
            return i18n("never")
        var seconds = Math.floor(Date.now() / 1000 - timestamp)
        if (seconds < 60)
            return i18np("%1 second", "%1 seconds", seconds)
        var minutes = Math.floor(seconds / 60)
        if (minutes < 60)
            return i18np("%1 minute", "%1 minutes", minutes)
        var hours = Math.floor(minutes / 60)
        if (hours < 24)
            return i18np("%1 hour", "%1 hours", hours)
        var days = Math.floor(hours / 24)
        return i18np("%1 day", "%1 days", days)
    }

    function formatBytes(bytes, signed) {
        var value = Number(bytes)
        var negative = signed && value < 0
        var abs = Math.abs(value)
        var units = ["B", "KB", "MB", "GB", "TB"]
        var unit = 0
        while (abs >= 1024 && unit < units.length - 1) {
            abs /= 1024
            unit++
        }
        var text
        if (unit === 0)
            text = i18np("%1 byte", "%1 bytes", Math.round(abs))
        else
            // Round to one decimal and pass a numeric value so i18n can
            // apply the locale's decimal separator (e.g. "1,5 MB" in German).
            text = i18n("%1 %2", Math.round(abs * 10) / 10, units[unit])
        return negative ? "-" + text : text
    }

    function populateModel() {
        updatesModel.clear()
        var packages = AmoUpdates.packages
        // Security first, then other important updates, then by operation:
        // remove, install, and finally upgrades (including reinstall/downgrade).
        var security = []
        var important = []
        var remove = []
        var install = []
        var upgrade = []
        for (var i = 0; i < packages.length; i++) {
            if (AmoUpdates.packageIsSecurity(packages[i]))
                security.push(packages[i])
            else if (AmoUpdates.packageIsImportant(packages[i]))
                important.push(packages[i])
            else if (AmoUpdates.packageOperation(packages[i]) === "Remove")
                remove.push(packages[i])
            else if (AmoUpdates.packageOperation(packages[i]) === "Install")
                install.push(packages[i])
            else
                upgrade.push(packages[i])
        }
        var ordered = security.concat(important, remove, install, upgrade)
        for (var j = 0; j < ordered.length; j++) {
            var id = ordered[j]
            var desc = AmoUpdates.packageDescription(id)
            updatesModel.append({"id": id, "name": AmoUpdates.packageName(id), "desc": desc, "version": AmoUpdates.packageVersion(id), "operation": AmoUpdates.packageOperation(id), "isSecurity": AmoUpdates.packageIsSecurity(id), "isImportant": AmoUpdates.packageIsImportant(id)})
        }

        topicUpdatesModel.clear()
        var topics = AmoUpdates.topicUpdates
        for (var j = 0; j < topics.length; j++) {
            var topicId = topics[j]
            topicUpdatesModel.append({
                "topicId": topicId,
                "displayName": AmoUpdates.topicUpdateName(topicId),
                "caution": AmoUpdates.topicUpdateCaution(topicId),
                "isSecurity": AmoUpdates.topicUpdateIsSecurity(topicId),
                "packageCount": AmoUpdates.topicUpdatePackageCount(topicId),
                "packages": AmoUpdates.topicUpdatePackages(topicId).join(", "),
                "childTopics": AmoUpdates.topicUpdateTopics(topicId).join(", ")
            })
        }
    }
}
