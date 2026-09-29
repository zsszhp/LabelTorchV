// Theme.qml - 标炬视觉设计系统（深色工业科技风）
// 所有颜色/字体/间距/圆角/阴影统一由此发放，业务侧禁止硬编码视觉值
pragma Singleton
import QtQuick

QtObject {
    // === 背景层次（由深到浅，保证卡片/侧栏/输入框可区分） ===
    readonly property color bgMain: "#0B1220"
    readonly property color bgSide: "#131C2E"
    readonly property color bgCard: "#1B263C"
    readonly property color bgInput: "#0F1829"
    readonly property color bgInputDropdown: "#1E2B44"
    readonly property color bgHover: "#243350"
    readonly property color bgSelected: "#2B3B58"
    readonly property color bgChart: "#070D18"
    readonly property color bgChartPanel: "#0A1220"
    readonly property color bgPreview: "#09101C"

    // === 主色（年轻青色系） ===
    readonly property color primary: "#22D3EE"
    readonly property color primaryGlow: "#67E8F9"
    readonly property color primaryDark: "#0284C7"

    // === 状态色（降饱和，长时间盯屏不易疲劳） ===
    readonly property color success: "#34D399"
    readonly property color danger: "#F87171"
    readonly property color warning: "#FBBF24"
    readonly property color info: primaryGlow

    // === 文字色 ===
    readonly property color textMain: "#E2E8F0"
    readonly property color textMuted: "#64748B"
    readonly property color textDisabled: "#475569"
    readonly property color textAccent: primaryGlow
    readonly property color textSecondary: "#94A3B8"

    // === 边框与分割线 ===
    readonly property color borderColor: "#2C3E5C"
    readonly property color borderHover: primaryGlow
    readonly property color dividerColor: "#243350"

    // === 焦点环（键盘/输入焦点） ===
    // rgba(34,211,238,0.28) 对应 #AARRGGBB
    readonly property color focusRing: "#4722D3EE"
    readonly property int focusRingWidth: 2

    // === 层次阴影（双层：贴近深影 + 外扩淡影） ===
    // 供 MultiEffect / 手绘阴影层使用，近影压边、远影拉出悬浮感
    readonly property color elevationNearColor: "#26000000"
    readonly property color elevationFarColor: "#14000000"
    readonly property real elevationNearBlur: 0.35
    readonly property real elevationFarBlur: 1.0
    readonly property real elevationNearOffsetY: 1
    readonly property real elevationFarOffsetY: 4

    // === 发光效果（强调高亮） ===
    readonly property color glowCyan: "#2022D3EE"
    readonly property color glowCyanStrong: "#4022D3EE"
    readonly property color glowBlue: "#200EA5E9"
    readonly property color glowRed: "#20F87171"
    readonly property color glowGreen: "#2034D399"

    // === 品牌/发光（顶栏 Logo 等） ===
    readonly property color logoText: "#C8D4E0"
    readonly property color logoBgText: "#FFFFFF"
    readonly property color glowCyanRaw: "#22D3EE"
    readonly property color overlayMask: "#B3000000"

    // === 磨砂玻璃 ===
    readonly property real glassOpacity: 0.92
    readonly property real glassOpacityLight: 0.70
    readonly property color glassBorder: borderColor
    readonly property color glassBorderGlow: glowCyan

    // === 图表专用色（与状态色/主色同族，降饱和） ===
    readonly property color chartBoxLoss: primary
    readonly property color chartSegLoss: success
    readonly property color chartClsLoss: primaryGlow
    readonly property color chartDflLoss: warning
    readonly property color chartSevereLoss: "#C084FC"
    readonly property color chartMap50B: success
    readonly property color chartMap5095B: "#FFFFFF"
    readonly property color chartMap50M: primaryGlow
    readonly property color chartMap5095M: "#C084FC"
    readonly property color chartRecallB: warning
    readonly property color chartRecallM: success
    readonly property color chartPrecisionB: warning
    readonly property color chartPrecisionM: success
    readonly property color chartGridLine: "#20F87171"
    readonly property color chartBaseline: warning

    // === 标签色 ===
    readonly property color tagBaseline: primary
    readonly property color tagBest: success
    readonly property color tagProduction: warning

    // === 字体（Qt 只认单一字体名，逗号列表无效） ===
    readonly property string fontFamily: "Microsoft YaHei UI"
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

    // === 圆角（弹窗/卡片更大，控件克制） ===
    readonly property int radiusSmall: 6
    readonly property int radiusNormal: 10
    readonly property int radiusLarge: 14
    readonly property int radiusXLarge: 18

    // === 动画 ===
    readonly property int animDuration: 200
    readonly property int animDurationSlow: 300
    readonly property int animDurationFast: 120

    // === 弹窗阴影/层次 ===
    readonly property color shadowDialog: "#66000000"
    readonly property color dialogTitleBg: "#1A2A42"
    readonly property color dialogFooterBg: "#152238"
    readonly property color dialogBodyBg: "#111C2E"
    readonly property color accentBar: primaryGlow

    // === Toast 反馈 ===
    readonly property int toastSuccessDuration: 2000   // 成功提示自动消失
    readonly property int toastErrorDuration: 0        // 0 = 常驻，需手动关闭
    readonly property int toastWidth: 320
    readonly property int toastMaxVisible: 3
    readonly property color toastSuccessBg: "#1A34D399"
    readonly property color toastErrorBg: "#1AF87171"
    readonly property color toastInfoBg: "#1A22D3EE"

    // === 空态与确认弹窗 ===
    readonly property int emptyIconSize: 56
    readonly property int confirmDialogWidth: 440
    readonly property int overlayMaskAlpha: 179         // 0xB3 的十进制，弹窗遮罩不透明度

    // === 导航分组 ===
    readonly property int navGroupGap: 14
    readonly property color navGroupDivider: "#2A3A56"

    // === 表单校验 ===
    readonly property color fieldErrorBorder: danger
    readonly property color fieldErrorText: "#FCA5A5"
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

    // === 类别配色（按色相环排序，中等饱和度，深底上可辨且不刺眼） ===
    readonly property var classColors: [
        "#F87171",  // 红
        "#FB923C",  // 橙
        "#FBBF24",  // 黄
        "#A3E635",  // 黄绿
        "#34D399",  // 翠绿
        "#2DD4BF",  // 青绿
        "#22D3EE",  // 青
        "#60A5FA",  // 蓝
        "#A78BFA",  // 紫
        "#E879F9"   // 品红
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
