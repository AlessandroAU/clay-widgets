#ifndef CLAY_WIDGETS_COLLAPSIBLE_H
#define CLAY_WIDGETS_COLLAPSIBLE_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before collapsible.h"
#endif

// A collapsible disclosure section. The header shows a twisty indicator and a
// title; clicking it (or focusing it and pressing Enter) toggles the caller's
// `open` bool. While open, the body content declared between Begin and End is
// shown, indented under the header. Lay several out in a column to form an
// accordion. Usage:
//
//   if (ClayWidgets_BeginCollapsible(&ui, CLAY_ID("Advanced"), CLAY_STRING("Advanced"), &showAdvanced)) {
//       ClayWidgets_Checkbox(&ui, CLAY_ID("Verbose"), CLAY_STRING("Verbose logging"), &verbose);
//       ClayWidgets_EndCollapsible(&ui, CLAY_ID("Advanced"));
//   }
//
// Returns true while open (declare the body and call End only then).
bool ClayWidgets_BeginCollapsible(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String title, bool *open);
void ClayWidgets_EndCollapsible(ClayWidgets_Context *ctx, Clay_ElementId id);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

bool ClayWidgets_BeginCollapsible(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String title, bool *open) {
    if (!ctx || !open) {
        return false;
    }

    bool over = Clay_PointerOver(id);
    bool focused = ClayWidgets__RegisterFocusable(ctx, id, over);
    bool clicked = ClayWidgets__ConsumeClick(ctx, over);
    if (!clicked && ClayWidgets__ActivateFocused(ctx, id)) {
        clicked = true;
    }
    if (clicked) {
        *open = !(*open);
    }

    // Header row: twisty indicator + title, with a hover fill and a focus ring
    // (a constant 1px border, transparent until focused, so nothing shifts).
    Clay_String indicator = *open ? CLAY_STRING("v") : CLAY_STRING(">");
    Clay_Color transparent = { 0, 0, 0, 0 };

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
            .padding = CLAY_PADDING_ALL(ctx->theme.spacing.sm),
            .childGap = ctx->theme.spacing.sm,
            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
        .backgroundColor = over ? ctx->theme.hoverColor : ctx->theme.surfaceAltColor,
        .cornerRadius = CLAY_CORNER_RADIUS(ctx->theme.radiusSm),
        .border = {
            .color = focused ? ctx->theme.focusRingColor : transparent,
            .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
        },
    }) {
        CLAY_AUTO_ID({
            .layout = {
                .sizing = { .width = CLAY_SIZING_FIXED(14), .height = CLAY_SIZING_FIT(0, 0) },
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
            },
        }) {
            CLAY_TEXT(indicator, {
                .textColor = ctx->theme.textMutedColor,
                .fontId = ctx->theme.fontBody,
                .fontSize = ctx->theme.fontSizeBody,
            });
        }
        CLAY_TEXT(title, {
            .textColor = ctx->theme.textColor,
            .fontId = ctx->theme.fontBody,
            .fontSize = ctx->theme.fontSizeBody,
        });
    }

    if (!*open) {
        return false;
    }

    // Body: indented under the header.
    Clay_ElementId bodyId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsCollapsibleBody"), id.id);
    ClayWidgets__BeginElement(bodyId, CLAY__INIT(Clay_ElementDeclaration){
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
            .padding = { .left = ctx->theme.spacing.lg, .right = 0, .top = ctx->theme.spacing.xs, .bottom = ctx->theme.spacing.xs },
            .childGap = ctx->theme.spacing.sm,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
    });

    return true;
}

void ClayWidgets_EndCollapsible(ClayWidgets_Context *ctx, Clay_ElementId id) {
    (void)id;
    if (!ctx) {
        return;
    }
    ClayWidgets__EndElement(); // body
}

#endif

#endif
