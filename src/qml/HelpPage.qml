import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.Page {
    id: page

    title: qsTr("Help")

    actions: [
        Kirigami.Action {
            icon.name: "arrow-back"
            text: qsTr("Back")
            onTriggered: pageStack.pop()
        }
    ]

    Kirigami.ColumnView {
        Kirigami.Heading {
            text: qsTr("Pairing a device")
            level: 3
        }
        Text {
            text: qsTr("You will need to pair a lighthouse before you can communicate with it. "
                      + "Go to the Pair page, start a scan and press Pair on the device you want to "
                      + "connect with. Version 2.0 lighthouses' names start with 'LHB-'. "
                      + "Vive base stations start with 'HTC BS'.")
            wrapMode: Text.WordWrap
        }

        Kirigami.Heading {
            text: qsTr("Nickname")
            level: 3
        }
        Text {
            text: qsTr("You may want to give a device a nickname to find it more easily in the list. "
                      + "Open the metadata page of the device and set the nickname. Leave it blank to "
                      + "remove the nickname.")
            wrapMode: Text.WordWrap
        }

        Kirigami.Heading {
            text: qsTr("Group")
            level: 3
        }
        Text {
            text: qsTr("Grouping items together makes it easier to change the state of multiple "
                      + "devices at the same time. Long press the devices to select them, then use "
                      + "'Add to group' in the selection toolbar. Long press a group header to select "
                      + "the group, then rename or delete it.")
            wrapMode: Text.WordWrap
        }

        Kirigami.Heading {
            text: qsTr("Metadata")
            level: 3
        }
        Text {
            text: qsTr("Every device has metadata and extra actions. Tap on the lighthouse item to "
                      + "open the metadata page, where you can identify the device (LED blink), "
                      + "put it to sleep, put it in standby or turn it on, and change its nickname.")
            wrapMode: Text.WordWrap
        }

        Kirigami.Heading {
            text: qsTr("Vive base station pair ID")
            level: 3
        }
        Text {
            text: qsTr("Vive base stations need a pair id before they can be controlled. Open the "
                      + "metadata page of a Vive base station and press 'Set pair ID'. Enter the first "
                      + "4 digits printed on the base station; the last 4 digits are taken from the "
                      + "device name automatically.")
            wrapMode: Text.WordWrap
        }
    }
}
