#ifndef CLAY_WIDGETS_SPLIT_PANE_H
#define CLAY_WIDGETS_SPLIT_PANE_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before split-pane.h"
#endif

typedef struct ClayWidgets_SplitOptions { Clay_Sizing sizing; bool vertical; float minFirst, minSecond; } ClayWidgets_SplitOptions;
// Begin opens pane one; Next closes it and opens pane two; End closes the pair.
// ratio is the fraction of the container occupied by pane one. IDs must be stable.
bool ClayWidgets_BeginSplit(ClayWidgets_Context *ctx, Clay_ElementId id, float *ratio, ClayWidgets_SplitOptions options);
void ClayWidgets_NextSplit(ClayWidgets_Context *ctx, Clay_ElementId id, bool vertical);
void ClayWidgets_EndSplit(ClayWidgets_Context *ctx);
typedef struct ClayWidgets_ResizableOptions { float minWidth, minHeight, maxWidth, maxHeight; } ClayWidgets_ResizableOptions;
bool ClayWidgets_BeginResizablePanel(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_Dimensions *size, ClayWidgets_ResizableOptions options);
void ClayWidgets_EndResizablePanel(ClayWidgets_Context *ctx);

#ifdef CLAY_WIDGETS_IMPLEMENTATION
bool ClayWidgets_BeginSplit(ClayWidgets_Context *ctx, Clay_ElementId id, float *ratio, ClayWidgets_SplitOptions options) {
    if (!ctx || !ratio) return false;
    Clay_ElementId divider = ClayWidgets__ChildId(id, CLAY_STRING("SplitDivider"),0);
    Clay_ElementData box = Clay_GetElementData(id);
    float extent = options.vertical ? box.boundingBox.height : box.boundingBox.width;
    float low = extent > 6 ? options.minFirst / extent : 0.05f;
    float high = extent > 6 ? 1 - (options.minSecond + 6) / extent : 0.95f;
    low = ClayWidgets__Clamp(low,0,1); high = ClayWidgets__Clamp(high,low,1);
    if (!isfinite(*ratio)) *ratio = 0.5f;
    bool over = !ctx->disabledDepth && Clay_PointerOver(divider);
    bool focused = ClayWidgets__RegisterFocusable(ctx, divider, over);
    if (ctx->input.pointerPressed && over) ctx->activeId = divider.id;
    if (!ctx->disabledDepth && ctx->activeId == divider.id && ctx->input.pointerDown && extent > 6) {
        *ratio = ((options.vertical ? ctx->input.mouseY - box.boundingBox.y : ctx->input.mouseX - box.boundingBox.x) - 3) / extent;
    }
    if (focused) {
        if (ctx->input.keyLeft || ctx->input.keyUp) *ratio -= 0.02f;
        if (ctx->input.keyRight || ctx->input.keyDown) *ratio += 0.02f;
        if (ctx->input.keyHome) *ratio = low;
        if (ctx->input.keyEnd) *ratio = high;
    }
    *ratio = ClayWidgets__Clamp(*ratio,low,high);
    if (over || ctx->activeId == divider.id) ClayWidgets__SetCursor(ctx, options.vertical ? CLAY_WIDGETS_CURSOR_RESIZE_Y : CLAY_WIDGETS_CURSOR_RESIZE_X);
    ClayWidgets__BeginElement(id, (Clay_ElementDeclaration){ .layout = { .sizing = options.sizing,
        .layoutDirection = options.vertical ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT } });
    ClayWidgets__BeginElement(ClayWidgets__ChildId(id,CLAY_STRING("FirstPane"),0), (Clay_ElementDeclaration){ .layout = {
        .sizing = { .width = options.vertical ? CLAY_SIZING_GROW(0) : CLAY_SIZING_PERCENT(*ratio),
            .height = options.vertical ? CLAY_SIZING_PERCENT(*ratio) : CLAY_SIZING_GROW(0) }, .layoutDirection = CLAY_TOP_TO_BOTTOM },
        .clip = { .horizontal = true, .vertical = true } });
    return true;
}
void ClayWidgets_NextSplit(ClayWidgets_Context *ctx, Clay_ElementId id, bool vertical) {
    if (!ctx) return;
    ClayWidgets__EndElement();
    Clay_ElementId divider = ClayWidgets__ChildId(id,CLAY_STRING("SplitDivider"),0);
    CLAY(divider, { .layout = { .sizing = {
        .width = vertical ? CLAY_SIZING_GROW(0) : CLAY_SIZING_FIXED(6),
        .height = vertical ? CLAY_SIZING_FIXED(6) : CLAY_SIZING_GROW(0) } },
        .backgroundColor = ctx->focusedId == divider.id ? ctx->theme.focusRingColor : ctx->theme.borderColor }) {}
    ClayWidgets__BeginElement(ClayWidgets__ChildId(id,CLAY_STRING("SecondPane"),0), (Clay_ElementDeclaration){ .layout = {
        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) }, .layoutDirection = CLAY_TOP_TO_BOTTOM },
        .clip = { .horizontal = true, .vertical = true } });
}
void ClayWidgets_EndSplit(ClayWidgets_Context *ctx) { if (ctx) { ClayWidgets__EndElement(); ClayWidgets__EndElement(); } }

