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
} ClayWidgets_ThemePreset;

ClayWidgets_Theme ClayWidgets_DefaultTheme(void);
ClayWidgets_Theme ClayWidgets_ThemeSlate(void);
ClayWidgets_Theme ClayWidgets_ThemeSand(void);
ClayWidgets_Theme ClayWidgets_ThemeForest(void);
ClayWidgets_Theme ClayWidgets_ThemeWin95(void);
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

    theme.radiusSm = 6;
    theme.radiusMd = 10;

    theme.fontBody = 0;
    theme.fontHeading = 0;
    theme.fontMono = 0;

    theme.fontSizeBody = 16;
    theme.fontSizeHeading = 24;
    theme.fontSizeSmall = 13;

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

// A classic Microsoft Windows (95/98/2000-era) look: the "3D face" gray control
// surface, black text, a navy selection accent, and square corners. The beveled
// grey aesthetic depends on sharp rectangles, so this preset zeroes the corner
// radii that ClayWidgets__BuildTheme sets by default.
ClayWidgets_Theme ClayWidgets_ThemeWin95(void) {
    ClayWidgets_Theme theme = ClayWidgets__BuildTheme(
        (Clay_Color){0, 0, 0, 255},          // textColor        - black
        (Clay_Color){64, 64, 64, 255},       // textMutedColor   - dim label grey
        (Clay_Color){192, 192, 192, 255},    // surfaceColor     - 3D face grey
        (Clay_Color){192, 192, 192, 255},    // surfaceAltColor  - button/tab face
        (Clay_Color){0, 0, 128, 255},        // accentColor      - navy selection
        (Clay_Color){0, 0, 128, 90},         // accentMutedColor - navy wash
        (Clay_Color){128, 128, 128, 255},    // borderColor      - shadow grey
        (Clay_Color){212, 208, 200, 255},    // hoverColor       - lit face grey
        (Clay_Color){160, 160, 160, 255},    // pressedColor     - sunken grey
        (Clay_Color){0, 0, 128, 255}         // focusRingColor   - navy focus
    );

    // Square, beveled-era corners - the defining trait of the classic look.
    theme.radiusSm = 0;
    theme.radiusMd = 0;

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
        case CLAY_WIDGETS_THEME_PRESET_SLATE:
        default:
            return ClayWidgets_ThemeSlate();
    }
}

#endif

#endif