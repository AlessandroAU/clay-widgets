#ifndef CLAY_WIDGETS_LISTBOX_H
#define CLAY_WIDGETS_LISTBOX_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before listbox.h"
#endif

// A single-select list box: a bordered surface listing `items`, one selectable
// row each. Clicking a row selects it; when the list is focused, Up/Down move
// the selection. `*selectedIndex` is the caller-owned selection (use -1 for
// "nothing selected"). Returns true on the frame the selection changes.
//
//   static const Clay_String fruits[] = { CLAY_STRING("Apple"), CLAY_STRING("Pear") };
//   ClayWidgets_ListBox(&ui, CLAY_ID("Fruit"), fruits, 2, &selectedFruit);
//
// All rows are rendered (the list grows to fit its content); wrap it in a
// scroll panel if you need a fixed height with many items.
bool ClayWidgets_ListBox(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    const Clay_String *items,
    int32_t itemCount,
    int32_t *selectedIndex
);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

bool ClayWidgets_ListBox(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    const Clay_String *items,
    int32_t itemCount,
    int32_t *selectedIndex
) {
    if (!ctx || !items || itemCount <= 0 || !selectedIndex) {
        return false;
    }

    bool over = Clay_PointerOver(id);
    if (over) {
        ClayWidgets__SetCursor(ctx, CLAY_WIDGETS_CURSOR_POINTER);
    }
    bool focused = ClayWidgets__RegisterFocusable(ctx, id, over);
    bool changed = false;
    int32_t initial = *selectedIndex;
    *selectedIndex = ClayWidgets__MaxI32(-1,ClayWidgets__MinI32(*selectedIndex,itemCount-1));

    // Keyboard selection when focused.
    if (focused) {
        if (ctx->input.keyHome) *selectedIndex = 0;
        if (ctx->input.keyEnd) *selectedIndex = itemCount - 1;
        if (ClayWidgets__TypeAhead(ctx, id.id)) {
            for (int32_t i = 0; i < itemCount; ++i) {
                int32_t n = ((*selectedIndex < 0 ? 0 : *selectedIndex) + i) % itemCount;
                if (ClayWidgets__PrefixMatches(items[n], ctx->typeAhead, ctx->typeAheadLength)) { *selectedIndex = n; break; }
            }
        }
        if (ctx->input.keyDown) {
            int32_t next = (*selectedIndex < 0) ? 0 : ClayWidgets__MinI32(*selectedIndex + 1, itemCount - 1);
            if (next != *selectedIndex) { *selectedIndex = next; changed = true; }
        }
        if (ctx->input.keyUp) {
            int32_t next = (*selectedIndex < 0) ? 0 : ClayWidgets__MaxI32(*selectedIndex - 1, 0);
            if (next != *selectedIndex) { *selectedIndex = next; changed = true; }
        }
    }
    changed = changed || initial != *selectedIndex;
    if (changed && ctx->scrollPanelDepth > 0 && ctx->scrollPanelDepth <= CLAY_WIDGETS_MAX_SCROLL_NESTING) {
        Clay_ElementId panel = {0}; panel.id = ctx->scrollPanelStack[ctx->scrollPanelDepth - 1];
        ClayWidgets__ScrollIntoView(ctx, ClayWidgets__ChildId(id, CLAY_STRING("ClayWidgetsListBoxItem"), *selectedIndex), panel);
    }

    // Resolve row clicks before laying out so the new selection paints this frame.
    for (int32_t i = 0; i < itemCount; i++) {
        Clay_ElementId rowId = ClayWidgets__ChildId(id, CLAY_STRING("ClayWidgetsListBoxItem"), i);
        if (ClayWidgets__ConsumeClick(ctx, Clay_PointerOver(rowId))) {
            if (*selectedIndex != i) { *selectedIndex = i; changed = true; }
        }
    }

    // The list is a white well like an entry field; its rows are the highlight
    // bar, so they lose the gap and rounding that separate them on flat themes.
    bool beveled = ClayWidgets__IsBeveled(ctx);
    ClayWidgets_SetEdge(ctx, id, CLAY_WIDGETS_EDGE_SUNKEN);

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
            .padding = CLAY_PADDING_ALL((uint16_t)(beveled ? 2 : ctx->theme.spacing.xs)),
            .childGap = (uint16_t)(beveled ? 0 : 2),
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = ctx->theme.fieldColor,
        .cornerRadius = CLAY_CORNER_RADIUS((float)ctx->theme.radiusMd),
        .border = ClayWidgets__Border(ctx, focused ? ctx->theme.focusRingColor : ctx->theme.borderColor),
    }) {
        for (int32_t i = 0; i < itemCount; i++) {
            Clay_ElementId rowId = ClayWidgets__ChildId(id, CLAY_STRING("ClayWidgetsListBoxItem"), i);
            bool rowOver = Clay_PointerOver(rowId);
            bool selected = (i == *selectedIndex);

            Clay_Color rowBg = ClayWidgets__FadeToClear(ctx->theme.selectionColor);
            Clay_Color rowText = ctx->theme.textColor;
            if (selected) {
                rowBg = ctx->theme.accentMutedColor;
            } else if (rowOver) {
                rowBg = ctx->theme.selectionColor;
                rowText = ctx->theme.onSelectionColor;
            }

            CLAY(rowId, {
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                    .padding = {
                        .left = ctx->theme.spacing.sm,
                        .right = ctx->theme.spacing.sm,
                        .top = ctx->theme.spacing.sm,
                        .bottom = ctx->theme.spacing.sm,
                    },
                    .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
                },
                .backgroundColor = rowBg,
                .cornerRadius = CLAY_CORNER_RADIUS((float)ctx->theme.radiusSm),
                .transition = ClayWidgets__ColorTransition(ctx),
            }) {
                CLAY_TEXT(items[i], {
                    .textColor = rowText,
                    .fontId = ctx->theme.fontBody,
                    .fontSize = ctx->theme.fontSizeBody,
                    .wrapMode = CLAY_TEXT_WRAP_NONE,
                });
            }
        }
    }

    return changed;
}

#endif

#endif
