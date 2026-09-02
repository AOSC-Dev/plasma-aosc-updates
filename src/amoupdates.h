/***************************************************************************
 *   Copyright (C) 2026 by AOSC-Dev                                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#pragma once

#include <QObject>
#include <QStringList>
#include <QHash>
#include <QTimer>
#include <QJsonObject>
#include <QPointer>

#include "amoclient.h"
#include "amojob.h"

class KNotification;
class KUiServerV2JobTracker;

/**
 * @brief Backend singleton exposed to QML as `AmoUpdates`.
 *
 * This mirrors the role of `PkUpdates` in plasma-pk-updates: it wraps the
 * amo D-Bus client and exposes a QML-friendly API for checking updates,
 * listing them, and applying them.
 */
class AmoUpdates : public QObject
{
    Q_OBJECT

    // ---- Properties consumed by the QML UI ----
    Q_PROPERTY(int count READ count NOTIFY updatesChanged)
    Q_PROPERTY(bool isActive READ isActive NOTIFY activeChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString iconName READ iconName NOTIFY updatesChanged)
    Q_PROPERTY(bool isSystemUpToDate READ isSystemUpToDate NOTIFY updatesChanged)
    Q_PROPERTY(QString timestamp READ timestamp NOTIFY updatesChanged)
    Q_PROPERTY(int percentage READ percentage NOTIFY percentageChanged)
    Q_PROPERTY(bool isNetworkOnline READ isNetworkOnline NOTIFY networkStateChanged)
    Q_PROPERTY(bool isNetworkMobile READ isNetworkMobile NOTIFY networkStateChanged)
    Q_PROPERTY(bool isOnBattery READ isOnBattery NOTIFY isOnBatteryChanged)
    Q_PROPERTY(bool lastCheckSuccessful READ lastCheckSuccessful NOTIFY updatesChanged)
    Q_PROPERTY(QStringList packages READ packages NOTIFY updatesChanged)
    Q_PROPERTY(QStringList topicUpdates READ topicUpdates NOTIFY updatesChanged)
    Q_PROPERTY(int topicUpdateCount READ topicUpdateCount NOTIFY updatesChanged)
    Q_PROPERTY(bool hasImportantUpdates READ hasImportantUpdates NOTIFY updatesChanged)
    Q_PROPERTY(bool hasSecurityUpdates READ hasSecurityUpdates NOTIFY updatesChanged)
    Q_PROPERTY(qint64 totalDownloadSize READ totalDownloadSize NOTIFY updatesChanged)
    Q_PROPERTY(qint64 diskSizeDelta READ diskSizeDelta NOTIFY updatesChanged)

public:
    explicit AmoUpdates(QObject *parent = nullptr);

    int count() const { return m_client.updates().size(); }
    bool isActive() const { return m_active; }
    QString message() const;
    QString statusMessage() const { return m_statusMessage; }
    QString errorMessage() const { return m_errorMessage; }
    QString iconName() const;
    bool isSystemUpToDate() const { return count() == 0; }
    QString timestamp() const { return m_timestamp; }
    int percentage() const { return m_percentage; }
    bool isNetworkOnline() const { return m_networkOnline; }
    bool isNetworkMobile() const { return m_networkMobile; }
    bool isOnBattery() const { return m_onBattery; }
    bool lastCheckSuccessful() const { return m_lastCheckSuccessful; }
    /// Qualified "name:arch" ids of all packages in the transaction.
    QStringList packages() const;
    QStringList topicUpdates() const;
    int topicUpdateCount() const { return m_client.topicUpdates().size(); }
    bool hasImportantUpdates() const { return m_client.hasImportantUpdates(); }
    bool hasSecurityUpdates() const { return m_client.hasSecurityUpdates(); }

