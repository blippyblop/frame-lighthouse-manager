#include "lighthousev2device.h"

#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QLowEnergyCharacteristic>
#include <QBluetoothUuid>

#include <QtEndian>

namespace {
// Power state bytes as reported/read on the power characteristic.
constexpr int BYTE_SLEEP = 0x00;
constexpr int BYTE_ON = 0x01;
constexpr int BYTE_STANDBY = 0x02;
constexpr int BYTE_STATE_ON = 0x0b;
constexpr int BYTE_STATE_BOOTING_1 = 0x01;
constexpr int BYTE_STATE_BOOTING_2 = 0x08;
constexpr int BYTE_STATE_BOOTING_3 = 0x09;
} // namespace

LighthouseV2Device::LighthouseV2Device(const QString &id, const QString &name, QObject *parent)
    : LighthouseDevice(id, name, parent)
{
    m_deviceType = QStringLiteral("Lighthouse v2");
}

Power::State LighthouseV2Device::powerStateFromByte(int byte) const
{
    switch (byte) {
    case BYTE_SLEEP: return Power::Sleep;
    case BYTE_STANDBY: return Power::Standby;
    case BYTE_STATE_ON: return Power::On;
    case BYTE_STATE_BOOTING_1:
    case BYTE_STATE_BOOTING_2:
    case BYTE_STATE_BOOTING_3: return Power::Booting;
    default: return Power::Unknown;
    }
}

bool LighthouseV2Device::onServicesDiscovered(QLowEnergyController *controller)
{
    if (!LighthouseDevice::onServicesDiscovered(controller)) {
        return false;
    }

    m_powerCharacteristic = characteristicFor(QBluetoothUuid(POWER_CHARACTERISTIC));
    if (m_powerCharacteristic.uuid() != QBluetoothUuid(POWER_CHARACTERISTIC)) {
        return false;
    }

    QLowEnergyService *service = serviceForCharacteristic(QBluetoothUuid(POWER_CHARACTERISTIC));
    if (service) {
        service->discoverDetails();
        // State read results arrive here; connect once.
        connect(service, &QLowEnergyService::characteristicRead, this,
                [this](const QLowEnergyCharacteristic &c, const QByteArray &) {
                    if (c.uuid() == QBluetoothUuid(POWER_CHARACTERISTIC) && !c.value().isEmpty()) {
                        setPowerState(static_cast<int>(static_cast<quint8>(c.value().constData()[0])));
                    }
                });
    }

    const QLowEnergyCharacteristic identify = characteristicFor(QBluetoothUuid(IDENTIFY_CHARACTERISTIC));
    if (identify.uuid() == QBluetoothUuid(IDENTIFY_CHARACTERISTIC)) {
        m_identifyCharacteristic = identify;
    }

    // Channel is a little-endian uint32.
    const QBluetoothUuid channelUuid(CHANNEL_CHARACTERISTIC);
    for (const QBluetoothUuid &serviceUuid : controller->services()) {
        QLowEnergyService *service = controller->createServiceObject(serviceUuid);
        const QLowEnergyCharacteristic channel = service->characteristic(channelUuid);
        if (channel.uuid() != channelUuid) {
            continue;
        }
        service->discoverDetails();
        connect(service, &QLowEnergyService::characteristicRead, this,
                [this, channelUuid](const QLowEnergyCharacteristic &c, const QByteArray &) {
                    if (c.uuid() == channelUuid && c.value().size() >= 4) {
                        const quint32 channelNumber = qFromLittleEndian<quint32>(
                            reinterpret_cast<const uchar *>(c.value().constData()));
                        m_metadata.insert(QStringLiteral("Channel"),
                                          QString::number(channelNumber));
                        Q_EMIT metadataChanged();
                    }
                });
        service->readCharacteristic(channel);
        break;
    }
    return true;
}

void LighthouseV2Device::pollState(QLowEnergyController *controller)
{
    Q_UNUSED(controller)
    if (m_powerCharacteristic.uuid() != QBluetoothUuid(POWER_CHARACTERISTIC)) {
        return;
    }
    QLowEnergyService *service = serviceForCharacteristic(QBluetoothUuid(POWER_CHARACTERISTIC));
    if (!service) {
        return;
    }
    service->readCharacteristic(m_powerCharacteristic);
}

bool LighthouseV2Device::changeState(int newState)
{
    if (m_powerCharacteristic.uuid() != QBluetoothUuid(POWER_CHARACTERISTIC) || !m_controller) {
        return false;
    }
    QLowEnergyService *service = serviceFor(QBluetoothUuid(CONTROL_SERVICE));
    if (!service) {
        service = serviceForCharacteristic(QBluetoothUuid(POWER_CHARACTERISTIC));
    }
    if (!service) {
        return false;
    }

    quint8 byte = 0;
    switch (newState) {
    case Power::On: byte = BYTE_ON; break;
    case Power::Sleep: byte = BYTE_SLEEP; break;
    case Power::Standby: byte = BYTE_STANDBY; break;
    case Power::Booting:
    case Power::Unknown:
        return false;
    default: return false;
    }
    service->writeCharacteristic(m_powerCharacteristic, QByteArray(1, static_cast<char>(byte)),
                                 QLowEnergyService::WriteWithoutResponse);
    return true;
}

void LighthouseV2Device::identify()
{
    if (m_identifyCharacteristic.uuid() != QBluetoothUuid(IDENTIFY_CHARACTERISTIC) || !m_controller) {
        return;
    }
    QLowEnergyService *service = serviceForCharacteristic(QBluetoothUuid(IDENTIFY_CHARACTERISTIC));
    if (!service) {
        return;
    }
    // Writing any byte starts the identify (LED blink) action.
    service->writeCharacteristic(m_identifyCharacteristic, QByteArray(1, '\0'),
                                 QLowEnergyService::WriteWithoutResponse);
}

void LighthouseV2Device::onDisconnected()
{
    m_powerCharacteristic = QLowEnergyCharacteristic();
    m_identifyCharacteristic = QLowEnergyCharacteristic();
}
