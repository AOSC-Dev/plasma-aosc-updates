/***************************************************************************
 *   Copyright (C) 2024 by Amo Updates contributors                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

import QtQuick
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami
import org.kde.plasma.amo 1.0

PlasmoidItem
{
    id: root

    fullRepresentation: Full {
        checkDaily: root.checkDaily
        checkWeekly: root.checkWeekly
        checkMonthly: root.checkMonthly
    }
    toolTipSubText: AmoUpdates.message
    Plasmoid.icon: AmoUpdates.iconName

    switchWidth: Kirigami.Units.gridUnit * 10;
    switchHeight: Kirigami.Units.gridUnit * 10;

    property bool checkDaily: plasmoid.configuration.daily
    property bool checkWeekly: plasmoid.configuration.weekly
    property bool checkMonthly: plasmoid.configuration.monthly
    property bool autoCheck: plasmoid.configuration.auto_check
    property bool checkOnMobile: plasmoid.configuration.check_on_mobile

    property double lastCheckAttempt: 0
    property double lastCheckTimestamp: plasmoid.configuration.last_check_timestamp
    readonly property int secsAutoCheckLimit: 10 * 60

    readonly property int secsInDay: 60 * 60 * 24;
    readonly property int secsInWeek: secsInDay * 7;
    readonly property int secsInMonth: secsInDay * 30;

    readonly property bool networkAllowed: AmoUpdates.isNetworkMobile ? checkOnMobile : AmoUpdates.isNetworkOnline
    readonly property bool batteryAllowed: AmoUpdates.isOnBattery ? plasmoid.configuration.check_on_battery : true

    Timer {
        id: timer
        repeat: true
        triggeredOnStart: true
        interval: 1000 * 60; // Re-evaluate the configured interval every minute.
        onTriggered: {
            if (needsForcedUpdate() && networkAllowed && batteryAllowed) {
                lastCheckAttempt = Date.now() / 1000;
                AmoUpdates.checkUpdates(false /* manual */);
            }
        }
    }

    Binding {
        target: plasmoid
        property: "status"
        value: AmoUpdates.isActive || !AmoUpdates.isSystemUpToDate ? PlasmaCore.Types.ActiveStatus : PlasmaCore.Types.PassiveStatus;
    }

    compactRepresentation: MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton

        Kirigami.Icon {
            anchors.fill: parent
            source: AmoUpdates.iconName
            active: parent.containsMouse
        }

        onClicked: root.expanded = !root.expanded
    }

    function needsForcedUpdate() {
        if (!autoCheck) {
            return false;
        }

        var now = Date.now() / 1000;
        if (now - lastCheckAttempt < secsAutoCheckLimit) {
            return false;
        }

        if (lastCheckTimestamp <= 0) {
            return true;
        }

        var secs = now - lastCheckTimestamp;
        if (checkDaily) {
            return secs >= secsInDay;
        } else if (checkWeekly) {
            return secs >= secsInWeek;
        } else if (checkMonthly) {
            return secs >= secsInMonth;
        }
        return false;
    }

    Connections {
        target: AmoUpdates
        function onUpdatesChanged() {
            var timestamp = AmoUpdates.lastRefreshTimestamp()
            if (timestamp > 0 && timestamp > lastCheckTimestamp) {
                plasmoid.configuration.last_check_timestamp = timestamp
                plasmoid.configuration.writeConfig()
                lastCheckAttempt = Date.now() / 1000
            }
        }
        function onNetworkStateChanged() { timer.restart() }
        function onIsOnBatteryChanged() { timer.restart() }
        function onOpenRequested() { root.expanded = true }
    }

    Component.onCompleted: {
        timer.start()
        // Always do an initial check when the plasmoid loads, so the tray
        // icon reflects the current state even if the last check was recent
        // (e.g. new updates appeared on the server since then).
        if (autoCheck && networkAllowed && batteryAllowed) {
            lastCheckAttempt = Date.now() / 1000;
            AmoUpdates.checkUpdates(false /* manual */);
        }
    }
}
