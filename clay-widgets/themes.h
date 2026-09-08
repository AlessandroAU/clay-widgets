#ifndef CLAY_WIDGETS_THEMES_DETAIL_H
#define CLAY_WIDGETS_THEMES_DETAIL_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before themes.h"
#endif

typedef enum ClayWidgets_ThemePreset {
    CLAY_WIDGETS_THEME_PRESET_SLATE = 1,
    CLAY_WIDGETS_THEME_PRESET_SAND = 2,
    CLAY_WIDGETS_THEME_PRESET_FOREST = 3,
    CLAY_WIDGETS_THEME_PRESET_WIN95 = 4,
    CLAY_WIDGETS_THEME_PRESET_MAC_LIGHT = 5,
    CLAY_WIDGETS_THEME_PRESET_MAC_DARK = 6,
} ClayWidgets_ThemePreset;

ClayWidgets_Theme ClayWidgets_DefaultTheme(void);
ClayWidgets_Theme ClayWidgets_ThemeSlate(void);
ClayWidgets_Theme ClayWidgets_ThemeSand(void);
ClayWidgets_Theme ClayWidgets_ThemeForest(void);
ClayWidgets_Theme ClayWidgets_ThemeWin95(void);
ClayWidgets_Theme ClayWidgets_ThemeMacLight(void);
ClayWidgets_Theme ClayWidgets_ThemeMacDark(void);
ClayWidgets_Theme ClayWidgets_ThemeFromPreset(ClayWidgets_ThemePreset preset);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

static ClayWidgets_Theme ClayWidgets__BuildTheme(
    Clay_Color textColor,
    Clay_Color textMutedColor,
    Clay_Color surfaceColor,
    Clay_Color surfaceAltColor,
    Clay_Color accentColor,
    Clay_Color accentMutedColor,
    Clay_Color borderColor,
    Clay_Color hoverColor,
    Clay_Color pressedColor,
    Clay_Color focusRingColor
) {
    ClayWidgets_Theme theme;
    theme.textColor = textColor;
    theme.textMutedColor = textMutedColor;
    theme.surfaceColor = surfaceColor;
    theme.surfaceAltColor = surfaceAltColor;
    theme.accentColor = accentColor;
    theme.accentMutedColor = accentMutedColor;
    theme.borderColor = borderColor;
    theme.hoverColor = hoverColor;
    theme.pressedColor = pressedColor;
    theme.focusRingColor = focusRingColor;

    // Semantic status palette and overlay colors. One set of defaults for all
    // presets so a status reads identically across themes; presets override
    // after building (as Win95 does with the radii) when the look demands it.
    theme.successColor = (Clay_Color){46, 160, 67, 255};
    theme.warningColor = (Clay_Color){191, 135, 0, 255};
    theme.dangerColor = (Clay_Color){207, 54, 54, 255};
    theme.onAccentColor = (Clay_Color){245, 248, 252, 255};
    theme.scrimColor = (Clay_Color){0, 0, 0, 150};
    theme.disabledMix = 0.5f;

    // Highlight bar and typed-into surfaces both follow the preset's own colors
    // by default, so a theme only has to name them when it wants them to differ
    // (as the classic preset does: a navy selection bar and paper-white fields).
    theme.selectionColor = hoverColor;
    theme.onSelectionColor = textColor;
    theme.fieldColor = surfaceAltColor;

    // Flat 1px borders, no 3D edges and no drop shadow. The edge palette is
    // still filled in - derived from the preset's own surface and border - so a
    // caller that flips edgeStyle to BEVEL gets a coherent look rather than
    // four transparent bands.
    theme.edgeStyle = CLAY_WIDGETS_EDGE_STYLE_FLAT;
    theme.edgeHighlightColor = ClayWidgets__MixColor(surfaceAltColor, (Clay_Color){255, 255, 255, 255}, 0.55f);
    theme.edgeLightColor = ClayWidgets__MixColor(surfaceAltColor, (Clay_Color){255, 255, 255, 255}, 0.25f);
    theme.edgeShadowColor = borderColor;
    theme.edgeDarkColor = ClayWidgets__MixColor(borderColor, (Clay_Color){0, 0, 0, 255}, 0.6f);
    theme.shadowColor = (Clay_Color){0, 0, 0, 110};
    theme.shadowOffset = 0;

    theme.radiusSm = 6;
    theme.radiusMd = 10;

    theme.fontBody = 0;
    theme.fontHeading = 0;
    theme.fontMono = 0;

    theme.fontSizeBody = 18;
    theme.fontSizeHeading = 26;
    theme.fontSizeSmall = 14;

    theme.spacing.xs = 4;
    theme.spacing.sm = 8;
    theme.spacing.md = 12;
    theme.spacing.lg = 18;

    return theme;
}

