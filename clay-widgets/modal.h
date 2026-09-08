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
//
// While open, the modal traps keyboard focus: widgets outside it leave the Tab
// order and give up focus, so Tab cycles the dialog's own controls and Enter
// can't activate anything behind the scrim (which already captures the
// pointer).
bool ClayWidgets_BeginModal(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String title, bool *open);
typedef struct ClayWidgets_ModalOptions {
    bool draggable; // Drag the title bar; each opening starts centered.
    float width;    // Zero uses 440px; clamped to the viewport.
} ClayWidgets_ModalOptions;
bool ClayWidgets_BeginModalEx(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String title, bool *open, ClayWidgets_ModalOptions options);
void ClayWidgets_EndModal(ClayWidgets_Context *ctx, Clay_ElementId id);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

static Clay_ElementId ClayWidgets__ModalScrimId(Clay_ElementId id) {
    return Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsModalScrim"), id.id);
}
static Clay_ElementId ClayWidgets__ModalDialogId(Clay_ElementId id) {
    return Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsModalDialog"), id.id);
}

bool ClayWidgets_BeginModal(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String title, bool *open) {
    return ClayWidgets_BeginModalEx(ctx,id,title,open,(ClayWidgets_ModalOptions){0});
}

typedef struct ClayWidgets__ModalDragState {
    Clay_Vector2 offset, startOffset, startPointer;
} ClayWidgets__ModalDragState;

