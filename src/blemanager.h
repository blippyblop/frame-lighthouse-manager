#pragma once

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QStringList>

#include "settingsstore.h"

class QBluetoothDeviceDiscoveryAgent;
class QBluetoothDeviceInfo;
class QBluetoothLocalDevice;
class QLowEnergyController;
class LighthouseDevice;
class ViveBaseStationDevice;

// Manages BLE scanning and the set of known lighthouse devices.
class BleManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(bool adapterOn READ adapterOn NOTIFY adapterOnChanged)
    Q_PROPERTY(QStringList knownDeviceIds READ knownDeviceIds NOTIFY devicesChanged)

public:
    explicit BleManager(QObject *parent = nullptr);

    SettingsStore &settings() { return m_settings; }
    const SettingsStore &settings() const { return m_settings; }

    bool scanning() const { return m_scanning; }
    bool adapterOn() const { return m_adapterOn; }
    QStringList knownDeviceIds() const;

    LighthouseDevice *deviceById(const QString &id) const;

    // All known devices (currently discovered + last seen).
    QList<LighthouseDevice *> devices() const;

    Q_INVOKABLE void startScan();
    Q_INVOKABLE void stopScan();

    // Pair a device through BlueZ (delegation to the OS).
    Q_INVOKABLE bool pairDevice(const QString &address);

    Q_INVOKABLE void setNickname(const QString &deviceId, const QString &nickname);

    // Ask a Vive base station to store its pair id (from the label).
    Q_INVOKABLE void setVivePairId(const QString &deviceId, const QString &hexId);
    Q_INVOKABLE void clearVivePairId(const QString &deviceId);
    Q_INVOKABLE QString vivePairIdHint(const QString &deviceId) const;
    Q_INVOKABLE QString vivePairIdHex(const QString &deviceId) const;

Q_SIGNALS:
    void scanningChanged();
    void adapterOnChanged();
    void devicesChanged();

private:
    void onDeviceDiscovered(const QBluetoothDeviceInfo &info);
    void onDiscoveryFinished();
    LighthouseDevice *createDevice(const QBluetoothDeviceInfo &info);
    void refreshAdapterState();

    SettingsStore m_settings;
    QHash<QString, QPointer<LighthouseDevice>> m_devices;
    QPointer<QBluetoothDeviceDiscoveryAgent> m_discoveryAgent;
    QBluetoothLocalDevice *m_localDevice = nullptr;
    bool m_scanning = false;
    bool m_adapterOn = false;
};
