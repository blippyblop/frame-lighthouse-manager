#pragma once

#include <QHash>
#include <QPointer>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "settingsstore.h"

class BluezDevice;
class LighthouseDevice;
class ViveBaseStationDevice;

// Manages BLE scanning and the set of known lighthouse devices.
// Scanning and GATT go through BlueZ D-Bus directly (the SteamOS Frame
// does not ship QtBluetooth).
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

public slots:
    // Old-style slots for D-Bus signals (the firmware's Qt6 only supports
    // QDBusConnection::connect with const char* slots).
    void onAdapterPropertiesChanged(const QString &iface, const QVariantMap &changed, const QStringList &invalidated);
    void onManagerInterfacesAdded(const QString &path, const QVariantMap &interfaces);
    void onManagerInterfacesRemoved(const QString &path, const QStringList &interfaces);

Q_SIGNALS:
    void scanningChanged();
    void adapterOnChanged();
    void devicesChanged();

private:
    void handleDiscoveredDevice(const QString &name, const QString &address);
    LighthouseDevice *createDevice(const QString &name, const QString &address);
    BluezDevice *bluezFor(const QString &address);
    void seedKnownDevices();
    void refreshAdapterState();

    SettingsStore m_settings;
    QHash<QString, QPointer<LighthouseDevice>> m_devices;
    QHash<QString, QString> m_devicePaths;   // lowercase address -> BlueZ object path
    QHash<QString, QPointer<BluezDevice>> m_bluezDevices; // lowercase address -> wrapper
    QString m_adapterPath;
    bool m_scanning = false;
    bool m_adapterOn = false;
};