bool ClayWidgets_BeginModalEx(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String title, bool *open, ClayWidgets_ModalOptions options) {
    if (!ctx || !open || !*open) {
        return false;
    }

    Clay_ElementId scrimId = ClayWidgets__ModalScrimId(id);
    Clay_ElementId dialogId = ClayWidgets__ModalDialogId(id);
    Clay_ElementId closeId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsModalClose"), id.id);
    Clay_ElementId titleId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsModalTitle"), id.id);

    // Dismissal from the previous frame's geometry: Escape, or a press on the
    // scrim that is not inside the dialog. Detected before any element opens so
    // the modal vanishes on the same frame it is dismissed.
    bool topmost = !ctx->focusTrapPrevId || ctx->focusTrapPrevId == dialogId.id;
    bool dismiss = topmost && ctx->input.keyEscape && !ctx->openComboId && !ctx->openMenuId && !ctx->openContextMenuId;
    if (topmost && ctx->input.pointerPressed && Clay_PointerOver(scrimId) && !Clay_PointerOver(dialogId)) {
        dismiss = true;
    }
    if (dismiss) {
        *open = false;
        ctx->input.keyEscape = false;
        ctx->clickConsumed = true;
        return false;
    }

    float screenW = ctx->layoutDimensions.width;
    float screenH = ctx->layoutDimensions.height;
    float dialogWidth = fminf(options.width > 0 ? options.width : 440.0f, fmaxf(1.0f,screenW - 40.0f));

    // Trap keyboard focus inside the dialog while it is open: widgets outside
    // (registered while insideFocusTrap is false) drop out of the Tab order
    // starting next frame. See the focus trap fields on the context.
    if (ctx->modalCount >= 16 || !ClayWidgets__PushOverlay(ctx, 1000)) return false;
    int32_t modalIndex = ctx->modalCount++;
    bool newlyOpened = modalIndex >= ctx->modalCountPrev || ctx->modalIds[modalIndex] != dialogId.id;
    if (newlyOpened) {
        ctx->modalReturnFocus[modalIndex] = ctx->focusedId;
        ctx->focusCount = 0;
        ctx->focusedId = 0;
        ctx->focusFirst = true;
        ctx->focusTrapPrevId = dialogId.id;
        ctx->input.keyEnter = ctx->input.keySpace = false; // opening event must not activate the initial control
    }
    ctx->modalIds[modalIndex] = dialogId.id;
    ctx->modalParents[ctx->overlayDepth - 1] = ctx->currentModalId;
    ctx->currentModalId = dialogId.id;
    ctx->focusTrapId = dialogId.id;
    ctx->insideFocusTrap = true;

    Clay_Vector2 offset = {0};
    ClayWidgets__ModalDragState *drag = options.draggable
        ? (ClayWidgets__ModalDragState *)ClayWidgets_GetState(ctx,titleId.id,sizeof(ClayWidgets__ModalDragState)) : NULL;
    if (drag) {
        if (newlyOpened) memset(drag,0,sizeof(*drag));
        bool overTitle = topmost && Clay_PointerOver(titleId) && !Clay_PointerOver(closeId);
        if (overTitle || ctx->activeId == titleId.id) ClayWidgets__SetCursor(ctx,CLAY_WIDGETS_CURSOR_POINTER);
        if (overTitle && ctx->input.pointerPressed) {
            ctx->activeId = titleId.id;
            drag->startPointer = (Clay_Vector2){ctx->input.mouseX,ctx->input.mouseY};
            drag->startOffset = drag->offset;
        }
        if (ctx->activeId == titleId.id && ctx->input.pointerDown) {
            drag->offset.x = drag->startOffset.x + ctx->input.mouseX - drag->startPointer.x;
            drag->offset.y = drag->startOffset.y + ctx->input.mouseY - drag->startPointer.y;
        }
        Clay_ElementData previous = Clay_GetElementData(dialogId);
        float height = previous.found ? previous.boundingBox.height : 0;
        float limitX = fmaxf(0,(screenW-dialogWidth)*0.5f-8);
        float limitY = fmaxf(0,(screenH-height)*0.5f-8);
        drag->offset.x = ClayWidgets__Clamp(drag->offset.x,-limitX,limitX);
        drag->offset.y = ClayWidgets__Clamp(drag->offset.y,-limitY,limitY);
        offset = drag->offset;
    }

    // Scrim: full-screen, centers the dialog, captures the pointer so the UI
    // behind it is inert.
    ClayWidgets__BeginElement(scrimId, CLAY__INIT(Clay_ElementDeclaration){
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIXED(screenW), .height = CLAY_SIZING_FIXED(screenH) },
            .padding = CLAY_PADDING_ALL(ctx->theme.spacing.lg),
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
        },
        .backgroundColor = ctx->theme.scrimColor,
        .floating = {
            .zIndex = ClayWidgets__OverlayZ(ctx, 0),
            .attachPoints = {
                .element = CLAY_ATTACH_POINT_LEFT_TOP,
                .parent = CLAY_ATTACH_POINT_LEFT_TOP,
            },
            .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE,
            .attachTo = CLAY_ATTACH_TO_ROOT,
        },
        .transition = ClayWidgets__ScrimFadeIn(ctx),
    });

    // Dialog panel.
    ClayWidgets__BeginElement(dialogId, CLAY__INIT(Clay_ElementDeclaration){
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIXED(dialogWidth), .height = CLAY_SIZING_FIT(0, 0) },
            .padding = CLAY_PADDING_ALL(ctx->theme.spacing.lg),
            .childGap = ctx->theme.spacing.md,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = ctx->theme.surfaceColor,
        .cornerRadius = CLAY_CORNER_RADIUS((float)ctx->theme.radiusMd),
        .floating = {
            .offset = offset, .parentId = scrimId.id, .zIndex = ClayWidgets__OverlayZ(ctx,1),
            .attachPoints = { .element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER },
            .attachTo = options.draggable ? CLAY_ATTACH_TO_ELEMENT_WITH_ID : CLAY_ATTACH_TO_NONE,
        },
        .border = {
            .color = ctx->theme.borderColor,
            .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
        },
    });

    // Title row: heading on the left, close button on the right.
    CLAY(titleId, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
            .childGap = ctx->theme.spacing.sm,
            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
        // A render command with this ID keeps pointer capture alive during drag.
        .backgroundColor = ctx->theme.surfaceColor,
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

        // Fix the close button to a square (side = the label's line height
        // plus the button's own padding); left to fit, the lone narrow X
        // glyph would produce a tall rectangle.
        float closeSize = (float)ctx->theme.fontSizeBody + 2.0f * (float)ctx->theme.spacing.md;
        ClayWidgets_ButtonOptions closeOptions = {
            CLAY_WIDGETS_BUTTON_DEFAULT,
            false,
            { CLAY_SIZING_FIXED(closeSize), CLAY_SIZING_FIXED(closeSize) },
        };
        if (ClayWidgets_ButtonEx(ctx, closeId, CLAY_STRING("X"), closeOptions)) {
            *open = false;
        }
    }

    ClayWidgets_Separator(ctx);

    ClayWidgets__Describe(ctx,dialogId,CLAY_WIDGETS_ROLE_DIALOG,title,false,false);
    return true;
}

void ClayWidgets_EndModal(ClayWidgets_Context *ctx, Clay_ElementId id) {
    (void)id;
    if (!ctx) {
        return;
    }
    ctx->currentModalId = ctx->modalParents[ctx->overlayDepth - 1];
    ctx->insideFocusTrap = ctx->currentModalId != 0;
    ClayWidgets__PopOverlay(ctx);
    ClayWidgets__EndElement(); // dialog panel
    ClayWidgets__EndElement(); // scrim
}

#endif

#endif
