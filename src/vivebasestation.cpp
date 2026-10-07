#include "vivebasestation.h"

namespace {
constexpr uchar COMMAND_HEADER = 0x12;
constexpr uchar ACTION_ON = 0x00;
constexpr uchar ACTION_SLEEP = 0x02;
} // namespace

ViveBaseStationDevice::ViveBaseStationDevice(const QString &id, const QString &name, QObject *parent)
    : LighthouseDevice(id, name, parent)
{
    m_deviceType = QStringLiteral("Vive base station");

    // "HTC BS XXXX" -> the last 4 hex digits are the pair id end hint.
    const int suffixStart = name.length() - 4;
    if (suffixStart >= 0 && suffixStart < name.length()) {
        bool ok = false;
        const int value = name.mid(suffixStart).toUInt(&ok, 16);
        if (ok) {
            m_pairIdHint = name.mid(suffixStart).toUpper();
        }
    }
}

QString ViveBaseStationDevice::pairIdHint() const
{
    return m_pairIdHint;
}

int ViveBaseStationDevice::pairId() const
{
    return m_pairId;
}

void ViveBaseStationDevice::setPairId(int id)
{
    if (m_pairId == id) {
        return;
    }
    m_pairId = id;
    if (id >= 0) {
        m_metadata.insert(QStringLiteral("Id"),
                           QStringLiteral("0x%1").arg(id, 8, 16, QLatin1Char('0')).toUpper());
    } else {
        m_metadata.insert(QStringLiteral("Id"), QString());
    }
    Q_EMIT metadataChanged();
    Q_EMIT pairIdChanged();
}

bool ViveBaseStationDevice::hasPairId() const
{
    return m_pairId >= 0;
}

bool ViveBaseStationDevice::onServicesDiscovered()
{
    if (!LighthouseDevice::onServicesDiscovered()) {
        return false;
    }
    if (!hasCharacteristic(POWER_CHARACTERISTIC)) {
        return false;
    }
    return true;
}

void ViveBaseStationDevice::pollState()
{
    // No state readback for Vive base stations: the state is derived from
    // whether a pair id is stored.
    const Power::State state = hasPairId() ? Power::Unknown : Power::Booting;
    if (m_powerState != static_cast<int>(state)) {
        m_powerState = static_cast<int>(state);
        Q_EMIT powerStateChanged();
    }
}

bool ViveBaseStationDevice::changeState(int newState)
{
    if (!m_bluez || !m_bluez->gattConnected()) {
        return false;
    }
    QString charPath = characteristicPathInService(POWER_SERVICE, POWER_CHARACTERISTIC);
    if (charPath.isEmpty()) {
        charPath = m_bluez->characteristicPath(POWER_CHARACTERISTIC);
    }
    if (charPath.isEmpty()) {
        return false;
    }
    if (!hasPairId()) {
        return false;
    }

    QByteArray command(20, '\0');
    command[0] = COMMAND_HEADER;
    quint16 subCommand = 0;
    uchar action = ACTION_ON;
    switch (newState) {
    case Power::On:
        action = ACTION_ON;
        subCommand = 0x0000;
        break;
    case Power::Sleep:
        action = ACTION_SLEEP;
        subCommand = 0x0001;
        break;
    default:
        return false;
    }
    command[1] = action;
    command[2] = static_cast<char>(subCommand >> 8);
    command[3] = static_cast<char>(subCommand & 0xff);
    quint32 idLE = m_pairId;
    command[4] = static_cast<char>(idLE & 0xff);
    command[5] = static_cast<char>((idLE >> 8) & 0xff);
    command[6] = static_cast<char>((idLE >> 16) & 0xff);
    command[7] = static_cast<char>((idLE >> 24) & 0xff);

    writeCharacteristicPath(charPath, command, true);
    return true;
}
