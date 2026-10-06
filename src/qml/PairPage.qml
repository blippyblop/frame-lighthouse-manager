import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.Page {
    id: page

    title: qsTr("Pair a new device")

    actions: [
        Kirigami.Action {
            icon.name: "arrow-back"
            text: qsTr("Back")
            onTriggered: pageStack.pop()
        },
        Kirigami.Action {
            icon.name: bleManager.scanning ? "process-stop" : "find"
            text: bleManager.scanning ? qsTr("Stop scanning") : qsTr("Scan")
            onTriggered: bleManager.scanning ? bleManager.stopScan() : bleManager.startScan()
        }
    ]

    Kirigami.ColumnView {
        Kirigami.Heading {
            text: qsTr("Scanning for lighthouses... Make sure your lighthouses are powered on. "
                      + "Version 2.0 lighthouses are named 'LHB-...' and Vive base stations 'HTC BS ...'.")
            wrapMode: Text.WordWrap
        }
        Kirigami.Separator {}

        Kirigami.ColumnView {
            Layout.fillHeight: true
            ListView {
                id: pairList
                model: pairListModel
                delegate: Item {
                    required property var model
                    height: Kirigami.Units.gridSize * 2
                    Kirigami.ColumnView {
                        RowLayout {
                            Layout.fillWidth: true
                            Kirigami.Heading {
                                text: model.deviceName
                                level: 2
                            }
                            Text {
                                text: model.deviceId
                                color: Kirigami.Theme.disabledTextColor
                            }
                            QQC2.ToolButton {
                                icon.name: "bluetooth"
                                text: qsTr("Pair")
                                onClicked: {
                                    var ok = bleManager.pairDevice(model.deviceId)
                                    pairHint.text = ok
                                        ? qsTr("Pairing request sent to BlueZ for %1").arg(model.deviceId)
                                        : qsTr("Pairing failed for %1").arg(model.deviceId)
                                    pairHint.visible = true
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    ListModel {
        id: pairListModel

        function refresh() {
            clear()
            var ids = bleManager.knownDeviceIds()
            for (var i = 0; i < ids.length; i++) {
                var dev = bleManager.deviceById(ids[i])
                append({ "deviceId": ids[i], "deviceName": dev ? dev.displayName : ids[i] })
            }
        }
    }

    Kirigami.InlineMessage {
        id: pairHint
        text: ""
    }

    Connections {
        target: bleManager
        function onDevicesChanged() {
            pairListModel.refresh()
        }
    }
    Component.onCompleted: pairListModel.refresh()
}
