#pragma once

#include "lighthousedevice.h"

// HTC Vive base station, BLE names start with "HTC BS".
class ViveBaseStationDevice : public LighthouseDevice
{
    Q_OBJECT

public:
    static constexpr char POWER_SERVICE[] = "0000cb00-0000-1000-8000-00805f9b34fb";
    static constexpr char POWER_CHARACTERISTIC[] = "0000cb01-0000-1000-8000-00805f9b34fb";

    explicit ViveBaseStationDevice(const QString &id, const QString &name, QObject *parent = nullptr);

    // Last 4 hex digits of the BLE name; the user provides the first 4.
    QString pairIdHint() const;

    int pairId() const;
    void setPairId(int id);
    bool hasPairId() const;

    bool changeState(int newState) override;

Q_SIGNALS:
    void pairIdChanged();

protected:
    bool onServicesDiscovered() override;
    void pollState() override;

private:
    int m_pairId = -1;
    QString m_pairIdHint;
};
