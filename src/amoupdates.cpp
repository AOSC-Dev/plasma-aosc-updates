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
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDBusArgument>
#include <QLocale>
#include <QDebug>

#include <KNotification>
#include <KLocalizedString>
#include <KConfigGroup>
#include <KSharedConfig>

#include <utility>

namespace {

QVariant unwrapDBusVariant(const QVariant &value)
{
    if (value.canConvert<QDBusVariant>())
        return qvariant_cast<QDBusVariant>(value).variant();
    return value;
}

}

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
    connect(&m_client, &AmoClient::descriptionsChanged,
            this, &AmoUpdates::onDescriptionsChanged);
    connect(&m_client, &AmoClient::updatesChanged,
            this, &AmoUpdates::onUpdatesChangedExternally);
    connect(&m_client, &AmoClient::errorOccurred,
            this, [this](const QString &msg) {
                setMessage(msg);
                setActive(false);
                setStatusMessage(QStringLiteral("Error"));
                setErrorMessage(msg);
            });

    QDBusConnection bus = QDBusConnection::systemBus();
    bus.connect(QStringLiteral("org.freedesktop.NetworkManager"),
                QStringLiteral("/org/freedesktop/NetworkManager"),
                QStringLiteral("org.freedesktop.DBus.Properties"),
                QStringLiteral("PropertiesChanged"),
                this,
                SLOT(onNetworkPropertiesChanged(QString,QVariantMap,QStringList)));
    bus.connect(QStringLiteral("org.freedesktop.UPower"),
                QStringLiteral("/org/freedesktop/UPower"),
                QStringLiteral("org.freedesktop.DBus.Properties"),
                QStringLiteral("PropertiesChanged"),
                this,
                SLOT(onPowerPropertiesChanged(QString,QVariantMap,QStringList)));
    refreshSystemState();

    setMessage(QStringLiteral("Idle"));
    setStatusMessage(QStringLiteral("Idle"));
    setErrorMessage(QString());
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
    m_isManualCheck = manual;

    if (m_active)
        return;

    setActive(true);
    resetProgress();
    setErrorMessage(QString());
    setStatusMessage(QStringLiteral("Checking for updates..."));
    m_client.refresh();
}

void AmoUpdates::installUpdates(const QStringList &packageIds)
{
    if (m_active || packageIds.isEmpty())
        return;

    setActive(true);
    resetProgress();
    setErrorMessage(QString());
    setStatusMessage(QStringLiteral("Installing updates..."));
    m_client.applyChanges(packageIds, QStringList(), false);
}

void AmoUpdates::installAllUpdates()
{
    if (m_active)
        return;

    setActive(true);
    resetProgress();
    setErrorMessage(QString());
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
    return m_packageMap.value(packageId).description;
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
    setErrorMessage(QString());
    setTimestamp(QLocale().toString(QDateTime::currentDateTime(), QLocale::ShortFormat));
    m_lastRefreshTimestamp = QDateTime::currentMSecsSinceEpoch() / 1000.0;

    if (updates.isEmpty()) {
        setMessage(QStringLiteral("System is up to date"));
    } else {
        setMessage(QStringLiteral("%1 update(s) available").arg(updates.size()));
        showUpdatesNotification(updates.size());
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
        setErrorMessage(error);
        emit updateError(error);
        // For automatic checks, transient failures (no network, another
        // package manager holding the lock, ...) shouldn't spam the user
        // with a notification on every occurrence. Only notify when the
        // failure persists across consecutive checks, mirroring the
        // behaviour of plasma-pk-updates.
        const bool notify = m_isManualCheck || maybeNotifyTransientError(error);
        if (notify)
            showErrorNotification(error);
        emit updatesChanged();
        return;
    }

    // A successful check resets the consecutive-failure counter.
    resetFailedAutoRefreshCount();

    // After a successful refresh, fetch the actual update list.
    m_client.fetchUpdates();
}

