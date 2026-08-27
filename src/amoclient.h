/***************************************************************************
 *   Copyright (C) 2024 by AOSC Updates contributors                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#pragma once

#include <QObject>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QJsonObject>
#include <QList>
#include <QString>

/**
 * @brief A single package that can be updated.
 *
 * Mirrors the relevant fields of oma's InstallEntry so the UI can
 * display name, version change, download size and operation type.
 */
struct UpdatePackage
{
    QString name;          // e.g. "firefox"
    QString oldVersion;    // empty for fresh installs
    QString newVersion;
    QString arch;
    QString description;
    qint64 downloadSize = 0;
    QString operation;     // "Upgrade", "Install", "ReInstall", "Downgrade"
    bool automatic = false;
};

/**
 * @brief Details for a topic update matched by oma-tum.
 */
struct TopicUpdate
{
    QString id;
    QString kind;
    QString name;
    QString caution;
    QStringList packages;
    QStringList topics;
    int packageCount = 0;
    bool security = false;
};

/**
 * @brief The result of a completed amo task.
 */
struct AmoResult
{
    quint64 requestId = 0;
    bool success = false;
    QString error;
};

/**
 * @brief Thin QDBus wrapper around the `io.aosc.Amo1` interface.
 *
 * amo runs as a system D-Bus service (io.aosc.Amo at /io/aosc/Amo) and
 * exposes methods to refresh package metadata, list available updates and
 * apply changes. This class turns those calls into Qt signals.
 */
class AmoClient : public QObject
{
    Q_OBJECT

public:
    explicit AmoClient(QObject *parent = nullptr);

    /**
     * @brief Whether the system D-Bus connection is available.
     */
    bool isAvailable() const;

    /**
     * @brief Request a metadata refresh. Emits refreshStarted with the
     *        request id, then refreshFinished when done.
     */
    void refresh();

    /**
     * @brief Fetch the list of available updates. Emits updatesListed.
     */
    void fetchUpdates();

    /**
     * @brief Apply changes. `install`/`remove` are package names,
     *        `upgradeAll` upgrades every available package.
     */
    void applyChanges(const QStringList &install,
                      const QStringList &remove,
                      bool upgradeAll);

    /**
     * @brief The last known list of available updates.
     */
    QList<UpdatePackage> updates() const { return m_updates; }

    /**
     * @brief Topic update manifests matching the current transaction.
     */
    QList<TopicUpdate> topicUpdates() const { return m_topicUpdates; }

    /**
     * @brief Whether any matched TUM covers the transaction, i.e. there are
     *        important updates (security or otherwise).
     */
    bool hasImportantUpdates() const { return m_hasImportantUpdates; }

    /**
     * @brief Whether any matched TUM marks the transaction as a security
     *        update.
     */
    bool hasSecurityUpdates() const { return m_hasSecurityUpdates; }

    /**
     * @brief Total download size of all available updates (bytes).
     */
    qint64 totalDownloadSize() const { return m_totalDownloadSize; }

    /**
     * @brief Disk size delta of all available updates (bytes).
     */
    qint64 diskSizeDelta() const { return m_diskSizeDelta; }

signals:
    void refreshStarted(quint64 requestId);
    void refreshFinished(bool success, const QString &error);
    void updatesListed(const QList<UpdatePackage> &updates,
                       qint64 totalDownloadSize,
                       qint64 diskSizeDelta);
    void descriptionsChanged();
    void applyStarted(quint64 requestId);
    void applyFinished(bool success, const QString &error);
    void statusChanged(const QString &statusJson);
    void errorOccurred(const QString &message);
    /// Emitted when amo's file watcher detects changes to dpkg status or
    /// apt lists (i.e. the set of available updates may have changed).
    void updatesChanged();

private slots:
    void onRefreshReply(QDBusPendingCallWatcher *watcher);
    void onUpdatesReply(QDBusPendingCallWatcher *watcher);
    void onApplyReply(QDBusPendingCallWatcher *watcher);
    void onDescriptionReply(QDBusPendingCallWatcher *watcher);
    void onStatusSignal(const QDBusMessage &message);
    void onResultReportSignal(const QDBusMessage &message);
    void onUpdatesChangedSignal(const QDBusMessage &message);
    /// Called when the amo D-Bus service name gains or loses an owner, i.e.
    /// the service was started, stopped or restarted. Fails any pending
    /// task whose result will never be delivered.
    void onNameOwnerChanged(const QString &name,
                            const QString &oldOwner,
                            const QString &newOwner);

private:
    void parseUpdates(const QString &json);
    void handleResult(const QString &json);

    enum class TaskType {
        None,
        Refresh,
        Apply,
    };

    QDBusInterface *m_iface;
    QList<UpdatePackage> m_updates;
    QList<TopicUpdate> m_topicUpdates;
    bool m_hasImportantUpdates = false;
    bool m_hasSecurityUpdates = false;
    qint64 m_totalDownloadSize = 0;
    qint64 m_diskSizeDelta = 0;
    int m_pendingDescriptions = 0;
    quint64 m_lastRequestId = 0;
    TaskType m_pendingTask = TaskType::None;
};
