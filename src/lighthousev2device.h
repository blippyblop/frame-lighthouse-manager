#pragma once

#include "lighthousedevice.h"

// SteamVR Lighthouse (Base Station 2.0), BLE names start with "LHB-".
class LighthouseV2Device : public LighthouseDevice
{
    Q_OBJECT

public:
    static constexpr char POWER_CHARACTERISTIC[] = "00001525-1212-efde-1523-785feabcd124";
    static constexpr char CHANNEL_CHARACTERISTIC[] = "00001524-1212-efde-1523-785feabcd124";
    static constexpr char IDENTIFY_CHARACTERISTIC[] = "00008421-1212-efde-1523-785feabcd124";
    static constexpr char CONTROL_SERVICE[] = "00001523-1212-efde-1523-785feabcd124";

    explicit LighthouseV2Device(const QString &id, const QString &name, QObject *parent = nullptr);

    Power::State powerStateFromByte(int byte) const override;
    bool changeState(int newState) override;
    void identify() override;

protected:
    bool onServicesDiscovered(QLowEnergyController *controller) override;
    void pollState(QLowEnergyController *controller) override;
    void onDisconnected() override;
};
