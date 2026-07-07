// Dashboard view: live task stats, a data table and quick actions.
#ifndef CLAY_WIDGETS_DEMO_DASHBOARD_H
#define CLAY_WIDGETS_DEMO_DASHBOARD_H

#include "demo/demo_state.h"

namespace {

static void StatCard(ClayWidgets_Context &ui, Clay_ElementId id, Clay_String value, Clay_String caption) {
    ClayWidgets_BeginCardEx(&ui, id, Clay_String{}, CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0));
    CLAY_TEXT(value, {
        .textColor = ui.theme.textColor,
        .fontId = ui.theme.fontHeading,
        .fontSize = static_cast<uint16_t>(ui.theme.fontSizeHeading + 6),
    });
    MutedLabel(ui, caption);
    ClayWidgets_EndCard(&ui, id);
}

static void DrawDashboardView(ClayWidgets_Context &ui, DemoState &s, bool compactLayout) {
    int32_t doneCount = CountCompletedTasks(s);
    int32_t openCount = s.taskCount - doneCount;
    float completion = s.taskCount > 0 ? static_cast<float>(doneCount) / static_cast<float>(s.taskCount) : 0.0f;

    ClayWidgets_BeginScrollPanel(&ui, CLAY_ID("DashboardPanel"),
        ClayWidgets_ScrollPanelOptions{ CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0), 0, 0, ui.theme.spacing.md });
    {
        CLAY(CLAY_ID("StatRow"), {
            .layout = {
                .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                .childGap = ui.theme.spacing.md,
                .layoutDirection = compactLayout ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
            },
        }) {
            StatCard(ui, CLAY_ID("StatOpen"), FormatString(s, "%d", openCount), CLAY_STRING("open tasks"));
            StatCard(ui, CLAY_ID("StatDone"), FormatString(s, "%d", doneCount), CLAY_STRING("completed"));

            ClayWidgets_BeginCardEx(&ui, CLAY_ID("StatProgress"), Clay_String{}, CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0));
            CLAY_TEXT(FormatString(s, "%d%%", static_cast<int>(std::lround(completion * 100.0f))), {
                .textColor = ui.theme.textColor,
                .fontId = ui.theme.fontHeading,
                .fontSize = static_cast<uint16_t>(ui.theme.fontSizeHeading + 6),
            });
            ClayWidgets_ProgressBar(&ui, CLAY_ID("DashProgress"), completion, CLAY_STRING("Completion"));
            ClayWidgets_EndCard(&ui, CLAY_ID("StatProgress"));
        }

        ClayWidgets_BeginCard(&ui, CLAY_ID("WelcomeCard"), CLAY_STRING("Welcome"));
        {
            ClayWidgets_Label(&ui, CLAY_STRING(
                "clay-widgets is a small immediate-mode widget kit built on Clay's layout engine, rendered here with raylib. "
                "Every frame this whole interface is rebuilt from plain structs - there is no retained widget tree to keep in sync.\n"
                "\n"
                "The Tasks view is a tiny working app built from the kit, the Gallery catalogs every widget, and Settings changes "
                "how the demo itself looks and behaves."));
            MutedLabel(ui, CLAY_STRING("Tab / Shift+Tab move focus, Enter activates, Escape dismisses. Right-click task rows for a context menu."));
            CLAY(CLAY_ID("WelcomeBadges"), {
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                    .childGap = ui.theme.spacing.sm,
                    .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                },
            }) {
                ClayWidgets_Badge(&ui, CLAY_STRING("immediate mode"), CLAY_WIDGETS_BADGE_ACCENT);
                ClayWidgets_Badge(&ui, CLAY_STRING("keyboard navigable"), CLAY_WIDGETS_BADGE_SUCCESS);
                ClayWidgets_Badge(&ui, CLAY_STRING("4 theme presets"), CLAY_WIDGETS_BADGE_NEUTRAL);
                ClayWidgets_Badge(&ui, CLAY_STRING("runs on web"), CLAY_WIDGETS_BADGE_WARNING);
            }
        }
        ClayWidgets_EndCard(&ui, CLAY_ID("WelcomeCard"));

        ClayWidgets_BeginCard(&ui, CLAY_ID("BuildsCard"), CLAY_STRING("Recent builds"));
        {
            ClayWidgets_TableColumn buildCols[] = {
                { CLAY_STRING("Pipeline"), CLAY_SIZING_GROW(0) },
                { CLAY_STRING("Status"), CLAY_SIZING_FIXED(110) },
                { CLAY_STRING("Duration"), CLAY_SIZING_FIXED(100) },
            };
            ClayWidgets_BeginTable(&ui, CLAY_ID("BuildsTable"), buildCols, 3);
            for (int32_t r = 0; r < kBuildRowCount; ++r) {
                Clay_ElementId rowId = Clay_GetElementIdWithIndex(CLAY_STRING("BuildRow"), static_cast<uint32_t>(r));
                Clay_String cells[] = { kBuildNames[r], kBuildStates[r], kBuildTimes[r] };
                if (ClayWidgets_TableRow(&ui, rowId, cells, 3, r, r == s.selectedBuild)) {
                    s.selectedBuild = r;
                    SetStatus(s, "Selected build: %.*s", static_cast<int>(kBuildNames[r].length), kBuildNames[r].chars);
                }
            }
            ClayWidgets_EndTable(&ui, CLAY_ID("BuildsTable"));

            CLAY(CLAY_ID("BuildLegend"), {
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                    .childGap = ui.theme.spacing.sm,
                    .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                },
            }) {
                ClayWidgets_Badge(&ui, CLAY_STRING("passing"), CLAY_WIDGETS_BADGE_SUCCESS);
                ClayWidgets_Badge(&ui, CLAY_STRING("flaky"), CLAY_WIDGETS_BADGE_WARNING);
                ClayWidgets_Badge(&ui, CLAY_STRING("broken"), CLAY_WIDGETS_BADGE_DANGER);
            }
        }
        ClayWidgets_EndCard(&ui, CLAY_ID("BuildsCard"));

        ClayWidgets_BeginCard(&ui, CLAY_ID("ActionsCard"), CLAY_STRING("Quick actions"));
        {
            CLAY(CLAY_ID("ActionsRow"), {
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                    .childGap = ui.theme.spacing.sm,
                    .layoutDirection = compactLayout ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                },
            }) {
                if (ClayWidgets_ButtonEx(&ui, CLAY_ID("ActionAddTask"), CLAY_STRING("Add Sample Task"),
                        ClayWidgets_ButtonOptions{CLAY_WIDGETS_BUTTON_PRIMARY, false})) {
                    AddSampleTask(ui, s);
                }
                if (ClayWidgets_Button(&ui, CLAY_ID("ActionToast"), CLAY_STRING("Show a Toast"))) {
                    SetStatus(s, "Toast requested");
                    Notify(ui, s, CLAY_STRING("Toasts float above every view"), CLAY_WIDGETS_BADGE_ACCENT, 3.0f);
                }
                if (ClayWidgets_ButtonEx(&ui, CLAY_ID("ActionClearDone"), CLAY_STRING("Clear Completed"),
                        ClayWidgets_ButtonOptions{CLAY_WIDGETS_BUTTON_DANGER, false})) {
                    if (doneCount > 0) {
                        s.pendingClearCompleted = true;
                        SetStatus(s, "Cleared %d completed task(s)", doneCount);
                        Notify(ui, s, CLAY_STRING("Completed tasks cleared"), CLAY_WIDGETS_BADGE_DANGER, 2.5f);
                    } else {
                        SetStatus(s, "Nothing to clear");
                    }
                }
                ClayWidgets_ButtonEx(&ui, CLAY_ID("ActionDeploy"), CLAY_STRING("Deploy"),
                    ClayWidgets_ButtonOptions{CLAY_WIDGETS_BUTTON_DEFAULT, true});
            }
            MutedLabel(ui, CLAY_STRING("The last button is disabled: inert, muted, and skipped by Tab."));
        }
        ClayWidgets_EndCard(&ui, CLAY_ID("ActionsCard"));
    }
    ClayWidgets_EndScrollPanel(&ui, CLAY_ID("DashboardPanel"));
}

} // namespace

#endif // CLAY_WIDGETS_DEMO_DASHBOARD_H
