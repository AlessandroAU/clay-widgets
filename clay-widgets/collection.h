#ifndef CLAY_WIDGETS_COLLECTION_H
#define CLAY_WIDGETS_COLLECTION_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before collection.h"
#endif

// Callbacks return borrowed strings that remain valid through rendering.
typedef Clay_String (*ClayWidgets_ItemTextFunction)(int32_t index, void *userData);
typedef struct ClayWidgets_VirtualListOptions { float height, rowHeight; bool disabled; } ClayWidgets_VirtualListOptions;
bool ClayWidgets_VirtualList(ClayWidgets_Context *ctx, Clay_ElementId id, int32_t count,
    ClayWidgets_ItemTextFunction text, void *userData, int32_t *selected, ClayWidgets_VirtualListOptions options);

// Filter text is caller-owned. The selected index always addresses the original items.
bool ClayWidgets_SearchableCombo(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String label,
    const Clay_String *items, int32_t count, int32_t *selected, char *query, int32_t queryCapacity);

typedef Clay_String (*ClayWidgets_TableCellFunction)(int32_t row, int32_t column, void *userData);
typedef int (*ClayWidgets_TableCompareFunction)(int32_t rowA, int32_t rowB, int32_t column, void *userData);
typedef struct ClayWidgets_TableState {
    float widths[12]; // zero = initialize from column sizing; grow columns share spare space
    int32_t sortColumn;
    bool descending;
    bool sortChanged, selectionChanged, widthChanged; // reset each call
    int32_t anchor, focusedRow;
    float dragX, dragWidth;
    bool sorted;
    uint16_t initializedColumns;
    uint16_t fixedWidthColumns; // explicit widths and dragged columns stop automatically growing
} ClayWidgets_TableState;
typedef struct ClayWidgets_TableOptions { float height, rowHeight, minColumnWidth; bool disabled; } ClayWidgets_TableOptions;
// order is a caller-owned permutation of [0,rowCount); selection is indexed by
// original record, so sorting preserves selection. Both arrays have rowCount entries.
// Initialize order[i]=i and state={}; callbacks never mutate order during layout.
bool ClayWidgets_DataTable(ClayWidgets_Context *ctx, Clay_ElementId id,
    const ClayWidgets_TableColumn *columns, int32_t columnCount, int32_t rowCount,
    ClayWidgets_TableCellFunction cell, ClayWidgets_TableCompareFunction compare, void *userData,
    int32_t *order, bool *selection, ClayWidgets_TableState *state, ClayWidgets_TableOptions options);