typedef struct ClayWidgets__ResizeState { float x,y,w,h; } ClayWidgets__ResizeState;
bool ClayWidgets_BeginResizablePanel(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_Dimensions *size, ClayWidgets_ResizableOptions options) {
    if (!ctx || !size) return false;
    Clay_ElementId handle = ClayWidgets__ChildId(id,CLAY_STRING("PanelResize"),0);
    ClayWidgets__ResizeState *state = (ClayWidgets__ResizeState *)ClayWidgets_GetState(ctx,handle.id,sizeof(ClayWidgets__ResizeState));
    bool over = !ctx->disabledDepth && Clay_PointerOver(handle);
    bool focused = ClayWidgets__RegisterFocusable(ctx,handle,over);
    if (over) ClayWidgets__SetCursor(ctx,CLAY_WIDGETS_CURSOR_RESIZE_XY);
    if (state && ctx->input.pointerPressed && over) {
        ctx->activeId=handle.id; state->x=ctx->input.mouseX; state->y=ctx->input.mouseY; state->w=size->width; state->h=size->height;
    }
    if (state && !ctx->disabledDepth && ctx->activeId==handle.id && ctx->input.pointerDown) {
        size->width=state->w+ctx->input.mouseX-state->x; size->height=state->h+ctx->input.mouseY-state->y;
        ClayWidgets__SetCursor(ctx,CLAY_WIDGETS_CURSOR_RESIZE_XY);
    }
    if (focused) {
        if (ctx->input.keyLeft) size->width-=10;
        if (ctx->input.keyRight) size->width+=10;
        if (ctx->input.keyUp) size->height-=10;
        if (ctx->input.keyDown) size->height+=10;
    }
    float minW=fmaxf(24,options.minWidth), minH=fmaxf(24,options.minHeight);
    size->width=ClayWidgets__Clamp(isfinite(size->width)?size->width:minW,minW,fmaxf(minW,options.maxWidth>0?options.maxWidth:ctx->layoutDimensions.width));
    size->height=ClayWidgets__Clamp(isfinite(size->height)?size->height:minH,minH,fmaxf(minH,options.maxHeight>0?options.maxHeight:ctx->layoutDimensions.height));
    ClayWidgets__BeginElement(id,(Clay_ElementDeclaration){ .layout = {
        .sizing = { .width = CLAY_SIZING_FIXED(size->width), .height = CLAY_SIZING_FIXED(size->height) },
        .padding = { .right=14,.bottom=14 }, .layoutDirection=CLAY_TOP_TO_BOTTOM },
        .backgroundColor=ctx->theme.surfaceAltColor, .clip={.horizontal=true,.vertical=true} });
    CLAY(handle,{ .layout={ .sizing={.width=CLAY_SIZING_FIXED(14),.height=CLAY_SIZING_FIXED(14)} },
        .backgroundColor=ctx->theme.borderColor,
        .floating={ .parentId=id.id,.zIndex=ClayWidgets__OverlayZ(ctx,20),
            .attachPoints={.element=CLAY_ATTACH_POINT_RIGHT_BOTTOM,.parent=CLAY_ATTACH_POINT_RIGHT_BOTTOM},
            .attachTo=CLAY_ATTACH_TO_ELEMENT_WITH_ID,.clipTo=CLAY_CLIP_TO_ATTACHED_PARENT } }) {}
    return true;
}
void ClayWidgets_EndResizablePanel(ClayWidgets_Context *ctx) { if (ctx) ClayWidgets__EndElement(); }
#endif
#endif