    // ---- Methods callable from QML ----
    Q_INVOKABLE void checkUpdates(bool manual);
    Q_INVOKABLE void installUpdates(const QStringList &packageIds);
    Q_INVOKABLE void installAllUpdates();
    Q_INVOKABLE QString packageName(const QString &packageId) const;
    Q_INVOKABLE QString packageVersion(const QString &packageId) const;
    Q_INVOKABLE QString packageDescription(const QString &packageId) const;
    Q_INVOKABLE qint64 packageDownloadSize(const QString &packageId) const;
    Q_INVOKABLE QString packageOperation(const QString &packageId) const;
    Q_INVOKABLE QString topicUpdateName(const QString &topicId) const;
    Q_INVOKABLE QString topicUpdateCaution(const QString &topicId) const;
    Q_INVOKABLE bool topicUpdateIsSecurity(const QString &topicId) const;
    /// Whether any matched TUM lists the package as affected, i.e. it is
    /// part of a security update. \a packageId is the "name:arch" id from
    /// packages().
    Q_INVOKABLE bool packageIsSecurity(const QString &packageId) const;
    /// Whether any matched TUM lists the package as affected, i.e. it is
    /// part of an important update (security or otherwise). \a packageId is
    /// the "name:arch" id from packages().
    Q_INVOKABLE bool packageIsImportant(const QString &packageId) const;
    Q_INVOKABLE int topicUpdatePackageCount(const QString &topicId) const;
    Q_INVOKABLE QStringList topicUpdatePackages(const QString &topicId) const;
    Q_INVOKABLE QStringList topicUpdateTopics(const QString &topicId) const;
    /// Number of available updates that are part of a security TUM.
    int securityUpdateCount() const;
    qint64 totalDownloadSize() const;
    qint64 diskSizeDelta() const;
    Q_INVOKABLE double lastRefreshTimestamp() const;

signals:
    void updatesChanged();
    void activeChanged();
    void messageChanged();
    void statusMessageChanged();
    void errorMessageChanged();
    void percentageChanged();
    void networkStateChanged();
    void isOnBatteryChanged();
    void updatesInstalled();
    void updateError(const QString &message);
    /// Emitted when amo's file watcher detects changes (dpkg status / apt
    /// lists), i.e. the set of available updates may have changed.
    void updatesChangedExternally();
    /// Emitted when the user activates the "Open" action of the updates
    /// notification, so the QML side can expand the plasmoid.
    void openRequested();

private slots:
    void onUpdatesListed(const QList<UpdatePackage> &updates,
                         qint64 totalDownloadSize,
                         qint64 diskSizeDelta);
    void onRefreshFinished(bool success, const QString &error);
    void onApplyFinished(bool success, const QString &error);
    void onStatusChanged(const QString &statusJson);
    void onDescriptionsChanged();
    void onUpdatesChangedExternally();
    void onNetworkPropertiesChanged(const QString &interfaceName,
                                    const QVariantMap &changedProperties,
                                    const QStringList &invalidatedProperties);
    void onPowerPropertiesChanged(const QString &interfaceName,
                                  const QVariantMap &changedProperties,
                                  const QStringList &invalidatedProperties);

private:
    /// What the backend is currently doing; drives the computed `message`.
    enum class Activity {
        Idle,
        CheckingUpdates,
        InstallingUpdates,
    };

    void setActive(bool active);
    void setStatusMessage(const QString &message);
    void setErrorMessage(const QString &message);
    void setPercentage(int percentage);
    void setNetworkOnline(bool online);
    void setNetworkMobile(bool mobile);
    void setOnBattery(bool onBattery);
    void refreshSystemState();
    void setTimestamp(const QString &timestamp);
    void setLastCheckSuccessful(bool ok);
    void showUpdatesNotification(int count);
    void showErrorNotification(const QString &message);
    void showInstalledNotification();

    /// Create (or reuse) the KJob that mirrors the current operation and
    /// register it with the job tracker, so the notification applet shows a
    /// Dolphin-style progress notification.
    void startProgressJob(const QString &title);
    /// Forward the current progress to the job (no-op when no job is active).
    void updateProgressJob();
    /// Finish the job (success or failure).
    void finishProgressJob(bool success, const QString &error);

    /**
     * @brief Whether \a error is a transient failure (e.g. no network, a
     *        lock held by another package manager) that may resolve on its
     *        own, so automatic checks shouldn't spam the user.
     */
    static bool isTransientError(const QString &error);

    /**
     * @brief Decide whether a failed automatic check should show a
     *        notification. Transient failures are suppressed on the first
     *        occurrence, then notify once they persist.
     */
    bool maybeNotifyTransientError(const QString &error);

    /**
     * @brief Reset the consecutive-failure counter after a successful check.
     */
    void resetFailedAutoRefreshCount();

    QString findEventMessage(const QJsonObject &obj) const;
    int findEventPercent(const QJsonObject &obj) const;
    void resetProgress();
    void updateDownloadProgress(const QString &eventName, const QJsonValue &eventValue);

    AmoClient m_client;
    bool m_active = false;
    Activity m_activity = Activity::Idle;
    /// Whether at least one update check has completed (successfully or not)
    /// since the plasmoid started; distinguishes "not checked yet" from
    /// "last check failed" in the computed `message`.
    bool m_checkDone = false;
    QString m_statusMessage;
    QString m_errorMessage;
    QString m_timestamp;
    int m_percentage = 0;
    qint64 m_downloadTotal = 0;
    qint64 m_downloaded = 0;
    bool m_hasGlobalDownloadProgress = false;
    bool m_networkOnline = true;
    bool m_networkMobile = false;
    bool m_onBattery = false;
    bool m_lastCheckSuccessful = false;
    bool m_isManualCheck = false;
    double m_lastRefreshTimestamp = 0.0;
    int m_lastUpdateCount = 0;
    bool m_lastNotificationWasImportant = false;
    QPointer<KNotification> m_lastNotification;
    QHash<QString, UpdatePackage> m_packageMap;

    /// Job shown as a Dolphin-style progress notification while an
    /// operation is running; owned by the job tracker.
    AmoJob *m_progressJob = nullptr;
    KUiServerV2JobTracker *m_jobTracker = nullptr;
};
