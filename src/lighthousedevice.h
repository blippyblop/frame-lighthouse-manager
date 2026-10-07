#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantMap>

#include "bluez.h"
#include "powerstate.h"

// Base class for a lighthouse that can be controlled over BLE.
// The GATT side is driven through BlueZ D-Bus (see BluezDevice) since the
// SteamOS Frame does not ship QtBluetooth.
// Handles connection lifecycle, metadata reading and power polling.
class LighthouseDevice : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(QString displayName READ displayName NOTIFY displayNameChanged)
    Q_PROPERTY(QString deviceType READ deviceType CONSTANT)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(int powerState READ powerState NOTIFY powerStateChanged)
    Q_PROPERTY(QString powerStateText READ powerStateText NOTIFY powerStateChanged)
    Q_PROPERTY(int powerStateByte READ powerStateByte NOTIFY powerStateChanged)
    Q_PROPERTY(QString firmwareVersion READ firmwareVersion NOTIFY metadataChanged)
    Q_PROPERTY(QVariantMap metadata READ metadata NOTIFY metadataChanged)

public:
    explicit LighthouseDevice(const QString &id, const QString &name, QObject *parent = nullptr);
    ~LighthouseDevice() override;

    QString id() const { return m_id; }
    QString name() const { return m_name; }
    QString displayName() const;
    QString deviceType() const { return m_deviceType; }
    bool connected() const { return m_connected; }
    int powerState() const { return m_powerState; }
    QString powerStateText() const { return Power::textOf(Power::stateOf(m_powerState)); }
    int powerStateByte() const { return m_powerStateByte; }
    QString firmwareVersion() const { return m_firmwareVersion; }
    QVariantMap metadata() const { return m_metadata; }

    void setNickname(const QString &nickname);
    QString nickname() const;

    // Update the advertised name if it changed.
    void setNameIfNeeded(const QString &name)
    {
        if (!name.isEmpty() && name != m_name) {
            m_name = name;
            Q_EMIT nameChanged();
            Q_EMIT displayNameChanged();
        }
    }

    // Set the power state. Returns whether the request was accepted.
    Q_INVOKABLE virtual bool changeState(int newState);
    Q_INVOKABLE virtual void identify();

    void connectToDevice(BluezDevice *bluez);
    void disconnect();

    // Map a device specific state byte to a global state.
    virtual Power::State powerStateFromByte(int byte) const { Q_UNUSED(byte); return Power::Unknown; }
    // Poll interval in milliseconds.
    virtual int minUpdateInterval() const { return 1000; }

Q_SIGNALS:
    void nameChanged();
    void displayNameChanged();
    void connectedChanged();
    void powerStateChanged();
    void metadataChanged();

protected:
    // Called once GATT service discovery has completed.
    virtual bool onServicesDiscovered();
    // Called on every poll tick while connected; subclasses read state.
    virtual void pollState();
    // Called when the device disconnects.
    virtual void onDisconnected();
    // Called for every GATT characteristic value that was read.
    virtual void onCharacteristicRead(const QString &uuid, const QByteArray &value);

    bool hasCharacteristic(const QString &uuid) const;
    void readCharacteristic(const QString &uuid);
    bool writeCharacteristic(const QString &uuid, const QByteArray &value, bool withoutResponse = true);
    // Characteristic path inside a specific service ("" if not found there).
    QString characteristicPathInService(const QString &serviceUuid, const QString &uuid) const;
    void writeCharacteristicPath(const QString &charPath, const QByteArray &value, bool withoutResponse = true);

    void readStringCharacteristic(const QString &uuid, const QString &metaKey);

    void setPowerState(int byte);

    QString m_id;
    QString m_name;
    QString m_deviceType = QStringLiteral("Lighthouse");
    QString m_nickname;
    bool m_connected = false;
    int m_powerState = Power::Unknown;
    int m_powerStateByte = -1;
    QString m_firmwareVersion;
    QVariantMap m_metadata;
    QHash<QString, QString> m_metaKeysByUuid;

    QPointer<BluezDevice> m_bluez;
    QTimer m_pollTimer;
    bool m_valid = false;
};
