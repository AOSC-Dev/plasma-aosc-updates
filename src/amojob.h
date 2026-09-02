/***************************************************************************
 *   Copyright (C) 2026 by AOSC-Dev                                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#pragma once

#include <KJob>

/**
 * @brief A KJob that mirrors the progress of an amo operation.
 *
 * amo is not a KIO job, so to get the same "Dolphin-style" progress
 * notification (a notification with a progress bar, plus progress in the
 * task manager) we wrap the operation in a KJob and register it with
 * KUiServerV2JobTracker, which forwards it to org.kde.JobViewServer.
 *
 * The job is driven externally by AmoUpdates: it never starts itself, it
 * just exposes setters that forward to the KJob progress API.
 */
class AmoJob : public KJob
{
    Q_OBJECT

public:
    explicit AmoJob(QObject *parent = nullptr);

    void start() override;

    /// Update the overall progress (0-100) and the status message shown in
    /// the notification body.
    void setProgress(int percent, const QString &message);

    /// Finish the job. On success the notification disappears; on failure
    /// it stays with the error text.
    void finish(bool success, const QString &error);
};
