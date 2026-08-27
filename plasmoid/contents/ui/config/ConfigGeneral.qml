import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    id: root

    property alias cfg_daily: daily.checked
    property alias cfg_weekly: weekly.checked
    property alias cfg_monthly: monthly.checked
    property alias cfg_check_on_battery: battery.checked
    property alias cfg_check_on_mobile: mobile.checked
    property alias cfg_auto_check: autoCheck.checked

    Kirigami.FormLayout {
        id: form

        CheckBox {
            id: autoCheck
            Kirigami.FormData.label: i18nc("@label", "Automatic update checks:")
            text: i18n("Check for updates automatically")
        }

        ButtonGroup {
            id: intervalGroup
        }

        RadioButton {
            id: daily
            Kirigami.FormData.label: i18nc("@label check interval for updates", "Check interval:")
            ButtonGroup.group: intervalGroup
            enabled: autoCheck.checked
            text: i18n("Daily")
        }

        RadioButton {
            id: weekly
            ButtonGroup.group: intervalGroup
            enabled: autoCheck.checked
            text: i18n("Weekly")
        }

        RadioButton {
            id: monthly
            ButtonGroup.group: intervalGroup
            enabled: autoCheck.checked
            text: i18n("Monthly")
        }

        CheckBox {
            id: battery
            Kirigami.FormData.label: i18nc("@label part of a sentence", "Check for updates when:")
            enabled: autoCheck.checked
            text: i18nc("@option:check part of a sentence: Check for updates when", "On battery")
        }

        CheckBox {
            id: mobile
            enabled: autoCheck.checked
            text: i18nc("@option:check part of a sentence: Check for updates when", "On a mobile connection")
        }
    }
}
