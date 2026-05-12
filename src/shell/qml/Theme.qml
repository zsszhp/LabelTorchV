pragma Singleton
import QtQuick

QtObject {
    readonly property color bgPrimary: "#1e1e2e"
    readonly property color bgSecondary: "#181825"
    readonly property color bgTertiary: "#11111b"
    readonly property color bgCard: "#181825"
    readonly property color bgHover: "#313244"
    readonly property color bgSelected: "#313244"
    readonly property color bgInput: "#313244"

    readonly property color accentPrimary: "#89b4fa"
    readonly property color accentSecondary: "#74c7ec"
    readonly property color accentSuccess: "#a6e3a1"
    readonly property color accentWarning: "#f9e2af"
    readonly property color accentError: "#f38ba8"
    readonly property color accentPurple: "#cba6f7"

    readonly property color textPrimary: "#cdd6f4"
    readonly property color textSecondary: "#a6adc8"
    readonly property color textDisabled: "#585b70"
    readonly property color textMuted: "#6c7086"

    readonly property color borderNormal: "#45475a"
    readonly property color borderFocus: "#89b4fa"
    readonly property color divider: "#313244"

    readonly property color tagBaseline: "#89b4fa"
    readonly property color tagBest: "#a6e3a1"
    readonly property color tagProduction: "#f9e2af"

    readonly property string fontFamily: "Microsoft YaHei, Segoe UI, sans-serif"
    readonly property string fontFamilyMono: "Consolas, Courier New, monospace"
    readonly property int fontSizeSmall: 11
    readonly property int fontSizeNormal: 13
    readonly property int fontSizeLarge: 16
    readonly property int fontSizeTitle: 20
    readonly property int fontSizeDisplay: 24

    readonly property int spacingTiny: 2
    readonly property int spacingSmall: 4
    readonly property int spacingNormal: 8
    readonly property int spacingLarge: 16
    readonly property int spacingXLarge: 24

    readonly property int radiusSmall: 4
    readonly property int radiusNormal: 8
    readonly property int radiusLarge: 12

    readonly property int animDuration: 200

    readonly property int navWidth: 220
    readonly property int statusBarHeight: 40
    readonly property int logPanelHeight: 160
    readonly property int toolbarHeight: 36

    readonly property var classColors: [
        "#f38ba8", "#a6e3a1", "#89b4fa", "#f9e2af",
        "#fab387", "#94e2d5", "#cba6f7", "#f5c2e7",
        "#89dceb", "#b4befe"
    ]

    function classColor(index) {
        return classColors[index % classColors.length]
    }
}
