#pragma once

#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariantMap>

// Thin wrapper around BlueZ D-Bus for one remote device
// (object path like /org/bluez/hci0/dev_aa_bb_cc_dd_ee_ff).
//
// Handles the GATT lifecycle (connect, service discovery, characteristic
// read/write) over org.bluez.Device1 / GattDevice1 / GattService1 /
// GattCharacteristic1, without depending on QtBluetooth (which the
// SteamOS Frame does not ship).
class BluezDevice : public QObject
{
    Q_OBJECT
public:
    explicit BluezDevice(const QString &objectPath, QObject *parent = nullptr);

    QString objectPath() const { return m_objectPath; }
    QString address() const { return m_address; }
    QString name() const { return m_name; }

    bool gattConnected() const { return m_gattConnected; }
    bool discoveryDone() const { return m_discoveryDone; }

    void connectGatt();
    void disconnectGatt();

    bool hasCharacteristic(const QString &uuid) const
    {
        return m_charPathsByUuid.contains(uuid);
    }
    QString characteristicPath(const QString &uuid) const
    {
        return m_charPathsByUuid.value(uuid);
    }
    // Characteristic with the given uuid inside the service with the given uuid
    // (empty string if not found).
    QString characteristicPathInService(const QString &serviceUuid, const QString &uuid) const;
    QString servicePath(const QString &serviceUuid) const
    {
        return m_servicePathsByUuid.value(serviceUuid);
    }

    // Async operations; no-op when not connected or when the characteristic
    // is not indexed yet.
    void readCharacteristic(const QString &uuid);
    void writeCharacteristic(const QString &uuid, const QByteArray &value, bool withoutResponse = true);
    void writeCharacteristicPath(const QString &charPath, const QByteArray &value, bool withoutResponse = true);

public slots:
    // Old-style slots for D-Bus signals (the firmware's Qt6 only supports
    // QDBusConnection::connect with const char* slots).
    void onDevicePropertiesChanged(const QString &iface, const QVariantMap &changed, const QStringList &invalidated);
    void onInterfacesAdded(const QString &path, const QVariantMap &interfaces);
    void onInterfacesRemoved(const QString &path, const QStringList &interfaces);

Q_SIGNALS:
    void gattConnectedChanged(bool connected);
    void discoveryFinished();
    void characteristicRead(const QString &uuid, const QByteArray &value);
    void nameChanged(const QString &name);

private:
    void setGattConnected(bool connected);
    void requestServiceDiscovery();
    void indexManagedObjects();
    void addInterfaceObjects(const QString &path, const QVariantMap &interfaces);
    void doWrite(const QString &charPath, const QByteArray &value, const QVariantMap &options, bool isRetry);
    void updateName();

    QString m_objectPath;
    QString m_address;
    QString m_name;
    bool m_gattConnected = false;
    bool m_discoveryDone = false;
    bool m_connectRequested = false;

    QHash<QString, QString> m_servicePathsByUuid;
    QHash<QString, QString> m_charPathsByUuid;
    QHash<QString, QString> m_charServiceByUuid;

    QTimer m_settleTimer;
};

// Static helpers for the BlueZ manager/adapter.
namespace Bluez
{
    // Object path of the default adapter ("" if none).
    QString defaultAdapterPath();
    bool adapterPowered(const QString &adapterPath);
}
