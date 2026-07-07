#ifndef CLAY_WIDGETS_TEXT_INPUT_H
#define CLAY_WIDGETS_TEXT_INPUT_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before text-input.h"
#endif

bool ClayWidgets_TextInput(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    Clay_String label,
    char *buffer,
    int32_t capacity,
    ClayWidgets_TextInputOptions options
);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

bool ClayWidgets_TextInput(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    Clay_String label,
    char *buffer,
    int32_t capacity,
    ClayWidgets_TextInputOptions options
) {
    if (!ctx || !buffer || capacity <= 0) {
        return false;
    }

    int32_t length = ClayWidgets__StrLenBounded(buffer, capacity - 1);
    bool changed = false;

    const uint16_t fontId = ctx->theme.fontBody;
    const uint16_t fontSize = ctx->theme.fontSizeBody;
    const uint16_t letterSpacing = 0;
    const float horizontalInset = (float)ctx->theme.spacing.sm;
    const float fieldHeight = (float)(ctx->theme.fontSizeBody + (int32_t)ctx->theme.spacing.md + 8);
    Clay_ElementId fieldId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsTextInputField"), id.id);

    bool over = Clay_PointerOver(id);
    // The field clips both axes but scrolls neither via Clay (its horizontal text
    // scroll is manual), so let a vertical wheel over it fall through to an
    // enclosing scroll panel instead of being swallowed.
    ClayWidgets__RegisterWheelFallthrough(ctx, fieldId, over);
    ClayWidgets__RegisterFocusable(ctx, id, over);
    if (ctx->input.pointerPressed && !over && ctx->focusedId == id.id) {
        ctx->focusedId = 0;
    }
    bool focused = (ctx->focusedId == id.id);

    if (focused && ctx->textInputId != id.id) {
        ctx->textInputId = id.id;
        ctx->textCursor = length;
        ctx->textSelectionAnchor = length;
        ctx->textScrollX = 0.0f;
        ctx->textPointerSelecting = false;
        ctx->caretBlinkTime = 0.0f;
    }
    if (ctx->textInputId == id.id) {
        ClayWidgets__ClampSelectionToLength(ctx, length);
    }

    Clay_ElementData fieldData = Clay_GetElementData(fieldId);
    float availableWidth = fieldData.found ? (fieldData.boundingBox.width - horizontalInset * 2.0f) : 0.0f;
    if (availableWidth <= 0.0f) {
        availableWidth = 1.0f;
    }
    float contentOriginX = fieldData.found ? (fieldData.boundingBox.x + horizontalInset) : 0.0f;

    if (focused && fieldData.found) {
        if (ctx->input.pointerPressed && over) {
            int32_t hit = ClayWidgets__CursorFromMouse(ctx, buffer, length, ctx->input.mouseX, contentOriginX, ctx->textScrollX, fontId, fontSize, letterSpacing);
            bool isDoubleClick = ClayWidgets__WasDoubleClick(ctx, id.id);
            ClayWidgets__RecordClick(ctx, id.id);

            int32_t wordStart = 0;
            int32_t wordEnd = 0;
            if (isDoubleClick && length > 0) {
                ClayWidgets__FindWordBounds(buffer, length, hit, &wordStart, &wordEnd);
            }

            if (wordEnd > wordStart) {
                ctx->textSelectionAnchor = wordStart;
                ctx->textCursor = wordEnd;
                ctx->textPointerSelecting = false;
                ctx->caretBlinkTime = 0.0f;
            } else {
                ClayWidgets__MoveCaret(ctx, hit, ctx->input.shiftDown);
                ctx->textPointerSelecting = true;
            }
        } else if (ctx->input.pointerDown && ctx->textPointerSelecting) {
            int32_t hit = ClayWidgets__CursorFromMouse(ctx, buffer, length, ctx->input.mouseX, contentOriginX, ctx->textScrollX, fontId, fontSize, letterSpacing);
            ClayWidgets__MoveCaret(ctx, hit, true);
        }
    }
    if (ctx->input.pointerReleased) {
        ctx->textPointerSelecting = false;
    }

    if (focused) {
        bool extend = ctx->input.shiftDown;
        bool hadSelection = ClayWidgets__HasSelection(ctx);
        int32_t selStart = 0;
        int32_t selEnd = 0;
        ClayWidgets__SelectionRange(ctx, &selStart, &selEnd);

        if (ctx->input.keySelectAll && length > 0) {
            ctx->textSelectionAnchor = 0;
            ctx->textCursor = length;
            ctx->caretBlinkTime = 0.0f;
        }

        if (ctx->input.keyLeft) {
            int32_t target = (hadSelection && !extend)
                ? selStart
                : ClayWidgets__Utf8PrevBoundary(buffer, ctx->textCursor);
            ClayWidgets__MoveCaret(ctx, target, extend);
        }

        if (ctx->input.keyRight) {
            int32_t target = (hadSelection && !extend)
                ? selEnd
                : ClayWidgets__Utf8NextBoundary(buffer, length, ctx->textCursor);
            ClayWidgets__MoveCaret(ctx, target, extend);
        }

        if (ctx->input.keyHome) {
            ClayWidgets__MoveCaret(ctx, 0, extend);
        }

        if (ctx->input.keyEnd) {
            ClayWidgets__MoveCaret(ctx, length, extend);
        }

        if (ctx->input.keyBackspace) {
            if (ClayWidgets__DeleteSelection(buffer, &length, ctx)) {
                changed = true;
            } else if (ctx->textCursor > 0) {
                int32_t from = ClayWidgets__Utf8PrevBoundary(buffer, ctx->textCursor);
                memmove(buffer + from, buffer + ctx->textCursor, (size_t)(length - ctx->textCursor + 1));
                length -= (ctx->textCursor - from);
                ClayWidgets__MoveCaret(ctx, from, false);
                changed = true;
            }
        }

        if (ctx->input.keyDelete) {
            if (ClayWidgets__DeleteSelection(buffer, &length, ctx)) {
                changed = true;
            } else if (ctx->textCursor < length) {
                int32_t to = ClayWidgets__Utf8NextBoundary(buffer, length, ctx->textCursor);
                memmove(buffer + ctx->textCursor, buffer + to, (size_t)(length - to + 1));
                length -= (to - ctx->textCursor);
                ctx->caretBlinkTime = 0.0f;
                changed = true;
            }
        }

        if (ctx->input.textUtf8 && ctx->input.textUtf8Length > 0) {
            if (ClayWidgets__InsertTextAtCaret(ctx, buffer, &length, capacity, ctx->input.textUtf8, ctx->input.textUtf8Length)) {
                changed = true;
            }
        }

        if (ctx->input.keyEnter && options.clearOnEnter && length > 0) {
            buffer[0] = '\0';
            length = 0;
            ClayWidgets__MoveCaret(ctx, 0, false);
            changed = true;
        }

        if (ctx->input.keyEscape) {
            ctx->focusedId = 0;
            ctx->textPointerSelecting = false;
            focused = false;
        }
    }

    if (focused) {
        ClayWidgets__UpdateTextScroll(ctx, id.id, buffer, length, availableWidth, fontId, fontSize, letterSpacing);
    }

    float textScrollX = focused ? ctx->textScrollX : 0.0f;

    Clay_Dimensions lineMetrics = ClayWidgets__MeasureSlice(ctx, "Ag", 2, fontId, fontSize, letterSpacing);
    float textHeight = lineMetrics.height > 0.0f ? lineMetrics.height : (float)fontSize;
    float textOffsetY = (fieldHeight - textHeight) * 0.5f;
    if (textOffsetY < 0.0f) {
        textOffsetY = 0.0f;
    }

    Clay_String displayText;
    if (length > 0) {
        displayText = ClayWidgets__StringFromCString(buffer);
    } else if (options.placeholder) {
        displayText = ClayWidgets__StringFromCString(options.placeholder);
    } else {
        displayText = ClayWidgets__StringFromCString("");
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

        CLAY(fieldId, {
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_GROW(0),
                    .height = CLAY_SIZING_FIXED(fieldHeight),
                },
                .padding = CLAY_PADDING_ALL(ctx->theme.spacing.sm),
                .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
            },
            .backgroundColor = focused ? ctx->theme.hoverColor : ctx->theme.surfaceAltColor,
            .cornerRadius = CLAY_CORNER_RADIUS(ctx->theme.radiusSm),
            .clip = { .horizontal = true, .vertical = true, .childOffset = { -textScrollX, 0.0f } },
            .border = {
                .color = focused ? ctx->theme.focusRingColor : ctx->theme.borderColor,
                .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
            },
        }) {
            if (focused && ClayWidgets__HasSelection(ctx) && length > 0) {
                int32_t selStart = 0;
                int32_t selEnd = 0;
                ClayWidgets__SelectionRange(ctx, &selStart, &selEnd);
                float startX = ClayWidgets__MeasureWidth(ctx, buffer, selStart, fontId, fontSize, letterSpacing);
                float endX = ClayWidgets__MeasureWidth(ctx, buffer, selEnd, fontId, fontSize, letterSpacing);
                float selWidth = endX - startX;
                if (selWidth > 0.0f) {
                    CLAY_AUTO_ID({
                        .layout = {
                            .sizing = {
                                .width = CLAY_SIZING_FIXED(selWidth),
                                .height = CLAY_SIZING_FIXED(textHeight),
                            },
                        },
                        .backgroundColor = ctx->theme.accentMutedColor,
                        .cornerRadius = CLAY_CORNER_RADIUS(3),
                        .floating = {
                            .offset = { horizontalInset + startX, textOffsetY },
                            .attachPoints = {
                                .element = CLAY_ATTACH_POINT_LEFT_TOP,
                                .parent = CLAY_ATTACH_POINT_LEFT_TOP,
                            },
                            .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
                            .attachTo = CLAY_ATTACH_TO_PARENT,
                            .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT,
                        },
                    }) {}
                }
            }

            CLAY_TEXT(displayText, {
                .textColor = (length > 0) ? ctx->theme.textColor : ctx->theme.textMutedColor,
                .fontId = fontId,
                .fontSize = fontSize,
                .wrapMode = CLAY_TEXT_WRAP_NONE,
            });

            if (focused && ((int32_t)(ctx->caretBlinkTime * 2.0f) % 2 == 0)) {
                float caretX = ClayWidgets__MeasureWidth(ctx, buffer, ctx->textCursor, fontId, fontSize, letterSpacing);
                CLAY_AUTO_ID({
                    .layout = {
                        .sizing = {
                            .width = CLAY_SIZING_FIXED(1),
                            .height = CLAY_SIZING_FIXED(textHeight),
                        },
                    },
                    .backgroundColor = ctx->theme.textColor,
                }) {}
            }
        }
    }

    (void)textOffsetY;

    return changed;
}

#endif

#endif