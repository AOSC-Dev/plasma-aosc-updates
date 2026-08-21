/***************************************************************************
 *   Copyright (C) 2024 by Amo Updates contributors                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "amoupdates.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDateTime>
#include <QLocale>
#include <QDebug>

AmoUpdates::AmoUpdates(QObject *parent)
    : QObject(parent)
{
    connect(&m_client, &AmoClient::updatesListed,
            this, &AmoUpdates::onUpdatesListed);
    connect(&m_client, &AmoClient::refreshFinished,
            this, &AmoUpdates::onRefreshFinished);
    connect(&m_client, &AmoClient::applyFinished,
            this, &AmoUpdates::onApplyFinished);
    connect(&m_client, &AmoClient::statusChanged,
            this, &AmoUpdates::onStatusChanged);
    connect(&m_client, &AmoClient::errorOccurred,
            this, [this](const QString &msg) {
                setMessage(msg);
                setActive(false);
                setStatusMessage(QStringLiteral("Error"));
            });

    setMessage(QStringLiteral("Idle"));
    setStatusMessage(QStringLiteral("Idle"));
}

QString AmoUpdates::iconName() const
{
    if (isActive())
        return QStringLiteral("update-busy");
    if (!isSystemUpToDate())
        return QStringLiteral("update-high");
    return QStringLiteral("update-none");
}

QStringList AmoUpdates::packages() const
{
    QStringList result;
    const auto updates = m_client.updates();
    for (const UpdatePackage &pkg : updates) {
        result << pkg.name;
    }
    return result;
}

void AmoUpdates::checkUpdates(bool manual)
{
    Q_UNUSED(manual);

    if (m_active)
        return;

    setActive(true);
    setStatusMessage(QStringLiteral("Checking for updates..."));
    m_client.refresh();
}

void AmoUpdates::installUpdates(const QStringList &packageIds)
{
    if (m_active || packageIds.isEmpty())
        return;

    setActive(true);
    setStatusMessage(QStringLiteral("Installing updates..."));
    m_client.applyChanges(packageIds, QStringList(), false);
}

void AmoUpdates::installAllUpdates()
{
    if (m_active)
        return;

    setActive(true);
    setStatusMessage(QStringLiteral("Installing all updates..."));
    m_client.applyChanges(QStringList(), QStringList(), true);
}

QString AmoUpdates::packageName(const QString &packageId) const
{
    return m_packageMap.value(packageId).name;
}

QString AmoUpdates::packageVersion(const QString &packageId) const
{
    const UpdatePackage pkg = m_packageMap.value(packageId);
    if (pkg.operation == QStringLiteral("Upgrade") && !pkg.oldVersion.isEmpty()) {
        return QStringLiteral("%1 → %2").arg(pkg.oldVersion, pkg.newVersion);
    }
    return pkg.newVersion;
}

QString AmoUpdates::packageDescription(const QString &packageId) const
{
    Q_UNUSED(packageId);
    // amo does not currently expose per-package descriptions in the
    // updates list; return an empty string so the UI shows a placeholder.
    return QString();
}

QString AmoUpdates::packageOperation(const QString &packageId) const
{
    return m_packageMap.value(packageId).operation;
}

qint64 AmoUpdates::packageDownloadSize(const QString &packageId) const
{
    return m_packageMap.value(packageId).downloadSize;
}

qint64 AmoUpdates::totalDownloadSize() const
{
    return m_client.totalDownloadSize();
}

qint64 AmoUpdates::diskSizeDelta() const
{
    return m_client.diskSizeDelta();
}

double AmoUpdates::lastRefreshTimestamp() const
{
    return m_lastRefreshTimestamp;
}

void AmoUpdates::onUpdatesListed(const QList<UpdatePackage> &updates,
                                 qint64 totalDownloadSize,
                                 qint64 diskSizeDelta)
{
    Q_UNUSED(totalDownloadSize);
    Q_UNUSED(diskSizeDelta);

    m_packageMap.clear();
    for (const UpdatePackage &pkg : updates) {
        m_packageMap.insert(pkg.name, pkg);
    }

    setActive(false);
    setLastCheckSuccessful(true);
    setTimestamp(QLocale().toString(QDateTime::currentDateTime(), QLocale::ShortFormat));
    m_lastRefreshTimestamp = QDateTime::currentMSecsSinceEpoch() / 1000.0;

    if (updates.isEmpty()) {
        setMessage(QStringLiteral("System is up to date"));
    } else {
        setMessage(QStringLiteral("%1 update(s) available").arg(updates.size()));
    }

    emit updatesChanged();
}

void AmoUpdates::onRefreshFinished(bool success, const QString &error)
{
    if (!success) {
        setActive(false);
        setLastCheckSuccessful(false);
        setMessage(QStringLiteral("Update check failed"));
        setStatusMessage(error);
        emit updateError(error);
        return;
    }

    // After a successful refresh, fetch the actual update list.
    m_client.fetchUpdates();
}

void AmoUpdates::onApplyFinished(bool success, const QString &error)
{
    setActive(false);

    if (success) {
        setMessage(QStringLiteral("Updates installed"));
        setStatusMessage(QStringLiteral("Idle"));
        emit updatesInstalled();
        // Re-check to refresh the badge.
        m_client.fetchUpdates();
    } else {
        setMessage(QStringLiteral("Update failed"));
        setStatusMessage(error);
        emit updateError(error);
    }
}

void AmoUpdates::onStatusChanged(const QString &statusJson)
{
    // amo's Status signal carries three kinds of JSON:
    //  1. DpkgProgress (flat): {"status":"pmstatus","stage":"dpkg",
    //      "package_or_dpkg_exec":"...","percent":45.0,"description":"..."}
    //  2. Tagged oma events: {"DownloadEvent":{"NewProgressSpinner":{...}}}
    //     whose innermost object carries a "msg" string.
    //  3. Completion marker: {"status":"finished","request_id":...}
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(statusJson.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return;

    if (!doc.isObject())
        return;
    const QJsonObject obj = doc.object();

    // Tagged oma event: {"DownloadEvent": {...}} / {"...Event": {...}}
    if (obj.size() == 1) {
        const QJsonValue tagged = obj.begin().value();
        if (tagged.isObject()) {
            const QString msg = findEventMessage(tagged.toObject());
            if (!msg.isEmpty()) {
                setStatusMessage(msg);
            }
            const int percent = findEventPercent(tagged.toObject());
            if (percent >= 0) {
                setPercentage(percent);
            }
            return;
        }
    }

    // Completion marker {"status":"finished"} - handled via ResultReport.
    if (obj.value(QStringLiteral("status")).toString() == QStringLiteral("finished"))
        return;

    // Flat DpkgProgress: prefer the human-readable "description".
    const QString description = obj.value(QStringLiteral("description")).toString();
    if (!description.isEmpty()) {
        setStatusMessage(description);
    }

    // Progress percentage if present.
    const QJsonValue percentValue = obj.value(QStringLiteral("percent"));
    if (percentValue.isDouble()) {
        setPercentage(percentValue.toInt());
    }
}

QString AmoUpdates::findEventMessage(const QJsonObject &obj) const
{
    // Recursively look for a "msg" string inside tagged oma events.
    if (obj.contains(QStringLiteral("msg"))) {
        const QJsonValue msg = obj.value(QStringLiteral("msg"));
        if (msg.isString()) {
            return msg.toString();
        }
    }
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (it.value().isObject()) {
            const QString found = findEventMessage(it.value().toObject());
            if (!found.isEmpty()) {
                return found;
            }
        }
    }
    return QString();
}

int AmoUpdates::findEventPercent(const QJsonObject &obj) const
{
    if (obj.contains(QStringLiteral("percent"))) {
        const QJsonValue p = obj.value(QStringLiteral("percent"));
        if (p.isDouble()) {
            return p.toInt();
        }
    }
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (it.value().isObject()) {
            const int found = findEventPercent(it.value().toObject());
            if (found >= 0) {
                return found;
            }
        }
    }
    return -1;
}

void AmoUpdates::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    emit activeChanged();
}

void AmoUpdates::setMessage(const QString &message)
{
    if (m_message == message)
        return;
    m_message = message;
    emit messageChanged();
}

void AmoUpdates::setStatusMessage(const QString &message)
{
    if (m_statusMessage == message)
        return;
    m_statusMessage = message;
    emit statusMessageChanged();
}

void AmoUpdates::setPercentage(int percentage)
{
    if (m_percentage == percentage)
        return;
    m_percentage = percentage;
    emit percentageChanged();
}

void AmoUpdates::setNetworkOnline(bool online)
{
    if (m_networkOnline == online)
        return;
    m_networkOnline = online;
    emit networkStateChanged();
}

void AmoUpdates::setOnBattery(bool onBattery)
{
    if (m_onBattery == onBattery)
        return;
    m_onBattery = onBattery;
    emit isOnBatteryChanged();
}

void AmoUpdates::setTimestamp(const QString &timestamp)
{
    if (m_timestamp == timestamp)
        return;
    m_timestamp = timestamp;
    emit updatesChanged();
}

void AmoUpdates::setLastCheckSuccessful(bool ok)
{
    if (m_lastCheckSuccessful == ok)
        return;
    m_lastCheckSuccessful = ok;
    emit updatesChanged();
}
