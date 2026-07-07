// Floating layers: context menus and the delete-confirmation modal.
#ifndef CLAY_WIDGETS_DEMO_FLOATING_H
#define CLAY_WIDGETS_DEMO_FLOATING_H

#include "demo/demo_state.h"

namespace {

static void DrawFloatingLayers(ClayWidgets_Context &ui, DemoState &s) {
    if (ClayWidgets_BeginContextMenu(&ui, CLAY_ID("TaskMenu"))) {
        bool valid = s.contextTask >= 0 && s.contextTask < s.taskCount;
        if (valid) {
            DemoTask &t = s.tasks[s.contextTask];
            if (ClayWidgets_MenuItem(&ui, CLAY_ID("TaskMenuToggle"),
                    t.done ? CLAY_STRING("Mark as active") : CLAY_STRING("Mark as done"))) {
                t.done = !t.done;
                SetStatus(s, "%s: %s", t.done ? "Completed" : "Reopened", t.title);
            }
            if (ClayWidgets_MenuItem(&ui, CLAY_ID("TaskMenuDuplicate"), CLAY_STRING("Duplicate"))) {
                DuplicateTask(ui, s, s.contextTask);
            }
            ClayWidgets_MenuSeparator(&ui);
            if (ClayWidgets_MenuItem(&ui, CLAY_ID("TaskMenuDelete"), CLAY_STRING("Delete..."))) {
                RequestDeleteTask(s, s.contextTask);
            }
        } else {
            ClayWidgets_MenuItem(&ui, CLAY_ID("TaskMenuNone"), CLAY_STRING("(no task)"));
        }
        ClayWidgets_EndContextMenu(&ui, CLAY_ID("TaskMenu"));
    }

    if (ClayWidgets_BeginContextMenu(&ui, CLAY_ID("GalleryMenu"))) {
        if (ClayWidgets_MenuItem(&ui, CLAY_ID("CtxCut"), CLAY_STRING("Cut"))) {
            SetStatus(s, "Context: Cut");
        }
        if (ClayWidgets_MenuItem(&ui, CLAY_ID("CtxCopy"), CLAY_STRING("Copy"))) {
            SetStatus(s, "Context: Copy");
        }
        if (ClayWidgets_MenuItem(&ui, CLAY_ID("CtxPaste"), CLAY_STRING("Paste"))) {
            SetStatus(s, "Context: Paste");
        }
        ClayWidgets_MenuSeparator(&ui);
        if (ClayWidgets_MenuItem(&ui, CLAY_ID("CtxProps"), CLAY_STRING("Properties..."))) {
            SetStatus(s, "Context: Properties");
        }
        ClayWidgets_EndContextMenu(&ui, CLAY_ID("GalleryMenu"));
    }

    if (ClayWidgets_BeginModal(&ui, CLAY_ID("ConfirmDeleteModal"), CLAY_STRING("Delete task?"), &s.showDeleteModal)) {
        int32_t target = (s.deleteTarget >= 0 && s.deleteTarget < s.taskCount)
            ? s.deleteTarget
            : (s.taskCount > 0 ? s.selectedTask : -1);
        if (target >= 0 && target < s.taskCount) {
            ClayWidgets_Label(&ui, FormatString(s, "\"%s\" will be permanently removed.", s.tasks[target].title));
        } else {
            ClayWidgets_Label(&ui, CLAY_STRING("There is no task to delete."));
        }
        MutedLabel(ui, CLAY_STRING("The scrim dims and blocks everything behind this dialog until it is dismissed."));

        CLAY(CLAY_ID("ModalButtonRow"), {
            .layout = {
                .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                .childGap = ui.theme.spacing.sm,
                .childAlignment = { .x = CLAY_ALIGN_X_RIGHT, .y = CLAY_ALIGN_Y_CENTER },
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
            },
        }) {
            if (ClayWidgets_Button(&ui, CLAY_ID("ModalCancel"), CLAY_STRING("Cancel"))) {
                s.showDeleteModal = false;
            }
            if (ClayWidgets_ButtonEx(&ui, CLAY_ID("ModalDelete"), CLAY_STRING("Delete"),
                    ClayWidgets_ButtonOptions{CLAY_WIDGETS_BUTTON_DANGER, target < 0})) {
                SetStatus(s, "Deleted: %s", s.tasks[target].title);
                s.pendingDelete = target;
                s.showDeleteModal = false;
                Notify(ui, s, CLAY_STRING("Task deleted"), CLAY_WIDGETS_BADGE_DANGER, 2.5f);
            }
        }

        ClayWidgets_EndModal(&ui, CLAY_ID("ConfirmDeleteModal"));
    }
}

} // namespace

#endif // CLAY_WIDGETS_DEMO_FLOATING_H
