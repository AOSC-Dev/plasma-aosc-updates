/***************************************************************************
 *   Copyright (C) 2024 by Amo Updates contributors                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "amoclient.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingReply>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>

namespace {
const QString kService = QStringLiteral("io.aosc.Amo");
const QString kPath = QStringLiteral("/io/aosc/Amo");
const QString kInterface = QStringLiteral("io.aosc.Amo1");
}

AmoClient::AmoClient(QObject *parent)
    : QObject(parent)
    , m_iface(new QDBusInterface(kService, kPath, kInterface,
                                 QDBusConnection::systemBus(), this))
{
    // Subscribe to the status signal for progress reporting.
    QDBusConnection::systemBus().connect(
        kService, kPath, kInterface, QStringLiteral("Status"),
        this, SLOT(onStatusSignal(QDBusMessage)));

    // Subscribe to the ResultReport signal; new amo versions push the
    // ResultReport JSON here instead of exposing GetLastResult.
    // Note: zbus exports the Rust `result_report` method as the D-Bus
    // signal "ResultReport" (camelCase).
    QDBusConnection::systemBus().connect(
        kService, kPath, kInterface, QStringLiteral("ResultReport"),
        this, SLOT(onResultReportSignal(QDBusMessage)));
}

bool AmoClient::isAvailable() const
{
    return m_iface->isValid();
}

void AmoClient::refresh()
{
    if (!m_iface->isValid()) {
        emit errorOccurred(QStringLiteral("Cannot connect to amo D-Bus service."));
        return;
    }

    m_pendingTask = TaskType::Refresh;
    QDBusPendingCall call = m_iface->asyncCall(QStringLiteral("Refresh"));
    auto *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, &AmoClient::onRefreshReply);
}

void AmoClient::fetchUpdates()
{
    if (!m_iface->isValid()) {
        emit errorOccurred(QStringLiteral("Cannot connect to amo D-Bus service."));
        return;
    }

    QDBusPendingCall call = m_iface->asyncCall(QStringLiteral("UpdatesList"));
    auto *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, &AmoClient::onUpdatesReply);
}

void AmoClient::applyChanges(const QStringList &install,
                             const QStringList &remove,
                             bool upgradeAll)
{
    if (!m_iface->isValid()) {
        emit errorOccurred(QStringLiteral("Cannot connect to amo D-Bus service."));
        return;
    }

    m_pendingTask = TaskType::Apply;
    QDBusPendingCall call = m_iface->asyncCall(
        QStringLiteral("ApplyChanges"),
        QVariant::fromValue(install),
        QVariant::fromValue(remove),
        upgradeAll);
    auto *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, &AmoClient::onApplyReply);
}

void AmoClient::onRefreshReply(QDBusPendingCallWatcher *watcher)
{
    watcher->deleteLater();

    QDBusPendingReply<quint64> reply(*watcher);
    if (reply.isError()) {
        m_pendingTask = TaskType::None;
        emit refreshFinished(false, reply.error().message());
        return;
    }

    m_lastRequestId = reply.value();
    emit refreshStarted(m_lastRequestId);
    // The result arrives later via the result_report signal.
}

void AmoClient::onUpdatesReply(QDBusPendingCallWatcher *watcher)
{
    watcher->deleteLater();

    QDBusPendingReply<QString> reply(*watcher);
    if (reply.isError()) {
        emit errorOccurred(reply.error().message());
        return;
    }

    parseUpdates(reply.value());
    emit updatesListed(m_updates, m_totalDownloadSize, m_diskSizeDelta);
}

void AmoClient::onApplyReply(QDBusPendingCallWatcher *watcher)
{
    watcher->deleteLater();

    QDBusPendingReply<quint64> reply(*watcher);
    if (reply.isError()) {
        m_pendingTask = TaskType::None;
        emit applyFinished(false, reply.error().message());
        return;
    }

    m_lastRequestId = reply.value();
    emit applyStarted(m_lastRequestId);
    // The result arrives later via the result_report signal.
}

void AmoClient::onStatusSignal(const QDBusMessage &message)
{
    if (message.type() != QDBusMessage::SignalMessage)
        return;

    const QList<QVariant> args = message.arguments();
    if (args.isEmpty())
        return;

    emit statusChanged(args.first().toString());
}

void AmoClient::onResultReportSignal(const QDBusMessage &message)
{
    if (message.type() != QDBusMessage::SignalMessage)
        return;

    const QList<QVariant> args = message.arguments();
    if (args.isEmpty())
        return;

    handleResult(args.first().toString());
}

void AmoClient::parseUpdates(const QString &json)
{
    m_updates.clear();
    m_totalDownloadSize = 0;
    m_diskSizeDelta = 0;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse updates JSON:" << parseError.errorString();
        return;
    }

    const QJsonObject root = doc.object();

    m_diskSizeDelta = root.value(QStringLiteral("disk_size_delta")).toVariant().toLongLong();
    m_totalDownloadSize = root.value(QStringLiteral("total_download_size")).toVariant().toULongLong();

    const QJsonArray install = root.value(QStringLiteral("install")).toArray();
    for (const QJsonValue &value : install) {
        const QJsonObject obj = value.toObject();

        UpdatePackage pkg;
        pkg.name = obj.value(QStringLiteral("name")).toString();
        pkg.oldVersion = obj.value(QStringLiteral("old_version")).toString();
        pkg.newVersion = obj.value(QStringLiteral("new_version")).toString();
        pkg.arch = obj.value(QStringLiteral("arch")).toString();
        pkg.downloadSize = obj.value(QStringLiteral("download_size")).toVariant().toLongLong();
        pkg.automatic = obj.value(QStringLiteral("automatic")).toBool();

        // The `op` field is serialized as a string, e.g. "Upgrade".
        const QString op = obj.value(QStringLiteral("op")).toString();
        if (op == QStringLiteral("ReInstall")) {
            pkg.operation = QStringLiteral("ReInstall");
        } else if (op == QStringLiteral("Downgrade")) {
            pkg.operation = QStringLiteral("Downgrade");
        } else if (op == QStringLiteral("Upgrade")) {
            pkg.operation = QStringLiteral("Upgrade");
        } else {
            pkg.operation = QStringLiteral("Install");
        }

        m_totalDownloadSize += pkg.downloadSize;
        m_updates.append(pkg);
    }
}

void AmoClient::handleResult(const QString &json)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        const QString error = QStringLiteral("Failed to parse result: %1")
                                  .arg(parseError.errorString());
        const TaskType task = m_pendingTask;
        m_pendingTask = TaskType::None;
        if (task == TaskType::Apply) {
            emit applyFinished(false, error);
        } else {
            emit refreshFinished(false, error);
        }
        return;
    }

    const QJsonObject root = doc.object();
    const QJsonValue statusValue = root.value(QStringLiteral("status"));

    bool success = false;
    QString error;

    // The status field is either the string "Success", or an object
    // {"Failed": "<message>"}.
    if (statusValue.isString()) {
        success = (statusValue.toString() == QStringLiteral("Success"));
    } else if (statusValue.isObject()) {
        const QJsonObject statusObj = statusValue.toObject();
        if (statusObj.contains(QStringLiteral("Success"))) {
            success = true;
        } else if (statusObj.contains(QStringLiteral("Failed"))) {
            success = false;
            error = statusObj.value(QStringLiteral("Failed")).toString();
        }
    }

    // Dispatch the result to the task that is currently pending.
    const TaskType task = m_pendingTask;
    m_pendingTask = TaskType::None;
    if (task == TaskType::Apply) {
        emit applyFinished(success, error);
    } else {
        emit refreshFinished(success, error);
    }
}