ClayWidgets_Theme ClayWidgets_DefaultTheme(void) {
    return ClayWidgets_ThemeSlate();
}

ClayWidgets_Theme ClayWidgets_ThemeSlate(void) {
    return ClayWidgets__BuildTheme(
        (Clay_Color){245, 245, 245, 255},
        (Clay_Color){176, 184, 196, 255},
        (Clay_Color){33, 37, 43, 255},
        (Clay_Color){45, 50, 58, 255},
        (Clay_Color){44, 153, 255, 255},
        (Clay_Color){44, 153, 255, 110},
        (Clay_Color){80, 88, 100, 255},
        (Clay_Color){59, 66, 77, 255},
        (Clay_Color){70, 79, 92, 255},
        (Clay_Color){130, 197, 255, 255}
    );
}

ClayWidgets_Theme ClayWidgets_ThemeSand(void) {
    return ClayWidgets__BuildTheme(
        (Clay_Color){56, 44, 32, 255},
        (Clay_Color){126, 107, 84, 255},
        (Clay_Color){238, 226, 205, 255},
        (Clay_Color){226, 211, 188, 255},
        (Clay_Color){195, 120, 63, 255},
        (Clay_Color){195, 120, 63, 110},
        (Clay_Color){174, 152, 122, 255},
        (Clay_Color){216, 199, 173, 255},
        (Clay_Color){202, 182, 154, 255},
        (Clay_Color){223, 142, 81, 255}
    );
}

ClayWidgets_Theme ClayWidgets_ThemeForest(void) {
    return ClayWidgets__BuildTheme(
        (Clay_Color){234, 244, 236, 255},
        (Clay_Color){159, 181, 165, 255},
        (Clay_Color){25, 44, 37, 255},
        (Clay_Color){33, 59, 48, 255},
        (Clay_Color){92, 188, 123, 255},
        (Clay_Color){92, 188, 123, 110},
        (Clay_Color){68, 104, 84, 255},
        (Clay_Color){43, 74, 60, 255},
        (Clay_Color){54, 90, 72, 255},
        (Clay_Color){145, 223, 171, 255}
    );
}

