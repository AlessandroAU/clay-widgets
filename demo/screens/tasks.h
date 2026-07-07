// Tasks view: a working to-do manager built from the kit.
#ifndef CLAY_WIDGETS_DEMO_TASKS_H
#define CLAY_WIDGETS_DEMO_TASKS_H

#include "demo/demo_state.h"

namespace {

static void DrawTasksView(ClayWidgets_Context &ui, DemoState &s, bool compactLayout) {
    Clay_SizingAxis listWidth = compactLayout ? CLAY_SIZING_GROW(0) : CLAY_SIZING_PERCENT(0.58f);
    Clay_SizingAxis listHeight = compactLayout ? CLAY_SIZING_PERCENT(0.55f) : CLAY_SIZING_GROW(0);
    Clay_SizingAxis detailWidth = compactLayout ? CLAY_SIZING_GROW(0) : CLAY_SIZING_PERCENT(0.42f);

    CLAY(CLAY_ID("TasksColumns"), {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
            .childGap = 16,
            .layoutDirection = compactLayout ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
        },
    }) {
        CLAY(CLAY_ID("TaskListColumn"), {
            .layout = {
                .sizing = { .width = listWidth, .height = listHeight },
                .childGap = ui.theme.spacing.sm,
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
            },
        }) {
            // --- Add form -------------------------------------------------
            // clearOnEnter empties the buffer inside the widget, so snapshot the
            // text first: Enter + now-empty buffer means "submitted".
            Clay_ElementId addInputId = CLAY_ID("NewTaskInput");
            char pendingTitle[kTaskTitleCap];
            std::snprintf(pendingTitle, sizeof(pendingTitle), "%s", s.newTaskTitle);

            ClayWidgets_TextInput(&ui, addInputId, CLAY_STRING("Add a task"),
                s.newTaskTitle, static_cast<int32_t>(sizeof(s.newTaskTitle)),
                ClayWidgets_TextInputOptions{"What needs doing? Press Enter to add", true});
            bool enterAdd = ui.input.keyEnter && ui.focusedId == addInputId.id
                && pendingTitle[0] != '\0' && s.newTaskTitle[0] == '\0';

            bool buttonAdd = false;
            CLAY(CLAY_ID("AddTaskRow"), {
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                    .childGap = ui.theme.spacing.sm,
                    .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_BOTTOM },
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                },
            }) {
                ClayWidgets_Combo(&ui, CLAY_ID("NewTaskPriority"), CLAY_STRING("Priority"),
                    kPriorityNames, 3, &s.newTaskPriority);
                buttonAdd = ClayWidgets_ButtonEx(&ui, CLAY_ID("AddTaskButton"), CLAY_STRING("Add Task"),
                    ClayWidgets_ButtonOptions{CLAY_WIDGETS_BUTTON_PRIMARY, false});
            }

            const char *titleToAdd = nullptr;
            if (enterAdd) {
                titleToAdd = pendingTitle;
            } else if (buttonAdd && s.newTaskTitle[0] != '\0') {
                titleToAdd = s.newTaskTitle;
            }
            if (titleToAdd) {
                if (AddTask(s, titleToAdd, s.newTaskPriority)) {
                    s.selectedTask = s.taskCount - 1;
                    SetStatus(s, "Added: %s", s.tasks[s.taskCount - 1].title);
                    Notify(ui, s, CLAY_STRING("Task added"), CLAY_WIDGETS_BADGE_SUCCESS, 2.5f);
                } else {
                    SetStatus(s, "Task list is full (%d max)", kMaxTasks);
                }
                if (titleToAdd == s.newTaskTitle) {
                    s.newTaskTitle[0] = '\0';
                }
            }