#ifdef CLAY_WIDGETS_IMPLEMENTATION
static void ClayWidgets__Spacer(float height) {
    if (height <= 0) return;
    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(1), .height = CLAY_SIZING_FIXED(height) } } }) {}
}
// `first`..`end` is the slice worth emitting - a few rows wider than the
// viewport - while `bottom` is the one row sitting on the viewport's bottom
// edge, or -1 when the content stops short of it. Only that row can paint
// into a rounded frame's bottom corners, so only it has to be rounded.
typedef struct ClayWidgets__VisibleRows { int32_t first, end, bottom; float rowHeight; } ClayWidgets__VisibleRows;
static ClayWidgets__VisibleRows ClayWidgets__BeginVirtualRows(ClayWidgets_Context *ctx, Clay_ElementId id,
    int32_t count, float height, float rowHeight, int32_t reveal) {
    height = height > 0 ? height : 240;
    rowHeight = rowHeight > 0 ? rowHeight : ClayWidgets__FieldHeight(ctx);
    if (count * rowHeight <= height) ClayWidgets__RegisterWheelFallthrough(ctx,id,Clay_PointerOver(id));
    Clay_ScrollContainerData scroll = Clay_GetScrollContainerData(id);
    float y = scroll.found && scroll.scrollPosition ? scroll.scrollPosition->y : 0;
    if (reveal >= 0 && reveal < count) {
        float top = reveal * rowHeight;
        if (top < -y) y = -top;
        else if (top + rowHeight > -y + height) y = height - top - rowHeight;
    }
    y = ClayWidgets__Clamp(y, -fmaxf(0, count * rowHeight - height), 0);
    if (scroll.found && scroll.scrollPosition) scroll.scrollPosition->y = y;
    int32_t first = ClayWidgets__MaxI32(0, (int32_t)(-y / rowHeight) - 1);
    first = ClayWidgets__MinI32(first, count);
    int32_t end = ClayWidgets__MinI32(count, first + (int32_t)(height / rowHeight) + 3);
    ClayWidgets__BeginScrollElement(id, (Clay_ElementDeclaration){
        .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(height) },
            .padding = { .right = CLAY_WIDGETS_SCROLLBAR_WIDTH + 4 }, .layoutDirection = CLAY_TOP_TO_BOTTOM },
        .clip = { .horizontal = true, .vertical = true },
    });
    ClayWidgets__Spacer(first * rowHeight);
    // Row i spans [i * rowHeight + y, (i + 1) * rowHeight + y) in viewport
    // coordinates, so the one crossing `height` is the row on the bottom edge.
    int32_t bottom = (int32_t)((height - y) / rowHeight);
    if (bottom >= count || bottom < first || bottom >= end) {
        bottom = -1;
    }
    return (ClayWidgets__VisibleRows){first, end, bottom, rowHeight};
}
static void ClayWidgets__EndVirtualRows(ClayWidgets_Context *ctx, Clay_ElementId id, int32_t count, ClayWidgets__VisibleRows rows, bool tableColumn) {
    ClayWidgets__Spacer((count - rows.end) * rows.rowHeight);
    // Tables use the entire gutter as a square track; lists inset their track.
    ClayWidgets__ScrollBarAt(ctx, id, tableColumn ? 0.0f : -2.0f, tableColumn);
    ClayWidgets__EndElement();
}

bool ClayWidgets_VirtualList(ClayWidgets_Context *ctx, Clay_ElementId id, int32_t count,
    ClayWidgets_ItemTextFunction text, void *data, int32_t *selected, ClayWidgets_VirtualListOptions options) {
    if (!ctx || !text || !selected || count < 0) return false;
    bool disabled = options.disabled || ctx->disabledDepth > 0;
    bool focused = !disabled && ClayWidgets__RegisterFocusable(ctx, id, Clay_PointerOver(id));
    int32_t before = *selected;
    *selected = ClayWidgets__MaxI32(-1,ClayWidgets__MinI32(*selected,count-1));
    if (focused && count) {
        if (ctx->input.keyHome) *selected = 0;
        if (ctx->input.keyEnd) *selected = count - 1;
        if (ctx->input.keyDown) *selected = *selected < 0 ? 0 : ClayWidgets__MinI32(count - 1, *selected + 1);
        if (ctx->input.keyUp) *selected = ClayWidgets__MaxI32(0, *selected - 1);
        if (ClayWidgets__TypeAhead(ctx, id.id)) for (int32_t i = 0; i < count; ++i) {
            if (ClayWidgets__PrefixMatches(text(i, data), ctx->typeAhead, ctx->typeAheadLength)) { *selected = i; break; }
        }
    }
    ClayWidgets__VisibleRows rows = ClayWidgets__BeginVirtualRows(ctx, id, count, options.height, options.rowHeight,
        *selected != before ? *selected : -1);
    for (int32_t i = rows.first; i < rows.end; ++i) {
        Clay_ElementId row = ClayWidgets__ChildId(id, CLAY_STRING("VirtualRow"), i);
        bool over = !disabled && Clay_PointerOver(row);
        if (ClayWidgets__ConsumeClick(ctx, over)) *selected = i;
        if (over) ClayWidgets__SetCursor(ctx, CLAY_WIDGETS_CURSOR_POINTER);
        CLAY(row, { .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(rows.rowHeight) },
                .padding = { .left = ctx->theme.spacing.sm }, .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } },
            .backgroundColor = *selected == i ? ctx->theme.accentMutedColor : over ? ctx->theme.selectionColor : ctx->theme.fieldColor,
        }) { CLAY_TEXT(text(i, data), { .textColor = disabled ? ctx->theme.textMutedColor : ctx->theme.textColor,
            .fontId = ctx->theme.fontBody, .fontSize = ctx->theme.fontSizeBody, .wrapMode = CLAY_TEXT_WRAP_NONE }); }
    }
    ClayWidgets__EndVirtualRows(ctx, id, count, rows, false);
    ClayWidgets_SemanticNode node = {0}; node.id = id; node.role = CLAY_WIDGETS_ROLE_LIST; node.disabled = disabled;
    ClayWidgets_Semantic(ctx, node);
    return before != *selected;
}

