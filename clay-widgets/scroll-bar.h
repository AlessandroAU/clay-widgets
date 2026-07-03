#ifndef CLAY_WIDGETS_SCROLL_BAR_H
#define CLAY_WIDGETS_SCROLL_BAR_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before scroll-bar.h"
#endif

void ClayWidgets_ScrollBar(
    ClayWidgets_Context *ctx,
    Clay_ElementId scrollContainerId
);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

void ClayWidgets_ScrollBar(
    ClayWidgets_Context *ctx,
    Clay_ElementId scrollContainerId
) {
    if (!ctx) {
        return;
    }

    Clay_ScrollContainerData scrollData = Clay_GetScrollContainerData(scrollContainerId);
    if (!scrollData.found || !scrollData.scrollPosition) {
        return;
    }

    float containerHeight = scrollData.scrollContainerDimensions.height;
    float contentHeight = scrollData.contentDimensions.height;

    if (containerHeight <= 0.0f || contentHeight <= 0.0f) {
        return;
    }

    float scrollableHeight = contentHeight - containerHeight;
    if (scrollableHeight <= 0.0f) {
        return;
    }

    const float trackWidth = 8.0f;
    const float trackPadding = 1.0f;
    float trackInnerHeight = containerHeight - trackPadding * 2.0f;
    if (trackInnerHeight <= 1.0f) {
        return;
    }

    float scrollProgress = ClayWidgets__Clamp((-scrollData.scrollPosition->y) / scrollableHeight, 0.0f, 1.0f);
    float scrollBarHeight = (containerHeight / contentHeight) * trackInnerHeight;
    scrollBarHeight = ClayWidgets__Clamp(scrollBarHeight, 10.0f, trackInnerHeight);
    float thumbTravel = trackInnerHeight - scrollBarHeight;
    float scrollBarY = scrollProgress * thumbTravel;

    Clay_ElementId scrollBarTrackId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsScrollBarTrack"), scrollContainerId.id);
    Clay_ElementId scrollBarId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsScrollBarThumb"), scrollContainerId.id);
    bool overThumb = Clay_PointerOver(scrollBarId);

    if (ctx->input.pointerPressed && overThumb) {
        ctx->activeId = scrollBarId.id;
        ctx->scrollBarDragContainerId = scrollContainerId.id;
        ctx->scrollBarDragStartMouseY = ctx->input.mouseY;
        ctx->scrollBarDragStartScrollY = scrollData.scrollPosition->y;
    }

    bool draggingThumb = ctx->input.pointerDown
        && ctx->activeId == scrollBarId.id
        && ctx->scrollBarDragContainerId == scrollContainerId.id;

    if (draggingThumb) {
        if (thumbTravel > 0.0f) {
            float mouseDeltaY = ctx->input.mouseY - ctx->scrollBarDragStartMouseY;
            float newScrollY = ctx->scrollBarDragStartScrollY - (mouseDeltaY / thumbTravel) * scrollableHeight;
            scrollData.scrollPosition->y = ClayWidgets__Clamp(newScrollY, -scrollableHeight, 0.0f);

            scrollProgress = ClayWidgets__Clamp((-scrollData.scrollPosition->y) / scrollableHeight, 0.0f, 1.0f);
            scrollBarY = scrollProgress * thumbTravel;
        }
    }

    if (ctx->input.pointerReleased && ctx->activeId == scrollBarId.id) {
        ctx->activeId = 0;
        ctx->scrollBarDragContainerId = 0;
        ctx->scrollBarDragStartMouseY = 0.0f;
        ctx->scrollBarDragStartScrollY = 0.0f;
    }

    CLAY(scrollBarTrackId, {
        .layout = {
            .sizing = {
                .width = CLAY_SIZING_FIXED(trackWidth),
                .height = CLAY_SIZING_FIXED(containerHeight),
            },
            .padding = CLAY_PADDING_ALL((uint16_t)trackPadding),
            .childGap = 0,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = ctx->theme.surfaceAltColor,
        .cornerRadius = CLAY_CORNER_RADIUS(4),
        .floating = {
            .offset = { .x = 3.0f, .y = 0.0f },
            .parentId = scrollContainerId.id,
            .zIndex = 100,
            .attachPoints = {
                .element = CLAY_ATTACH_POINT_RIGHT_TOP,
                .parent = CLAY_ATTACH_POINT_RIGHT_TOP,
            },
            .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID,
        },
        .clip = { .horizontal = true, .vertical = true },
        .border = {
            .color = ctx->theme.borderColor,
            .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
        },
    }) {
        if (scrollBarY > 0.0f) {
            CLAY_AUTO_ID({
                .layout = {
                    .sizing = {
                        .width = CLAY_SIZING_GROW(0),
                        .height = CLAY_SIZING_FIXED(scrollBarY),
                    },
                },
            }) {}
        }

        CLAY(scrollBarId, {
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_GROW(0),
                    .height = CLAY_SIZING_FIXED(scrollBarHeight),
                },
            },
            .backgroundColor = draggingThumb ? ctx->theme.accentColor : (overThumb ? ctx->theme.accentMutedColor : ctx->theme.borderColor),
            .cornerRadius = CLAY_CORNER_RADIUS(3),
        }) {}
    }
}

#endif

#endif