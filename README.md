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

The install puts the whole applet package, including its bundled QML plugin
(`org.kde.plasma.amo`, with the `AmoUpdates` singleton), into
`~/.local/share/plasma/plasmoids/org.kde.plasma.amo.updates`. The plugin ships
inside the package, so the applet is self-contained and needs no
`QML_IMPORT_PATH` or other environment setup — it works from any install
prefix out of the box.

After installing, restart the shell to pick up the new applet:

```sh
systemctl --user restart plasma-plasmashell.service
```

Then add "Amo Updates" from the system tray settings ("Extra Items").

## License

GPL-2.0-or-later
