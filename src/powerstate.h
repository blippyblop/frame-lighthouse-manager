#pragma once

#include <QObject>
#include <QString>

// Power state for lighthouses and base stations.
namespace Power {
enum State { Sleep = 0, On = 1, Unknown = 2, Booting = 3, Standby = 4 };

constexpr int indexOf(State s) { return static_cast<int>(s); }
constexpr State stateOf(int i) { return static_cast<State>(i); }

inline QString textOf(State s)
{
    switch (s) {
    case Sleep: return QStringLiteral("Sleep");
    case On: return QStringLiteral("On");
    case Unknown: return QStringLiteral("Unknown");
    case Booting: return QStringLiteral("Booting");
    case Standby: return QStringLiteral("Standby");
    }
    return QStringLiteral("Unknown");
}
} // namespace Power