// A classic Microsoft Windows (95/98-era) look: the "3D face" grey control
// surface, paper-white entry fields, black text, a navy selection bar and
// square, beveled corners.
//
// This is the one preset that switches edgeStyle to BEVEL, so its controls are
// drawn with two-tone 3D edges instead of flat 1px borders (see
// ClayWidgets_Edge) and its floating chrome casts a hard drop shadow. It also
// drops the corner radii to zero and tightens the type scale and spacing: the
// beveled look depends on sharp rectangles and a dense, small-text layout, and
// keeping the modern radii or the airy spacing reads as a grey modern UI rather
// than a classic one.
ClayWidgets_Theme ClayWidgets_ThemeWin95(void) {
    ClayWidgets_Theme theme = ClayWidgets__BuildTheme(
        (Clay_Color){0, 0, 0, 255},          // textColor        - black
        (Clay_Color){90, 90, 90, 255},       // textMutedColor   - dim label grey
        (Clay_Color){192, 192, 192, 255},    // surfaceColor     - 3D face grey
        (Clay_Color){192, 192, 192, 255},    // surfaceAltColor  - button/tab face
        (Clay_Color){0, 0, 128, 255},        // accentColor      - navy selection
        (Clay_Color){0, 0, 128, 90},         // accentMutedColor - navy wash
        (Clay_Color){128, 128, 128, 255},    // borderColor      - shadow grey
        (Clay_Color){198, 198, 198, 255},    // hoverColor       - barely-lit face
        (Clay_Color){176, 176, 176, 255},    // pressedColor     - sunken grey
        (Clay_Color){0, 0, 0, 255}           // focusRingColor   - black focus rect
    );

    // Square, beveled-era corners - the defining trait of the classic look.
    theme.radiusSm = 0;
    theme.radiusMd = 0;

    // Two-tone 3D edges, in the four system colors a classic control is built
    // from: 3DLIGHT and 3DHILIGHT catch the light from the top left, 3DSHADOW
    // and 3DDKSHADOW fall away to the bottom right.
    theme.edgeStyle = CLAY_WIDGETS_EDGE_STYLE_BEVEL;
    theme.edgeLightColor = (Clay_Color){223, 223, 223, 255};
    theme.edgeHighlightColor = (Clay_Color){255, 255, 255, 255};
    theme.edgeShadowColor = (Clay_Color){128, 128, 128, 255};
    theme.edgeDarkColor = (Clay_Color){10, 10, 10, 255};

    // The hard shadow menus and dialogs cast onto the surface behind them.
    theme.shadowColor = (Clay_Color){0, 0, 0, 96};
    theme.shadowOffset = 4;

    // Anything you type or pick into is a white well; the highlight bar behind
    // a menu item or list row is solid navy with white text.
    theme.fieldColor = (Clay_Color){255, 255, 255, 255};
    theme.selectionColor = (Clay_Color){0, 0, 128, 255};
    theme.onSelectionColor = (Clay_Color){255, 255, 255, 255};

    // Era-appropriate status colors: the saturated primaries of the classic
    // 16-color palette, with pure white for text on filled surfaces.
    theme.successColor = (Clay_Color){0, 128, 0, 255};
    theme.warningColor = (Clay_Color){128, 96, 0, 255};
    theme.dangerColor = (Clay_Color){192, 0, 0, 255};
    theme.onAccentColor = (Clay_Color){255, 255, 255, 255};

    // Small type and tight spacing, the way a 96-DPI dialog was laid out.
    theme.fontSizeBody = 16;
    theme.fontSizeHeading = 22;
    theme.fontSizeSmall = 13;
    theme.spacing.xs = 3;
    theme.spacing.sm = 6;
    theme.spacing.md = 9;
    theme.spacing.lg = 12;

    return theme;
}

// A modern macOS look (Big Sur and later): a near-white window with white
// control surfaces floating on it, hairline separators, generous corner radii
// and the system blue accent. Unlike the classic preset this needs no new
// drawing - the flat edge style already draws exactly what the look is made of -
// so it is a palette, a type scale and a geometry, and nothing else.
//
// The two variants differ only in their colors; everything about their
// proportions is shared through ClayWidgets__MacGeometry.
static void ClayWidgets__MacGeometry(ClayWidgets_Theme *theme) {
    // Softer than the library default: a macOS control is a rounded rectangle
    // first and a bordered box second.
    theme->radiusSm = 6;
    theme->radiusMd = 12;

    // Smaller body text with more room around it, the way a Mac window breathes.
    theme->fontSizeBody = 17;
    theme->fontSizeHeading = 24;
    theme->fontSizeSmall = 13;
    theme->spacing.xs = 4;
    theme->spacing.sm = 8;
    theme->spacing.md = 12;
    theme->spacing.lg = 20;

    // Menu items, dropdown items and hovered rows take the accent as a solid
    // bar with light text, the way a Mac menu highlights.
    theme->selectionColor = theme->accentColor;
    theme->onSelectionColor = (Clay_Color){255, 255, 255, 255};
    theme->onAccentColor = (Clay_Color){255, 255, 255, 255};

    // No drop shadow: a Mac popover casts a soft shadow on every side, and the
    // one this library can draw is a hard offset pair of strips, which would
    // read as a sticker rather than as depth. Better none than a wrong one.
    theme->shadowOffset = 0;
}

