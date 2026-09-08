#ifndef CLAY_WIDGETS_TEXT_AREA_H
#define CLAY_WIDGETS_TEXT_AREA_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before text-area.h"
#endif

// Multi-line plain-text editor over a caller-owned NUL-terminated UTF-8
// buffer. Hard lines (split on '\n') soft-wrap at word boundaries to the
// field's width by default; set options.noWrap for code-editor behavior
// where long lines scroll horizontally instead. Enter inserts a newline,
// Up/Down move by visual row remembering the caret's preferred column,
// Home/End move within the hard line, and paste accepts multi-line clipboard
// text (normalizing CRLF and lone CR to LF). Scrolling is handled by an
// internal Clay scroll container that follows the caret; a scrollbar is
// emitted automatically when the content overflows.
//
// Shares the focused-editing state (caret, selection, blink) with
// ClayWidgets_TextInput - at most one text editor is focused at a time.
// Returns true when the buffer was modified this frame.
bool ClayWidgets_TextArea(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    Clay_String label,
    char *buffer,
    int32_t capacity,
    ClayWidgets_TextAreaOptions options
);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

// Defined in scroll-bar.h, which is included after this header.
void ClayWidgets_ScrollBar(ClayWidgets_Context *ctx, Clay_ElementId scrollContainerId);

// Effectively-infinite wrap width: NextRowBreak short-circuits at or above
// this, making a hard line a single visual row (the noWrap mode, and the
// fallback before the field's geometry exists).
#define CLAY_WIDGETS__NO_WRAP_WIDTH 3.0e38f

// Hard-line boundaries. Offsets are byte offsets into the buffer; '\n' is a
// single byte and never part of a UTF-8 sequence, so plain byte scans are
// UTF-8 safe.
static int32_t ClayWidgets__LineStart(const char *text, int32_t offset) {
    while (offset > 0 && text[offset - 1] != '\n') {
        offset--;
    }
    return offset;
}

static int32_t ClayWidgets__LineEnd(const char *text, int32_t length, int32_t offset) {
    while (offset < length && text[offset] != '\n') {
        offset++;
    }
    return offset;
}

// Where the visual row starting at rowStart ends (exclusive; the next row
// starts there), greedily fitting whole words into availWidth. Spaces never
// force a wrap - they ride at the end of the row (and may hang past the
// right edge, as in mainstream editors); a single word wider than the row
// breaks at character granularity. Recomputed on the fly each frame - the
// measure cost is one call per word - a wrap cache is the known optimization
// if large documents ever need it.
static int32_t ClayWidgets__NextRowBreak(
    ClayWidgets_Context *ctx,
    const char *text,
    int32_t lineEnd,
    int32_t rowStart,
    float availWidth,
    uint16_t fontId,
    uint16_t fontSize,
    uint16_t letterSpacing
) {
    if (rowStart >= lineEnd || availWidth >= CLAY_WIDGETS__NO_WRAP_WIDTH) {
        return lineEnd;
    }
    if (ClayWidgets__MeasureWidth(ctx, text + rowStart, lineEnd - rowStart, fontId, fontSize, letterSpacing) <= availWidth) {
        return lineEnd;
    }

    int32_t i = rowStart;
    for (;;) {
        while (i < lineEnd && text[i] == ' ') {
            i++;
        }
        if (i >= lineEnd) {
            return lineEnd; // only spaces left; they hang on this row
        }
        int32_t wordStart = i;
        while (i < lineEnd && text[i] != ' ') {
            i = ClayWidgets__Utf8NextBoundary(text, lineEnd, i);
        }
        if (ClayWidgets__MeasureWidth(ctx, text + rowStart, i - rowStart, fontId, fontSize, letterSpacing) > availWidth) {
            if (wordStart > rowStart) {
                return wordStart; // wrap before this word; its leading spaces stay behind
            }
            // The row's first word alone overflows: fit as many characters as
            // possible, always keeping at least one so rows make progress.
            int32_t fit = ClayWidgets__Utf8NextBoundary(text, lineEnd, rowStart);
            for (;;) {
                int32_t next = ClayWidgets__Utf8NextBoundary(text, lineEnd, fit);
                if (next <= fit || next > i) {
                    break;
                }
                if (ClayWidgets__MeasureWidth(ctx, text + rowStart, next - rowStart, fontId, fontSize, letterSpacing) > availWidth) {
                    break;
                }
                fit = next;
            }
            return fit;
        }
    }
}

