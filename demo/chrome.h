// Chrome: header, menu bar, navigation and status bar.
#ifndef CLAY_WIDGETS_DEMO_CHROME_H
#define CLAY_WIDGETS_DEMO_CHROME_H

#include "demo/demo-state.h"

namespace {

static void DrawHeader(ClayWidgets_Context &ui, DemoState &s, bool compactLayout) {
    CLAY(CLAY_ID("Header"), {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
            .childGap = ui.theme.spacing.md,
            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
    }) {
        ClayWidgets_Heading(&ui, CLAY_STRING("clay-widgets"));
        ClayWidgets_Badge(&ui, CLAY_STRING("demo"), CLAY_WIDGETS_BADGE_ACCENT);

        CLAY(CLAY_ID("HeaderSpacer"), {
            .layout = {
                .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(1) },
            },
        }) {}

        // Quick theme switcher; the Settings page has the same state as radios.
        if (!compactLayout) {
            int32_t themeIndex = s.themePreset - 1;
            if (themeIndex < 0) themeIndex = 0;
            if (themeIndex > 3) themeIndex = 3;
            if (ClayWidgets_Segmented(&ui, CLAY_ID("HeaderThemeSeg"), kThemeNames, 4, &themeIndex)) {
                SetStatus(s, "Theme: %s", kThemeNames[themeIndex].chars);
            }
            s.themePreset = themeIndex + 1;
            ClayWidgets_Tooltip(&ui, CLAY_ID("HeaderThemeSeg"), CLAY_STRING("Restyle every widget instantly"));
        }
    }
}

static void DrawMenuBar(ClayWidgets_Context &ui, DemoState &s) {
    CLAY(CLAY_ID("MenuBar"), {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
            .padding = CLAY_PADDING_ALL(ui.theme.spacing.xs),
            .childGap = ui.theme.spacing.xs,
            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
        .backgroundColor = ui.theme.surfaceAltColor,
        .cornerRadius = CLAY_CORNER_RADIUS((float)ui.theme.radiusSm),
    }) {
        if (ClayWidgets_BeginMenu(&ui, CLAY_ID("FileMenu"), CLAY_STRING("File"))) {
            if (ClayWidgets_MenuItem(&ui, CLAY_ID("MenuNewTask"), CLAY_STRING("New Sample Task"))) {
                AddSampleTask(ui, s);
            }
            if (ClayWidgets_MenuItem(&ui, CLAY_ID("MenuReset"), CLAY_STRING("Reset Demo"))) {
                s.pendingReset = true;
                Notify(ui, s, CLAY_STRING("Demo reset"), CLAY_WIDGETS_BADGE_NEUTRAL, 2.5f);
            }
            ClayWidgets_MenuSeparator(&ui);
            if (ClayWidgets_MenuItem(&ui, CLAY_ID("MenuDeleteTask"), CLAY_STRING("Delete Task..."))) {
                RequestDeleteTask(s, s.selectedTask);
            }
            ClayWidgets_EndMenu(&ui, CLAY_ID("FileMenu"));
        }

        if (ClayWidgets_BeginMenu(&ui, CLAY_ID("EditMenu"), CLAY_STRING("Edit"))) {
            if (ClayWidgets_MenuItem(&ui, CLAY_ID("MenuUndo"), CLAY_STRING("Undo"))) {
                SetStatus(s, "Menu: Undo");
            }
            if (ClayWidgets_MenuItem(&ui, CLAY_ID("MenuRedo"), CLAY_STRING("Redo"))) {
                SetStatus(s, "Menu: Redo");
            }
            ClayWidgets_EndMenu(&ui, CLAY_ID("EditMenu"));
        }

        if (ClayWidgets_BeginMenu(&ui, CLAY_ID("ViewMenu"), CLAY_STRING("View"))) {
            if (ClayWidgets_MenuItem(&ui, CLAY_ID("MenuViewDashboard"), CLAY_STRING("Dashboard"))) {
                s.activeView = kViewDashboard;
            }
            if (ClayWidgets_MenuItem(&ui, CLAY_ID("MenuViewTasks"), CLAY_STRING("Tasks"))) {
                s.activeView = kViewTasks;
            }
            if (ClayWidgets_MenuItem(&ui, CLAY_ID("MenuViewGallery"), CLAY_STRING("Gallery"))) {
                s.activeView = kViewGallery;
            }
            if (ClayWidgets_MenuItem(&ui, CLAY_ID("MenuViewSettings"), CLAY_STRING("Settings"))) {
                s.activeView = kViewSettings;
            }
            ClayWidgets_EndMenu(&ui, CLAY_ID("ViewMenu"));
        }
    }
}

static void DrawNavBar(ClayWidgets_Context &ui, DemoState &s) {
    CLAY(CLAY_ID("NavBar"), {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
            .childGap = 10,
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
    }) {
        ClayWidgets_Tab(&ui, CLAY_ID("NavDashboard"), CLAY_STRING("Dashboard"), kViewDashboard, &s.activeView);
        ClayWidgets_Tab(&ui, CLAY_ID("NavTasks"), CLAY_STRING("Tasks"), kViewTasks, &s.activeView);
        ClayWidgets_Tab(&ui, CLAY_ID("NavGallery"), CLAY_STRING("Gallery"), kViewGallery, &s.activeView);
        ClayWidgets_Tab(&ui, CLAY_ID("NavSettings"), CLAY_STRING("Settings"), kViewSettings, &s.activeView);

        ClayWidgets_Tooltip(&ui, CLAY_ID("NavDashboard"), CLAY_STRING("Live stats, a data table and quick actions"));
        ClayWidgets_Tooltip(&ui, CLAY_ID("NavTasks"), CLAY_STRING("A working to-do manager built from the kit"));
        ClayWidgets_Tooltip(&ui, CLAY_ID("NavGallery"), CLAY_STRING("The full widget catalog"));
        ClayWidgets_Tooltip(&ui, CLAY_ID("NavSettings"), CLAY_STRING("Theme, behavior and diagnostics"));
    }
}

static void DrawStatusBar(ClayWidgets_Context &ui, DemoState &s) {
    CLAY(CLAY_ID("StatusBar"), {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
            .padding = {
                .left = ui.theme.spacing.md,
                .right = ui.theme.spacing.md,
                .top = ui.theme.spacing.xs,
                .bottom = ui.theme.spacing.xs,
            },
            .childGap = ui.theme.spacing.md,
            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
        .backgroundColor = ui.theme.surfaceAltColor,
        .cornerRadius = CLAY_CORNER_RADIUS((float)ui.theme.radiusSm),
    }) {
        ClayWidgets_Label(&ui, ClayStringFromCString(s.statusLine));

        CLAY(CLAY_ID("StatusSpacer"), {
            .layout = {
                .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(1) },
            },
        }) {}

        MutedLabel(ui, FormatString(s, "focus %u  |  %d fps", ui.focusedId, GetFPS()));
    }
}

} // namespace

#endif // CLAY_WIDGETS_DEMO_CHROME_H