bool ClayWidgets_SearchableCombo(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String label,
    const Clay_String *items, int32_t count, int32_t *selected, char *query, int32_t capacity) {
    if (!ctx || !items || count < 0 || !selected || !query || capacity <= 0) return false;
    Clay_ElementId search = ClayWidgets__ChildId(id, CLAY_STRING("ComboSearch"), 0);
    ClayWidgets_TextInputOptions input = {0}; input.placeholder = "Search...";
    ClayWidgets_TextInput(ctx, search, label, query, capacity, input);
    int32_t *map = count ? (int32_t *)malloc((size_t)count * sizeof(int32_t)) : NULL;
    Clay_String *filtered = count ? (Clay_String *)malloc((size_t)count * sizeof(Clay_String)) : NULL;
    if (count && (!map || !filtered)) { free(map); free(filtered); return false; }
    int32_t matches = 0, picked = -1;
    int32_t queryLength = ClayWidgets__StrLenBounded(query, capacity - 1);
    for (int32_t i = 0; i < count; ++i) {
        bool match = queryLength == 0;
        for (int32_t j = 0; !match && j <= items[i].length - queryLength; ++j) {
            Clay_String suffix = { .length = items[i].length - j, .chars = items[i].chars + j };
            match = ClayWidgets__PrefixMatches(suffix, query, queryLength);
        }
        if (match) { map[matches] = i; filtered[matches] = items[i]; if (i == *selected) picked = matches; ++matches; }
    }
    bool changed = false;
    if (matches) {
        if (ctx->comboHighlightIndex >= matches) ctx->comboHighlightIndex = matches - 1;
        changed = ClayWidgets_Combo(ctx, id, CLAY_STRING(""), filtered, matches, &picked);
        if (changed && picked >= 0) *selected = map[picked];
    } else {
        if (ctx->openComboId == id.id) ctx->openComboId = 0;
        ClayWidgets_Label(ctx, CLAY_STRING("No matches"));
    }
    // Clay retains the strings' chars, not this temporary array of descriptors.
    free(map); free(filtered);
    return changed;
}