// A caret offset exactly at a wrap break belongs to the FOLLOWING row (it
// renders at that row's start). A vertical move or click that would land
// there steps back one character so the caret stays on the row the user
// aimed at; proper caret affinity is the known follow-up.
static int32_t ClayWidgets__AvoidWrapBreak(
    const char *text,
    int32_t target,
    int32_t rowStart,
    int32_t rowEnd,
    int32_t hardLineEnd
) {
    if (target == rowEnd && rowEnd < hardLineEnd && target > rowStart) {
        return ClayWidgets__Utf8PrevBoundary(text, target);
    }
    return target;
}

// Global visual-row index containing `offset`, and that row's start offset -
// the caret's y and x for caret-follow scrolling.
static int32_t ClayWidgets__RowIndexForOffset(
    ClayWidgets_Context *ctx,
    const char *text,
    int32_t length,
    int32_t offset,
    float availWidth,
    uint16_t fontId,
    uint16_t fontSize,
    uint16_t letterSpacing,
    int32_t *rowStartOut
) {
    int32_t lineStart = 0;
    int32_t row = 0;
    for (;;) {
        int32_t lineEnd = ClayWidgets__LineEnd(text, length, lineStart);
        int32_t rowStart = lineStart;
        for (;;) {
            int32_t rowEnd = ClayWidgets__NextRowBreak(ctx, text, lineEnd, rowStart, availWidth, fontId, fontSize, letterSpacing);
            bool lastRowOfLine = rowEnd >= lineEnd;
            if (offset <= lineEnd && (offset < rowEnd || lastRowOfLine)) {
                *rowStartOut = rowStart;
                return row;
            }
            row++;
            if (lastRowOfLine) {
                break;
            }
            rowStart = rowEnd;
        }
        if (lineEnd >= length) {
            *rowStartOut = lineStart;
            return row > 0 ? row - 1 : 0; // offset past the end: clamp to the last row
        }
        lineStart = lineEnd + 1;
    }
}

// Maps a pointer position to a byte offset: the vertical hit picks the
// visual row (clamped to the first/last row), the horizontal hit picks the
// column within it. Origin and scroll offsets are the content clip's.
static int32_t ClayWidgets__TextAreaHit(
    ClayWidgets_Context *ctx,
    const char *buffer,
    int32_t length,
    float mouseX,
    float mouseY,
    Clay_BoundingBox contentBox,
    float scrollOffsetX,
    float scrollOffsetY,
    float lineHeight,
    float availWidth,
    uint16_t fontId,
    uint16_t fontSize,
    uint16_t letterSpacing
) {
    float localY = mouseY - contentBox.y - scrollOffsetY;
    int32_t targetRow = (localY >= 0.0f && lineHeight > 0.0f) ? (int32_t)(localY / lineHeight) : 0;
    if (targetRow < 0) {
        targetRow = 0;
    }
    float localX = mouseX - contentBox.x - scrollOffsetX;

    int32_t lineStart = 0;
    int32_t row = 0;
    for (;;) {
        int32_t lineEnd = ClayWidgets__LineEnd(buffer, length, lineStart);
        int32_t rowStart = lineStart;
        for (;;) {
            int32_t rowEnd = ClayWidgets__NextRowBreak(ctx, buffer, lineEnd, rowStart, availWidth, fontId, fontSize, letterSpacing);
            bool lastRowOfLine = rowEnd >= lineEnd;
            bool lastRowOfDoc = lastRowOfLine && lineEnd >= length;
            if (row == targetRow || lastRowOfDoc) {
                int32_t hit = rowStart + ClayWidgets__FindCursorFromLocalX(
                    ctx, buffer + rowStart, rowEnd - rowStart, localX, fontId, fontSize, letterSpacing);
                return ClayWidgets__AvoidWrapBreak(buffer, hit, rowStart, rowEnd, lineEnd);
            }
            row++;
            if (lastRowOfLine) {
                break;
            }
            rowStart = rowEnd;
        }
        lineStart = lineEnd + 1;
    }
}

