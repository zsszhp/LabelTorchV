// Theme.qml - V2 核心视觉设计系统
// 深靛蓝与粉红强调色组合，专业工业级界面
pragma Singleton
import QtQuick

QtObject {
    // === 背景色系（深靛蓝层次感） ===
    readonly property color bgPrimary: "#0D0E15"
    readonly property color bgSecondary: "#141622"
    readonly property color bgTertiary: "#1C1F30"
    readonly property color bgCard: "#1A1D2E"
    readonly property color bgHover: "#1C1F30"
    readonly property color bgSelected: "#1C1F30"
    readonly property color bgInput: "#1C1F30"

    // === 强调色 ===
    readonly property color accentPrimary: "#FF4A70"
    readonly property color accentSecondary: "#8B5CF6"
    readonly property color accentSuccess: "#10B981"
    readonly property color accentWarning: "#F59E0B"
    readonly property color accentError: "#EF4444"
    readonly property color accentPurple: "#8B5CF6"

    // === 文字色 ===
    readonly property color textPrimary: "#EAEAEA"
    readonly property color textSecondary: "#9F9FAC"
    readonly property color textDisabled: "#585966"
    readonly property color textMuted: "#585966"

    // === 状态色 ===
    readonly property color statusSuccess: "#10B981"
    readonly property color statusWarning: "#F59E0B"
    readonly property color statusError: "#EF4444"
    readonly property color statusInfo: "#8B5CF6"

    // === 边框与分割线 ===
    readonly property color border: "#2A2D42"
    readonly property color borderNormal: "#2A2D42"
    readonly property color borderFocus: "#FF4A70"
    readonly property color divider: "#252840"

    readonly property color tagBaseline: "#FF4A70"
    readonly property color tagBest: "#10B981"
    readonly property color tagProduction: "#F59E0B"

    // === 字体 ===
    readonly property string fontFamily: "\"Segoe UI\", \"Microsoft YaHei\", sans-serif"
    readonly property string fontFamilyMono: "\"Cascadia Code\", \"Consolas\", monospace"
    readonly property int fontSizeCaption: 11
    readonly property int fontSizeSmall: 11
    readonly property int fontSizeNormal: 13
    readonly property int fontSizeSubheading: 14
    readonly property int fontSizeLarge: 18
    readonly property int fontSizeTitle: 18
    readonly property int fontSizeDisplay: 24

    // === 间距 ===
    readonly property int spacingTiny: 2
    readonly property int spacingSmall: 4
    readonly property int spacingNormal: 8
    readonly property int spacingLarge: 16
    readonly property int spacingXLarge: 24

    // === 圆角 ===
    readonly property int radiusSmall: 4
    readonly property int radiusNormal: 8
    readonly property int radiusLarge: 12

    // === 动画 ===
    readonly property int animDuration: 200
    readonly property int animDurationSlow: 250

    // === 布局尺寸 ===
    readonly property int sidebarExpandedWidth: 200
    readonly property int sidebarCollapsedWidth: 64
    readonly property int statusBarHeight: 40
    readonly property int logPanelHeight: 160
    readonly property int toolbarHeight: 36

    // === 类别配色 ===
    readonly property var classColors: [
        "#f38ba8", "#a6e3a1", "#89b4fa", "#f9e2af",
        "#fab387", "#94e2d5", "#cba6f7", "#f5c2e7",
        "#89dceb", "#b4befe"
    ]

    function classColor(index) {
        return classColors[index % classColors.length]
    }
}
