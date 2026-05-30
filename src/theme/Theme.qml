// Theme.qml - V4 核心视觉设计系统 (赛博蓝 · 高端科技 · 高可读性)
pragma Singleton
import QtQuick

QtObject {
    // === 背景色系（赛博蓝灰，亮度 15-25%，告别纯黑） ===
    readonly property color bgPrimary: "#1A1D2E"
    readonly property color bgSecondary: "#222639"
    readonly property color bgTertiary: "#2A2F44"
    readonly property color bgCard: "#252A3E"
    readonly property color bgHover: "#343A52"
    readonly property color bgSelected: "#3B4263"
    readonly property color bgInput: "#1E2235"

    // === 强调色（高饱和度赛博蓝+紫，视觉冲击力强） ===
    readonly property color accentPrimary: "#4DA6FF"
    readonly property color accentSecondary: "#B794F6"
    readonly property color accentSuccess: "#36D399"
    readonly property color accentWarning: "#FBBF24"
    readonly property color accentError: "#F87171"
    readonly property color accentPurple: "#B794F6"

    // === 磨砂玻璃通透度 ===
    readonly property real glassOpacity: 0.92
    readonly property real glassOpacityLight: 0.70
    readonly property color glassBorder: "#3A4260"
    readonly property color glassBorderGlow: "#604DA6FF"

    // === 文字色（超高对比度，深色背景上清晰锐利） ===
    readonly property color textPrimary: "#F5F7FA"
    readonly property color textSecondary: "#C8CED8"
    readonly property color textDisabled: "#6B7585"
    readonly property color textMuted: "#929BA8"

    // === 状态色 ===
    readonly property color statusSuccess: "#36D399"
    readonly property color statusWarning: "#FBBF24"
    readonly property color statusError: "#F87171"
    readonly property color statusInfo: "#B794F6"

    // === 边框与分割线（清晰可辨，告别隐身边框） ===
    readonly property color border: "#3A4260"
    readonly property color borderNormal: "#3A4260"
    readonly property color borderFocus: "#4DA6FF"
    readonly property color divider: "#2E3450"

    readonly property color tagBaseline: "#4DA6FF"
    readonly property color tagBest: "#36D399"
    readonly property color tagProduction: "#FBBF24"

    // === 字体 ===
    readonly property string fontFamily: "Segoe UI"
    readonly property string fontFamilyMono: "Cascadia Code"
    readonly property int fontSizeCaption: 11
    readonly property int fontSizeSmall: 12
    readonly property int fontSizeNormal: 13
    readonly property int fontSizeSubheading: 15
    readonly property int fontSizeLarge: 18
    readonly property int fontSizeTitle: 20
    readonly property int fontSizeDisplay: 26

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
    readonly property int animDurationSlow: 300

    // === 布局尺寸 ===
    readonly property int sidebarExpandedWidth: 220
    readonly property int sidebarCollapsedWidth: 64
    readonly property int statusBarHeight: 40
    readonly property int logPanelHeight: 180
    readonly property int toolbarHeight: 36

    // === 类别配色（高饱和度，深色背景上醒目） ===
    readonly property var classColors: [
        "#4DA6FF", "#36D399", "#B794F6", "#FBBF24",
        "#F87171", "#FB923C", "#38BDF8", "#E879F9",
        "#2DD4BF", "#F472B6"
    ]

    function classColor(index) {
        return classColors[index % classColors.length]
    }
}
