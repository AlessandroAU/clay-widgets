#ifndef CLAY_WIDGETS_TEXT_EDIT_H
#define CLAY_WIDGETS_TEXT_EDIT_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before text-edit.h"
#endif

// Adapter callbacks execute during layout. Strings are borrowed for that call.
void ClayWidgets_SetSemanticFunction(ClayWidgets_Context *ctx, ClayWidgets_SemanticFunction fn, void *userData);
void ClayWidgets_SetImeFunction(ClayWidgets_Context *ctx, ClayWidgets_ImeFunction fn, void *userData);
void ClayWidgets_RequestFocus(ClayWidgets_Context *ctx, Clay_ElementId id);
void ClayWidgets_Semantic(ClayWidgets_Context *ctx, ClayWidgets_SemanticNode node);

#ifdef CLAY_WIDGETS_IMPLEMENTATION
#include <stdlib.h>

void ClayWidgets_SetSemanticFunction(ClayWidgets_Context *ctx, ClayWidgets_SemanticFunction fn, void *data) {
    if (ctx) { ctx->semantic = fn; ctx->semanticUserData = data; }
}
void ClayWidgets_SetImeFunction(ClayWidgets_Context *ctx, ClayWidgets_ImeFunction fn, void *data) {
    if (ctx) { ctx->ime = fn; ctx->imeUserData = data; }
}
void ClayWidgets_RequestFocus(ClayWidgets_Context *ctx, Clay_ElementId id) {
    if (ctx) ctx->requestedFocusId = id.id;
}
void ClayWidgets_Semantic(ClayWidgets_Context *ctx, ClayWidgets_SemanticNode node) {
    if (!ctx || !ctx->semantic) return;
    node.bounds = Clay_GetElementData(node.id).boundingBox;
    node.parentId = ctx->currentModalId;
    node.focused = ctx->focusedId == node.id.id;
    node.disabled = node.disabled || ctx->disabledDepth > 0;
    ctx->semantic(&node, ctx->semanticUserData);
}

static int32_t ClayWidgets__WordLeft(const char *text, int32_t cursor) {
    while (cursor > 0 && !ClayWidgets__IsWordByte((unsigned char)text[ClayWidgets__Utf8PrevBoundary(text, cursor)])) cursor = ClayWidgets__Utf8PrevBoundary(text, cursor);
    while (cursor > 0 && ClayWidgets__IsWordByte((unsigned char)text[ClayWidgets__Utf8PrevBoundary(text, cursor)])) cursor = ClayWidgets__Utf8PrevBoundary(text, cursor);
    return cursor;
}
static int32_t ClayWidgets__WordRight(const char *text, int32_t length, int32_t cursor) {
    while (cursor < length && ClayWidgets__IsWordByte((unsigned char)text[cursor])) cursor = ClayWidgets__Utf8NextBoundary(text, length, cursor);
    while (cursor < length && !ClayWidgets__IsWordByte((unsigned char)text[cursor])) cursor = ClayWidgets__Utf8NextBoundary(text, length, cursor);
    return cursor;
}

typedef struct ClayWidgets__EditTransaction {
    ClayWidgets_Input input;
    char *before;
    int32_t cursor, anchor;
    bool historyMoved;
    int32_t historyPosition;
} ClayWidgets__EditTransaction;

static void ClayWidgets__StoreSnapshot(ClayWidgets_TextSnapshot *snapshot, ClayWidgets_Context *ctx, const char *text) {
    strcpy(snapshot->text, text);
    snapshot->cursor = ctx->textCursor;
    snapshot->anchor = ctx->textSelectionAnchor;
}

