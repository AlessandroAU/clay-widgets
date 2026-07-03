#ifndef CLAY_WIDGETS_MODAL_H
#define CLAY_WIDGETS_MODAL_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before modal.h"
#endif

// A modal dialog: a full-screen dimming scrim that captures input plus a
// centered, titled panel drawn on top. The caller owns an `open` bool. Usage:
//
//   if (ClayWidgets_BeginModal(&ui, CLAY_ID("Confirm"), CLAY_STRING("Delete?"), &showDialog)) {
//       ClayWidgets_Label(&ui, CLAY_STRING("This cannot be undone."));
//       if (ClayWidgets_Button(&ui, CLAY_ID("ConfirmOk"), CLAY_STRING("Delete"))) {
//           doDelete();
//           showDialog = false;
//       }
//       ClayWidgets_EndModal(&ui, CLAY_ID("Confirm"));
//   }
//
// Everything declared between Begin and End becomes the dialog body. BeginModal
// returns false (and opens no elements) when the dialog is closed, so the body
// and EndModal are skipped. The dialog closes on Escape, on a click of the
// scrim outside the panel, or on the title-bar close button.
bool ClayWidgets_BeginModal(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String title, bool *open);
void ClayWidgets_EndModal(ClayWidgets_Context *ctx, Clay_ElementId id);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

static Clay_ElementId ClayWidgets__ModalScrimId(Clay_ElementId id) {
    return Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsModalScrim"), id.id);
}
static Clay_ElementId ClayWidgets__ModalDialogId(Clay_ElementId id) {
    return Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsModalDialog"), id.id);
}

bool ClayWidgets_BeginModal(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String title, bool *open) {
    if (!ctx || !open || !*open) {
        return false;
    }

    Clay_ElementId scrimId = ClayWidgets__ModalScrimId(id);
    Clay_ElementId dialogId = ClayWidgets__ModalDialogId(id);
    Clay_ElementId closeId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsModalClose"), id.id);

    // Dismissal from the previous frame's geometry: Escape, or a press on the
    // scrim that is not inside the dialog. Detected before any element opens so
    // the modal vanishes on the same frame it is dismissed.
    bool dismiss = ctx->input.keyEscape;
    if (ctx->input.pointerPressed && Clay_PointerOver(scrimId) && !Clay_PointerOver(dialogId)) {
        dismiss = true;
    }
    if (dismiss) {
        *open = false;
        return false;
    }

    float screenW = ctx->layoutDimensions.width;
    float screenH = ctx->layoutDimensions.height;
    float dialogWidth = (screenW < 480.0f) ? (screenW - 40.0f) : 440.0f;
    if (dialogWidth < 120.0f) {
        dialogWidth = 120.0f;
    }

    // Scrim: full-screen, centers the dialog, captures the pointer so the UI
    // behind it is inert.
    Clay__OpenElementWithId(scrimId);
    Clay__ConfigureOpenElement(CLAY__INIT(Clay_ElementDeclaration){
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIXED(screenW), .height = CLAY_SIZING_FIXED(screenH) },
            .padding = CLAY_PADDING_ALL(ctx->theme.spacing.lg),
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
        },
        .backgroundColor = (Clay_Color){0, 0, 0, 150},
        .floating = {
            .zIndex = 500,
            .attachPoints = {
                .element = CLAY_ATTACH_POINT_LEFT_TOP,
                .parent = CLAY_ATTACH_POINT_LEFT_TOP,
            },
            .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE,
            .attachTo = CLAY_ATTACH_TO_ROOT,
        },
    });

    // Dialog panel.
    Clay__OpenElementWithId(dialogId);
    Clay__ConfigureOpenElement(CLAY__INIT(Clay_ElementDeclaration){
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIXED(dialogWidth), .height = CLAY_SIZING_FIT(0, 0) },
            .padding = CLAY_PADDING_ALL(ctx->theme.spacing.lg),
            .childGap = ctx->theme.spacing.md,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = ctx->theme.surfaceColor,
        .cornerRadius = CLAY_CORNER_RADIUS(ctx->theme.radiusMd),
        .border = {
            .color = ctx->theme.borderColor,
            .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
        },
    });

    // Title row: heading on the left, close button on the right.
    CLAY_AUTO_ID({
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
            .childGap = ctx->theme.spacing.sm,
            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
    }) {
        CLAY_AUTO_ID({
            .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) } },
        }) {
            if (title.length > 0 && title.chars) {
                CLAY_TEXT(title, {
                    .textColor = ctx->theme.textColor,
                    .fontId = ctx->theme.fontHeading,
                    .fontSize = ctx->theme.fontSizeHeading,
                });
            }
        }

        if (ClayWidgets_Button(ctx, closeId, CLAY_STRING("X"))) {
            *open = false;
        }
    }

    ClayWidgets_Separator(ctx);

    return true;
}

void ClayWidgets_EndModal(ClayWidgets_Context *ctx, Clay_ElementId id) {
    (void)id;
    if (!ctx) {
        return;
    }
    Clay__CloseElement(); // dialog panel
    Clay__CloseElement(); // scrim
}

#endif

#endif
