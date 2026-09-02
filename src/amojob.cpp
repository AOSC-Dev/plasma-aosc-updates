/***************************************************************************
 *   Copyright (C) 2026 by AOSC-Dev                                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "amojob.h"

#include <KLocalizedString>

AmoJob::AmoJob(QObject *parent)
    : KJob(parent)
{
    setCapabilities(KJob::NoCapabilities);
}

void AmoJob::start()
{
    // The job is driven externally by AmoUpdates; nothing to do here.
    startElapsedTimer();
}

void AmoJob::setProgress(int percent, const QString &message)
{
    setPercent(qBound(0, percent, 100));
    emit infoMessage(this, message);
}

void AmoJob::finish(bool success, const QString &error)
{
    if (!success) {
        setError(KJob::UserDefinedError);
        setErrorText(error);
    }
    emitResult();
}
