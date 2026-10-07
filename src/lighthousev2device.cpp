#include "lighthousev2device.h"

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

bool LighthouseV2Device::onServicesDiscovered()
{
    if (!LighthouseDevice::onServicesDiscovered()) {
        return false;
    }

    if (!hasCharacteristic(POWER_CHARACTERISTIC)) {
        return false;
    }

    // State read results arrive through onCharacteristicRead; the channel is
    // a little-endian uint32 read once here.
    if (hasCharacteristic(CHANNEL_CHARACTERISTIC)) {
        readCharacteristic(CHANNEL_CHARACTERISTIC);
    }
    return true;
}

void LighthouseV2Device::onCharacteristicRead(const QString &uuid, const QByteArray &value)
{
    LighthouseDevice::onCharacteristicRead(uuid, value);

    if (uuid == QLatin1String(POWER_CHARACTERISTIC) && !value.isEmpty()) {
        setPowerState(static_cast<int>(static_cast<quint8>(value.constData()[0])));
    }
    if (uuid == QLatin1String(CHANNEL_CHARACTERISTIC) && value.size() >= 4) {
        const quint32 channelNumber = qFromLittleEndian<quint32>(
            reinterpret_cast<const uchar *>(value.constData()));
        m_metadata.insert(QStringLiteral("Channel"), QString::number(channelNumber));
        Q_EMIT metadataChanged();
    }
}

void LighthouseV2Device::pollState()
{
    if (hasCharacteristic(POWER_CHARACTERISTIC)) {
        readCharacteristic(POWER_CHARACTERISTIC);
    }
}

bool LighthouseV2Device::changeState(int newState)
{
    if (!m_bluez || !m_bluez->gattConnected()) {
        return false;
    }
    QString charPath = characteristicPathInService(CONTROL_SERVICE, POWER_CHARACTERISTIC);
    if (charPath.isEmpty()) {
        charPath = m_bluez->characteristicPath(POWER_CHARACTERISTIC);
    }
    if (charPath.isEmpty()) {
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
    writeCharacteristicPath(charPath, QByteArray(1, static_cast<char>(byte)), true);
    return true;
}

void LighthouseV2Device::identify()
{
    if (!m_bluez || !m_bluez->gattConnected()) {
        return;
    }
    QString charPath = characteristicPathInService(CONTROL_SERVICE, IDENTIFY_CHARACTERISTIC);
    if (charPath.isEmpty()) {
        charPath = m_bluez->characteristicPath(IDENTIFY_CHARACTERISTIC);
    }
    if (charPath.isEmpty()) {
        return;
    }
    // Writing any byte starts the identify (LED blink) action.
    writeCharacteristicPath(charPath, QByteArray(1, '\0'), true);
}