static int ClayWidgets__CompareRows(int32_t a, int32_t b, ClayWidgets_TableState *state,
    ClayWidgets_TableCompareFunction compare, void *data) {
    int result = compare(a, b, state->sortColumn, data);
    if (result) return state->descending ? (result < 0 ? 1 : -1) : (result < 0 ? -1 : 1);
    return a < b ? -1 : a > b ? 1 : 0;
}
static void ClayWidgets__SiftRows(int32_t *order, int32_t count, int32_t root, ClayWidgets_TableState *state,
    ClayWidgets_TableCompareFunction compare, void *data) {
    while (root < count / 2) {
        int32_t child = root * 2 + 1;
        if (child + 1 < count && ClayWidgets__CompareRows(order[child], order[child+1], state, compare, data) < 0) ++child;
        if (ClayWidgets__CompareRows(order[root], order[child], state, compare, data) >= 0) break;
        int32_t temp = order[root]; order[root] = order[child]; order[child] = temp; root = child;
    }
}
static void ClayWidgets__SortRows(int32_t *order, int32_t count, ClayWidgets_TableState *state,
    ClayWidgets_TableCompareFunction compare, void *data) {
    for (int32_t i = count / 2; i > 0; --i) ClayWidgets__SiftRows(order, count, i-1, state, compare, data);
    for (int32_t i = count - 1; i > 0; --i) {
        int32_t temp = order[0]; order[0] = order[i]; order[i] = temp;
        ClayWidgets__SiftRows(order, i, 0, state, compare, data);
    }
}
static void ClayWidgets__TableSelect(ClayWidgets_Context *ctx, int32_t row, int32_t count, int32_t *order,
    bool *selection, ClayWidgets_TableState *state) {
    if (!selection || row < 0 || row >= count) return;
    if (!ctx->input.controlDown) for (int32_t i = 0; i < count; ++i) selection[i] = false;
    if (ctx->input.shiftDown) {
        int32_t anchor = ClayWidgets__MaxI32(0, ClayWidgets__MinI32(state->anchor, count - 1));
        for (int32_t i = ClayWidgets__MinI32(anchor,row); i <= ClayWidgets__MaxI32(anchor,row); ++i) selection[order[i]] = true;
    } else {
        selection[order[row]] = ctx->input.controlDown ? !selection[order[row]] : true;
        state->anchor = row;
    }
    state->focusedRow = row; state->selectionChanged = true;
}