            // --- Filter ---------------------------------------------------
            CLAY(CLAY_ID("TaskFilterRow"), {
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                    .childGap = ui.theme.spacing.sm,
                    .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                },
            }) {
                MutedLabel(ui, CLAY_STRING("Show"));
                ClayWidgets_Segmented(&ui, CLAY_ID("TaskFilter"), kTaskFilterNames, 3, &s.taskFilter);
            }

            // --- Task list (the new SelectRow widget) ----------------------
            int32_t shownCount = 0;
            ClayWidgets_BeginScrollPanel(&ui, CLAY_ID("TaskListPanel"),
                ClayWidgets_ScrollPanelOptions{ CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0), 0, 0, ui.theme.spacing.xs });
            {
                if (s.taskCount == 0) {
                    ClayWidgets_Label(&ui, CLAY_STRING("No tasks yet - add one above."));
                }
                for (int32_t i = 0; i < s.taskCount; ++i) {
                    DemoTask &t = s.tasks[i];
                    if (s.taskFilter == 1 && t.done) continue;
                    if (s.taskFilter == 2 && !t.done) continue;
                    ++shownCount;

                    Clay_ElementId rowId = Clay_GetElementIdWithIndex(CLAY_STRING("TaskRow"), static_cast<uint32_t>(i));
                    Clay_Color swatch = kPrioritySwatches[t.priority];
                    if (t.done) {
                        swatch.a = 96;
                    }
                    Clay_String trailing = t.done ? CLAY_STRING("done") : kPriorityNames[t.priority];

                    if (ClayWidgets_SelectRowEx(&ui, rowId, swatch, ClayStringFromCString(t.title), trailing, s.selectedTask == i)) {
                        s.selectedTask = i;
                        SetStatus(s, "Selected: %s", t.title);
                    }
                    if (ClayWidgets_RightClicked(&ui, rowId)) {
                        s.contextTask = i;
                        s.selectedTask = i;
                        ClayWidgets_OpenContextMenu(&ui, CLAY_ID("TaskMenu"), ui.input.mouseX, ui.input.mouseY);
                    }
                }
                if (s.taskCount > 0 && shownCount == 0) {
                    ClayWidgets_Label(&ui, CLAY_STRING("No tasks match this filter."));
                }
            }
            ClayWidgets_EndScrollPanel(&ui, CLAY_ID("TaskListPanel"));

            MutedLabel(ui, FormatString(s, "%d of %d tasks shown - right-click a row for actions", shownCount, s.taskCount));
        }

        // --- Detail editor: widgets bound straight into the task struct ----
        ClayWidgets_BeginScrollPanel(&ui, CLAY_ID("TaskDetailPanel"),
            ClayWidgets_ScrollPanelOptions{ detailWidth, CLAY_SIZING_GROW(0), 0, 0, ui.theme.spacing.md });
        {
            ClayWidgets_BeginCard(&ui, CLAY_ID("TaskDetailCard"), CLAY_STRING("Task details"));
            if (s.taskCount == 0) {
                ClayWidgets_Label(&ui, CLAY_STRING("Nothing selected - add a task on the left."));
            } else {
                DemoTask &t = s.tasks[s.selectedTask];

                ClayWidgets_TextInput(&ui, CLAY_ID("DetailTitle"), CLAY_STRING("Title"),
                    t.title, static_cast<int32_t>(sizeof(t.title)),
                    ClayWidgets_TextInputOptions{"Task title", false});

                ClayWidgets_Label(&ui, CLAY_STRING("Priority"));
                ClayWidgets_Radio(&ui, CLAY_ID("DetailPriorityHigh"), CLAY_STRING("High"), 0, &t.priority);
                ClayWidgets_Radio(&ui, CLAY_ID("DetailPriorityMedium"), CLAY_STRING("Medium"), 1, &t.priority);
                ClayWidgets_Radio(&ui, CLAY_ID("DetailPriorityLow"), CLAY_STRING("Low"), 2, &t.priority);

                ClayWidgets_Toggle(&ui, CLAY_ID("DetailDone"), CLAY_STRING("Completed"), &t.done);

                ClayWidgets_Separator(&ui);

                CLAY(CLAY_ID("DetailButtonRow"), {
                    .layout = {
                        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                        .childGap = ui.theme.spacing.sm,
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                    },
                }) {
                    if (ClayWidgets_Button(&ui, CLAY_ID("DetailDuplicate"), CLAY_STRING("Duplicate"))) {
                        DuplicateTask(ui, s, s.selectedTask);
                    }
                    if (ClayWidgets_ButtonEx(&ui, CLAY_ID("DetailDelete"), CLAY_STRING("Delete..."),
                            ClayWidgets_ButtonOptions{CLAY_WIDGETS_BUTTON_DANGER, false})) {
                        RequestDeleteTask(s, s.selectedTask);
                    }
                }
                MutedLabel(ui, CLAY_STRING("Edits write straight into the task struct - the list on the left updates live."));
            }
            ClayWidgets_EndCard(&ui, CLAY_ID("TaskDetailCard"));
        }
        ClayWidgets_EndScrollPanel(&ui, CLAY_ID("TaskDetailPanel"));
    }
}

} // namespace

#endif // CLAY_WIDGETS_DEMO_TASKS_H
