import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Themed single-line input: theme tokens for fill/text/placeholder and a
// uniform hairline background. Layout sizing lives here so call sites stay flat.
TextField {
    id: field

    Layout.fillWidth: true
    Layout.preferredHeight: 36
    color: Theme.text
    placeholderTextColor: Theme.textDim
    selectByMouse: true
    background: Rectangle {
        // concentric with SettingsSectionCard (outer 12 - padding 18 clamps
        // to the small radius step) so nested curves stay parallel
        radius: Theme.rSmall
        color: Theme.inputBg
        border.width: 1
        border.color: field.activeFocus ? Theme.inputFocusBorder : Theme.inputBorder
        Behavior on border.color { ColorAnimation { duration: Theme.durFast } }
    }
}
