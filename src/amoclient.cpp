/***************************************************************************
 *   Copyright (C) 2024 by AOSC Updates contributors                       *
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
#include <QLocale>
#include <QDebug>

#include <KLocalizedString>

namespace {
constexpr auto kTranslationDomain = "plasma_applet_io.aosc.plasmaaoscupdates.updates";
const QString kService = QStringLiteral("io.aosc.Amo");
const QString kPath = QStringLiteral("/io/aosc/Amo");
const QString kInterface = QStringLiteral("io.aosc.Amo1");

QString localizedTumText(const QJsonObject &translations)
{
    const QString locale = QLocale().name();
    const QString language = locale.section(QLatin1Char('_'), 0, 0);
    const QStringList candidates = {
        locale,
        language,
        QStringLiteral("default"),
        QStringLiteral("en_US"),
    };

    for (const QString &candidate : candidates) {
        const QString value = translations.value(candidate).toString();
        if (!value.isEmpty())
            return value;
    }

    for (auto it = translations.constBegin(); it != translations.constEnd(); ++it) {
        if (it.value().isString() && !it.value().toString().isEmpty())
            return it.value().toString();
    }
    return QString();
}
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

    // Subscribe to the UpdatesChanged signal emitted by amo's file watcher
    // when dpkg status or apt lists change. Older amo versions don't have
    // this signal; the connection simply fails silently.
    QDBusConnection::systemBus().connect(
        kService, kPath, kInterface, QStringLiteral("UpdatesChanged"),
        this, SLOT(onUpdatesChangedSignal(QDBusMessage)));
}

bool AmoClient::isAvailable() const
{
    return m_iface->isValid();
}

void AmoClient::refresh()
{
    if (!m_iface->isValid()) {
        emit errorOccurred(i18nd(kTranslationDomain,
                                 "Cannot connect to amo D-Bus service."));
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
        emit errorOccurred(i18nd(kTranslationDomain,
                                 "Cannot connect to amo D-Bus service."));
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
        emit errorOccurred(i18nd(kTranslationDomain,
                                 "Cannot connect to amo D-Bus service."));
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

    m_pendingDescriptions = m_updates.size();
    if (m_pendingDescriptions == 0) {
        emit descriptionsChanged();
        return;
    }

    for (const UpdatePackage &pkg : std::as_const(m_updates)) {
        QDBusPendingCall call = m_iface->asyncCall(QStringLiteral("GetDescription"), pkg.name);
        auto *descriptionWatcher = new QDBusPendingCallWatcher(call, this);
        descriptionWatcher->setProperty("packageName", pkg.name);
        connect(descriptionWatcher, &QDBusPendingCallWatcher::finished,
                this, &AmoClient::onDescriptionReply);
    }
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

void AmoClient::onDescriptionReply(QDBusPendingCallWatcher *watcher)
{
    const QString packageName = watcher->property("packageName").toString();
    QDBusPendingReply<QString> reply(*watcher);
    if (!reply.isError()) {
        for (UpdatePackage &pkg : m_updates) {
            if (pkg.name == packageName) {
                pkg.description = reply.value();
                break;
            }
        }
    }

    watcher->deleteLater();
    --m_pendingDescriptions;
    if (m_pendingDescriptions == 0)
        emit descriptionsChanged();
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

void AmoClient::onUpdatesChangedSignal(const QDBusMessage &message)
{
    if (message.type() != QDBusMessage::SignalMessage)
        return;

    emit updatesChanged();
}

void AmoClient::parseUpdates(const QString &json)
{
    m_updates.clear();
    m_topicUpdates.clear();
    m_hasImportantUpdates = false;
    m_hasSecurityUpdates = false;
    m_totalDownloadSize = 0;
    m_diskSizeDelta = 0;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse updates JSON:" << parseError.errorString();
        return;
    }

    const QJsonObject root = doc.object();

    const QJsonArray tum = root.value(QStringLiteral("tum")).toArray();
    for (const QJsonValue &value : tum) {
        if (!value.isObject())
            continue;

        const QJsonObject obj = value.toObject();
        TopicUpdate topic;
        topic.id = obj.value(QStringLiteral("id")).toString();
        topic.kind = obj.value(QStringLiteral("kind")).toString();
        topic.name = localizedTumText(obj.value(QStringLiteral("name")).toObject());
        topic.caution = localizedTumText(obj.value(QStringLiteral("caution")).toObject());
        topic.packageCount = obj.value(QStringLiteral("package_count")).toInt();
        topic.security = obj.value(QStringLiteral("security")).toBool();

        const QJsonArray packages = obj.value(QStringLiteral("packages")).toArray();
        for (const QJsonValue &package : packages)
            topic.packages.append(package.toString());

        const QJsonArray topics = obj.value(QStringLiteral("topics")).toArray();
        for (const QJsonValue &childTopic : topics)
            topic.topics.append(childTopic.toString());

        if (!topic.id.isEmpty())
            m_topicUpdates.append(topic);
        m_hasImportantUpdates = true;
        m_hasSecurityUpdates = m_hasSecurityUpdates || topic.security;
    }

    // Keep the explicit aggregate flag for forward compatibility, while the
    // per-entry security flags remain the source of truth today.
    m_hasSecurityUpdates = m_hasSecurityUpdates
        || root.value(QStringLiteral("has_important_updates")).toBool();

    m_diskSizeDelta = root.value(QStringLiteral("disk_size_delta")).toVariant().toLongLong();
    // amo's response already contains the aggregate download size of all
    // install entries, so we must not accumulate it again from the per-package
    // download_size fields below.
    m_totalDownloadSize = root.value(QStringLiteral("total_download_size")).toVariant().toULongLong();

    const QJsonArray install = root.value(QStringLiteral("install")).toArray();
    for (const QJsonValue &value : install) {
        const QJsonObject obj = value.toObject();

        UpdatePackage pkg;
        pkg.name = obj.value(QStringLiteral("name")).toString();
        pkg.oldVersion = obj.value(QStringLiteral("old_version")).toString();
        pkg.newVersion = obj.value(QStringLiteral("new_version")).toString();
        pkg.arch = obj.value(QStringLiteral("arch")).toString();
        pkg.description.clear();
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

        m_updates.append(pkg);
    }

    // Packages that would be removed as part of the transaction. The
    // version field is the currently installed version.
    const QJsonArray remove = root.value(QStringLiteral("remove")).toArray();
    for (const QJsonValue &value : remove) {
        const QJsonObject obj = value.toObject();

        UpdatePackage pkg;
        pkg.name = obj.value(QStringLiteral("name")).toString();
        pkg.oldVersion = obj.value(QStringLiteral("version")).toString();
        pkg.newVersion.clear();
        pkg.arch = obj.value(QStringLiteral("arch")).toString();
        pkg.description.clear();
        pkg.downloadSize = 0;
        pkg.operation = QStringLiteral("Remove");
        // Distinguish automatic removals (e.g. orphaned dependencies) from
        // explicit ones for the UI.
        const QJsonArray details = obj.value(QStringLiteral("details")).toArray();
        for (const QJsonValue &detail : details) {
            if (detail.toString() == QStringLiteral("AutoRemove")) {
                pkg.automatic = true;
                break;
            }
        }

        m_updates.append(pkg);
    }
}

void AmoClient::handleResult(const QString &json)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        const QString error = i18nd(kTranslationDomain,
                                    "Failed to parse result: %1",
                                    parseError.errorString());
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
