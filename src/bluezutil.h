#pragma once

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QString>
#include <QVariant>
#include <initializer_list>

// The firmware's Qt 6.8 QDBus only exposes the message-based call/asyncCall
// API (the Qt5 string-based overloads are gone), and
// QDBusConnection::connect only accepts old-style const char* slots. These
// helpers keep the BlueZ call sites readable.
namespace bluez
{
struct Reply
{
    bool ok = false;    // a normal reply arrived
    bool noReply = false; // the call produced no reply at all (e.g. timeout)
    QVariant value;     // first argument of the reply, if any
    QString error;      // error message, when the bus answered with one
};

inline Reply call(const QDBusConnection &connection, const QString &service,
                  const QString &path, const QString &iface, const QString &member,
                  const std::initializer_list<QVariant> &args = {})
{
    QDBusMessage message = QDBusMessage::createMethodCall(service, path, iface, member);
    for (const QVariant &arg : args) {
        message << arg;
    }
    const QDBusMessage reply = connection.call(message);
    Reply out;
    if (reply.type() == QDBusMessage::ReplyMessage) {
        out.ok = true;
        if (!reply.arguments().isEmpty()) {
            out.value = reply.arguments().first();
        }
    } else if (reply.type() == QDBusMessage::ErrorMessage) {
        out.error = reply.errorMessage();
    } else {
        out.noReply = true;
    }
    return out;
}

inline QDBusPendingCall asyncCall(const QDBusConnection &connection, const QString &service,
                                  const QString &path, const QString &iface, const QString &member,
                                  const std::initializer_list<QVariant> &args = {})
{
    QDBusMessage message = QDBusMessage::createMethodCall(service, path, iface, member);
    for (const QVariant &arg : args) {
        message << arg;
    }
    return connection.asyncCall(message);
}
} // namespace bluez
