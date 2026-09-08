// Settings view: theme presets, behavior toggles and diagnostics.
#ifndef CLAY_WIDGETS_DEMO_SETTINGS_H
#define CLAY_WIDGETS_DEMO_SETTINGS_H

#include "demo/demo-state.h"

namespace {

static void DrawSettingsView(ClayWidgets_Context &ui, DemoState &s) {
    ClayWidgets_BeginScrollPanel(&ui, CLAY_ID("SettingsPanel"),
        ClayWidgets_ScrollPanelOptions{ CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0), 0, 0, ui.theme.spacing.md });
    {
        ClayWidgets_BeginCard(&ui, CLAY_ID("AppearanceCard"), CLAY_STRING("Appearance"));
        {
            ClayWidgets_Label(&ui, CLAY_STRING("Theme preset"));
            ClayWidgets_Radio(&ui, CLAY_ID("ThemeSlate"), CLAY_STRING("Slate"), CLAY_WIDGETS_THEME_PRESET_SLATE, &s.themePreset);
            ClayWidgets_Radio(&ui, CLAY_ID("ThemeSand"), CLAY_STRING("Sand"), CLAY_WIDGETS_THEME_PRESET_SAND, &s.themePreset);
            ClayWidgets_Radio(&ui, CLAY_ID("ThemeForest"), CLAY_STRING("Forest"), CLAY_WIDGETS_THEME_PRESET_FOREST, &s.themePreset);
            ClayWidgets_Radio(&ui, CLAY_ID("ThemeWin95"), CLAY_STRING("Windows"), CLAY_WIDGETS_THEME_PRESET_WIN95, &s.themePreset);
            ClayWidgets_Radio(&ui, CLAY_ID("ThemeMacLight"), CLAY_STRING("macOS"), CLAY_WIDGETS_THEME_PRESET_MAC_LIGHT, &s.themePreset);
            ClayWidgets_Radio(&ui, CLAY_ID("ThemeMacDark"), CLAY_STRING("macOS Dark"), CLAY_WIDGETS_THEME_PRESET_MAC_DARK, &s.themePreset);

            ClayWidgets_Separator(&ui);
            ClayWidgets_Toggle(&ui, CLAY_ID("AnimToggle"), CLAY_STRING("Animate hover and state changes"), &s.animationsOn);
            MutedLabel(ui, CLAY_STRING("Also forced off by the --no-anim screenshot flag (reduce-motion support)."));
        }
        ClayWidgets_EndCard(&ui, CLAY_ID("AppearanceCard"));

        ClayWidgets_BeginCard(&ui, CLAY_ID("BehaviorCard"), CLAY_STRING("Behavior"));
        {
            if (ClayWidgets_Checkbox(&ui, CLAY_ID("NotificationsCheck"), CLAY_STRING("Show toast notifications"), &s.notifications)) {
                SetStatus(s, s.notifications ? "Notifications enabled" : "Notifications disabled");
                Notify(ui, s, CLAY_STRING("Notifications enabled"), CLAY_WIDGETS_BADGE_SUCCESS, 2.5f);
            }
            ClayWidgets_Combo(&ui, CLAY_ID("LogLevelCombo"), CLAY_STRING("Log level"), kLogLevelNames, 5, &s.logLevel);

            if (ClayWidgets_BeginCollapsible(&ui, CLAY_ID("AdvancedSection"), CLAY_STRING("Advanced options"), &s.showAdvanced)) {
                ClayWidgets_Checkbox(&ui, CLAY_ID("VerboseCheck"), CLAY_STRING("Verbose logging"), &s.verboseLogging);
                ClayWidgets_Checkbox(&ui, CLAY_ID("GpuCheck"), CLAY_STRING("Experimental GPU path"), &s.experimentalGpu);
                ClayWidgets_EndCollapsible(&ui, CLAY_ID("AdvancedSection"));
            }
        }
        ClayWidgets_EndCard(&ui, CLAY_ID("BehaviorCard"));

        ClayWidgets_BeginCard(&ui, CLAY_ID("DiagnosticsCard"), CLAY_STRING("Diagnostics"));
        {
            ClayWidgets_Label(&ui, FormatString(s, "Screen: %d x %d", GetScreenWidth(), GetScreenHeight()));
            ClayWidgets_Label(&ui, FormatString(s, "Pointer: %.0f, %.0f", ui.input.mouseX, ui.input.mouseY));
            ClayWidgets_Label(&ui, FormatString(s, "Focused widget id: %u", ui.focusedId));
            if (s.deterministic) {
                ClayWidgets_Label(&ui, FormatString(s, "Frame rate: -- fps"));
            } else {
                ClayWidgets_Label(&ui, FormatString(s, "Frame rate: %d fps", GetFPS()));
            }
            ClayWidgets_Label(&ui, FormatString(s, "Animations: %s", ui.animationsEnabled ? "on" : "off"));

            ClayWidgets_Separator(&ui);
            if (ClayWidgets_ButtonEx(&ui, CLAY_ID("ResetDemoButton"), CLAY_STRING("Reset Demo State"),
                    ClayWidgets_ButtonOptions{CLAY_WIDGETS_BUTTON_DANGER, false})) {
                s.pendingReset = true;
                Notify(ui, s, CLAY_STRING("Demo reset"), CLAY_WIDGETS_BADGE_NEUTRAL, 2.5f);
            }
        }
        ClayWidgets_EndCard(&ui, CLAY_ID("DiagnosticsCard"));

        ClayWidgets_BeginCard(&ui, CLAY_ID("AboutCard"), CLAY_STRING("About"));
        {
            ClayWidgets_Label(&ui, CLAY_STRING(
                "clay-widgets demo - a single-file example application for the clay-widgets immediate-mode UI kit.\n"
                "\n"
                "Bring your own renderer: the widgets only emit Clay render commands. This build draws them with raylib "
                "and compiles unchanged to WebAssembly."));
        }
        ClayWidgets_EndCard(&ui, CLAY_ID("AboutCard"));
    }
    ClayWidgets_EndScrollPanel(&ui, CLAY_ID("SettingsPanel"));
}

} // namespace

#endif // CLAY_WIDGETS_DEMO_SETTINGS_H
