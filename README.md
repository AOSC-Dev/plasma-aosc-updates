# Plasma AOSC Updates

A Plasma 6 system tray applet that checks for and installs system updates
through the [amo](https://github.com/AOSC-Dev/amo) D-Bus service, with a UI
modelled after plasma-pk-updates.

## Features

- Lives in the system tray, like other status notifier items
- Icon appears only when updates are available (hides when up to date)
- Shows a list of available updates with checkboxes to pick what to install
- Performs full system upgrade or installs only the selected packages
- Displays download/progress status while working

## Requirements

- KDE Plasma 6 (KF6 / Qt6)
- [amo](https://github.com/AOSC-Dev/amo) >= current master
  (uses the `ResultReport` signal API; the old `GetLastResult` API is not
  supported)

## Building & Installing

```sh
cmake -B build -DCMAKE_INSTALL_PREFIX=~/.local
cmake --build build
cmake --install build
```

The install puts the QML plugin into
`~/.local/lib/qt6/qml/org/kde/plasma/amo` and the applet package into
`~/.local/share/plasma/plasmoids`. For distro layouts that use `lib64`, set
`-DAMO_QML_INSTALL_DIR=lib64/qt6/qml` when configuring.

Because Qt6's QML engine does not search `~/.local/lib/qt6/qml` by default,
point `QML_IMPORT_PATH` at it and restart the shell:

```sh
systemctl --user set-environment QML_IMPORT_PATH=$HOME/.local/lib/qt6/qml
systemctl --user restart plasma-plasmashell.service
```

Then add "Amo Updates" from the system tray settings ("Extra Items").

## License

GPL-2.0-or-later
