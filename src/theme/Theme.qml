// Theme.qml - V5 Dihuge DLTools 工业缺陷检测视觉设计系统
// 像素级对标参考UI：极深蓝黑 + 青色高亮 + 工业科技风
pragma Singleton
import QtQuick

QtObject {
    // === 背景色系（对标参考UI: --bg-main:#0F131A, --bg-side:#151A22, --bg-card:#1E2530） ===
    readonly property color bgMain: "#0F131A"
    readonly property color bgSide: "#151A22"
    readonly property color bgCard: "#1E2530"
    readonly property color bgInput: "#121720"
    readonly property color bgInputDropdown: "#1A2030"
    readonly property color bgHover: "#1E2530"
    readonly property color bgSelected: "#262F3D"
    readonly property color bgChart: "#05070A"
    readonly property color bgChartPanel: "#07090D"
    readonly property color bgPreview: "#0C0F14"

    // === 主色调（蓝色系，对标 #0077FF） ===
    readonly property color primary: "#0077FF"
    readonly property color primaryGlow: "#00E5FF"
    readonly property color primaryDark: "#0055CC"

    // === 状态色（对标参考UI语义色） ===
    readonly property color success: "#00E676"
    readonly property color danger: "#FF1744"
    readonly property color warning: "#FF9100"
    readonly property color info: primaryGlow

    // === 文字色（对标参考UI） ===
    readonly property color textMain: "#E2E8F0"
    readonly property color textMuted: "#64748B"
    readonly property color textDisabled: "#475569"
    readonly property color textAccent: primaryGlow

    // 二级文字色（比 textMuted 更亮，用于次要辅助信息）
    readonly property color textSecondary: "#94A3B8"

    // === 边框与分割线（对标参考UI: --border-color:#262F3D） ===
    readonly property color borderColor: "#262F3D"
    readonly property color borderHover: primaryGlow
    readonly property color dividerColor: "#1E2530"

    // === 渐变色 ===
    readonly property string gradientPrimary: "linear-gradient(135deg, " + primary + ", " + primaryDark + ")"
    readonly property string gradientLogo: "linear-gradient(135deg, " + primary + ", " + primaryGlow + ")"
    readonly property string gradientLogoText: "linear-gradient(to right, #ffffff, #94A3B8)"
    readonly property string gradientProgress: "linear-gradient(90deg, " + primary + ", " + primaryGlow + ")"
    readonly property string gradientNavActive: "linear-gradient(to bottom, transparent 70%, rgba(0, 229, 255, 0.05))"

    // === 发光效果 ===
    readonly property color glowCyan: "#2000E5FF"
    readonly property color glowCyanStrong: "#4000E5FF"
    readonly property color glowBlue: "#200077FF"
    readonly property color glowRed: "#20FF1744"
    readonly property color glowGreen: "#2000E676"

    // === 品牌/发光（顶栏 Logo 等） ===
    readonly property color logoText: "#C8D4E0"
    readonly property color logoBgText: "#FFFFFF"
    readonly property color glowCyanRaw: "#00E5FF"
    readonly property color overlayMask: "#B3000000"

    // === 磨砂玻璃 ===
    readonly property real glassOpacity: 0.92
    readonly property real glassOpacityLight: 0.70
    readonly property color glassBorder: borderColor
    readonly property color glassBorderGlow: glowCyan

    // === 图表专用色 ===
    readonly property color chartBoxLoss: "#0077FF"
    readonly property color chartSegLoss: "#00E676"
    readonly property color chartClsLoss: "#00E5FF"
    readonly property color chartDflLoss: "#FF9100"
    readonly property color chartSevereLoss: "#D500F9"
    readonly property color chartMap50B: "#00E676"
    readonly property color chartMap5095B: "#FFFFFF"
    readonly property color chartMap50M: "#00E5FF"
    readonly property color chartMap5095M: "#D500F9"
    readonly property color chartRecallB: "#FFD600"
    readonly property color chartRecallM: "#00E676"
    readonly property color chartPrecisionB: "#FFD600"
    readonly property color chartPrecisionM: "#00E676"
    readonly property color chartGridLine: "#20FF1744"
    readonly property color chartBaseline: "#FFD600"

    // === 标签色 ===
    readonly property color tagBaseline: primary
    readonly property color tagBest: success
    readonly property color tagProduction: "#FFD600"

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
    readonly property int radiusNormal: 6
    readonly property int radiusLarge: 8

    // === 动画 ===
    readonly property int animDuration: 200
    readonly property int animDurationSlow: 300
    readonly property int animDurationFast: 120

    // === Toast 反馈 ===
    readonly property int toastSuccessDuration: 2000   // 成功提示自动消失
    readonly property int toastErrorDuration: 0        // 0 = 常驻，需手动关闭
    readonly property int toastWidth: 320
    readonly property int toastMaxVisible: 3
    readonly property color toastSuccessBg: "#1A00E676"
    readonly property color toastErrorBg: "#1AFF1744"
    readonly property color toastInfoBg: "#1A00E5FF"

    // === 空态与确认弹窗 ===
    readonly property int emptyIconSize: 48
    readonly property int confirmDialogWidth: 440
    readonly property int overlayMaskAlpha: 179         // 0xB3 的十进制，弹窗遮罩不透明度

    // === 导航分组 ===
    readonly property int navGroupGap: 14
    readonly property color navGroupDivider: "#2A3344"

    // === 表单校验 ===
    readonly property color fieldErrorBorder: danger
    readonly property color fieldErrorText: "#FF8A9B"

    // === 布局尺寸 ===
    readonly property int headerHeight: 50
    readonly property int footerHeight: 34
    readonly property int sidebarWidth: 240
    readonly property int sidebarMinWidth: 120
    readonly property int filterBarHeight: 40
    readonly property int subTabHeight: 40
    readonly property int logPanelHeight: 180
    readonly property int toolbarHeight: 36

    // === 步进器尺寸 ===
    readonly property int stepperButtonWidth: 28
    readonly property int stepperValueWidth: 60
    readonly property int stepperHeight: 28

    // === 开关尺寸 ===
    readonly property int toggleWidth: 34
    readonly property int toggleHeight: 20
    readonly property int toggleSmallWidth: 28
    readonly property int toggleSmallHeight: 16

    // === 类别配色（高饱和度，深色背景上醒目） ===
    readonly property var classColors: [
        "#0077FF", "#00E676", "#D500F9", "#FFD600",
        "#FF1744", "#FF9100", "#00E5FF", "#E879F9",
        "#2DD4BF", "#F472B6"
    ]

    function classColor(index) {
        return classColors[index % classColors.length]
    }

    // 数值兜底：把 NaN / Infinity 归一到 fallback
    // 用于 QML 绑定表达式，避免非法几何进入布局引擎（Qt 6.11 Debug 会 qFatal）
    function safeNum(value, fallback) {
        if (typeof value !== "number") return fallback
        if (!isFinite(value)) return fallback
        return value
    }

    // 非负尺寸兜底：宽高/间距等不允许出现负数或 NaN
    function safeSize(value, fallback) {
        var n = safeNum(value, fallback)
        return n > 0 ? n : fallback
    }
}
