pragma Singleton
import QtQuick

// Central design tokens — black/grey professional palette.
// Organized as: color, spacing (8px grid), type scale, radius, elevation, motion.
// Keep component files token-driven: no random hex or 13px in pages.
QtObject {
    // ================= COLOR =================
    // ---- backgrounds (black-grey) ----
    readonly property color bg:          "#141414"
    readonly property color surface:     "#1E1E1E"
    readonly property color card:        "#1E1E1E"

    // ---- accents ----
    readonly property color accent:      "#3A3F4A"
    readonly property color accentHover: "#4A5160"
    readonly property color selected:    "#33383F"

    // hover highlight — neutral grey tint (no blue)
    readonly property color hoverBg:     "#252525"
    readonly property color hoverBgStrong: "#2E2E2E"

    // ---- status ----
    readonly property color ok:          "#5FA87A"
    readonly property color warn:        "#C9A15A"
    readonly property color danger:      "#C55A5A"

    // ---- slider ----
    readonly property color sliderTrack: "#33383F"
    readonly property color sliderFill:  "#5FA87A"
    readonly property color sliderHandle:"#E8E8E8"

    // ---- glass (frosted, subtle grey) ----
    readonly property color glass:       Qt.rgba(255,255,255,0.04)
    readonly property color glassHover:  Qt.rgba(255,255,255,0.09)
    readonly property color glassPress:  Qt.rgba(255,255,255,0.13)

    // ---- interaction / focus (a11y: visible focus ring on keyboard nav) ----
    // neutral grey — never blue
    readonly property color focusRing:   Qt.rgba(1,1,1,0.45)
    // input hairline + focus border (white ring is invisible on white glass,
    // so glass mode switches to the accent)
    readonly property color inputBorder: glassMode
             ? Qt.rgba(0, 0, 0, 0.18)
             : Qt.rgba(255,255,255,0.08)
    readonly property color inputFocusBorder: glassMode ? accent : focusRing

    // ---- glass-mode variants (referenced by the appearance block below) ----
    // Glass mode: WHITE text on the grey-tinted frosted glass (readability).
    // NOTE: sidebar/navigation KEEPS its dark colour in every mode (user rule).
    readonly property color sidebar:     "#0D0D0D"
    readonly property color inputBg:     glassMode
             ? Qt.rgba(1, 1, 1, 0.09)
             : "#262626"
    readonly property color text:        glassMode ? "#FFFFFF" : "#F0F0F0"
    readonly property color textDim:     glassMode ? Qt.rgba(1,1,1,0.75) : "#9A9A9A"
    readonly property color textMuted:   glassMode ? Qt.rgba(1,1,1,0.65) : "#B0B0B0"
    readonly property color glassBorder: glassMode
             ? Qt.rgba(1, 1, 1, 0.27)
             : Qt.rgba(255,255,255,0.08)
    // status indicator idle colour: dark neutral on glass so the dot/ring stays
    // visible against the light frosted panels (white would vanish)
    readonly property color statusIdle:  glassMode ? Qt.rgba(0,0,0,0.40) : "#9A9A9A"
    // section divider bar: light on glass (Theme.accent would be a dark smudge)
    readonly property color sectionBar:  glassMode ? Qt.rgba(1,1,1,0.80) : Theme.accent

    // ---- navigation text: ALWAYS light (sidebar stays dark in every mode) ----
    readonly property color navText:      "#F0F0F0"
    readonly property color navTextDim:   "#9A9A9A"
    readonly property color navTextMuted: "#B0B0B0"

    // ================= SPACING (8px grid) =================
    readonly property int sp1: 4
    readonly property int sp2: 8
    readonly property int sp3: 12
    readonly property int sp4: 16
    readonly property int sp5: 24
    readonly property int sp6: 32
    readonly property int sp7: 48

    // ================= TYPE SCALE =================
    readonly property int fsCaption: 11
    readonly property int fsSmall:   12
    readonly property int fsBody:    13
    readonly property int fsDefault: 14
    readonly property int fsTitle:   16
    readonly property int fsPage:    20
    readonly property int fsHero:    28

    // typography hierarchy (semantic usage)
    readonly property int typeH1: fsHero      // page hero / big numbers
    readonly property int typeH2: fsPage      // page title
    readonly property int typeH3: fsTitle     // card title
    readonly property int typeBody: fsDefault // body / buttons
    readonly property int typeMeta: fsSmall   // secondary info
    readonly property int typeCaption: fsCaption // captions / badges

    // ================= RADIUS =================
    readonly property int rSmall: 6
    readonly property int rMd:    8
    readonly property int rLg:    10
    readonly property int rXl:    14
    readonly property int rFull:  999

    // ================= MOTION (ease-out) =================
    readonly property int durFast:   120
    readonly property int durMid:    200
    readonly property int durSlow:   320

    // ================= WALLPAPER / APPEARANCE STATE (set by Main.qml) =================
    // wallpaperActive = a wallpaper is currently set (drives dark translucent fills)
    // appearanceMode  = "" (默认深色) | "glass" (壁纸玻璃 — light frosted glass over wallpaper)
    // glassOpacity    = wallpaper layer opacity in glass mode (0.05..0.20, default 0.10)
    // wallpaperTint   = average wallpaper colour — bleeds into the glass ("环境色融合")
    property bool wallpaperActive: false
    property string appearanceMode: ""
    property real glassOpacity: 0.10
    property color wallpaperTint: "#FFFFFF"
    readonly property bool glassMode: appearanceMode === "glass"

    // ---- glass overlay tokens (Layer 2) ----
    // brightness-adaptive black scrim: dark wallpaper barely dimmed, bright
    // wallpaper pressed up to glassScrimMax so white text stays readable
    readonly property real glassScrimMin: 0.06
    readonly property real glassScrimK:  0.55
    readonly property real glassScrimMax: 0.38
    // environment tint from the wallpaper itself (≤8%, never a blue wash)
    readonly property real glassTintAlpha: 0.08
    // top/bottom vignette strength (dark desktop depth, capped at 0.22)
    readonly property real vignetteAlpha: 0.15

    // ---- cards: glass strongly tinted by the wallpaper colour (45% tint +
    // 20% grey + 35% white) so a blue wallpaper gives blue glass, yet greyed
    // enough for WHITE text to stay readable. Alphas reduced ~25% (更透明).
    readonly property color cardFill: glassMode
             ? Qt.rgba(1, 1, 1, 0.14)
             : wallpaperActive ? Qt.rgba(24/255, 24/255, 24/255, 0.85) : Theme.surface
    readonly property color cardFillHover: glassMode
             ? Qt.rgba(1, 1, 1, 0.21)
             : wallpaperActive ? Qt.rgba(31/255, 31/255, 31/255, 0.85) : Theme.hoverBgStrong
    readonly property color cardFillPress: glassMode
             ? Qt.rgba(1, 1, 1, 0.24)
             : wallpaperActive ? Qt.rgba(38/255, 38/255, 38/255, 0.85) : Theme.hoverBgStrong

    // ---- buttons ----
    readonly property color btnFill: glassMode
             ? Qt.rgba(1, 1, 1, 0.14)
             : wallpaperActive ? Qt.rgba(36/255, 36/255, 36/255, 0.95) : Qt.rgba(1,1,1,0.08)
    readonly property color btnFillHover: glassMode
             ? Qt.rgba(1, 1, 1, 0.21)
             : wallpaperActive ? Qt.rgba(52/255, 52/255, 52/255, 0.95) : Qt.rgba(1,1,1,0.14)
    readonly property color btnFillPress: glassMode
             ? Qt.rgba(1, 1, 1, 0.24)
             : wallpaperActive ? Qt.rgba(60/255, 60/255, 60/255, 0.95) : Qt.rgba(1,1,1,0.20)
    readonly property color btnBorder: glassMode
             ? Qt.rgba(1, 1, 1, 0.27)
             : wallpaperActive ? Qt.rgba(255,255,255,0.10) : Qt.rgba(1,1,1,0.12)

    // ---- inputs: frosted glass (tinted) ----
    readonly property color inputFill: glassMode
             ? Qt.rgba(1, 1, 1, 0.09)
             : wallpaperActive ? Qt.rgba(28/255, 28/255, 28/255, 0.95) : Theme.inputBg

    // ---- chat: wallpaper shows through on glass ----
    readonly property color chatBg: glassMode
             ? Qt.rgba(1, 1, 1, 0.10)
             : wallpaperActive ? Qt.rgba(15/255, 15/255, 15/255, 0.60) : Theme.bg
    readonly property color chatPanelBg: glassMode
             ? Qt.rgba(1, 1, 1, 0.17)
             : wallpaperActive ? Qt.rgba(18/255, 18/255, 18/255, 0.90) : Theme.surface
    // AI bubble: NOT transparent in glass mode — back to the original opaque
    // dark so white AI text always stays crisp (user rule)
    readonly property color aiBubbleFill: glassMode ? Theme.surface
             : wallpaperActive ? Qt.rgba(37/255, 37/255, 37/255, 0.95) : Theme.surface
    // user bubble keeps its accent hue (opaque) on both modes
    readonly property color userBubbleFill: Theme.accent
    // text on the (accent-coloured) user bubble stays white in every mode
    readonly property color onUserBubble: "#FFFFFF"
}
