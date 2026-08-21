/***************************************************************************
 *   Copyright (C) 2024 by Amo Updates contributors                        *
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

#include "amoclient.h"

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
    Q_PROPERTY(QString iconName READ iconName NOTIFY updatesChanged)
    Q_PROPERTY(bool isSystemUpToDate READ isSystemUpToDate NOTIFY updatesChanged)
    Q_PROPERTY(QString timestamp READ timestamp NOTIFY updatesChanged)
    Q_PROPERTY(int percentage READ percentage NOTIFY percentageChanged)
    Q_PROPERTY(bool isNetworkOnline READ isNetworkOnline NOTIFY networkStateChanged)
    Q_PROPERTY(bool isNetworkMobile READ isNetworkMobile CONSTANT)
    Q_PROPERTY(bool isOnBattery READ isOnBattery NOTIFY isOnBatteryChanged)
    Q_PROPERTY(bool lastCheckSuccessful READ lastCheckSuccessful NOTIFY updatesChanged)
    Q_PROPERTY(QStringList packages READ packages NOTIFY updatesChanged)

public:
    explicit AmoUpdates(QObject *parent = nullptr);

    int count() const { return m_client.updates().size(); }
    bool isActive() const { return m_active; }
    QString message() const { return m_message; }
    QString statusMessage() const { return m_statusMessage; }
    QString iconName() const;
    bool isSystemUpToDate() const { return count() == 0; }
    QString timestamp() const { return m_timestamp; }
    int percentage() const { return m_percentage; }
    bool isNetworkOnline() const { return m_networkOnline; }
    bool isNetworkMobile() const { return false; }
    bool isOnBattery() const { return m_onBattery; }
    bool lastCheckSuccessful() const { return m_lastCheckSuccessful; }
    QStringList packages() const;

    // ---- Methods callable from QML ----
    Q_INVOKABLE void checkUpdates(bool manual);
    Q_INVOKABLE void installUpdates(const QStringList &packageIds);
    Q_INVOKABLE void installAllUpdates();
    Q_INVOKABLE QString packageName(const QString &packageId) const;
    Q_INVOKABLE QString packageVersion(const QString &packageId) const;
    Q_INVOKABLE QString packageDescription(const QString &packageId) const;
    Q_INVOKABLE qint64 packageDownloadSize(const QString &packageId) const;
    Q_INVOKABLE QString packageOperation(const QString &packageId) const;
    Q_INVOKABLE qint64 totalDownloadSize() const;
    Q_INVOKABLE qint64 diskSizeDelta() const;
    Q_INVOKABLE double lastRefreshTimestamp() const;

signals:
    void updatesChanged();
    void activeChanged();
    void messageChanged();
    void statusMessageChanged();
    void percentageChanged();
    void networkStateChanged();
    void isOnBatteryChanged();
    void updatesInstalled();
    void updateError(const QString &message);

private slots:
    void onUpdatesListed(const QList<UpdatePackage> &updates,
                         qint64 totalDownloadSize,
                         qint64 diskSizeDelta);
    void onRefreshFinished(bool success, const QString &error);
    void onApplyFinished(bool success, const QString &error);
    void onStatusChanged(const QString &statusJson);

private:
    void setActive(bool active);
    void setMessage(const QString &message);
    void setStatusMessage(const QString &message);
    void setPercentage(int percentage);
    void setNetworkOnline(bool online);
    void setOnBattery(bool onBattery);
    void setTimestamp(const QString &timestamp);
    void setLastCheckSuccessful(bool ok);

    QString findEventMessage(const QJsonObject &obj) const;
    int findEventPercent(const QJsonObject &obj) const;

    AmoClient m_client;
    bool m_active = false;
    QString m_message;
    QString m_statusMessage;
    QString m_timestamp;
    int m_percentage = 0;
    bool m_networkOnline = true;
    bool m_onBattery = false;
    bool m_lastCheckSuccessful = false;
    double m_lastRefreshTimestamp = 0.0;
    QHash<QString, UpdatePackage> m_packageMap;
};
