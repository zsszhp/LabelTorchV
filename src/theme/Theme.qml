// Theme.qml - 标炬视觉设计系统（Apple HIG 风格：中性石墨 + 系统蓝强调）
// 设计基准：macOS 深色专业应用（Final Cut / Xcode）的材质分层——
// 表面只靠亮度差区分，发丝线分隔，唯一强调色为 Apple 系统蓝，彩色只承载状态语义
// 所有颜色/字体/间距/圆角/阴影统一由此发放，业务侧禁止硬编码视觉值
pragma Singleton
import QtQuick

QtObject {
    // === 背景层次（中性石墨，无色偏；由深到浅区分表面） ===
    readonly property color bgMain: "#1C1C1E"          // 窗口基底（Apple systemGray6 dark）
    readonly property color bgSide: "#161618"          // 侧栏（比基底更沉一层）
    readonly property color bgCard: "#2C2C2E"          // 卡片/浮起面板（systemGray5）
    readonly property color bgInput: "#232325"         // 输入框（内凹，比基底暗）
    readonly property color bgInputDropdown: "#323234"
    readonly property color bgHover: "#3A3A3C"         // 悬停（systemGray4）
    readonly property color bgSelected: "#48484A"      // 选中（systemGray3）
    readonly property color bgChart: "#141416"
    readonly property color bgChartPanel: "#1C1C1E"
    readonly property color bgPreview: "#0A0A0B"       // 画布预览：近黑，让图片成为主角

    // === 主色（Apple 系统蓝，唯一强调色） ===
    readonly property color primary: "#0A84FF"
    readonly property color primaryGlow: "#409CFF"     // 悬停/高亮态的亮蓝
    readonly property color primaryDark: "#0060DF"

    // === 状态色（Apple 原生语义色） ===
    readonly property color success: "#30D158"
    readonly property color danger: "#FF453A"
    readonly property color warning: "#FF9F0A"
    readonly property color info: primaryGlow

    // === 文字色（Apple label 体系） ===
    readonly property color textMain: "#F5F5F7"
    readonly property color textMuted: "#7C7C82"
    readonly property color textDisabled: "#5A5A5F"
    readonly property color textAccent: primaryGlow
    readonly property color textSecondary: "#AEAEB2"

    // === 边框与分割线（发丝线：白 ~10% 叠加在石墨上） ===
    readonly property color borderColor: "#333336"
    readonly property color borderHover: primaryGlow
    readonly property color dividerColor: "#2A2A2C"

    // === 焦点环（键盘/输入焦点） ===
    // rgba(10,132,255,0.28) 对应 #AARRGGBB
    readonly property color focusRing: "#470A84FF"
    readonly property int focusRingWidth: 2

    // === 层次阴影（Apple 柔和大模糊、低不透明度，不抢注意力） ===
    // 供 MultiEffect / 手绘阴影层使用，近影压边、远影拉出悬浮感
    readonly property color elevationNearColor: "#38000000"
    readonly property color elevationFarColor: "#28000000"
    readonly property real elevationNearBlur: 0.5
    readonly property real elevationFarBlur: 1.2
    readonly property real elevationNearOffsetY: 2
    readonly property real elevationFarOffsetY: 8

    // === 发光效果（Apple 风格克制：仅保留极淡光晕，避免霓虹感） ===
    readonly property color glowCyan: "#140A84FF"
    readonly property color glowCyanStrong: "#280A84FF"
    readonly property color glowBlue: "#140A84FF"
    readonly property color glowRed: "#20FF453A"
    readonly property color glowGreen: "#2030D158"

    // === 品牌/弹窗遮罩 ===
    readonly property color logoText: "#E8E8ED"
    readonly property color logoBgText: "#FFFFFF"
    readonly property color glowCyanRaw: "#0A84FF"
    readonly property color overlayMask: "#99000000"

    // === 磨砂玻璃 ===
    readonly property real glassOpacity: 0.94
    readonly property real glassOpacityLight: 0.72
    readonly property color glassBorder: borderColor
    readonly property color glassBorderGlow: glowCyan

    // === 图表专用色（Apple 系统色板，同族亮度分层） ===
    readonly property color chartBoxLoss: primary
    readonly property color chartSegLoss: success
    readonly property color chartClsLoss: "#64D2FF"     // systemTeal
    readonly property color chartDflLoss: warning
    readonly property color chartSevereLoss: "#BF5AF2"  // systemPurple
    readonly property color chartMap50B: success
    readonly property color chartMap5095B: "#FFFFFF"
    readonly property color chartMap50M: "#64D2FF"
    readonly property color chartMap5095M: "#BF5AF2"
    readonly property color chartRecallB: warning
    readonly property color chartRecallM: success
    readonly property color chartPrecisionB: warning
    readonly property color chartPrecisionM: success
    readonly property color chartGridLine: "#1FF5F5F7"
    readonly property color chartBaseline: warning

    // === 标签色 ===
    readonly property color tagBaseline: primary
    readonly property color tagBest: success
    readonly property color tagProduction: warning
    // 图像 Tag 语义色（重要→系统紫，自定义 Tag 走中性灰）
    readonly property color tagImportant: "#AF52DE"

    // === 字体（Qt 单一字体名；Latin 走 Segoe UI 贴近 SF 质感，中文自动回退雅黑） ===
    readonly property string fontFamily: "Segoe UI"
    readonly property string fontFamilyMono: "Consolas"
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
    readonly property int spacingMedium: 12
    readonly property int spacingLarge: 16
    readonly property int spacingXLarge: 24

    // === 圆角（Apple 连续圆角节奏：控件 5 / 卡片 8 / 弹窗 12 / 大容器 16） ===
    readonly property int radiusSmall: 5
    readonly property int radiusNormal: 8
    readonly property int radiusLarge: 12
    readonly property int radiusXLarge: 16

    // === 动画 ===
    readonly property int animDuration: 200
    readonly property int animDurationSlow: 280
    readonly property int animDurationFast: 120

    // === 弹窗阴影/层次（Apple sheet：体色统一石墨，靠阴影分层） ===
    readonly property color shadowDialog: "#88000000"
    readonly property color dialogTitleBg: "#28282A"
    readonly property color dialogFooterBg: "#1C1C1E"
    readonly property color dialogBodyBg: "#202022"
    readonly property color accentBar: primary

    // === Toast 反馈 ===
    readonly property int toastSuccessDuration: 2000   // 成功提示自动消失
    readonly property int toastErrorDuration: 0        // 0 = 常驻，需手动关闭
    readonly property int toastWidth: 320
    readonly property int toastMaxVisible: 3
    readonly property color toastSuccessBg: "#2E30D158"
    readonly property color toastErrorBg: "#2EFF453A"
    readonly property color toastInfoBg: "#2E0A84FF"

    // === 空态与确认弹窗 ===
    readonly property int emptyIconSize: 56
    readonly property int confirmDialogWidth: 440
    readonly property int overlayMaskAlpha: 153        // 0x99 的十进制，弹窗遮罩 60%

    // === 导航分组 ===
    readonly property int navGroupGap: 14
    readonly property color navGroupDivider: "#2A2A2C"

    // === 表单校验 ===
    readonly property color fieldErrorBorder: danger
    readonly property color fieldErrorText: "#FF7B72"
    readonly property int formLabelWidth: 96

    // === 布局尺寸 ===
    readonly property int headerHeight: 56
    readonly property int footerHeight: 34
    readonly property int sidebarWidth: 240
    readonly property int sidebarMinWidth: 120
    readonly property int filterBarHeight: 40
    readonly property int subTabHeight: 40
    readonly property int logPanelHeight: 180
    readonly property int toolbarHeight: 36

    // === 按钮尺寸 ===
    readonly property int buttonHeight: 36
    readonly property int buttonHeightCompact: 32

    // === 步进器尺寸 ===
    readonly property int stepperButtonWidth: 28
    readonly property int stepperValueWidth: 60
    readonly property int stepperHeight: 28

    // === 开关尺寸 ===
    readonly property int toggleWidth: 34
    readonly property int toggleHeight: 20
    readonly property int toggleSmallWidth: 28
    readonly property int toggleSmallHeight: 16

    // === 类别配色（Apple 系统色板：中高亮度，石墨底上清晰可辨） ===
    readonly property var classColors: [
        "#FF453A",  // 红
        "#FF9F0A",  // 橙
        "#FFD60A",  // 黄
        "#30D158",  // 绿
        "#64D2FF",  // 青
        "#0A84FF",  // 蓝
        "#BF5AF2",  // 紫
        "#FF375F",  // 粉
        "#AC8E68",  // 棕
        "#98989D"   // 灰
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
