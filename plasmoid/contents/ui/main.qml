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

    fullRepresentation: Full {}
    toolTipSubText: AmoUpdates.message
    Plasmoid.icon: AmoUpdates.iconName

    switchWidth: Kirigami.Units.gridUnit * 10;
    switchHeight: Kirigami.Units.gridUnit * 10;

    property bool checkDaily: plasmoid.configuration.daily
    property bool checkWeekly: plasmoid.configuration.weekly
    property bool checkMonthly: plasmoid.configuration.monthly

    property double lastCheckAttempt: AmoUpdates.lastRefreshTimestamp()
    readonly property int secsAutoCheckLimit: 10 * 60

    readonly property int secsInDay: 60 * 60 * 24;
    readonly property int secsInWeek: secsInDay * 7;
    readonly property int secsInMonth: secsInDay * 30;

    readonly property bool networkAllowed: AmoUpdates.isNetworkOnline
    readonly property bool batteryAllowed: AmoUpdates.isOnBattery ? plasmoid.configuration.check_on_battery : true

    Timer {
        id: timer
        repeat: true
        triggeredOnStart: true
        interval: 1000 * 60 * 60; // 1 hour
        onTriggered: {
            if (needsForcedUpdate() && networkAllowed && batteryAllowed) {
                lastCheckAttempt = Date.now();
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
        if ((Date.now() - lastCheckAttempt)/1000 < secsAutoCheckLimit) {
            return false;
        }

        var secs = (Date.now() - AmoUpdates.lastRefreshTimestamp())/1000; // compare with the saved timestamp
        if (secs < 0) { // never checked before
            return true;
        } else if (checkDaily) {
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
        function onNetworkStateChanged() { timer.restart() }
        function onIsOnBatteryChanged() { timer.restart() }
    }

    Component.onCompleted: {
        timer.start()
    }
}