bool AmoUpdates::isTransientError(const QString &error)
{
    const QString lowered = error.toLower();
    return lowered.contains(QStringLiteral("network"))
        || lowered.contains(QStringLiteral("no such host"))
        || lowered.contains(QStringLiteral("resolve"))
        || lowered.contains(QStringLiteral("timed out"))
        || lowered.contains(QStringLiteral("timeout"))
        || lowered.contains(QStringLiteral("connection refused"))
        || lowered.contains(QStringLiteral("connection reset"))
        || lowered.contains(QStringLiteral("another task is already running"))
        || lowered.contains(QStringLiteral("lock"));
}

bool AmoUpdates::maybeNotifyTransientError(const QString &error)
{
    if (!isTransientError(error))
        return true;

    KConfigGroup grp(KSharedConfig::openConfig(QStringLiteral("plasma-amo-updates")),
                     QStringLiteral("General"));
    qint64 failCount = grp.readEntry(QStringLiteral("FailedAutoRefreshCount"), qint64(0));
    failCount += 1;
    grp.writeEntry(QStringLiteral("FailedAutoRefreshCount"), failCount);
    grp.sync();

    // Suppress the notification for the first transient failure only; if it
    // keeps happening, the user should be informed.
    return failCount > 1;
}

void AmoUpdates::resetFailedAutoRefreshCount()
{
    KConfigGroup grp(KSharedConfig::openConfig(QStringLiteral("plasma-amo-updates")),
                     QStringLiteral("General"));
    grp.writeEntry(QStringLiteral("FailedAutoRefreshCount"), qint64(0));
    grp.sync();
}

