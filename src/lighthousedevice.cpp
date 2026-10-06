#include "lighthousedevice.h"

#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QLowEnergyCharacteristic>
#include <QBluetoothUuid>

namespace {
// Standard GATT Device Information Service characteristic UUIDs.
constexpr char MODEL_NUMBER[] = "00002a24-0000-1000-8000-00805f9b34fb";
constexpr char SERIAL_NUMBER[] = "00002a25-0000-1000-8000-00805f9b34fb";
constexpr char FIRMWARE_REVISION[] = "00002a26-0000-1000-8000-00805f9b34fb";
constexpr char HARDWARE_REVISION[] = "00002a27-0000-1000-8000-00805f9b34fb";
constexpr char MANUFACTURER_NAME[] = "00002a29-0000-1000-8000-00805f9b34fb";

constexpr char POWER_STATE_UNKNOWN_CHAR[] = "0000";
} // namespace

LighthouseDevice::LighthouseDevice(const QString &id, const QString &name, QObject *parent)
    : QObject(parent), m_id(id), m_name(name)
{
    m_pollTimer.setInterval(minUpdateInterval());
    connect(&m_pollTimer, &QTimer::timeout, this, [this] {
        if (m_connected && m_controller) {
            pollState(m_controller);
        }
    });
}

LighthouseDevice::~LighthouseDevice()
{
    // Receiver-side connections are dropped automatically on destruction.
}

QString LighthouseDevice::displayName() const
{
    return m_nickname.isEmpty() ? m_name : m_nickname;
}

QString LighthouseDevice::nickname() const
{
    return m_nickname;
}

void LighthouseDevice::setNickname(const QString &nickname)
{
    if (m_nickname != nickname) {
        m_nickname = nickname;
        Q_EMIT displayNameChanged();
    }
}

QLowEnergyService *LighthouseDevice::serviceFor(const QBluetoothUuid &uuid) const
{
    if (!m_controller) {
        return nullptr;
    }
    for (const QBluetoothUuid &serviceUuid : m_controller->services()) {
        if (serviceUuid == uuid) {
            return m_controller->createServiceObject(uuid);
        }
    }
    return nullptr;
}

QLowEnergyService *LighthouseDevice::serviceForCharacteristic(const QBluetoothUuid &charUuid) const
{
    if (!m_controller) {
        return nullptr;
    }
    for (const QBluetoothUuid &serviceUuid : m_controller->services()) {
        QLowEnergyService *service = m_controller->createServiceObject(serviceUuid);
        if (service->characteristic(charUuid).uuid() == charUuid) {
            return service;
        }
    }
    return nullptr;
}

QLowEnergyCharacteristic LighthouseDevice::characteristicFor(const QBluetoothUuid &uuid) const
{
    if (!m_controller) {
        return {};
    }
    for (const QBluetoothUuid &serviceUuid : m_controller->services()) {
        QLowEnergyService *service = m_controller->createServiceObject(serviceUuid);
        const QLowEnergyCharacteristic characteristic = service->characteristic(uuid);
        if (characteristic.uuid() == uuid) {
            return characteristic;
        }
    }
    return {};
}

void LighthouseDevice::readStringCharacteristic(QLowEnergyController *controller, const QByteArray &uuid,
                                                 const QString &metaKey)
{
    const QBluetoothUuid characteristicUuid(uuid);
    for (const QBluetoothUuid &serviceUuid : controller->services()) {
        QLowEnergyService *service = controller->createServiceObject(serviceUuid);
        const QLowEnergyCharacteristic characteristic = service->characteristic(characteristicUuid);
        if (characteristic.uuid() != characteristicUuid) {
            continue;
        }
        service->discoverDetails();
        // Read is async; the value arrives via characteristicRead.
        connect(service, &QLowEnergyService::characteristicRead, this,
                [this, characteristicUuid, metaKey](const QLowEnergyCharacteristic &c, const QByteArray &) {
                    if (c.uuid() == characteristicUuid) {
                        QString value = QString::fromUtf8(c.value()).trimmed();
                        m_metadata.insert(metaKey, value);
                        if (metaKey == QStringLiteral("Firmware version")) {
                            m_firmwareVersion = value;
                        }
                        Q_EMIT metadataChanged();
                    }
                });
        service->readCharacteristic(characteristic);
        return;
    }
}

void LighthouseDevice::setPowerState(int byte)
{
    if (m_powerStateByte != byte) {
        m_powerStateByte = byte;
        m_powerState = static_cast<int>(powerStateFromByte(byte));
        Q_EMIT powerStateChanged();
    }
}

void LighthouseDevice::connectToDevice(QLowEnergyController *controller)
{
    if (m_controller) {
        return;
    }
    m_controller = controller;
    connect(controller, &QLowEnergyController::connected, this, [this] {
        m_connected = true;
        Q_EMIT connectedChanged();
        if (m_controller) {
            m_controller->discoverServices();
        }
    });
    connect(controller, &QLowEnergyController::disconnected, this, [this] {
        m_connected = false;
        m_valid = false;
        m_pollTimer.stop();
        Q_EMIT connectedChanged();
        onDisconnected();
        m_controller = nullptr;
    });
    connect(controller, &QLowEnergyController::discoveryFinished, this, [this] {
        if (m_controller && onServicesDiscovered(m_controller)) {
            m_valid = true;
            m_pollTimer.start();
        } else {
            if (m_controller) {
                m_controller->disconnectFromDevice();
            }
        }
    });
    connect(controller, &QLowEnergyController::serviceDiscovered, this, [this](const QBluetoothUuid &uuid) {
        if (!m_valid && m_controller) {
            // Metadata characteristics may arrive with each service; collect on discoveryFinished instead.
        }
        Q_UNUSED(uuid);
    });
    controller->connectToDevice();
}

void LighthouseDevice::disconnect()
{
    m_pollTimer.stop();
    if (m_controller && m_controller->state() != QLowEnergyController::UnconnectedState) {
        m_controller->disconnectFromDevice();
    }
    m_connected = false;
    Q_EMIT connectedChanged();
    m_valid = false;
}

bool LighthouseDevice::changeState(int newState)
{
    Q_UNUSED(newState);
    return false;
}

void LighthouseDevice::identify()
{
}

bool LighthouseDevice::onServicesDiscovered(QLowEnergyController *controller)
{
    // Read standard metadata characteristics (best effort).
    readStringCharacteristic(controller, MODEL_NUMBER, QStringLiteral("Model number"));
    readStringCharacteristic(controller, SERIAL_NUMBER, QStringLiteral("Serial number"));
    readStringCharacteristic(controller, FIRMWARE_REVISION, QStringLiteral("Firmware version"));
    readStringCharacteristic(controller, HARDWARE_REVISION, QStringLiteral("Hardware revision"));
    readStringCharacteristic(controller, MANUFACTURER_NAME, QStringLiteral("Manufacturer name"));
    return true;
}

void LighthouseDevice::pollState(QLowEnergyController *controller)
{
    Q_UNUSED(controller);
}

void LighthouseDevice::onDisconnected()
{
}