ClayWidgets_Theme ClayWidgets_ThemeMacLight(void) {
    ClayWidgets_Theme theme = ClayWidgets__BuildTheme(
        (Clay_Color){29, 29, 31, 255},       // textColor        - near black
        (Clay_Color){134, 134, 139, 255},    // textMutedColor   - secondary label
        (Clay_Color){245, 245, 247, 255},    // surfaceColor     - window background
        (Clay_Color){255, 255, 255, 255},    // surfaceAltColor  - cards and controls
        (Clay_Color){0, 122, 255, 255},      // accentColor      - system blue
        (Clay_Color){0, 122, 255, 72},       // accentMutedColor - selected row wash
        (Clay_Color){209, 209, 214, 255},    // borderColor      - hairline
        (Clay_Color){242, 242, 247, 255},    // hoverColor       - control, hovered
        (Clay_Color){229, 229, 234, 255},    // pressedColor     - control, held
        (Clay_Color){0, 113, 227, 255}       // focusRingColor   - keyboard focus
    );

    theme.successColor = (Clay_Color){52, 199, 89, 255};
    theme.warningColor = (Clay_Color){255, 149, 0, 255};
    theme.dangerColor = (Clay_Color){255, 59, 48, 255};
    theme.scrimColor = (Clay_Color){0, 0, 0, 90};
    theme.disabledMix = 0.55f;

    ClayWidgets__MacGeometry(&theme);
    return theme;
}

ClayWidgets_Theme ClayWidgets_ThemeMacDark(void) {
    ClayWidgets_Theme theme = ClayWidgets__BuildTheme(
        (Clay_Color){245, 245, 247, 255},    // textColor        - near white
        (Clay_Color){152, 152, 157, 255},    // textMutedColor   - secondary label
        (Clay_Color){28, 28, 30, 255},       // surfaceColor     - window background
        (Clay_Color){44, 44, 46, 255},       // surfaceAltColor  - cards and controls
        (Clay_Color){10, 132, 255, 255},     // accentColor      - system blue, dark
        (Clay_Color){10, 132, 255, 96},      // accentMutedColor - selected row wash
        (Clay_Color){58, 58, 60, 255},       // borderColor      - hairline
        (Clay_Color){58, 58, 60, 255},       // hoverColor       - control, hovered
        (Clay_Color){72, 72, 74, 255},       // pressedColor     - control, held
        (Clay_Color){64, 156, 255, 255}      // focusRingColor   - keyboard focus
    );

    theme.successColor = (Clay_Color){48, 209, 88, 255};
    theme.warningColor = (Clay_Color){255, 159, 10, 255};
    theme.dangerColor = (Clay_Color){255, 69, 58, 255};
    theme.scrimColor = (Clay_Color){0, 0, 0, 140};

    ClayWidgets__MacGeometry(&theme);
    // Entry fields recede in dark mode rather than lifting: a text field is
    // darker than the card it sits on, not lighter.
    theme.fieldColor = (Clay_Color){28, 28, 30, 255};
    return theme;
}

ClayWidgets_Theme ClayWidgets_ThemeFromPreset(ClayWidgets_ThemePreset preset) {
    switch (preset) {
        case CLAY_WIDGETS_THEME_PRESET_SAND:
            return ClayWidgets_ThemeSand();
        case CLAY_WIDGETS_THEME_PRESET_FOREST:
            return ClayWidgets_ThemeForest();
        case CLAY_WIDGETS_THEME_PRESET_WIN95:
            return ClayWidgets_ThemeWin95();
        case CLAY_WIDGETS_THEME_PRESET_MAC_LIGHT:
            return ClayWidgets_ThemeMacLight();
        case CLAY_WIDGETS_THEME_PRESET_MAC_DARK:
            return ClayWidgets_ThemeMacDark();
        case CLAY_WIDGETS_THEME_PRESET_SLATE:
        default:
            return ClayWidgets_ThemeSlate();
    }
}

#endif

#endif