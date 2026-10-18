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
    property alias cfg_check_on_login: checkOnLogin.checked
    property alias cfg_auto_check: autoCheck.checked
    property alias cfg_suppress_pulse_animation: suppressPulse.checked
    property alias cfg_suppress_spinner_animation: suppressSpinner.checked

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
            text: i18nc("@option:check part of a sentence: Check for updates when", "Running on battery power")
        }

        CheckBox {
            id: mobile
            enabled: autoCheck.checked
            text: i18nc("@option:check part of a sentence: Check for updates when", "Connected via a mobile broadband connection")
        }

        CheckBox {
            id: checkOnLogin
            enabled: autoCheck.checked
            text: i18n("Check for updates at login")
        }

        // Tray icon animations. The two animations come from different places,
        // hence two separate switches:
        //
        //  - The bounce is drawn by the system tray for any item that asks for
        //    attention. It does not follow the global animation speed setting
        //    in System Settings: at the "Instant" setting it keeps looping,
        //    only faster, because animators with a duration of zero are given
        //    a 1 ms duration to keep them firing. The applet can therefore only
        //    avoid it by not requesting attention in the first place.
        //  - The spinner is drawn by this applet, so it does follow the global
        //    setting (and stops at "Instant"), and this switch turns it off on
        //    top of that.
        CheckBox {
            id: suppressPulse
            Kirigami.FormData.label: i18nc("@label:group", "Animation:")
            text: i18n("Do not bounce the tray icon for security updates")
        }

        CheckBox {
            id: suppressSpinner
            text: i18n("Do not spin the tray icon during an update")
        }
    }
}