bool ClayWidgets_DataTable(ClayWidgets_Context *ctx, Clay_ElementId id,
    const ClayWidgets_TableColumn *columns, int32_t columnCount, int32_t rowCount,
    ClayWidgets_TableCellFunction cell, ClayWidgets_TableCompareFunction compare, void *data,
    int32_t *order, bool *selection, ClayWidgets_TableState *state, ClayWidgets_TableOptions options) {
    if (!ctx || !columns || columnCount <= 0 || columnCount > 12 || rowCount < 0 || !cell || !state || (rowCount && !order)) return false;
    state->sortChanged = state->selectionChanged = state->widthChanged = false;
    bool disabled = options.disabled || ctx->disabledDepth > 0;
    bool focused = !disabled && ClayWidgets__RegisterFocusable(ctx, id, Clay_PointerOver(id));
    int32_t reveal = -1;
    if (focused && rowCount) {
        int32_t row = state->focusedRow;
        if (ctx->input.keyHome) row = 0;
        if (ctx->input.keyEnd) row = rowCount - 1;
        if (ctx->input.keyDown) row = ClayWidgets__MinI32(rowCount-1,row+1);
        if (ctx->input.keyUp) row = ClayWidgets__MaxI32(0,row-1);
        if (row != state->focusedRow) { ClayWidgets__TableSelect(ctx,row,rowCount,order,selection,state); reveal = row; }
        if (ctx->input.keySelectAll && selection) { for (int32_t i=0;i<rowCount;++i) selection[i]=true; state->selectionChanged=true; }
    }
    float minWidth = options.minColumnWidth > 0 ? options.minColumnWidth : 40;
    Clay_ElementId body = ClayWidgets__ChildId(id, CLAY_STRING("TableBody"),0);
    Clay_ElementId headerRow = ClayWidgets__ChildId(id, CLAY_STRING("TableHeaderRow"),0);
    Clay_ScrollContainerData bodyScroll = Clay_GetScrollContainerData(body);
    float horizontalOffset = bodyScroll.found && bodyScroll.scrollPosition ? bodyScroll.scrollPosition->x : 0;
    const uint16_t gutter = CLAY_WIDGETS_SCROLLBAR_WIDTH + 4;
    const float radius = (float)ctx->theme.radiusMd;
    // Clay's scissor is rectangular. Keep child fills inside the rounded frame
    // so scrolling rows cannot paint over its corners.
    // The frame inset has to clear the 3D edge on a beveled theme, or the
    // header row would paint over its inner band.
    const uint16_t frameInset = (uint16_t)fmaxf(ClayWidgets__IsBeveled(ctx) ? 2.0f : 1.0f, ceilf(radius * 0.3f));
    const float headerHeight = (float)ctx->theme.fontSizeBody + 2 * ctx->theme.spacing.sm;
    Clay_ElementData tableBox = Clay_GetElementData(id);
    float fixedWidth = 0;
    int32_t growCount = 0;
    for (int32_t i = 0; i < columnCount; ++i) {
        uint16_t bit = (uint16_t)(1u << i);
        if (!(state->initializedColumns & bit)) {
            if (state->widths[i] > 0 || columns[i].width.type != CLAY__SIZING_TYPE_GROW) state->fixedWidthColumns |= bit;
            if (state->widths[i] <= 0) state->widths[i] = columns[i].width.type == CLAY__SIZING_TYPE_FIXED
                ? columns[i].width.size.minMax.min : 120;
            state->initializedColumns |= bit;
        }
        if (state->fixedWidthColumns & bit) fixedWidth += state->widths[i];
        else ++growCount;
    }
    float availableWidth = tableBox.found ? tableBox.boundingBox.width - 2 * frameInset - gutter : fixedWidth + growCount * 120;
    float growWidth = growCount ? fmaxf(minWidth, (availableWidth - fixedWidth) / growCount) : 0;
    for (int32_t i = 0; i < columnCount; ++i) {
        if (!(state->fixedWidthColumns & (1u << i))) state->widths[i] = growWidth;
    }
    // What the header and the bottom row have to round to: the frame's own
    // radius, less the padding that insets them from it.
    const float frameRadius = fmaxf(0.0f, radius - (float)frameInset);
    ClayWidgets_SetEdge(ctx, id, CLAY_WIDGETS_EDGE_SUNKEN);
    ClayWidgets_SetEdge(ctx, headerRow, CLAY_WIDGETS_EDGE_RAISED_THIN);
    ClayWidgets__BeginElement(id, (Clay_ElementDeclaration){ .layout = {
        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
        .padding = CLAY_PADDING_ALL(frameInset), .layoutDirection = CLAY_TOP_TO_BOTTOM },
        .backgroundColor = ctx->theme.fieldColor,
        .cornerRadius = CLAY_CORNER_RADIUS(radius),
        .border = ClayWidgets__Border(ctx, ctx->theme.borderColor) });
    CLAY(headerRow, { .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(headerHeight) },
            .padding = { .right = gutter } },
        .backgroundColor = ctx->theme.surfaceAltColor,
        // The header sits on the frame's top corners, inset by its padding, and
        // Clay clips to rectangles - without the matching radius its fill paints
        // a square block outside the frame's arc.
        .cornerRadius = { .topLeft = frameRadius, .topRight = frameRadius, .bottomLeft = 0, .bottomRight = 0 },
        .clip = { .horizontal = true, .vertical = true, .childOffset = {horizontalOffset,0} },
        .border = ClayWidgets__EdgeBorder(ctx, ctx->theme.borderColor, CLAY__INIT(Clay_BorderWidth){ 0, 0, 0, 1, 0 }) }) {
        for (int32_t i=0;i<columnCount;++i) {
            Clay_ElementId header = ClayWidgets__ChildId(id, CLAY_STRING("TableHeader"), i);
            Clay_ElementId handle = ClayWidgets__ChildId(id, CLAY_STRING("TableResize"), i);
            bool overHandle = !disabled && Clay_PointerOver(handle);
            if (overHandle || ctx->activeId == handle.id) ClayWidgets__SetCursor(ctx, CLAY_WIDGETS_CURSOR_RESIZE_X);
            if (ctx->input.pointerPressed && overHandle) { ctx->activeId=handle.id; state->dragX=ctx->input.mouseX; state->dragWidth=state->widths[i]; }
            if (!disabled && ctx->activeId==handle.id && ctx->input.pointerDown) {
                state->widths[i]=fmaxf(minWidth,state->dragWidth+ctx->input.mouseX-state->dragX); state->widthChanged=true;
                state->fixedWidthColumns |= (uint16_t)(1u << i);
            }
            bool overHeader = !disabled && !overHandle && Clay_PointerOver(header);
            bool headerFocused = compare && !disabled && ClayWidgets__RegisterFocusable(ctx, header, overHeader);
            if (compare && overHeader) ClayWidgets__SetCursor(ctx, CLAY_WIDGETS_CURSOR_POINTER);
            if (compare && ctx->input.pointerPressed && overHeader) ctx->activeId = header.id;
            bool sort = compare && !disabled && (ClayWidgets__ConsumeClick(ctx,overHeader && ctx->releasedActiveId == header.id)
                || (headerFocused && ClayWidgets__ActivateFocused(ctx,header)));
            if (sort) {
                state->descending=state->sorted && state->sortColumn==i ? !state->descending : false;
                state->sorted=true;
                state->sortColumn=i; state->sortChanged=true;
                ClayWidgets__SortRows(order,rowCount,state,compare,data);
                state->anchor=state->focusedRow=0;
            }
            ClayWidgets__RegisterWheelFallthrough(ctx,header,Clay_PointerOver(header));
            CLAY(header, {
                .layout = {
                    .sizing = { .width = CLAY_SIZING_FIXED(state->widths[i]), .height = CLAY_SIZING_GROW(0) },
                    .padding = { .left = ctx->theme.spacing.sm, .right = ctx->theme.spacing.sm },
                    .childGap = ctx->theme.spacing.xs,
                    .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
                },
                .backgroundColor = overHeader && compare ? ctx->theme.hoverColor : ctx->theme.surfaceAltColor,
                .clip = { .horizontal = true, .vertical = true },
                .border = ClayWidgets__EdgeBorder(ctx, headerFocused ? ctx->theme.focusRingColor : ctx->theme.borderColor,
                    CLAY__INIT(Clay_BorderWidth){ 0, 1, 0, (uint16_t)(headerFocused ? 2 : 0), 0 }),
            }) {
                CLAY_TEXT(columns[i].title, { .textColor = disabled ? ctx->theme.textMutedColor : ctx->theme.textColor,
                    .fontId = ctx->theme.fontBody, .fontSize = ctx->theme.fontSizeBody, .wrapMode = CLAY_TEXT_WRAP_NONE });
                if (state->sorted && state->sortColumn == i) {
                    CLAY_TEXT(state->descending ? CLAY_STRING("v") : CLAY_STRING("^"), {
                        .textColor = ctx->theme.textMutedColor, .fontId = ctx->theme.fontBody,
                        .fontSize = ctx->theme.fontSizeSmall, .wrapMode = CLAY_TEXT_WRAP_NONE });
                }
            }
            // Keep an easy-to-grab hit target; paint only a thin separator inside it.
            CLAY(handle, { .layout = { .sizing = { .width = CLAY_SIZING_FIXED(6), .height = CLAY_SIZING_FIXED(headerHeight) },
                    .childAlignment = { .x = CLAY_ALIGN_X_CENTER } },
                .backgroundColor = ctx->activeId == handle.id ? ctx->theme.hoverColor : ClayWidgets__FadeToClear(ctx->theme.borderColor),
                .floating = { .offset = {-3,0}, .parentId = header.id, .zIndex = ClayWidgets__OverlayZ(ctx,2),
                    .attachPoints = { .element = CLAY_ATTACH_POINT_LEFT_CENTER, .parent = CLAY_ATTACH_POINT_RIGHT_CENTER },
                    .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID, .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT } }) {
                CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(1), .height = CLAY_SIZING_GROW(0) } },
                    .backgroundColor = overHandle || ctx->activeId == handle.id ? ctx->theme.focusRingColor : ctx->theme.borderColor }) {}
            }
        }
    }
    ClayWidgets__VisibleRows rows = ClayWidgets__BeginVirtualRows(ctx,body,rowCount,options.height,options.rowHeight,reveal);
    bool bodyScrolls = rowCount * rows.rowHeight > (options.height > 0 ? options.height : 240);
    if (bodyScrolls) {
        if (ctx->scrollPanelDepth < CLAY_WIDGETS_MAX_SCROLL_NESTING) ctx->scrollPanelStack[ctx->scrollPanelDepth] = body.id;
        ctx->scrollPanelDepth++;
    }
    for (int32_t v=rows.first;v<rows.end;++v) {
        int32_t record=order[v];
        if (record<0 || record>=rowCount) continue;
        Clay_ElementId row = ClayWidgets__ChildId(id, CLAY_STRING("DataRow"),record);
        if (!disabled && ClayWidgets__ConsumeClick(ctx,Clay_PointerOver(row))) ClayWidgets__TableSelect(ctx,v,rowCount,order,selection,state);
        // Only the row against the frame's bottom edge can paint into its
        // corners, and only while the content reaches that far.
        Clay_CornerRadius rowRadius = CLAY__INIT(Clay_CornerRadius) CLAY__DEFAULT_STRUCT;
        if (v == rows.bottom) {
            rowRadius.bottomLeft = rowRadius.bottomRight = frameRadius;
        }
        CLAY(row, { .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(rows.rowHeight) } },
            .backgroundColor = selection && selection[record] ? ctx->theme.accentMutedColor : v%2 ? ctx->theme.surfaceAltColor : ctx->theme.surfaceColor,
            .cornerRadius = rowRadius }) {
            for (int32_t col=0;col<columnCount;++col) {
                Clay_ElementId cellId = ClayWidgets__ChildId(row,CLAY_STRING("DataCell"),col);
                ClayWidgets__RegisterWheelFallthrough(ctx,cellId,Clay_PointerOver(cellId));
                CLAY(cellId, { .layout = { .sizing = { .width = CLAY_SIZING_FIXED(state->widths[col]), .height = CLAY_SIZING_GROW(0) },
                    .padding = { .left = ctx->theme.spacing.sm, .right = ctx->theme.spacing.sm }, .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } },
                    .clip = { .horizontal = true },
                    .border = { .color = ctx->theme.borderColor, .width = { .right = 1 } } }) {
                    CLAY_TEXT(cell(record,col,data), { .textColor = disabled ? ctx->theme.textMutedColor : ctx->theme.textColor,
                        .fontId = ctx->theme.fontBody, .fontSize = ctx->theme.fontSizeBody, .wrapMode = CLAY_TEXT_WRAP_NONE });
                }
            }
        }
    }
    if (bodyScrolls) ctx->scrollPanelDepth--;
    ClayWidgets__EndVirtualRows(ctx,body,rowCount,rows,true);
    // A fixed gutter forms the table's final column, including an empty header
    // cell. It remains visible even when there are too few rows to scroll.
    CLAY(ClayWidgets__ChildId(id,CLAY_STRING("TableScrollColumn"),0), {
        // Overlap the data cells' right border so the shared edge is one pixel.
        .layout = { .sizing = { .width = CLAY_SIZING_FIXED(gutter + 1),
            .height = CLAY_SIZING_FIXED(headerHeight + (options.height > 0 ? options.height : 240)) } },
        .backgroundColor = ctx->theme.surfaceAltColor,
        .floating = { .offset = { -(float)frameInset, (float)frameInset },
            .parentId = id.id, .zIndex = ClayWidgets__OverlayZ(ctx,10),
            .attachPoints = { .element = CLAY_ATTACH_POINT_RIGHT_TOP, .parent = CLAY_ATTACH_POINT_RIGHT_TOP },
            .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
            .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID, .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT },
        .border = ClayWidgets__EdgeBorder(ctx, ctx->theme.borderColor, CLAY__INIT(Clay_BorderWidth){ 1, 0, 0, 0, 0 })
    }) {
        CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(headerHeight) } },
            .border = ClayWidgets__EdgeBorder(ctx, ctx->theme.borderColor, CLAY__INIT(Clay_BorderWidth){ 0, 0, 0, 1, 0 }) }) {}
    }
    ClayWidgets__EndElement();
    return state->sortChanged || state->selectionChanged || state->widthChanged;
}
#endif
#endif
