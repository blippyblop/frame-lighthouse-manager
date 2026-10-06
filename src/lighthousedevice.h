#pragma once

#include <QObject>
#include <QVariantMap>
#include <QPointer>
#include <QTimer>
#include <QBluetoothUuid>
#include <QLowEnergyCharacteristic>

#include "powerstate.h"

class QLowEnergyController;
class QLowEnergyService;

// Base class for a lighthouse that can be controlled over BLE.
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

    void connectToDevice(QLowEnergyController *controller);
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
    // Called once the controller has finished service discovery.
    virtual bool onServicesDiscovered(QLowEnergyController *controller);
    // Called on every poll tick while connected; subclasses read state.
    virtual void pollState(QLowEnergyController *controller);
    // Called when the device disconnects.
    virtual void onDisconnected();

    QLowEnergyService *serviceFor(const QBluetoothUuid &uuid) const;
    // Find the service that owns the characteristic with the given UUID.
    QLowEnergyService *serviceForCharacteristic(const QBluetoothUuid &charUuid) const;
    QLowEnergyCharacteristic characteristicFor(const QBluetoothUuid &uuid) const;
    void readStringCharacteristic(QLowEnergyController *controller, const QByteArray &uuid, const QString &metaKey);

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

    QPointer<QLowEnergyController> m_controller;
    QPointer<QLowEnergyService> m_service;
    QLowEnergyCharacteristic m_powerCharacteristic;
    QLowEnergyCharacteristic m_identifyCharacteristic;
    QTimer m_pollTimer;
    bool m_valid = false;
};