void AmoUpdates::onApplyFinished(bool success, const QString &error)
{
    if (success) {
        setMessage(QStringLiteral("Updates installed"));
        setStatusMessage(QStringLiteral("Refreshing update list..."));
        setErrorMessage(QString());
        resetProgress();
        emit updatesInstalled();
        showInstalledNotification();
        // Stay active while re-fetching the list, then refresh the badge.
        setActive(true);
        m_client.fetchUpdates();
    } else {
        setActive(false);
        setMessage(QStringLiteral("Update failed"));
        setStatusMessage(error);
        setErrorMessage(error);
        emit updateError(error);
        showErrorNotification(error);
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

    // Tagged oma events are externally tagged enum values. Depending on the
    // variant, the value is either an object (NewProgressBar/ProgressInc) or
    // a number (NewGlobalProgressBar/GlobalProgressAdd).
    if (obj.size() == 1) {
        const QString eventName = obj.begin().key();
        const QJsonValue eventValue = obj.begin().value();
        if (eventName != QStringLiteral("status")) {
            updateDownloadProgress(eventName, eventValue);

            const QJsonValue tagged = eventValue;
            if (tagged.isObject()) {
                const QString msg = findEventMessage(tagged.toObject());
                if (!msg.isEmpty()) {
                    setStatusMessage(msg);
                }
                const int percent = findEventPercent(tagged.toObject());
                if (percent >= 0) {
                    setPercentage(percent);
                }
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
        const int dpkgPercent = qBound(0, percentValue.toInt(), 100);
        if (m_hasGlobalDownloadProgress) {
            setPercentage(50 + dpkgPercent / 2);
        } else {
            setPercentage(dpkgPercent);
        }
    }
}

void AmoUpdates::resetProgress()
{
    m_downloadTotal = 0;
    m_downloaded = 0;
    m_hasGlobalDownloadProgress = false;
    setPercentage(0);
}

void AmoUpdates::updateDownloadProgress(const QString &eventName,
                                        const QJsonValue &eventValue)
{
    if (eventName == QStringLiteral("NewGlobalProgressBar")) {
        m_downloadTotal = eventValue.toVariant().toLongLong();
        m_downloaded = 0;
        m_hasGlobalDownloadProgress = m_downloadTotal > 0;
        setPercentage(0);
        return;
    }

    if (eventName == QStringLiteral("GlobalProgressAdd")) {
        m_downloaded += eventValue.toVariant().toLongLong();
    } else if (eventName == QStringLiteral("GlobalProgressSub")) {
        m_downloaded -= eventValue.toVariant().toLongLong();
    } else if (eventName == QStringLiteral("ProgressInc") && !m_hasGlobalDownloadProgress) {
        // Older/variant event streams may not include global progress events.
        // In that case, accumulate per-file increments as a fallback.
        if (eventValue.isObject()) {
            m_downloaded += eventValue.toObject().value(QStringLiteral("size"))
                                .toVariant().toLongLong();
        }
    } else {
        return;
    }

    m_downloaded = qMax<qint64>(0, m_downloaded);
    if (m_downloadTotal <= 0)
        return;

    m_downloaded = qMin(m_downloaded, m_downloadTotal);
    const int downloadPercent = qBound(
        0, static_cast<int>((m_downloaded * 100) / m_downloadTotal), 100);
    setPercentage(downloadPercent / 2);
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

void AmoUpdates::setErrorMessage(const QString &message)
{
    if (m_errorMessage == message)
        return;
    m_errorMessage = message;
    emit errorMessageChanged();
}

void AmoUpdates::onDescriptionsChanged()
{
    m_packageMap.clear();
    for (const UpdatePackage &pkg : m_client.updates())
        m_packageMap.insert(pkg.name, pkg);
    emit updatesChanged();
}

void AmoUpdates::onUpdatesChangedExternally()
{
    // amo's file watcher detected a change to dpkg status or apt lists.
    // Re-fetch the update list so the tray icon reflects the new state.
    if (!m_active)
        m_client.fetchUpdates();
    emit updatesChangedExternally();
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

void AmoUpdates::setNetworkMobile(bool mobile)
{
    if (m_networkMobile == mobile)
        return;
    m_networkMobile = mobile;
    emit networkStateChanged();
}

void AmoUpdates::setOnBattery(bool onBattery)
{
    if (m_onBattery == onBattery)
        return;
    m_onBattery = onBattery;
    emit isOnBatteryChanged();
}

void AmoUpdates::refreshSystemState()
{
    QDBusInterface networkManager(QStringLiteral("org.freedesktop.NetworkManager"),
                                   QStringLiteral("/org/freedesktop/NetworkManager"),
                                   QStringLiteral("org.freedesktop.DBus.Properties"),
                                   QDBusConnection::systemBus());
    const QDBusReply<QVariant> networkState = networkManager.call(
        QStringLiteral("Get"),
        QStringLiteral("org.freedesktop.NetworkManager"),
        QStringLiteral("State"));
    if (networkState.isValid()) {
        // NetworkManager considers local, site and global connectivity usable
        // for update checks.  Disconnected and connecting states are not.
        setNetworkOnline(unwrapDBusVariant(networkState.value()).toUInt() >= 50);
    }

    // A connection is considered mobile if any active connection is of type
    // "gsm" or "cdma" (WWAN / cellular modems).
    const QDBusReply<QVariant> activeConnections = networkManager.call(
        QStringLiteral("Get"),
        QStringLiteral("org.freedesktop.NetworkManager"),
        QStringLiteral("ActiveConnections"));
    bool mobile = false;
    if (activeConnections.isValid()) {
        const QVariant unwrapped = unwrapDBusVariant(activeConnections.value());
        QStringList paths;
        if (unwrapped.canConvert<QDBusArgument>()) {
            paths = qdbus_cast<QStringList>(qvariant_cast<QDBusArgument>(unwrapped));
        } else {
            paths = unwrapped.toStringList();
        }
        for (const QString &path : paths) {
            if (path.isEmpty())
                continue;
            QDBusInterface activeConnection(
                QStringLiteral("org.freedesktop.NetworkManager"),
                path,
                QStringLiteral("org.freedesktop.DBus.Properties"),
                QDBusConnection::systemBus());
            const QDBusReply<QVariant> type = activeConnection.call(
                QStringLiteral("Get"),
                QStringLiteral("org.freedesktop.NetworkManager.Connection.Active"),
                QStringLiteral("Type"));
            if (type.isValid()) {
                const QString typeStr = unwrapDBusVariant(type.value()).toString();
                if (typeStr == QStringLiteral("gsm") || typeStr == QStringLiteral("cdma")) {
                    mobile = true;
                    break;
                }
            }
        }
    }
    setNetworkMobile(mobile);

    QDBusInterface upower(QStringLiteral("org.freedesktop.UPower"),
                          QStringLiteral("/org/freedesktop/UPower"),
                          QStringLiteral("org.freedesktop.DBus.Properties"),
                          QDBusConnection::systemBus());
    const QDBusReply<QVariant> onBattery = upower.call(
        QStringLiteral("Get"),
        QStringLiteral("org.freedesktop.UPower"),
        QStringLiteral("OnBattery"));
    if (onBattery.isValid())
        setOnBattery(unwrapDBusVariant(onBattery.value()).toBool());
}

void AmoUpdates::onNetworkPropertiesChanged(const QString &interfaceName,
                                            const QVariantMap &changedProperties,
                                            const QStringList &invalidatedProperties)
{
    Q_UNUSED(invalidatedProperties);
    if (interfaceName != QStringLiteral("org.freedesktop.NetworkManager"))
        return;

    if (changedProperties.contains(QStringLiteral("State"))) {
        setNetworkOnline(unwrapDBusVariant(changedProperties.value(QStringLiteral("State"))).toUInt() >= 50);
    }

    // ActiveConnections changed (a connection was established or torn down),
    // so re-evaluate whether we're on a mobile connection.
    if (changedProperties.contains(QStringLiteral("ActiveConnections"))) {
        refreshSystemState();
    }
}

void AmoUpdates::onPowerPropertiesChanged(const QString &interfaceName,
                                          const QVariantMap &changedProperties,
                                          const QStringList &invalidatedProperties)
{
    Q_UNUSED(invalidatedProperties);
    if (interfaceName != QStringLiteral("org.freedesktop.UPower") ||
        !changedProperties.contains(QStringLiteral("OnBattery")))
        return;

    setOnBattery(unwrapDBusVariant(changedProperties.value(QStringLiteral("OnBattery"))).toBool());
}

void AmoUpdates::setTimestamp(const QString &timestamp)
{
    if (m_timestamp == timestamp)
        return;
    m_timestamp = timestamp;
}

void AmoUpdates::setLastCheckSuccessful(bool ok)
{
    if (m_lastCheckSuccessful == ok)
        return;
    m_lastCheckSuccessful = ok;
}

void AmoUpdates::showUpdatesNotification(int count)
{
    // Only notify when the number of available updates changed since the
    // last notification, so automatic checks don't spam the user.
    if (count == m_lastUpdateCount && m_lastNotification)
        return;

    if (m_lastNotification)
        m_lastNotification->close();

    m_lastUpdateCount = count;
    m_lastNotification = new KNotification(QStringLiteral("updatesAvailable"),
                                           KNotification::Persistent,
                                           this);
    m_lastNotification->setComponentName(QStringLiteral("plasma-amo-updates"));
    m_lastNotification->setTitle(i18n("Software Updates Available"));
    m_lastNotification->setText(i18np("You have %1 new update", "You have %1 new updates", count));
    m_lastNotification->setIconName(QStringLiteral("update-high"));
    KNotificationAction *openAction = m_lastNotification->addDefaultAction(i18n("Open"));
    connect(openAction, &KNotificationAction::activated, this, [this]() {
        emit openRequested();
    });
    connect(m_lastNotification, &KNotification::closed, this, [this]() {
        m_lastNotification = nullptr;
        m_lastUpdateCount = 0;
    });
    m_lastNotification->sendEvent();
}

void AmoUpdates::showErrorNotification(const QString &message)
{
    auto *notification = new KNotification(QStringLiteral("updateError"),
                                           KNotification::CloseOnTimeout,
                                           this);
    notification->setComponentName(QStringLiteral("plasma-amo-updates"));
    notification->setTitle(i18n("Update check failed"));
    notification->setText(message);
    notification->setIconName(QStringLiteral("dialog-error"));
    notification->sendEvent();
}

void AmoUpdates::showInstalledNotification()
{
    auto *notification = new KNotification(QStringLiteral("updatesInstalled"),
                                           KNotification::CloseOnTimeout,
                                           this);
    notification->setComponentName(QStringLiteral("plasma-amo-updates"));
    notification->setTitle(i18n("Updates installed"));
    notification->setText(i18n("The system has been updated successfully."));
    notification->setIconName(QStringLiteral("update-none"));
    notification->sendEvent();
}