static ClayWidgets__EditTransaction ClayWidgets__BeginEdit(ClayWidgets_Context *ctx, Clay_ElementId id,
    char *buffer, int32_t *length, int32_t capacity, bool readOnly, ClayWidgets_TextHistory *history,
    ClayWidgets_ValidateTextFunction validate, ClayWidgets_EditResult *result) {
    ClayWidgets__EditTransaction tx = {0};
    tx.input = ctx->input; tx.cursor = ctx->textCursor; tx.anchor = ctx->textSelectionAnchor;
    if (result) memset(result, 0, sizeof(*result));
    ctx->clipboardFailed = false;
    if (validate && !readOnly) {
        tx.before = (char *)malloc((size_t)*length + 1);
        if (tx.before) memcpy(tx.before, buffer, (size_t)*length + 1);
        else { readOnly = true; if (result) result->rejected = true; }
    }
    if (history && *length < CLAY_WIDGETS_HISTORY_BYTES) {
        if (history->id != id.id || history->count <= 0 || history->count > CLAY_WIDGETS_HISTORY_DEPTH
            || history->position < 0 || history->position >= history->count
            || strcmp(history->entries[history->position].text, buffer)) {
            history->id = id.id; history->count = 1; history->position = 0;
            ClayWidgets__StoreSnapshot(&history->entries[0], ctx, buffer);
        }
        ClayWidgets__StoreSnapshot(&history->entries[history->position], ctx, buffer);
        tx.historyPosition = history->position;
        if (!readOnly && ctx->input.compositionLength <= 0 && (ctx->input.keyUndo || ctx->input.keyRedo)) {
            int32_t pos = history->position + (ctx->input.keyUndo ? -1 : 1);
            if (pos >= 0 && pos < history->count && (int32_t)strlen(history->entries[pos].text) < capacity) {
                history->position = pos; tx.historyMoved = true;
                strcpy(buffer, history->entries[pos].text); *length = (int32_t)strlen(buffer);
                ClayWidgets__MoveCaret(ctx, history->entries[pos].cursor, false);
                ctx->textSelectionAnchor = history->entries[pos].anchor;
            }
        }
    } else if (history) {
        history->count = 0;
        if (result) result->historyUnavailable = true;
    }
    if (readOnly || tx.historyMoved || ctx->input.compositionLength > 0) {
        ctx->input.keyBackspace = ctx->input.keyDelete = ctx->input.keyCut = ctx->input.keyPaste = false;
        ctx->input.keyEnter = false;
        ctx->input.textUtf8Length = 0;
    }
    return tx;
}

static bool ClayWidgets__EndEdit(ClayWidgets_Context *ctx, char *buffer, int32_t *length, bool changed,
    ClayWidgets_TextHistory *history, ClayWidgets_ValidateTextFunction validate, void *data,
    ClayWidgets_EditResult *result, ClayWidgets__EditTransaction tx, bool multiline, bool readOnly) {
    changed = changed || tx.historyMoved;
    if (changed && validate && tx.before && !validate(buffer, *length, data)) {
        strcpy(buffer, tx.before); *length = (int32_t)strlen(buffer);
        ctx->textCursor = tx.cursor; ctx->textSelectionAnchor = tx.anchor;
        if (history && tx.historyMoved) history->position = tx.historyPosition;
        changed = false;
        if (result) result->rejected = true;
    }
    free(tx.before);
    if (changed && history && !tx.historyMoved) {
        if (*length < CLAY_WIDGETS_HISTORY_BYTES && history->count > 0) {
            history->count = history->position + 1;
            if (history->count == CLAY_WIDGETS_HISTORY_DEPTH) {
                memmove(history->entries, history->entries + 1, sizeof(history->entries[0]) * (CLAY_WIDGETS_HISTORY_DEPTH - 1));
                history->count--;
            }
            history->position = history->count++;
            ClayWidgets__StoreSnapshot(&history->entries[history->position], ctx, buffer);
        } else { history->count = 0; if (result) result->historyUnavailable = true; }
    }
    if (result) {
        result->changed = changed;
        result->clipboardFailed = ctx->clipboardFailed;
        result->submitted = !readOnly && tx.input.compositionLength <= 0 && tx.input.keyEnter && (!multiline || tx.input.controlDown);
        result->cancelled = tx.input.keyEscape;
    }
    ctx->input = tx.input;
    return changed;
}

static void ClayWidgets__Composition(ClayWidgets_Context *ctx, Clay_ElementId anchor, float x, float y, float height) {
    Clay_BoundingBox caret = Clay_GetElementData(anchor).boundingBox;
    caret.x += x; caret.y += y; caret.width = 1; caret.height = height;
    if (ctx->ime) ctx->ime(caret, ctx->input.compositionUtf8, ctx->input.compositionLength, ctx->input.compositionCursor, ctx->imeUserData);
    if (!ctx->input.compositionUtf8 || ctx->input.compositionLength <= 0) return;
    Clay_String preedit = { .length = ctx->input.compositionLength, .chars = ctx->input.compositionUtf8 };
    CLAY_AUTO_ID({ .layout={ .sizing={.width=CLAY_SIZING_FIT(0),.height=CLAY_SIZING_FIXED(height)} },
        .backgroundColor=ctx->theme.surfaceColor,
        .floating={ .offset={x,y},.parentId=anchor.id,.zIndex=ClayWidgets__OverlayZ(ctx,2),
            .attachPoints={.element=CLAY_ATTACH_POINT_LEFT_TOP,.parent=CLAY_ATTACH_POINT_LEFT_TOP},
            .pointerCaptureMode=CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
            .attachTo=CLAY_ATTACH_TO_ELEMENT_WITH_ID,.clipTo=CLAY_CLIP_TO_ATTACHED_PARENT },
        .border={ .color=ctx->theme.accentColor,.width={.bottom=1} } }) {
        CLAY_TEXT(preedit,{.textColor=ctx->theme.textColor,.fontId=ctx->theme.fontBody,
            .fontSize=ctx->theme.fontSizeBody,.wrapMode=CLAY_TEXT_WRAP_NONE});
    }
}
#endif
#endif