bool ClayWidgets_TextArea(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    Clay_String label,
    char *buffer,
    int32_t capacity,
    ClayWidgets_TextAreaOptions options
) {
    if (!ctx || !buffer || capacity <= 0) {
        return false;
    }
    options.disabled = options.disabled || ctx->disabledDepth > 0;
    if (options.disabled && ctx->activeId == id.id) ctx->activeId = 0;


    int32_t length = ClayWidgets__StrLenBounded(buffer, capacity - 1);
    bool changed = false;
    if (options.result) memset(options.result, 0, sizeof(*options.result));

    const uint16_t fontId = ctx->theme.fontBody;
    const uint16_t fontSize = ctx->theme.fontSizeBody;
    const uint16_t letterSpacing = 0;
    const float caretPad = 2.0f;
    Clay_ElementId fieldId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsTextAreaField"), id.id);
    Clay_ElementId contentId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsTextAreaContent"), id.id);
    Clay_ElementId scrollTrackId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsScrollBarTrack"), contentId.id);

    Clay_Dimensions lineMetrics = ClayWidgets__MeasureSlice(ctx, "Ag", 2, fontId, fontSize, letterSpacing);
    float lineHeight = lineMetrics.height > 0.0f ? lineMetrics.height : (float)fontSize;

    bool over = Clay_PointerOver(id);
    // I-beam over the editable text, held through a drag-selection even when
    // the pointer leaves the field. The scrollbar emitted below overwrites
    // this with the pointer cursor when its thumb is hovered.
    if (!options.disabled && (over || (ctx->textInputId == id.id && ctx->textPointerSelecting))) {
        ClayWidgets__SetCursor(ctx, CLAY_WIDGETS_CURSOR_TEXT);
    }
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

    bool justFocused = focused && ctx->textInputId != id.id;
    if (justFocused) {
        ctx->textInputId = id.id;
        ctx->textCursor = length;
        ctx->textSelectionAnchor = length;
        ctx->textScrollX = 0.0f;
        ctx->textPreferredX = -1.0f;
        ctx->textPointerSelecting = false;
        ctx->caretBlinkTime = 0.0f;
    }
    if (ctx->textInputId == id.id) {
        ClayWidgets__ClampSelectionToLength(ctx, length);
        while (ctx->textCursor > 0 && ctx->textCursor < length && ClayWidgets__IsUtf8ContinuationByte((unsigned char)buffer[ctx->textCursor])) --ctx->textCursor;
        while (ctx->textSelectionAnchor > 0 && ctx->textSelectionAnchor < length && ClayWidgets__IsUtf8ContinuationByte((unsigned char)buffer[ctx->textSelectionAnchor])) --ctx->textSelectionAnchor;
    }
    int32_t prevCursor = ctx->textCursor;
    int32_t prevAnchor = ctx->textSelectionAnchor;

    // Last frame's geometry and retained scroll offsets, for pointer hits and
    // caret-follow. One frame of lag, like all pointer queries in this
    // library - a resize re-wraps on the next frame.
    Clay_ElementData contentData = Clay_GetElementData(contentId);
    Clay_ScrollContainerData scrollData = Clay_GetScrollContainerData(contentId);
    float scrollOffsetX = (scrollData.found && scrollData.scrollPosition) ? scrollData.scrollPosition->x : 0.0f;
    float scrollOffsetY = (scrollData.found && scrollData.scrollPosition) ? scrollData.scrollPosition->y : 0.0f;

    // The soft-wrap width. noWrap - and the first frame, before geometry
    // exists - uses the effectively-infinite width, making every hard line a
    // single visual row.
    float wrapWidth = CLAY_WIDGETS__NO_WRAP_WIDTH;
    if (!options.noWrap && contentData.found && contentData.boundingBox.width > lineHeight) {
        wrapWidth = contentData.boundingBox.width - caretPad;
    }

    // The content clip is a real Clay scroll container, so a wheel over it is
    // consumed whenever it can scroll; when the content fits, let the wheel
    // fall through to an enclosing scroll panel instead of being swallowed.
    bool canScrollVertically = scrollData.found
        && scrollData.contentDimensions.height > scrollData.scrollContainerDimensions.height + 0.5f;
    if (!canScrollVertically) {
        ClayWidgets__RegisterWheelFallthrough(ctx, contentId, over);
    }

    if (focused && contentData.found) {
        // Clicks on the scrollbar drag the thumb (scroll-bar.h); they must not
        // also re-place the caret underneath it.
        bool overScrollBar = Clay_PointerOver(scrollTrackId);
        if (ctx->input.pointerPressed && over && !overScrollBar) {
            int32_t hit = ClayWidgets__TextAreaHit(ctx, buffer, length,
                ctx->input.mouseX, ctx->input.mouseY, contentData.boundingBox,
                scrollOffsetX, scrollOffsetY, lineHeight, wrapWidth, fontId, fontSize, letterSpacing);
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
                ctx->textPreferredX = -1.0f;
                ctx->textPointerSelecting = false;
                ctx->caretBlinkTime = 0.0f;
            } else {
                ClayWidgets__MoveCaret(ctx, hit, ctx->input.shiftDown);
                ctx->textPointerSelecting = true;
            }
        } else if (ctx->input.pointerDown && ctx->textPointerSelecting) {
            int32_t hit = ClayWidgets__TextAreaHit(ctx, buffer, length,
                ctx->input.mouseX, ctx->input.mouseY, contentData.boundingBox,
                scrollOffsetX, scrollOffsetY, lineHeight, wrapWidth, fontId, fontSize, letterSpacing);
            ClayWidgets__MoveCaret(ctx, hit, true);
        }
    }
    if (ctx->input.pointerReleased) {
        ctx->textPointerSelecting = false;
    }

    ClayWidgets__EditTransaction transaction = {0};
    if (focused) {
        transaction = ClayWidgets__BeginEdit(ctx, id, buffer, &length, capacity, options.readOnly,
            options.history, options.validate, options.result);
        bool extend = ctx->input.shiftDown;
        bool hadSelection = ClayWidgets__HasSelection(ctx);
        int32_t selStart = 0;
        int32_t selEnd = 0;
        ClayWidgets__SelectionRange(ctx, &selStart, &selEnd);

        if (ctx->input.keySelectAll && length > 0) {
            ctx->textSelectionAnchor = 0;
            ctx->textCursor = length;
            ctx->textPreferredX = -1.0f;
            ctx->caretBlinkTime = 0.0f;
        }

        if (ctx->input.keyLeft) {
            int32_t target = (hadSelection && !extend)
                ? selStart
                : (ctx->input.controlDown ? ClayWidgets__WordLeft(buffer, ctx->textCursor) : ClayWidgets__Utf8PrevBoundary(buffer, ctx->textCursor));
            ClayWidgets__MoveCaret(ctx, target, extend);
        }

        if (ctx->input.keyRight) {
            int32_t target = (hadSelection && !extend)
                ? selEnd
                : (ctx->input.controlDown ? ClayWidgets__WordRight(buffer, length, ctx->textCursor) : ClayWidgets__Utf8NextBoundary(buffer, length, ctx->textCursor));
            ClayWidgets__MoveCaret(ctx, target, extend);
        }

        // Up/Down move by visual row (a wrapped hard line is several rows),
        // keeping the caret's preferred horizontal position: stepping through
        // a short row and back into a long one returns the caret to its
        // original column, like every editor. MoveCaret clears the preferred
        // X (any horizontal motion invalidates it), so it is re-stamped after
        // the move.
        if (ctx->input.keyUp || ctx->input.keyDown) {
            int32_t lineStart = ClayWidgets__LineStart(buffer, ctx->textCursor);
            int32_t lineEnd = ClayWidgets__LineEnd(buffer, length, ctx->textCursor);

            // The caret's row within its hard line, tracking the previous
            // row's start for Up.
            int32_t rowStart = lineStart;
            int32_t prevRowStart = -1;
            int32_t rowEnd;
            for (;;) {
                rowEnd = ClayWidgets__NextRowBreak(ctx, buffer, lineEnd, rowStart, wrapWidth, fontId, fontSize, letterSpacing);
                if (ctx->textCursor < rowEnd || rowEnd >= lineEnd) {
                    break;
                }
                prevRowStart = rowStart;
                rowStart = rowEnd;
            }

            float caretX = ctx->textPreferredX >= 0.0f
                ? ctx->textPreferredX
                : ClayWidgets__MeasureWidth(ctx, buffer + rowStart, ctx->textCursor - rowStart, fontId, fontSize, letterSpacing);
            int32_t target;
            bool jumpedToBoundary = false; // Up on the first row / Down on the last
            if (ctx->input.keyUp) {
                if (prevRowStart >= 0) {
                    // Previous row of the same hard line.
                    target = prevRowStart + ClayWidgets__FindCursorFromLocalX(
                        ctx, buffer + prevRowStart, rowStart - prevRowStart, caretX, fontId, fontSize, letterSpacing);
                    target = ClayWidgets__AvoidWrapBreak(buffer, target, prevRowStart, rowStart, lineEnd);
                } else if (lineStart == 0) {
                    target = 0; // no row above: go to the very start
                    jumpedToBoundary = true;
                } else {
                    // Last row of the previous hard line.
                    int32_t prevLineEnd = lineStart - 1;
                    int32_t prevRow = ClayWidgets__LineStart(buffer, prevLineEnd);
                    for (;;) {
                        int32_t prevRowEnd = ClayWidgets__NextRowBreak(ctx, buffer, prevLineEnd, prevRow, wrapWidth, fontId, fontSize, letterSpacing);
                        if (prevRowEnd >= prevLineEnd) {
                            break;
                        }
                        prevRow = prevRowEnd;
                    }
                    target = prevRow + ClayWidgets__FindCursorFromLocalX(
                        ctx, buffer + prevRow, prevLineEnd - prevRow, caretX, fontId, fontSize, letterSpacing);
                }
            } else {
                if (rowEnd < lineEnd) {
                    // Next row of the same hard line.
                    int32_t nextRowEnd = ClayWidgets__NextRowBreak(ctx, buffer, lineEnd, rowEnd, wrapWidth, fontId, fontSize, letterSpacing);
                    target = rowEnd + ClayWidgets__FindCursorFromLocalX(
                        ctx, buffer + rowEnd, nextRowEnd - rowEnd, caretX, fontId, fontSize, letterSpacing);
                    target = ClayWidgets__AvoidWrapBreak(buffer, target, rowEnd, nextRowEnd, lineEnd);
                } else if (lineEnd >= length) {
                    target = length; // no row below: go to the very end
                    jumpedToBoundary = true;
                } else {
                    // First row of the next hard line.
                    int32_t nextStart = lineEnd + 1;
                    int32_t nextLineEnd = ClayWidgets__LineEnd(buffer, length, nextStart);
                    int32_t nextRowEnd = ClayWidgets__NextRowBreak(ctx, buffer, nextLineEnd, nextStart, wrapWidth, fontId, fontSize, letterSpacing);
                    target = nextStart + ClayWidgets__FindCursorFromLocalX(
                        ctx, buffer + nextStart, nextRowEnd - nextStart, caretX, fontId, fontSize, letterSpacing);
                    target = ClayWidgets__AvoidWrapBreak(buffer, target, nextStart, nextRowEnd, nextLineEnd);
                }
            }
            ClayWidgets__MoveCaret(ctx, target, extend);
            // The boundary jump is a horizontal move (the caret lands at the
            // document start/end column), so it forfeits the remembered
            // column - matching mainstream editors.
            if (!jumpedToBoundary) {
                ctx->textPreferredX = caretX;
            }
        }

        if (ctx->input.keyHome) {
            ClayWidgets__MoveCaret(ctx, (ctx->input.controlDown ? 0 : ClayWidgets__LineStart(buffer, ctx->textCursor)), extend);
        }

        if (ctx->input.keyEnd) {
            ClayWidgets__MoveCaret(ctx, (ctx->input.controlDown ? length : ClayWidgets__LineEnd(buffer, length, ctx->textCursor)), extend);
        }

        if (ctx->input.keyBackspace) {
            if (ClayWidgets__DeleteSelection(buffer, &length, ctx)) {
                changed = true;
            } else if (ctx->textCursor > 0) {
                int32_t from = ctx->input.controlDown ? ClayWidgets__WordLeft(buffer, ctx->textCursor) : ClayWidgets__Utf8PrevBoundary(buffer, ctx->textCursor);
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
                int32_t to = ctx->input.controlDown ? ClayWidgets__WordRight(buffer, length, ctx->textCursor) : ClayWidgets__Utf8NextBoundary(buffer, length, ctx->textCursor);
                memmove(buffer + ctx->textCursor, buffer + to, (size_t)(length - to + 1));
                length -= (to - ctx->textCursor);
                ctx->caretBlinkTime = 0.0f;
                changed = true;
            }
        }

        if ((ctx->input.keyCopy || ctx->input.keyCut) && ClayWidgets__HasSelection(ctx)) {
            int32_t copyStart = 0;
            int32_t copyEnd = 0;
            ClayWidgets__SelectionRange(ctx, &copyStart, &copyEnd);
            bool copied = ClayWidgets__CopyToClipboard(ctx, buffer + copyStart, copyEnd - copyStart);
            if (copied && ctx->input.keyCut && ClayWidgets__DeleteSelection(buffer, &length, ctx)) {
                changed = true;
            }
        }

        // Paste keeps newlines (unlike the single-line field) and normalizes
        // line endings to LF: the runs between '\r' bytes are inserted as-is
        // (a following '\n' arrives with the next run, so CRLF collapses to
        // LF), and a lone '\r' is replaced by an inserted '\n'.
        if (ctx->input.keyPaste && ctx->getClipboardText) {
            const char *clip = ctx->getClipboardText(ctx->clipboardUserData);
            if (clip) {
                int32_t clipLength = ClayWidgets__StrLenBounded(clip, capacity - 1);
                int32_t runStart = 0;
                for (int32_t i = 0; i <= clipLength; ++i) {
                    if (i == clipLength || clip[i] == '\r') {
                        if (ClayWidgets__InsertTextAtCaret(ctx, buffer, &length, capacity, clip + runStart, i - runStart)) {
                            changed = true;
                        }
                        bool loneCarriageReturn = i < clipLength && (i + 1 >= clipLength || clip[i + 1] != '\n');
                        if (loneCarriageReturn && ClayWidgets__InsertTextAtCaret(ctx, buffer, &length, capacity, "\n", 1)) {
                            changed = true;
                        }
                        runStart = i + 1;
                    }
                }
            }
        }

        if (ctx->input.textUtf8 && ctx->input.textUtf8Length > 0) {
            if (ClayWidgets__InsertTextAtCaret(ctx, buffer, &length, capacity, ctx->input.textUtf8, ctx->input.textUtf8Length)) {
                changed = true;
            }
        }

        if (ctx->input.keyEnter && !ctx->input.controlDown) {
            if (ClayWidgets__InsertTextAtCaret(ctx, buffer, &length, capacity, "\n", 1)) {
                changed = true;
            }
        }

        changed = ClayWidgets__EndEdit(ctx, buffer, &length, changed, options.history, options.validate,
            options.validationUserData, options.result, transaction, true, options.readOnly);

        if (ctx->input.keyEscape) {
            ctx->focusedId = 0;
            ctx->textPointerSelecting = false;
            focused = false;
        }
    }

    // Scroll the caret into view whenever it moved or the text changed - and
    // only then, so wheel-scrolling away from the caret isn't fought. Uses
    // last frame's container geometry; a large jump settles over two frames.
    bool caretMoved = focused
        && (justFocused || changed || ctx->textCursor != prevCursor || ctx->textSelectionAnchor != prevAnchor);
    if (caretMoved && scrollData.found && scrollData.scrollPosition) {
        float viewWidth = scrollData.scrollContainerDimensions.width;
        float viewHeight = scrollData.scrollContainerDimensions.height;
        int32_t caretRowStart = 0;
        int32_t caretRow = ClayWidgets__RowIndexForOffset(ctx, buffer, length, ctx->textCursor,
            wrapWidth, fontId, fontSize, letterSpacing, &caretRowStart);
        float caretX = ClayWidgets__MeasureWidth(ctx, buffer + caretRowStart, ctx->textCursor - caretRowStart, fontId, fontSize, letterSpacing);
        float caretTop = (float)caretRow * lineHeight;

        if (viewHeight > 0.0f) {
            float y = scrollData.scrollPosition->y;
            if (caretTop < -y) {
                y = -caretTop;
            } else if (caretTop + lineHeight > -y + viewHeight) {
                y = -(caretTop + lineHeight - viewHeight);
            }
            scrollData.scrollPosition->y = ClayWidgets__ClampF32(y, -1.0e9f, 0.0f);
        }
        if (viewWidth > 0.0f) {
            float x = scrollData.scrollPosition->x;
            if (caretX < -x + caretPad) {
                x = -(caretX - caretPad);
            } else if (caretX > -x + viewWidth - caretPad) {
                x = -(caretX - viewWidth + caretPad);
            }
            scrollData.scrollPosition->x = ClayWidgets__ClampF32(x, -1.0e9f, 0.0f);
        }
    }

    float fieldHeight = options.height > 0.0f
        ? options.height
        : lineHeight * 5.0f + (float)(ctx->theme.spacing.sm * 2);

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

        // Outer field owns the background, rounding and border; the clip lives
        // on the inner content element for the same border-shaving reason as
        // the single-line input.
        CLAY(fieldId, {
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_GROW(0),
                    .height = CLAY_SIZING_FIXED(fieldHeight),
                },
                .padding = {
                    .left = ctx->theme.spacing.md,
                    .right = ctx->theme.spacing.md,
                    .top = ctx->theme.spacing.sm,
                    .bottom = ctx->theme.spacing.sm,
                },
            },
            .backgroundColor = options.disabled
                ? ClayWidgets__MixColor(ctx->theme.surfaceAltColor, ctx->theme.surfaceColor, ctx->theme.disabledMix)
                : (focused ? ctx->theme.hoverColor : ctx->theme.surfaceAltColor),
            .cornerRadius = CLAY_CORNER_RADIUS((float)ctx->theme.radiusMd),
            .border = {
                .color = focused ? ctx->theme.focusRingColor : ctx->theme.borderColor,
                .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
            },
        }) {
            // The content element is a genuine Clay scroll container on both
            // axes; the scroll-aware Begin stamps clip.childOffset from its
            // own retained offset after opening.
            ClayWidgets__BeginScrollElement(contentId, CLAY__INIT(Clay_ElementDeclaration){
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                },
                .clip = { .horizontal = true, .vertical = true },
            });

            int32_t selStart = 0;
            int32_t selEnd = 0;
            bool showSelection = focused && ClayWidgets__HasSelection(ctx) && length > 0;
            if (showSelection) {
                ClayWidgets__SelectionRange(ctx, &selStart, &selEnd);
            }
            bool showCaret = focused && ((int32_t)(ctx->caretBlinkTime * 2.0f) % 2 == 0);
            const int16_t overlayZ = ClayWidgets__OverlayZ(ctx, 1);

            // A floating overlay is scissored to a single clip rectangle in
            // Clay - the content clip's box - so an enclosing scroll panel's
            // clip does NOT apply to it. When the field straddles a panel
            // edge, clamp the selection and caret overlays vertically to the
            // panel's box by hand so they can't paint past an edge the field
            // itself is clipped to. (The scrollbar handles the same problem
            // with CLAY_CLIP_TO_ATTACHED_PARENT; see scroll-bar.h.)
            float overlayClampMinY = -3.4e38f;
            float overlayClampMaxY = 3.4e38f;
            if (ctx->scrollPanelDepth > 0 && ctx->scrollPanelDepth <= CLAY_WIDGETS_MAX_SCROLL_NESTING) {
                Clay_ElementId panelClipId = CLAY__INIT(Clay_ElementId) CLAY__DEFAULT_STRUCT;
                panelClipId.id = ctx->scrollPanelStack[ctx->scrollPanelDepth - 1];
                Clay_ElementData panelClipData = Clay_GetElementData(panelClipId);
                if (panelClipData.found) {
                    overlayClampMinY = panelClipData.boundingBox.y;
                    overlayClampMaxY = panelClipData.boundingBox.y + panelClipData.boundingBox.height;
                }
            }

            int32_t lineStart = 0;
            int32_t globalRow = 0;
            for (;;) {
                int32_t lineEnd = ClayWidgets__LineEnd(buffer, length, lineStart);
                int32_t rowStart = lineStart;
                for (;;) {
                    int32_t rowEnd = ClayWidgets__NextRowBreak(ctx, buffer, lineEnd, rowStart, wrapWidth, fontId, fontSize, letterSpacing);
                    bool lastRowOfLine = rowEnd >= lineEnd;
                    // A caret at a wrap break belongs to the following row;
                    // one at the hard line end belongs to the line's last row.
                    bool caretOnRow = showCaret
                        && ctx->textCursor >= rowStart
                        && (ctx->textCursor < rowEnd || (lastRowOfLine && ctx->textCursor == rowEnd));

                    // This row's overlay slice after the panel clamp: the y
                    // offset within the row and the clamped height. Uses last
                    // frame's content origin, like all geometry queries here.
                    float overlayTop = 0.0f;
                    float overlayHeight = lineHeight;
                    if (contentData.found) {
                        float rowTop = contentData.boundingBox.y + scrollOffsetY + (float)globalRow * lineHeight;
                        float visibleTop = rowTop < overlayClampMinY ? overlayClampMinY : rowTop;
                        float visibleBottom = (rowTop + lineHeight) > overlayClampMaxY ? overlayClampMaxY : (rowTop + lineHeight);
                        overlayTop = visibleTop - rowTop;
                        overlayHeight = visibleBottom - visibleTop;
                    }
                    bool overlayVisible = overlayHeight > 0.0f;

                    CLAY_AUTO_ID({
                        .layout = {
                            .sizing = { .width = CLAY_SIZING_FIT(0, 0), .height = CLAY_SIZING_FIXED(lineHeight) },
                        },
                    }) {
                        // Selection first so the text paints over it. Rendered
                        // per row: partial on the boundary rows, full otherwise.
                        // A selected empty row has no visible rect (zero width).
                        if (overlayVisible && showSelection && selEnd > rowStart && selStart <= rowEnd) {
                            int32_t from = ClayWidgets__MaxI32(selStart, rowStart);
                            int32_t to = ClayWidgets__MinI32(selEnd, rowEnd);
                            float fromX = ClayWidgets__MeasureWidth(ctx, buffer + rowStart, from - rowStart, fontId, fontSize, letterSpacing);
                            float toX = ClayWidgets__MeasureWidth(ctx, buffer + rowStart, to - rowStart, fontId, fontSize, letterSpacing);
                            if (toX > fromX) {
                                CLAY_AUTO_ID({
                                    .layout = {
                                        .sizing = {
                                            .width = CLAY_SIZING_FIXED(toX - fromX),
                                            .height = CLAY_SIZING_FIXED(overlayHeight),
                                        },
                                    },
                                    .backgroundColor = ctx->theme.accentMutedColor,
                                    .cornerRadius = CLAY_CORNER_RADIUS(3),
                                    .floating = {
                                        .offset = { fromX, overlayTop },
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

                        if (rowEnd > rowStart) {
                            Clay_String rowText = {0};
                            rowText.chars = buffer + rowStart;
                            rowText.length = rowEnd - rowStart;
                            rowText.isStaticallyAllocated = false;
                            CLAY_TEXT(rowText, {
                                .textColor = options.disabled ? ctx->theme.textMutedColor : ctx->theme.textColor,
                                .fontId = fontId,
                                .fontSize = fontSize,
                                .wrapMode = CLAY_TEXT_WRAP_NONE,
                            });
                        } else if (length == 0 && options.placeholder) {
                            CLAY_TEXT(ClayWidgets__StringFromCString(options.placeholder), {
                                .textColor = ctx->theme.textMutedColor,
                                .fontId = fontId,
                                .fontSize = fontSize,
                                .wrapMode = CLAY_TEXT_WRAP_NONE,
                            });
                        }

                        if (caretOnRow && overlayVisible) {
                            float caretX = ClayWidgets__MeasureWidth(ctx, buffer + rowStart, ctx->textCursor - rowStart, fontId, fontSize, letterSpacing);
                            CLAY_AUTO_ID({
                                .layout = {
                                    .sizing = {
                                        .width = CLAY_SIZING_FIXED(1),
                                        .height = CLAY_SIZING_FIXED(overlayHeight),
                                    },
                                },
                                .backgroundColor = ctx->theme.textColor,
                                .floating = {
                                    .offset = { caretX, overlayTop },
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

                    globalRow++;
                    if (lastRowOfLine) {
                        break;
                    }
                    rowStart = rowEnd;
                }

                if (lineEnd >= length) {
                    break;
                }
                lineStart = lineEnd + 1;
            }

            ClayWidgets__EndElement(); // content clip

            ClayWidgets_ScrollBar(ctx, contentId);
        }
    }

    ClayWidgets_SemanticNode semantic = {0};
    semantic.id = id; semantic.role = CLAY_WIDGETS_ROLE_TEXT_FIELD; semantic.label = label;
    semantic.value = (Clay_String){ .length = length, .chars = buffer };
    semantic.disabled = options.disabled; semantic.readOnly = options.readOnly;
    ClayWidgets_Semantic(ctx, semantic);
    if (focused && !options.readOnly) {
        int32_t rowStart = 0;
        int32_t row = ClayWidgets__RowIndexForOffset(ctx,buffer,length,ctx->textCursor,wrapWidth,fontId,fontSize,letterSpacing,&rowStart);
        float x = ClayWidgets__MeasureWidth(ctx,buffer+rowStart,ctx->textCursor-rowStart,fontId,fontSize,letterSpacing);
        float y = row * lineHeight;
        if (scrollData.found && scrollData.scrollPosition) { x += scrollData.scrollPosition->x; y += scrollData.scrollPosition->y; }
        ClayWidgets__Composition(ctx,contentId,x,y,lineHeight);
    }
    return changed;
}

#endif

#endif
