import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.Page {
    id: page

    property string deviceId: ""
    property var device: deviceId.length > 0 ? bleManager.deviceById(deviceId) : null

    title: qsTr("Lighthouse Metadata")

    actions: [
        Kirigami.Action {
            icon.name: "arrow-back"
            text: qsTr("Back")
            onTriggered: pageStack.pop()
        }
    ]

    function rebuildMetadataModel() {
        metadataModel.clear()
        if (!device) {
            return
        }
        metadataModel.append({ "name": qsTr("Device type"), "value": device.deviceType })
        metadataModel.append({ "name": qsTr("Name"), "value": device.name })
        if (device.firmwareVersion !== "") {
            metadataModel.append({ "name": qsTr("Firmware version"),
                                   "value": device.firmwareVersion })
        }
        var keys = Object.keys(device.metadata)
        for (var i = 0; i < keys.length; i++) {
            var value = device.metadata[keys[i]]
            if (value !== "" && value !== null) {
                metadataModel.append({ "name": keys[i], "value": String(value) })
            }
        }
    }

    ListModel {
        id: metadataModel
    }

    onDeviceIdChanged: rebuildMetadataModel()
    Component.onCompleted: rebuildMetadataModel()

    Column {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Column {
            Layout.fillWidth: true
            Layout.fillHeight: true
            ListView {
                model: metadataModel
                delegate: Item {
                    required property var model
                    height: Kirigami.Units.gridSize * 2
                    Kirigami.Heading {
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.smallSpacing
                        text: "%1: %2".arg(model.name, model.value)
                        level: 2
                    }
                }
            }
        }

        Column {
            RowLayout {
                QQC2.ToolButton {
                    icon.name: "display-on"
                    text: qsTr("Identify")
                    enabled: device !== null && device.connected
                    onClicked: device.identify()
                }
                QQC2.ToolButton {
                    icon.name: "system-suspend"
                    text: qsTr("Standby")
                    enabled: device !== null && device.connected
                    onClicked: device.changeState(4)
                }
                QQC2.ToolButton {
                    icon.name: "system-power"
                    text: qsTr("Sleep")
                    enabled: device !== null && device.connected
                    onClicked: device.changeState(0)
                }
                QQC2.ToolButton {
                    icon.name: "system-run"
                    text: qsTr("On")
                    enabled: device !== null && device.connected
                    onClicked: device.changeState(1)
                }
            }
        }

        Kirigami.Separator {}

        Column {
            RowLayout {
                Kirigami.Heading {
                    text: qsTr("Nickname")
                    level: 3
                }
                QQC2.TextField {
                    Layout.fillWidth: true
                    id: nicknameField
                    text: device ? device.nickname : ""
                    placeholderText: qsTr("Not set")
                    onEditingFinished: device !== null
                        ? bleManager.setNickname(page.deviceId, text) : null
                }
            }
        }

        Kirigami.Separator {}

        Column {
            visible: device !== null && device.deviceType === "Vive base station"
            RowLayout {
                Kirigami.Heading {
                    text: qsTr("Pair ID")
                    level: 3
                }
                Text {
                    text: (device !== null && bleManager.vivePairIdHex(page.deviceId).length > 0)
                        ? bleManager.vivePairIdHex(page.deviceId)
                        : qsTr("Not set")
                }
                QQC2.ToolButton {
                    icon.name: "configure"
                    text: qsTr("Set pair ID")
                    onClicked: viveDialog.open()
                }
                QQC2.ToolButton {
                    icon.name: "edit-clear"
                    text: qsTr("Clear pair ID")
                    enabled: bleManager.vivePairIdHex(page.deviceId).length > 0
                    onClicked: {
                        bleManager.clearVivePairId(page.deviceId)
                        page.rebuildMetadataModel()
                    }
                }
            }
        }
    }

    Kirigami.PromptDialog {
        id: viveDialog
        title: qsTr("Vive base station pair ID")
        subtitle: qsTr("Enter the first 4 digits of the pair ID printed on the base station. "
                       + "The last 4 digits come from the device name.")
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: qsTr("Save")
                onTriggered: {
                    bleManager.setVivePairId(page.deviceId, viveIdField.text)
                    viveDialog.close()
                    page.rebuildMetadataModel()
                }
            }
        ]
    Column {
        QQC2.TextField {
            id: viveIdField
            text: ""
            placeholderText: qsTr("e.g. ABC1 (hint: %1)")
                .arg(bleManager.vivePairIdHint(page.deviceId))
            maximumLength: 8
            Layout.fillWidth: true
        }
    }
    }
}
