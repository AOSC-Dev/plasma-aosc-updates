/***************************************************************************
 *   Copyright (C) 2024 by AOSC Updates contributors                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "qmlplugins.h"

#include <QtQml/qqml.h>
#include <QQmlEngine>

#include "amoupdates.h"

void AmoUpdatesPlugin::registerTypes(const char *uri)
{
    Q_UNUSED(uri);

    // Register AmoUpdates as a singleton so QML can use it without
    // instantiating it: `AmoUpdates.checkUpdates()` etc.
    qmlRegisterSingletonType<AmoUpdates>(
        "org.kde.plasma.amo", 1, 0, "AmoUpdates",
        [](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject * {
            Q_UNUSED(engine);
            Q_UNUSED(scriptEngine);
            return new AmoUpdates;
        });
}
