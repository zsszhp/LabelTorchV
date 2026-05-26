// Theme.qml - V2 核心视觉设计系统 (顶级工业科幻美学版)
pragma Singleton
import QtQuick

QtObject {
    // === 背景色系（深邃太空蓝与冷灰渐变） ===
    readonly property color bgPrimary: "#06080F"
    readonly property color bgSecondary: "#0B0E17"
    readonly property color bgTertiary: "#121625"
    readonly property color bgCard: "#101422"
    readonly property color bgHover: "#161C30"
    readonly property color bgSelected: "#1C243C"
    readonly property color bgInput: "#090C15"

    // === 顶级霓虹强调色（Glow Accent） ===
    readonly property color accentPrimary: "#FF2A5F"     // AI 极光粉
    readonly property color accentSecondary: "#8A2BE2"   // 量子跃迁紫
    readonly property color accentSuccess: "#00F2FE"     // 极光青蓝
    readonly property color accentWarning: "#FFD700"     // 警示金黄
    readonly property color accentError: "#FF003C"       // 警告鲜红
    readonly property color accentPurple: "#8A2BE2"

    // === 磨砂玻璃通透度 ===
    readonly property real glassOpacity: 0.85
    readonly property real glassOpacityLight: 0.60
    readonly property color glassBorder: "#202538"
    readonly property color glassBorderGlow: "#40FF2A5F"

    // === 文字色 ===
    readonly property color textPrimary: "#F5F6F8"
    readonly property color textSecondary: "#A0A5B5"
    readonly property color textDisabled: "#4E5265"
    readonly property color textMuted: "#6B728E"

    // === 状态色 ===
    readonly property color statusSuccess: "#00F2FE"
    readonly property color statusWarning: "#FFD700"
    readonly property color statusError: "#FF003C"
    readonly property color statusInfo: "#8A2BE2"

    // === 边框与分割线 ===
    readonly property color border: "#181D30"
    readonly property color borderNormal: "#181D30"
    readonly property color borderFocus: "#FF2A5F"
    readonly property color divider: "#141829"

    readonly property color tagBaseline: "#FF2A5F"
    readonly property color tagBest: "#00F2FE"
    readonly property color tagProduction: "#FFD700"

    // === 字体 ===
    readonly property string fontFamily: "Segoe UI"
    readonly property string fontFamilyMono: "Cascadia Code"
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
        "#FF2A5F", "#00F2FE", "#8A2BE2", "#FFD700",
        "#FF5722", "#4CAF50", "#00BCD4", "#9C27B0",
        "#3F51B5", "#E91E63"
    ]

    function classColor(index) {
        return classColors[index % classColors.length]
    }
}
