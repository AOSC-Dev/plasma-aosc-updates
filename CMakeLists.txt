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
find_package(KF6DBusAddons REQUIRED)
find_package(KF6Notifications REQUIRED)
find_package(KF6IconThemes REQUIRED)
find_package(KF6Declarative REQUIRED)
find_package(Plasma REQUIRED)

# Keep the user-local layout compatible with Qt 6's default QML import path
# convention.  This can be overridden for distro packaging, for example with
# -DAMO_QML_INSTALL_DIR=lib64/qt6/qml.
set(AMO_QML_INSTALL_DIR "lib/qt6/qml" CACHE PATH
    "Install directory for the Amo QML plugin")

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
    KF6::DBusAddons
    KF6::Notifications
    KF6::IconThemes
)

# Install the QML plugin into the org/kde/plasma/amo module directory.
install(TARGETS amoqmlplugin
    LIBRARY DESTINATION ${AMO_QML_INSTALL_DIR}/org/kde/plasma/amo
)
install(FILES src/qmldir
    DESTINATION ${AMO_QML_INSTALL_DIR}/org/kde/plasma/amo
)

# ---------------------------------------------------------------------------
# Plasmoid package
# ---------------------------------------------------------------------------
plasma_install_package(plasmoid org.kde.plasma.amo.updates)

# Install gettext catalogs used by the plasmoid's QML i18n() calls.
KI18N_INSTALL(po)
