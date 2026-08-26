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
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.extras as PlasmaExtras
import org.kde.ksvg as KSvg
import org.kde.kirigami as Kirigami

PlasmaExtras.ListItem {
    id: packageDelegate

    readonly property bool expanded: ListView.isCurrentItem

    // TUM-matched updates take precedence over the operation label: security
    // updates show "Security update" in red, other important updates show
    // "Important update" in blue, routine upgrades keep their operation label.
    // isSecurity / isImportant come from the ListModel (do NOT declare
    // readonly properties with the same name, they would shadow the model).
    readonly property string operationLabel: isSecurity ? i18n("Security update")
                                            : isImportant ? i18n("Important update")
                                            : operation === "Downgrade" ? i18n("Downgrade")
                                            : operation === "ReInstall" ? i18n("Reinstall")
                                            : operation === "Install" ? i18n("Install")
                                            : i18n("Upgrade")
    readonly property color operationColor: isSecurity ? Kirigami.Theme.negativeTextColor
                                            : isImportant ? Kirigami.Theme.highlightColor
                                            : operation === "Downgrade" ? Kirigami.Theme.negativeTextColor
                                            : operation === "Upgrade" ? Kirigami.Theme.positiveTextColor
                                            : Kirigami.Theme.neutralTextColor

    width: ListView.view ? ListView.view.width : parent ? parent.width : 0
    implicitHeight: innerLayout.implicitHeight + (Kirigami.Units.smallSpacing * 2)
    enabled: true
    checked: containsMouse || expanded
    // The binding is overwritten on clicks, as this is for some reason a Button
    onClicked: checked = Qt.binding(function(){ return containsMouse || expanded; });

    RowLayout {
        id: innerLayout
        anchors.fill: parent

        ColumnLayout {
            spacing: Kirigami.Units.smallSpacing / 2
            Layout.fillWidth: true

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents3.Label {
                    id: nameLabel
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: i18nc("Package Name (Version)", "%1 (%2)", name, version)
                }

                PlasmaComponents3.Label {
                    visible: operation !== ""
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    font.weight: Font.DemiBold
                    color: operationColor
                    text: operationLabel
                }
            }

            PlasmaComponents3.Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.6
                text: desc
            }

            ColumnLayout {
                visible: packageDelegate.expanded
                spacing: Kirigami.Units.smallSpacing / 2

                KSvg.SvgItem {
                    Layout.preferredHeight: lineSvg.elementSize("horizontal-line").height
                    Layout.fillWidth: true

                    elementId: "horizontal-line";

                    svg: KSvg.Svg {
                        id: lineSvg;
                        imagePath: "widgets/line";
                    }
                }

                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    font.weight: Font.DemiBold
                    text: i18nc("description of the update", "Update Description")
                }

                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    opacity: 0.6
                    text: desc == "" ? i18n("No description available") : desc
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
