#ifndef CLAY_WIDGETS_COMBO_H
#define CLAY_WIDGETS_COMBO_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before combo.h"
#endif

bool ClayWidgets_Combo(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    Clay_String label,
    const Clay_String *items,
    int32_t itemCount,
    int32_t *selectedIndex
);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

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

    bool triggerOver = Clay_PointerOver(triggerId);
    bool anyOver = Clay_PointerOver(id) || triggerOver || Clay_PointerOver(dropdownId);
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
        if (!Clay_PointerOver(triggerId) && !Clay_PointerOver(dropdownId)) {
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
            isOpen = true;
        }
    }

    if (focused && !isOpen && ClayWidgets__ActivateFocused(ctx, id)) {
        ctx->openComboId = id.id;
        ctx->comboHighlightIndex = (*selectedIndex >= 0 && *selectedIndex < itemCount)
            ? *selectedIndex : 0;
        isOpen = true;
    }

    if (focused && isOpen) {
        if (ctx->input.keyUp) {
            ctx->comboHighlightIndex = ctx->comboHighlightIndex > 0
                ? ctx->comboHighlightIndex - 1 : itemCount - 1;
        }
        if (ctx->input.keyDown) {
            ctx->comboHighlightIndex = ctx->comboHighlightIndex < itemCount - 1
                ? ctx->comboHighlightIndex + 1 : 0;
        }
        if (ctx->input.keyEnter) {
            if (ctx->comboHighlightIndex >= 0 && ctx->comboHighlightIndex < itemCount) {
                *selectedIndex = ctx->comboHighlightIndex;
                changed = true;
            }
            ctx->openComboId = 0;
            isOpen = false;
        }
    }

    if (isOpen) {
        for (int32_t i = 0; i < itemCount; i++) {
            Clay_ElementId itemId = Clay_GetElementIdWithIndex(
                CLAY_STRING("ClayWidgetsComboItem"),
                (uint32_t)((uint64_t)id.id * 1000003u + (uint32_t)i)
            );
            bool itemOver = Clay_PointerOver(itemId);
            if (itemOver) {
                ctx->comboHighlightIndex = i;
            }
            if (ClayWidgets__ConsumeClick(ctx, itemOver)) {
                *selectedIndex = i;
                ctx->openComboId = 0;
                isOpen = false;
                changed = true;
                break;
            }
        }
    }

    Clay_ElementData triggerData = Clay_GetElementData(triggerId);
    float dropdownMinWidth = triggerData.found ? triggerData.boundingBox.width : 0.0f;

    const float fieldHeight = (float)(ctx->theme.fontSizeBody + (int32_t)ctx->theme.spacing.md + 8);
    float r = (float)ctx->theme.radiusSm;

    Clay_String displayText = (*selectedIndex >= 0 && *selectedIndex < itemCount)
        ? items[*selectedIndex]
        : CLAY_STRING("Select...");

    Clay_Color triggerBg = ctx->theme.surfaceAltColor;
    if (triggerOver || isOpen) {
        triggerBg = ctx->theme.hoverColor;
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
                .padding = (Clay_Padding){
                    .left = ctx->theme.spacing.sm,
                    .right = ctx->theme.spacing.sm,
                    .top = 0,
                    .bottom = 0,
                },
                .childGap = ctx->theme.spacing.sm,
                .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
            },
            .backgroundColor = triggerBg,
            .cornerRadius = isOpen
                ? (Clay_CornerRadius){ r, r, 0.0f, 0.0f }
                : (Clay_CornerRadius){ r, r, r, r },
            .border = {
                .color = (focused || isOpen) ? ctx->theme.focusRingColor : ctx->theme.borderColor,
                .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
            },
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
            CLAY_TEXT(CLAY_STRING("v"), {
                .textColor = ctx->theme.textMutedColor,
                .fontId = ctx->theme.fontBody,
                .fontSize = ctx->theme.fontSizeBody,
            });
        }

        if (isOpen) {
            CLAY(dropdownId, {
                .layout = {
                    .sizing = {
                        .width = CLAY_SIZING_FIT(dropdownMinWidth, 0),
                        .height = CLAY_SIZING_FIT(0, 0),
                    },
                    .childGap = 0,
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                },
                .backgroundColor = ctx->theme.surfaceAltColor,
                .cornerRadius = (Clay_CornerRadius){ 0.0f, 0.0f, r, r },
                .floating = {
                    .parentId = triggerId.id,
                    .zIndex = 10,
                    .attachPoints = {
                        .element = CLAY_ATTACH_POINT_LEFT_TOP,
                        .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM,
                    },
                    .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID,
                },
                .border = {
                    .color = ctx->theme.focusRingColor,
                    .width = { .left = 1, .right = 1, .top = 0, .bottom = 1 },
                },
            }) {
                for (int32_t i = 0; i < itemCount; i++) {
                    Clay_ElementId itemId = Clay_GetElementIdWithIndex(
                        CLAY_STRING("ClayWidgetsComboItem"),
                        (uint32_t)((uint64_t)id.id * 1000003u + (uint32_t)i)
                    );
                    bool itemOver = Clay_PointerOver(itemId);
                    bool isHighlighted = (i == ctx->comboHighlightIndex);
                    bool isSelected = (i == *selectedIndex);

                    Clay_Color itemBg = ctx->theme.surfaceAltColor;
                    if (itemOver || isHighlighted) {
                        itemBg = ctx->theme.hoverColor;
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
                    }) {
                        CLAY_TEXT(items[i], {
                            .textColor = ctx->theme.textColor,
                            .fontId = ctx->theme.fontBody,
                            .fontSize = ctx->theme.fontSizeBody,
                            .wrapMode = CLAY_TEXT_WRAP_NONE,
                        });
                    }
                }
            }
        }
    }

    return changed;
}

#endif

#endif