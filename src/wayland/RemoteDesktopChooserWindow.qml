// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import screencast 1.0
import org.deepin.dtk 1.0 as D

D.DialogWindow {
    id: root
    readonly property int dialogWidth: 840
    readonly property int dialogHeight: screenSharingEnabled ? 392 : 300

    width: dialogWidth
    height: dialogHeight
    // Do not bind the window constraints to height. QQuickWindow may clamp
    // height when the constraints change, which creates a binding loop.
    minimumWidth: dialogWidth
    minimumHeight: dialogHeight
    maximumWidth: dialogWidth
    maximumHeight: dialogHeight
    modality: Qt.WindowModal

    property string clientAppName
    property bool keyboardRequested: false
    property bool pointerRequested: false
    property bool screenSharingEnabled: false
    property bool allowScreens: true
    property bool allowWindows: true
    property alias keyboardEnabled: keyboardCheckBox.checked
    property alias pointerEnabled: pointerCheckBox.checked
    property alias allowRestore: restoreCheckBox.checked
    property alias viewLayoutIndex: viewLayout.currentIndex
    property alias outputIndex: screensView.currentIndex
    property alias toplevelIndex: toplevelsView.currentIndex
    property var outputsModel: screensView.model
    property var toplevelsModel: toplevelsView.model
    readonly property real itemMargin: 10
    readonly property real scrollBarMargin: 50

    signal accept()
    signal reject()

    ColumnLayout {
        width: parent.width
        spacing: 8

        Label {
            text: qsTr("Application [%1] requests remote desktop access").arg(root.clientAppName)
            font.bold: true
            Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        }
        Label {
            text: qsTr("Choose what the application may control and share")
            Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: root.itemMargin
            Layout.rightMargin: root.itemMargin
            spacing: root.itemMargin * 2

            ColumnLayout {
                visible: root.keyboardRequested || root.pointerRequested
                Layout.preferredWidth: root.screenSharingEnabled ? 220 : implicitWidth
                Layout.fillWidth: !root.screenSharingEnabled
                Layout.alignment: Qt.AlignTop
                spacing: root.itemMargin

                Label {
                    id: inputTitle
                    text: qsTr("Remote input")
                    font.bold: true
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.screenSharingEnabled
                                            ? Math.max(implicitHeight, sharingTitle.implicitHeight)
                                            : implicitHeight
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignTop
                    wrapMode: Text.WordWrap
                }

                Item {
                    visible: sourceSwitcher.visible
                    Layout.preferredHeight: sourceSwitcher.implicitHeight
                }

                Background {
                    radius: viewLayout.radius
                    darkColor: viewLayout.darkColor
                    lightColor: viewLayout.lightColor
                    Layout.fillWidth: true
                    Layout.minimumHeight: root.screenSharingEnabled
                                          ? viewLayout.viewHeight
                                          : inputOptions.implicitHeight + root.itemMargin * 2
                    Layout.preferredHeight: Layout.minimumHeight
                    Layout.maximumHeight: Layout.minimumHeight

                    ColumnLayout {
                        id: inputOptions
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: root.itemMargin
                        spacing: root.itemMargin
                        CheckBox {
                            id: keyboardCheckBox
                            Layout.fillWidth: true
                            visible: root.keyboardRequested
                            checked: root.keyboardRequested
                            text: qsTr("Keyboard")
                        }
                        CheckBox {
                            id: pointerCheckBox
                            Layout.fillWidth: true
                            visible: root.pointerRequested
                            checked: root.pointerRequested
                            text: qsTr("Mouse and touchpad")
                        }
                    }
                }
            }

            ColumnLayout {
                visible: root.screenSharingEnabled
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignTop
                spacing: root.itemMargin

                Label {
                    id: sharingTitle
                    text: qsTr("Select the screen or window to share")
                    font.bold: true
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(implicitHeight, inputTitle.implicitHeight)
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignTop
                    wrapMode: Text.WordWrap
                    Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
                }

                RowLayout {
                    id: sourceSwitcher
                    visible: root.screenSharingEnabled && root.allowScreens && root.allowWindows
                    spacing: root.itemMargin
                    Layout.alignment: Qt.AlignHCenter
                    Button {
                        text: qsTr("Screen")
                        highlighted: viewLayout.currentIndex === 0
                        flat: !highlighted
                        onClicked: viewLayout.currentIndex = 0
                    }
                    Button {
                        text: qsTr("Window")
                        highlighted: viewLayout.currentIndex === 1
                        flat: !highlighted
                        onClicked: viewLayout.currentIndex = 1
                    }
                }

                StackLayout {
                    id: viewLayout
                    readonly property real viewHeight: delegateHeight * 5
                    readonly property real radius: 6
                    readonly property real delegateHeight: 36
                    readonly property color darkColor: "black"
                    readonly property color lightColor: "white"

                    Layout.fillWidth: true
                    Layout.minimumHeight: viewHeight
                    Layout.preferredHeight: viewHeight
                    Layout.maximumHeight: viewHeight
                    currentIndex: root.allowScreens ? 0 : 1

                    Background {
                        radius: viewLayout.radius
                        darkColor: viewLayout.darkColor
                        lightColor: viewLayout.lightColor
                        OutputListView {
                            id: screensView
                            anchors.fill: parent
                            rightMargin: root.scrollBarMargin
                            model: ScreenListModel {}
                            itemHeight: viewLayout.delegateHeight
                            currentIndex: -1
                        }
                    }
                    Background {
                        radius: viewLayout.radius
                        darkColor: viewLayout.darkColor
                        lightColor: viewLayout.lightColor
                        ToplevelList {
                            id: toplevelsView
                            anchors.fill: parent
                            rightMargin: root.scrollBarMargin
                            model: ToplevelListModel {}
                            itemHeight: viewLayout.delegateHeight
                            currentIndex: -1
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: root.itemMargin
            Item {
                Layout.fillWidth: true
                implicitHeight: restoreCheckBox.implicitHeight

                CheckBox {
                    id: restoreCheckBox
                    anchors.verticalCenter: parent.verticalCenter
                    checked: true
                    text: qsTr("Allow restoring on future sessions")
                }
            }

            Row {
                Layout.alignment: Qt.AlignHCenter
                spacing: root.itemMargin

                Button {
                    text: qsTr("Reject")
                    onClicked: root.reject()
                }
                D.RecommandButton {
                    text: qsTr("Allow")
                    enabled: root.screenSharingEnabled
                             ? ((root.viewLayoutIndex === 0 && root.allowScreens && root.outputIndex >= 0)
                                || (root.viewLayoutIndex === 1 && root.allowWindows && root.toplevelIndex >= 0))
                             : (root.keyboardEnabled || root.pointerEnabled)
                    onClicked: root.accept()
                }
            }

            Item { Layout.fillWidth: true }
        }
    }
}
