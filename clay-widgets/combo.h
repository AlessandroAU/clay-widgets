#ifndef CLAY_WIDGETS_COMBO_H
#define CLAY_WIDGETS_COMBO_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before combo.h"
#endif

// A combo box (dropdown select). The dropdown caps its height (at ~40% of the
// layout) and scrolls when the item list is longer, opens upward when there is
// not enough room below the trigger, and keeps the keyboard-highlighted item
// scrolled into view.
bool ClayWidgets_Combo(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    Clay_String label,
    const Clay_String *items,
    int32_t itemCount,
    int32_t *selectedIndex
);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

// Defined in scroll-bar.h, which widgets.h includes after this file; the
// dropdown reuses it for its own scroll container.
void ClayWidgets_ScrollBar(ClayWidgets_Context *ctx, Clay_ElementId scrollContainerId);

bool ClayWidgets_Combo(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    Clay_String label,
    const Clay_String *items,
    int32_t itemCount,
    int32_t *selectedIndex
) {
    if (!ctx || !items || itemCount <= 0 || !selectedIndex) {
        return false;
    }

    bool changed = false;
    Clay_ElementId triggerId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsComboTrigger"), id.id);
    Clay_ElementId dropdownId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsComboDropdown"), id.id);
    // The dropdown's scroll bar floats a few pixels past the dropdown's right
    // edge, so the dismiss-on-outside-press check below must treat it as
    // inside. Same derivation as ClayWidgets_ScrollBar uses.
    Clay_ElementId dropdownScrollBarId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsScrollBarTrack"), dropdownId.id);

    bool triggerOver = Clay_PointerOver(triggerId);
    bool anyOver = Clay_PointerOver(id) || triggerOver || Clay_PointerOver(dropdownId);
    if (triggerOver || Clay_PointerOver(dropdownId)) {
        ClayWidgets__SetCursor(ctx, CLAY_WIDGETS_CURSOR_POINTER);
    }
    bool focused = ClayWidgets__RegisterFocusable(ctx, id, anyOver);
    bool isOpen = (ctx->openComboId == id.id);

    if (isOpen && !focused && ctx->focusedId != 0) {
        ctx->openComboId = 0;
        isOpen = false;
    }

    if (isOpen && focused && ctx->input.keyEscape) {
        ctx->openComboId = 0;
        isOpen = false;
    }

    if (isOpen && ctx->input.pointerPressed) {
        if (!Clay_PointerOver(triggerId) && !Clay_PointerOver(dropdownId) && !Clay_PointerOver(dropdownScrollBarId)) {
            ctx->openComboId = 0;
            isOpen = false;
        }
    }

    if (ClayWidgets__ConsumeClick(ctx, triggerOver)) {
        if (isOpen) {
            ctx->openComboId = 0;
            isOpen = false;
        } else {
            ctx->openComboId = id.id;
            ctx->comboHighlightIndex = (*selectedIndex >= 0 && *selectedIndex < itemCount)
                ? *selectedIndex : 0;
            ctx->comboScrollToHighlight = true;
            isOpen = true;
        }
    }

    bool openedByKeyboard = focused && !isOpen && ClayWidgets__ActivateFocused(ctx, id);
    if (openedByKeyboard) {
        ctx->openComboId = id.id;
        ctx->comboHighlightIndex = (*selectedIndex >= 0 && *selectedIndex < itemCount)
            ? *selectedIndex : 0;
        ctx->comboScrollToHighlight = true;
        isOpen = true;
    }

    if (focused && isOpen) {
        if (ctx->input.keyHome) { ctx->comboHighlightIndex = 0; ctx->comboScrollToHighlight = true; }
        if (ctx->input.keyEnd) { ctx->comboHighlightIndex = itemCount - 1; ctx->comboScrollToHighlight = true; }
        if (ClayWidgets__TypeAhead(ctx, id.id)) {
            for (int32_t i = 0; i < itemCount; ++i) {
                if (ClayWidgets__PrefixMatches(items[i], ctx->typeAhead, ctx->typeAheadLength)) {
                    ctx->comboHighlightIndex = i; ctx->comboScrollToHighlight = true; break;
                }
            }
        }
        if (ctx->input.keyUp) {
            ctx->comboHighlightIndex = ctx->comboHighlightIndex > 0
                ? ctx->comboHighlightIndex - 1 : itemCount - 1;
            ctx->comboScrollToHighlight = true;
        }
        if (ctx->input.keyDown) {
            ctx->comboHighlightIndex = ctx->comboHighlightIndex < itemCount - 1
                ? ctx->comboHighlightIndex + 1 : 0;
            ctx->comboScrollToHighlight = true;
        }
        if ((ctx->input.keyEnter || ctx->input.keySpace) && !openedByKeyboard) {
            if (ctx->comboHighlightIndex >= 0 && ctx->comboHighlightIndex < itemCount) {
                changed = *selectedIndex != ctx->comboHighlightIndex;
                *selectedIndex = ctx->comboHighlightIndex;
            }
            ctx->openComboId = 0;
            isOpen = false;
        }
    }

    if (isOpen) {
        for (int32_t i = 0; i < itemCount; i++) {
            Clay_ElementId itemId = ClayWidgets__ChildId(id, CLAY_STRING("ClayWidgetsComboItem"), i);
            bool itemOver = Clay_PointerOver(itemId);
            if (itemOver) {
                ctx->comboHighlightIndex = i;
            }
            if (ClayWidgets__ConsumeClick(ctx, itemOver)) {
                changed = *selectedIndex != i;
                *selectedIndex = i;
                ctx->openComboId = 0;
                isOpen = false;
                break;
            }
        }
    }

    Clay_ElementData triggerData = Clay_GetElementData(triggerId);
    Clay_ElementData dropdownData = Clay_GetElementData(dropdownId);
    float dropdownMinWidth = triggerData.found ? triggerData.boundingBox.width : 0.0f;

    const float fieldHeight = ClayWidgets__FieldHeight(ctx);
    float r = (float)ctx->theme.radiusSm;

    // The dropdown is a scroll container capped at a fraction of the layout
    // height, so a long item list scrolls instead of running off the screen.
    float itemHeightEstimate = (float)ctx->theme.fontSizeBody + 2.0f * (float)ctx->theme.spacing.sm;
    float maxDropdownHeight = ctx->layoutDimensions.height * 0.4f;
    if (maxDropdownHeight < itemHeightEstimate * 3.0f) {
        maxDropdownHeight = itemHeightEstimate * 3.0f;
    }
    bool mayScroll = itemHeightEstimate * (float)itemCount > maxDropdownHeight;

    // Open upward when the dropdown wouldn't fit below the trigger but would
    // above it. Uses last frame's actual dropdown height once it exists (the
    // item-height estimate on the opening frame).
    float dropdownHeightEstimate = itemHeightEstimate * (float)itemCount + 2.0f;
    if (dropdownHeightEstimate > maxDropdownHeight) {
        dropdownHeightEstimate = maxDropdownHeight;
    }
    if (isOpen && dropdownData.found) {
        dropdownHeightEstimate = dropdownData.boundingBox.height;
    }
    bool flipUp = false;
    if (isOpen && triggerData.found) {
        float spaceBelow = ctx->layoutDimensions.height
            - (triggerData.boundingBox.y + triggerData.boundingBox.height);
        float spaceAbove = triggerData.boundingBox.y;
        flipUp = spaceBelow < dropdownHeightEstimate && spaceAbove >= dropdownHeightEstimate;
    }

    // Scroll the keyboard-highlighted item into view (set on open and on
    // Up/Down, not on hover - scrolling under the pointer would re-highlight
    // and fight the wheel). Uses last frame's boxes, so the first open frame
    // leaves the flag set and applies one frame later.
    if (isOpen && ctx->comboScrollToHighlight) {
        Clay_ScrollContainerData dropScroll = Clay_GetScrollContainerData(dropdownId);
        Clay_ElementId highlightId = ClayWidgets__ChildId(id, CLAY_STRING("ClayWidgetsComboItem"), ctx->comboHighlightIndex);
        Clay_ElementData highlightData = Clay_GetElementData(highlightId);
        if (dropScroll.found && dropScroll.scrollPosition && highlightData.found && dropdownData.found) {
            float viewTop = dropdownData.boundingBox.y;
            float viewBottom = viewTop + dropdownData.boundingBox.height;
            float itemTop = highlightData.boundingBox.y;
            float itemBottom = itemTop + highlightData.boundingBox.height;
            float adjust = 0.0f;
            if (itemTop < viewTop) {
                adjust = viewTop - itemTop;
            } else if (itemBottom > viewBottom) {
                adjust = viewBottom - itemBottom;
            }
            if (adjust != 0.0f) {
                float maxScroll = dropScroll.contentDimensions.height - dropScroll.scrollContainerDimensions.height;
                if (maxScroll < 0.0f) {
                    maxScroll = 0.0f;
                }
                dropScroll.scrollPosition->y = ClayWidgets__Clamp(dropScroll.scrollPosition->y + adjust, -maxScroll, 0.0f);
            }
            ctx->comboScrollToHighlight = false;
        }
    }

    Clay_String displayText = (*selectedIndex >= 0 && *selectedIndex < itemCount)
        ? items[*selectedIndex]
        : CLAY_STRING("Select...");

    // Classic combo: a sunken white field with a raised drop-down button pinned
    // to its right edge, and a shadowed list panel under it.
    bool beveled = ClayWidgets__IsBeveled(ctx);
    Clay_ElementId arrowId = ClayWidgets__ChildId(id, CLAY_STRING("ClayWidgetsComboArrow"), 0);
    Clay_Color triggerBg = beveled ? ctx->theme.fieldColor : ctx->theme.surfaceAltColor;
    if ((triggerOver || isOpen) && !beveled) {
        triggerBg = ctx->theme.hoverColor;
    }
    ClayWidgets_SetEdge(ctx, triggerId, CLAY_WIDGETS_EDGE_SUNKEN);
    ClayWidgets_SetEdge(ctx, arrowId, isOpen ? CLAY_WIDGETS_EDGE_SUNKEN : CLAY_WIDGETS_EDGE_RAISED);
    if (focused) {
        ClayWidgets__FocusRect(ctx, triggerId);
    }
    ClayWidgets_SetEdge(ctx, dropdownId, CLAY_WIDGETS_EDGE_RAISED);
    ClayWidgets_SetShadow(ctx, dropdownId);

    // While open, square the trigger corners on the side the dropdown joins.
    Clay_CornerRadius triggerRadius = { r, r, r, r };
    if (isOpen) {
        triggerRadius = flipUp
            ? (Clay_CornerRadius){ 0.0f, 0.0f, r, r }
            : (Clay_CornerRadius){ r, r, 0.0f, 0.0f };
    }

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
            .childGap = ctx->theme.spacing.xs,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
    }) {
        if (label.length > 0 && label.chars) {
            CLAY_TEXT(label, {
                .textColor = ctx->theme.textMutedColor,
                .fontId = ctx->theme.fontBody,
                .fontSize = ctx->theme.fontSizeSmall,
            });
        }

        CLAY(triggerId, {
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_GROW(0),
                    .height = CLAY_SIZING_FIXED(fieldHeight),
                },
                // The drop-down button sits inside the field's sunken edge, so
                // on a beveled theme the field's own padding shrinks to it.
                .padding = (Clay_Padding){
                    .left = ctx->theme.spacing.sm,
                    .right = (uint16_t)(beveled ? 2 : ctx->theme.spacing.sm),
                    .top = (uint16_t)(beveled ? 2 : 0),
                    .bottom = (uint16_t)(beveled ? 2 : 0),
                },
                .childGap = ctx->theme.spacing.sm,
                .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
            },
            .backgroundColor = triggerBg,
            .cornerRadius = triggerRadius,
            .border = ClayWidgets__Border(ctx, (focused || isOpen) ? ctx->theme.focusRingColor : ctx->theme.borderColor),
            .transition = ClayWidgets__ColorTransition(ctx),
        }) {
            CLAY_AUTO_ID({
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                },
            }) {
                CLAY_TEXT(displayText, {
                    .textColor = ctx->theme.textColor,
                    .fontId = ctx->theme.fontBody,
                    .fontSize = ctx->theme.fontSizeBody,
                    .wrapMode = CLAY_TEXT_WRAP_NONE,
                });
            }
            CLAY(arrowId, {
                .layout = {
                    .sizing = {
                        .width = beveled ? CLAY_SIZING_FIXED(fieldHeight - 6.0f) : CLAY_SIZING_FIT(0, 0),
                        .height = beveled ? CLAY_SIZING_GROW(0) : CLAY_SIZING_FIT(0, 0),
                    },
                    .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                },
                .backgroundColor = beveled ? ctx->theme.surfaceAltColor : (Clay_Color){ 0, 0, 0, 0 },
            }) {
                CLAY_TEXT(CLAY_STRING("v"), {
                    .textColor = beveled ? ctx->theme.textColor : ctx->theme.textMutedColor,
                    .fontId = ctx->theme.fontBody,
                    .fontSize = ctx->theme.fontSizeBody,
                });
            }
        }

        if (isOpen && ClayWidgets__PushOverlay(ctx, 100)) {
            CLAY(dropdownId, {
                .layout = {
                    .sizing = {
                        .width = CLAY_SIZING_FIT(dropdownMinWidth, 0),
                        .height = CLAY_SIZING_FIT(0, maxDropdownHeight),
                    },
                    // Reserve a gutter so items never sit under the floating
                    // scroll bar when the list is long enough to scroll.
                    .padding = { .right = (uint16_t)(mayScroll ? CLAY_WIDGETS_SCROLLBAR_WIDTH + ctx->theme.spacing.xs : 0) },
                    .childGap = 0,
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                },
                .backgroundColor = beveled ? ctx->theme.fieldColor : ctx->theme.surfaceAltColor,
                .cornerRadius = flipUp
                    ? (Clay_CornerRadius){ r, r, 0.0f, 0.0f }
                    : (Clay_CornerRadius){ 0.0f, 0.0f, r, r },
                .floating = {
                    .parentId = triggerId.id,
                    .zIndex = ClayWidgets__OverlayZ(ctx, 0),
                    .attachPoints = flipUp
                        ? (Clay_FloatingAttachPoints){
                              .element = CLAY_ATTACH_POINT_LEFT_BOTTOM,
                              .parent = CLAY_ATTACH_POINT_LEFT_TOP,
                          }
                        : (Clay_FloatingAttachPoints){
                              .element = CLAY_ATTACH_POINT_LEFT_TOP,
                              .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM,
                          },
                    .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID,
                },
                .clip = { .vertical = true, .childOffset = Clay_GetScrollOffset() },
                .border = ClayWidgets__EdgeBorder(ctx, ctx->theme.focusRingColor, CLAY__INIT(Clay_BorderWidth){
                    1, 1, (uint16_t)(flipUp ? 1 : 0), (uint16_t)(flipUp ? 0 : 1), 0 }),
            }) {
                for (int32_t i = 0; i < itemCount; i++) {
                    Clay_ElementId itemId = ClayWidgets__ChildId(id, CLAY_STRING("ClayWidgetsComboItem"), i);
                    bool itemOver = Clay_PointerOver(itemId);
                    bool isHighlighted = (i == ctx->comboHighlightIndex);
                    bool isSelected = (i == *selectedIndex);

                    Clay_Color itemBg = beveled ? ctx->theme.fieldColor : ctx->theme.surfaceAltColor;
                    Clay_Color itemText = ctx->theme.textColor;
                    if (itemOver || isHighlighted) {
                        itemBg = ctx->theme.selectionColor;
                        itemText = ctx->theme.onSelectionColor;
                    } else if (isSelected) {
                        itemBg = ctx->theme.accentMutedColor;
                    }

                    CLAY(itemId, {
                        .layout = {
                            .sizing = {
                                .width = CLAY_SIZING_GROW(0),
                                .height = CLAY_SIZING_FIT(0, 0),
                            },
                            .padding = CLAY_PADDING_ALL(ctx->theme.spacing.sm),
                            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
                        },
                        .backgroundColor = itemBg,
                        .transition = ClayWidgets__ColorTransition(ctx),
                    }) {
                        CLAY_TEXT(items[i], {
                            .textColor = itemText,
                            .fontId = ctx->theme.fontBody,
                            .fontSize = ctx->theme.fontSizeBody,
                            .wrapMode = CLAY_TEXT_WRAP_NONE,
                        });
                    }
                }

                ClayWidgets_ScrollBar(ctx, dropdownId);
            }
            ClayWidgets__PopOverlay(ctx);
        }
    }

    return changed;
}

#endif

#endif
