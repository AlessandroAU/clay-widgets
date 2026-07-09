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
    const float horizontalInset = (float)ctx->theme.spacing.md;
    const float fieldHeight = ClayWidgets__FieldHeight(ctx);
    Clay_ElementId fieldId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsTextInputField"), id.id);
    // The text/caret/selection live in an inner element that owns the horizontal
    // clip+scroll; the bordered field wraps it. Keeping the clip off the bordered
    // element is deliberate — Clay scissors a clip element's OWN background and
    // border, which would shave the right/bottom border edges.
    Clay_ElementId innerId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsTextInputInner"), id.id);

    bool over = Clay_PointerOver(id);
    // The inner element clips both axes but scrolls neither via Clay (its horizontal
    // text scroll is manual), so let a vertical wheel over the field fall through to
    // an enclosing scroll panel instead of being swallowed. Applies even when
    // disabled - an inert field shouldn't eat the panel's scroll.
    ClayWidgets__RegisterWheelFallthrough(ctx, fieldId, over);
    if (options.disabled) {
        if (ctx->focusedId == id.id) {
            ctx->focusedId = 0;
        }
    } else {
        ClayWidgets__RegisterFocusable(ctx, id, over);
        if (ctx->input.pointerPressed && !over && ctx->focusedId == id.id) {
            ctx->focusedId = 0;
        }
    }
    bool focused = !options.disabled && (ctx->focusedId == id.id);

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

    // innerData is the padded content area (the clip element sits inside the
    // field's horizontal padding), so it maps 1:1 to the visible text region.
    Clay_ElementData innerData = Clay_GetElementData(innerId);
    float availableWidth = innerData.found ? innerData.boundingBox.width : 0.0f;
    if (availableWidth <= 0.0f) {
        availableWidth = 1.0f;
    }
    float contentOriginX = innerData.found ? innerData.boundingBox.x : 0.0f;

    if (focused && innerData.found) {
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

        // Clipboard, via the functions injected with
        // ClayWidgets_SetClipboardFunctions (ignored when none are set).
        // Copy/cut act on the selection; paste inserts at the caret, replacing
        // any selection, and stops at the first newline - this is a
        // single-line field.
        if ((ctx->input.keyCopy || ctx->input.keyCut) && ClayWidgets__HasSelection(ctx)) {
            int32_t copyStart = 0;
            int32_t copyEnd = 0;
            ClayWidgets__SelectionRange(ctx, &copyStart, &copyEnd);
            ClayWidgets__CopyToClipboard(ctx, buffer + copyStart, copyEnd - copyStart);
            if (ctx->input.keyCut && ClayWidgets__DeleteSelection(buffer, &length, ctx)) {
                changed = true;
            }
        }

        if (ctx->input.keyPaste && ctx->getClipboardText) {
            const char *clip = ctx->getClipboardText(ctx->clipboardUserData);
            if (clip) {
                int32_t clipLength = 0;
                while (clip[clipLength] != '\0' && clip[clipLength] != '\n' && clip[clipLength] != '\r'
                    && clipLength < CLAY_WIDGETS_TEXT_MAX_BYTES) {
                    clipLength++;
                }
                if (ClayWidgets__InsertTextAtCaret(ctx, buffer, &length, capacity, clip, clipLength)) {
                    changed = true;
                }
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

        // Outer field owns the background, rounding and border. It intentionally
        // does NOT clip: Clay scissors a clip element's own background+border, which
        // would shave the right/bottom border edges. Its horizontal padding is the
        // text inset (matches the buttons' padding).
        CLAY(fieldId, {
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_GROW(0),
                    .height = CLAY_SIZING_FIXED(fieldHeight),
                },
                .padding = { .left = (uint16_t)horizontalInset, .right = (uint16_t)horizontalInset },
                .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
            },
            .backgroundColor = options.disabled
                ? ClayWidgets__MixColor(ctx->theme.surfaceAltColor, ctx->theme.surfaceColor, ctx->theme.disabledMix)
                : (focused ? ctx->theme.hoverColor : ctx->theme.surfaceAltColor),
            .cornerRadius = CLAY_CORNER_RADIUS(ctx->theme.radiusMd),
            .border = {
                .color = focused ? ctx->theme.focusRingColor : ctx->theme.borderColor,
                .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
            },
        }) {
            // Inner element sits inside the padding and owns the horizontal
            // clip+scroll, so the text is clipped at the padding boundary (never
            // under the border).
            CLAY(innerId, {
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
                    .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
                },
                .clip = { .horizontal = true, .vertical = true, .childOffset = { -textScrollX, 0.0f } },
            }) {
                // Selection + caret are floating overlays attached to the inner
                // content element. Their zIndex must sit ABOVE any floating panel
                // this field lives in: a plain (zIndex 0) floating root is painted
                // before, and thus hidden behind, an enclosing panel's opaque
                // background. Their x offset subtracts textScrollX by hand because
                // floating children don't inherit the clip's childOffset.
                const int16_t overlayZ = 500;
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
                                .offset = { startX - textScrollX, textOffsetY },
                                .zIndex = overlayZ,
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
                    .textColor = (length > 0 && !options.disabled) ? ctx->theme.textColor : ctx->theme.textMutedColor,
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
                        .floating = {
                            .offset = { caretX - textScrollX, textOffsetY },
                            .zIndex = overlayZ,
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
        }
    }

    (void)textOffsetY;

    return changed;
}

#endif

#endif