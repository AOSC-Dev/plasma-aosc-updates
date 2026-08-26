/***************************************************************************
 *   Copyright (C) 2024 by AOSC Updates contributors                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#pragma once

#include <QQmlExtensionPlugin>

/**
 * @brief QML plugin that registers the `AmoUpdates` singleton.
 *
 * The plugin is loaded by the plasmoid's `metadata.json` via the
 * `X-Plasma-API` / `X-Plasma-MainScript` mechanism. It exposes the
 * `AmoUpdates` type in the `io.aosc.plasmaaoscupdates` module so QML files can
 * do `import io.aosc.plasmaaoscupdates 1.0` and use `AmoUpdates` directly.
 */
class AmoUpdatesPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface")

public:
    void registerTypes(const char *uri) override;
};
