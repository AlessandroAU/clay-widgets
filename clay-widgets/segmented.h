#ifndef CLAY_WIDGETS_SEGMENTED_H
#define CLAY_WIDGETS_SEGMENTED_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before segmented.h"
#endif

// A segmented control: a row of joined cells sharing one border, of which
// exactly one is selected - a radio group that reads as a single toolbar
// control. Click a cell to select it; when focused, Left/Right move the
// selection. `*selectedIndex` is the caller-owned selection (0..count-1).
// Returns true on the frame the selection changes.
//
//   static const Clay_String views[] = { CLAY_STRING("List"), CLAY_STRING("Grid") };
//   ClayWidgets_Segmented(&ui, CLAY_ID("ViewMode"), views, 2, &viewMode);
bool ClayWidgets_Segmented(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    const Clay_String *segments,
    int32_t segmentCount,
    int32_t *selectedIndex
);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

bool ClayWidgets_Segmented(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    const Clay_String *segments,
    int32_t segmentCount,
    int32_t *selectedIndex
) {
    if (!ctx || !segments || segmentCount <= 0 || !selectedIndex) {
        return false;
    }

    bool over = Clay_PointerOver(id);
    if (over) {
        ClayWidgets__SetCursor(ctx, CLAY_WIDGETS_CURSOR_POINTER);
    }
    bool focused = ClayWidgets__RegisterFocusable(ctx, id, over);
    bool changed = false;

    if (focused) {
        if (ctx->input.keyRight) {
            int32_t next = (*selectedIndex < 0) ? 0 : ClayWidgets__MinI32(*selectedIndex + 1, segmentCount - 1);
            if (next != *selectedIndex) { *selectedIndex = next; changed = true; }
        }
        if (ctx->input.keyLeft) {
            int32_t next = (*selectedIndex < 0) ? 0 : ClayWidgets__MaxI32(*selectedIndex - 1, 0);
            if (next != *selectedIndex) { *selectedIndex = next; changed = true; }
        }
    }

    // Resolve clicks before layout so the new selection paints this frame.
    for (int32_t i = 0; i < segmentCount; i++) {
        Clay_ElementId cellId = ClayWidgets__ChildId(id, CLAY_STRING("ClayWidgetsSegment"), i);
        if (ClayWidgets__ConsumeClick(ctx, Clay_PointerOver(cellId))) {
            if (*selectedIndex != i) { *selectedIndex = i; changed = true; }
        }
    }

    float r = (float)ctx->theme.radiusMd;

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
            .childGap = 0,
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
        .cornerRadius = CLAY_CORNER_RADIUS(r),
        .border = {
            .color = focused ? ctx->theme.focusRingColor : ctx->theme.borderColor,
            .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
        },
    }) {
        for (int32_t i = 0; i < segmentCount; i++) {
            Clay_ElementId cellId = ClayWidgets__ChildId(id, CLAY_STRING("ClayWidgetsSegment"), i);
            bool cellOver = Clay_PointerOver(cellId);
            bool selected = (i == *selectedIndex);

            Clay_Color cellBg = ctx->theme.surfaceAltColor;
            if (selected) {
                cellBg = ctx->theme.accentColor;
            } else if (cellOver) {
                cellBg = ctx->theme.hoverColor;
            }

            // Round only the outer corners of the two end cells so the group's
            // fill matches the rounded outer border; dividers sit between cells.
            Clay_CornerRadius cr = { 0.0f, 0.0f, 0.0f, 0.0f };
            if (i == 0) { cr.topLeft = r; cr.bottomLeft = r; }
            if (i == segmentCount - 1) { cr.topRight = r; cr.bottomRight = r; }

            CLAY(cellId, {
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                    .padding = {
                        .left = ctx->theme.spacing.md,
                        .right = ctx->theme.spacing.md,
                        .top = ctx->theme.spacing.sm,
                        .bottom = ctx->theme.spacing.sm,
                    },
                    .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                },
                .backgroundColor = cellBg,
                .cornerRadius = cr,
                .border = {
                    .color = ctx->theme.borderColor,
                    .width = { .left = (uint16_t)(i > 0 ? 1 : 0), .right = 0, .top = 0, .bottom = 0 },
                },
                .transition = ClayWidgets__ColorTransition(ctx),
            }) {
                CLAY_TEXT(segments[i], {
                    .textColor = selected ? ctx->theme.surfaceColor : ctx->theme.textColor,
                    .fontId = ctx->theme.fontBody,
                    .fontSize = ctx->theme.fontSizeBody,
                    .wrapMode = CLAY_TEXT_WRAP_NONE,
                    .textAlignment = CLAY_TEXT_ALIGN_CENTER,
                });
            }
        }
    }

    return changed;
}

#endif

#endif
