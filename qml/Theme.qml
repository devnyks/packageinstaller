import QtQuick

QtObject {
    property bool dark: false

    readonly property color background: dark ? "#111318" : "#f6f8fb"
    readonly property color surface: dark ? "#191c22" : "#ffffff"
    readonly property color surfaceRaised: dark ? "#22262e" : "#ffffff"
    readonly property color surfaceMuted: dark ? "#272b33" : "#edf1f6"
    readonly property color border: dark ? "#353a45" : "#dfe4ec"
    readonly property color text: dark ? "#f3f5f8" : "#172033"
    readonly property color textMuted: dark ? "#aeb6c4" : "#667085"
    readonly property color accent: dark ? "#75a7ff" : "#2368d1"
    readonly property color accentSoft: dark ? "#23395e" : "#e6efff"
    readonly property color success: dark ? "#68d391" : "#087443"
    readonly property color warning: dark ? "#f5c86b" : "#9a5b00"
    readonly property color danger: dark ? "#ff8d8d" : "#b42318"
}
