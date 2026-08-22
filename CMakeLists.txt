cmake_minimum_required(VERSION 3.16)

project(plasma-aosc-updates VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    DBus
    Qml
    Quick
    QuickControls2
)

find_package(KF6I18n REQUIRED)
find_package(KF6CoreAddons REQUIRED)
find_package(KF6Config REQUIRED)
find_package(KF6DBusAddons REQUIRED)
find_package(KF6Notifications REQUIRED)
find_package(KF6IconThemes REQUIRED)
find_package(KF6Declarative REQUIRED)
find_package(Plasma REQUIRED)

# ---------------------------------------------------------------------------
# QML plugin library exposing the AmoUpdates singleton to the plasmoid UI.
# ---------------------------------------------------------------------------
set(amo_qmlplugins_SRCS
    src/qmlplugins.cpp
    src/amoupdates.cpp
    src/amoclient.cpp
)

add_library(amoqmlplugin SHARED ${amo_qmlplugins_SRCS})

target_include_directories(amoqmlplugin PRIVATE src)

target_link_libraries(amoqmlplugin
    Qt6::Core
    Qt6::Gui
    Qt6::DBus
    Qt6::Qml
    Qt6::Quick
    KF6::I18n
    KF6::CoreAddons
    KF6::ConfigCore
    KF6::DBusAddons
    KF6::Notifications
    KF6::IconThemes
)

# Ship the QML plugin inside the plasmoid package itself, under
# contents/ui/org/kde/plasma/amo.  The UI imports it with a relative directory
# import (`import "org/kde/plasma/amo"`), which Qt resolves against the
# directory of the importing QML file (contents/ui).  The applet is therefore
# fully self-contained: it loads from any install prefix (~/.local included)
# without needing QML_IMPORT_PATH or any other environment setup.
set(AMO_PLASMOID_INSTALL_DIR
    "${KDE_INSTALL_DATADIR}/plasma/plasmoids/org.kde.plasma.amo.updates")

install(TARGETS amoqmlplugin
    LIBRARY DESTINATION ${AMO_PLASMOID_INSTALL_DIR}/contents/ui/org/kde/plasma/amo
)
install(FILES src/qmldir
    DESTINATION ${AMO_PLASMOID_INSTALL_DIR}/contents/ui/org/kde/plasma/amo
)

# KNotification only dispatches application-defined events that have a
# matching notifyrc description installed in the KDE data directory.
install(FILES plasma-amo-updates.notifyrc
    DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR}
)

# ---------------------------------------------------------------------------
# Plasmoid package
# ---------------------------------------------------------------------------
plasma_install_package(plasmoid org.kde.plasma.amo.updates)

# Install gettext catalogs used by the plasmoid's QML i18n() calls.
KI18N_INSTALL(po)
