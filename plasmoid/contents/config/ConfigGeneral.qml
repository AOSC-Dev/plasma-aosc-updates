import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    id: iconsPage

    Kirigami.FormLayout {
        id: form

        property alias cfg_daily: daily.checked
        property alias cfg_weekly: weekly.checked
        property alias cfg_monthly: monthly.checked
        property alias cfg_check_on_battery: battery.checked

        ButtonGroup {
            id: intervalGroup
        }

        RadioButton {
            id: daily
            Kirigami.FormData.label: i18nc("@label check interval for updates", "Check interval:")
            ButtonGroup.group: intervalGroup
            text: i18n("Daily")
        }

        RadioButton {
            id: weekly
            ButtonGroup.group: intervalGroup
            text: i18n("Weekly")
        }

        RadioButton {
            id: monthly
            ButtonGroup.group: intervalGroup
            text: i18n("Monthly")
        }

        CheckBox {
            id: battery
            Kirigami.FormData.label: i18nc("@label part of a sentence", "Check for updates when:")
            text: i18nc("@option:check part of a sentence: Check for updates when", "On battery")
        }
    }
}
