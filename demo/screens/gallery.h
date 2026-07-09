// Gallery view: the full widget catalog, grouped into cards.
#ifndef CLAY_WIDGETS_DEMO_GALLERY_H
#define CLAY_WIDGETS_DEMO_GALLERY_H

#include "demo/demo-state.h"

namespace {

static void DrawGalleryView(ClayWidgets_Context &ui, DemoState &s, DemoIcons &icons, bool compactLayout) {
    Clay_SizingAxis leftWidth = compactLayout ? CLAY_SIZING_GROW(0) : CLAY_SIZING_PERCENT(0.55f);
    Clay_SizingAxis leftHeight = compactLayout ? CLAY_SIZING_PERCENT(0.55f) : CLAY_SIZING_GROW(0);
    Clay_SizingAxis rightWidth = compactLayout ? CLAY_SIZING_GROW(0) : CLAY_SIZING_PERCENT(0.45f);

    CLAY(CLAY_ID("GalleryColumns"), {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
            .childGap = 16,
            .layoutDirection = compactLayout ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
        },
    }) {
        ClayWidgets_BeginScrollPanel(&ui, CLAY_ID("GalleryLeft"),
            ClayWidgets_ScrollPanelOptions{ leftWidth, leftHeight, 0, 0, ui.theme.spacing.md });
        {
            ClayWidgets_BeginCard(&ui, CLAY_ID("ButtonsCard"), CLAY_STRING("Buttons"));
            {
                MutedLabel(ui, CLAY_STRING("Four variants: default, primary (accent), danger, and disabled."));
                CLAY(CLAY_ID("GalleryButtonRow"), {
                    .layout = {
                        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                        .childGap = ui.theme.spacing.sm,
                        .layoutDirection = compactLayout ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                    },
                }) {
                    if (ClayWidgets_Button(&ui, CLAY_ID("GalleryDefault"), CLAY_STRING("Default"))) {
                        s.galleryClicks++;
                        SetStatus(s, "Default button clicked");
                    }
                    if (ClayWidgets_ButtonEx(&ui, CLAY_ID("GalleryPrimary"), CLAY_STRING("Primary"),
                            ClayWidgets_ButtonOptions{CLAY_WIDGETS_BUTTON_PRIMARY, false})) {
                        s.galleryClicks++;
                        SetStatus(s, "Primary button clicked");
                    }
                    if (ClayWidgets_ButtonEx(&ui, CLAY_ID("GalleryDanger"), CLAY_STRING("Danger"),
                            ClayWidgets_ButtonOptions{CLAY_WIDGETS_BUTTON_DANGER, false})) {
                        s.galleryClicks++;
                        SetStatus(s, "Danger button clicked");
                    }
                    ClayWidgets_ButtonEx(&ui, CLAY_ID("GalleryDisabled"), CLAY_STRING("Disabled"),
                        ClayWidgets_ButtonOptions{CLAY_WIDGETS_BUTTON_DEFAULT, true});
                }
                ClayWidgets_Tooltip(&ui, CLAY_ID("GalleryPrimary"), CLAY_STRING("Accent-filled call to action"));
                ClayWidgets_Tooltip(&ui, CLAY_ID("GalleryDanger"), CLAY_STRING("For destructive actions"));
                MutedLabel(ui, FormatString(s, "Clicked %d time(s)", s.galleryClicks));
            }
            ClayWidgets_EndCard(&ui, CLAY_ID("ButtonsCard"));

            ClayWidgets_BeginCard(&ui, CLAY_ID("TextChoiceCard"), CLAY_STRING("Text & choice"));
            {
                ClayWidgets_TextInput(&ui, CLAY_ID("GalleryInput"), CLAY_STRING("Text input"),
                    s.galleryText, static_cast<int32_t>(sizeof(s.galleryText)),
                    ClayWidgets_TextInputOptions{"Selection, word jumps, Ctrl+A", false});
                ClayWidgets_Combo(&ui, CLAY_ID("GalleryCombo"), CLAY_STRING("Combo box"),
                    kBuildConfigNames, 4, &s.buildConfig);
                ClayWidgets_Label(&ui, CLAY_STRING("List box (click or focus + arrows)"));
                ClayWidgets_ListBox(&ui, CLAY_ID("GalleryListBox"), kChannelNames, 4, &s.channel);
            }
            ClayWidgets_EndCard(&ui, CLAY_ID("TextChoiceCard"));

            ClayWidgets_BeginCard(&ui, CLAY_ID("RangesCard"), CLAY_STRING("Ranges & numbers"));
            {
                ClayWidgets_Label(&ui, CLAY_STRING("Slider + progress bar"));
                s.volume = ClayWidgets_Slider(&ui, CLAY_ID("GalleryVolume"), s.volume,
                    ClayWidgets_SliderOptions{0.0f, 1.0f, 0.01f, true, 0});
                ClayWidgets_ProgressBar(&ui, CLAY_ID("GalleryVolumeBar"), s.volume,
                    FormatString(s, "Volume %d%%", static_cast<int>(std::lround(s.volume * 100.0f))));

                ClayWidgets_Label(&ui, CLAY_STRING("Stepper"));
                ClayWidgets_Stepper(&ui, CLAY_ID("GalleryRetry"), &s.retryCount,
                    ClayWidgets_StepperOptions{0, 10, 1});

                ClayWidgets_Label(&ui, CLAY_STRING("Segmented control"));
                ClayWidgets_Segmented(&ui, CLAY_ID("GalleryDensity"), kDensityNames, 3, &s.density);
            }
            ClayWidgets_EndCard(&ui, CLAY_ID("RangesCard"));

            ClayWidgets_BeginCard(&ui, CLAY_ID("TabsCard"), CLAY_STRING("Attached tabs"));
            {
                CLAY(CLAY_ID("MiniTabPlane"), {
                    .layout = {
                        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    },
                    .backgroundColor = ui.theme.surfaceColor,
                    .cornerRadius = CLAY_CORNER_RADIUS(ui.theme.radiusMd),
                    .border = {
                        .color = ui.theme.borderColor,
                        .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
                    },
                }) {
                    CLAY(CLAY_ID("MiniTabStrip"), {
                        .layout = {
                            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                            .padding = { .left = ui.theme.spacing.sm, .right = ui.theme.spacing.sm, .top = ui.theme.spacing.xs, .bottom = 0 },
                            .childGap = ui.theme.spacing.xs,
                            .layoutDirection = CLAY_LEFT_TO_RIGHT,
                        },
                    }) {
                        for (int32_t i = 0; i < kMiniTabCount; ++i) {
                            Clay_ElementId tabId = Clay_GetElementIdWithIndex(CLAY_STRING("MiniTab"), static_cast<uint32_t>(i));
                            ClayWidgets_TabEx(&ui, tabId, kMiniTabs[i].label, i, &s.miniTab, CLAY_WIDGETS_TAB_STYLE_ATTACHED);
                        }
                    }
                    CLAY(CLAY_ID("MiniTabBody"), {
                        .layout = {
                            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                            .padding = CLAY_PADDING_ALL(ui.theme.spacing.md),
                            .childGap = ui.theme.spacing.sm,
                            .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        },
                        .border = {
                            .color = ui.theme.borderColor,
                            .width = { .left = 0, .right = 0, .top = 1, .bottom = 0 },
                        },
                    }) {
                        int32_t page = (s.miniTab >= 0 && s.miniTab < kMiniTabCount) ? s.miniTab : 0;
                        ClayWidgets_Label(&ui, kMiniTabs[page].body);
                    }
                }
            }
            ClayWidgets_EndCard(&ui, CLAY_ID("TabsCard"));
        }
        ClayWidgets_EndScrollPanel(&ui, CLAY_ID("GalleryLeft"));

        ClayWidgets_BeginScrollPanel(&ui, CLAY_ID("GalleryRight"),
            ClayWidgets_ScrollPanelOptions{ rightWidth, CLAY_SIZING_GROW(0), 0, 0, ui.theme.spacing.md });
        {
            ClayWidgets_BeginCard(&ui, CLAY_ID("TogglesCard"), CLAY_STRING("Toggles & checks"));
            {
                ClayWidgets_Checkbox(&ui, CLAY_ID("GalleryAutosave"), CLAY_STRING("Autosave"), &s.autosave);
                ClayWidgets_Toggle(&ui, CLAY_ID("GalleryAutosaveToggle"), CLAY_STRING("Autosave (same state as above)"), &s.autosave);
                ClayWidgets_CheckboxEx(&ui, CLAY_ID("GalleryManaged"), CLAY_STRING("Telemetry (managed by policy)"), &s.telemetryLocked, true);

                ClayWidgets_Label(&ui, CLAY_STRING("Render quality"));
                ClayWidgets_Radio(&ui, CLAY_ID("GalleryQualityDraft"), CLAY_STRING("Draft"), 1, &s.quality);
                ClayWidgets_Radio(&ui, CLAY_ID("GalleryQualityBalanced"), CLAY_STRING("Balanced"), 2, &s.quality);
                ClayWidgets_Radio(&ui, CLAY_ID("GalleryQualityBest"), CLAY_STRING("Best"), 3, &s.quality);
            }
            ClayWidgets_EndCard(&ui, CLAY_ID("TogglesCard"));

            ClayWidgets_BeginCard(&ui, CLAY_ID("DataCard"), CLAY_STRING("Data display"));
            {
                ClayWidgets_Label(&ui, CLAY_STRING("Tree view"));
                if (ClayWidgets_TreeNode(&ui, CLAY_ID("TreeSrc"), CLAY_STRING("src"), 0, &s.treeSrcOpen)) {
                    if (ClayWidgets_TreeLeaf(&ui, CLAY_ID("TreeMain"), CLAY_STRING("main.cpp"), 1)) {
                        SetStatus(s, "Tree: main.cpp");
                    }
                    if (ClayWidgets_TreeNode(&ui, CLAY_ID("TreeWidgets"), CLAY_STRING("clay-widgets"), 1, &s.treeWidgetsOpen)) {
                        if (ClayWidgets_TreeLeaf(&ui, CLAY_ID("TreeButtonH"), CLAY_STRING("button.h"), 2)) {
                            SetStatus(s, "Tree: button.h");
                        }
                        if (ClayWidgets_TreeLeaf(&ui, CLAY_ID("TreeListRowH"), CLAY_STRING("list-row.h"), 2)) {
                            SetStatus(s, "Tree: list-row.h");
                        }
                        if (ClayWidgets_TreeLeaf(&ui, CLAY_ID("TreeWidgetsH"), CLAY_STRING("widgets.h"), 2)) {
                            SetStatus(s, "Tree: widgets.h");
                        }
                    }
                }
                if (ClayWidgets_TreeNode(&ui, CLAY_ID("TreeAssets"), CLAY_STRING("assets"), 0, &s.treeAssetsOpen)) {
                    if (ClayWidgets_TreeLeaf(&ui, CLAY_ID("TreeFont"), CLAY_STRING("Roboto-Regular.ttf"), 1)) {
                        SetStatus(s, "Tree: Roboto-Regular.ttf");
                    }
                }

                ClayWidgets_Separator(&ui);
                CLAY(CLAY_ID("IconRow"), {
                    .layout = {
                        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                        .childGap = ui.theme.spacing.md,
                        .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                    },
                }) {
                    ClayWidgets_Icon(&ui, CLAY_ID("IconCheck"), &icons.check, 24, ui.theme.accentColor);
                    ClayWidgets_Icon(&ui, CLAY_ID("IconPlay"), &icons.play, 24, ui.theme.textColor);
                    ClayWidgets_Icon(&ui, CLAY_ID("IconCircle"), &icons.circle, 24, ui.theme.accentColor);
                    ClayWidgets_Icon(&ui, CLAY_ID("IconSquare"), &icons.square, 24, ui.theme.textMutedColor);
                    MutedLabel(ui, CLAY_STRING("Theme-tinted texture icons"));
                }

                CLAY(CLAY_ID("BadgeRow"), {
                    .layout = {
                        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                        .childGap = ui.theme.spacing.sm,
                        .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                    },
                }) {
                    ClayWidgets_Badge(&ui, CLAY_STRING("Neutral"), CLAY_WIDGETS_BADGE_NEUTRAL);
                    ClayWidgets_Badge(&ui, CLAY_STRING("Accent"), CLAY_WIDGETS_BADGE_ACCENT);
                    ClayWidgets_Badge(&ui, CLAY_STRING("Success"), CLAY_WIDGETS_BADGE_SUCCESS);
                    ClayWidgets_Badge(&ui, CLAY_STRING("Warning"), CLAY_WIDGETS_BADGE_WARNING);
                    ClayWidgets_Badge(&ui, CLAY_STRING("Danger"), CLAY_WIDGETS_BADGE_DANGER);
                }
            }
            ClayWidgets_EndCard(&ui, CLAY_ID("DataCard"));

            ClayWidgets_BeginCard(&ui, CLAY_ID("OverlaysCard"), CLAY_STRING("Overlays"));
            {
                MutedLabel(ui, CLAY_STRING("Toasts, modal dialog, context menu and tooltips."));
                CLAY(CLAY_ID("ToastRow"), {
                    .layout = {
                        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                        .childGap = ui.theme.spacing.sm,
                        .layoutDirection = compactLayout ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                    },
                }) {
                    if (ClayWidgets_Button(&ui, CLAY_ID("ToastSuccess"), CLAY_STRING("Success toast"))) {
                        Notify(ui, s, CLAY_STRING("Saved successfully"), CLAY_WIDGETS_BADGE_SUCCESS, 2.5f);
                    }
                    if (ClayWidgets_Button(&ui, CLAY_ID("ToastWarning"), CLAY_STRING("Warning toast"))) {
                        Notify(ui, s, CLAY_STRING("Disk space is low"), CLAY_WIDGETS_BADGE_WARNING, 2.5f);
                    }
                    if (ClayWidgets_Button(&ui, CLAY_ID("ToastDanger"), CLAY_STRING("Danger toast"))) {
                        Notify(ui, s, CLAY_STRING("Connection lost"), CLAY_WIDGETS_BADGE_DANGER, 2.5f);
                    }
                }
                if (ClayWidgets_Button(&ui, CLAY_ID("GalleryOpenModal"), CLAY_STRING("Confirm dialog..."))) {
                    RequestDeleteTask(s, s.selectedTask);
                }

                CLAY(CLAY_ID("GalleryCtxTarget"), {
                    .layout = {
                        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                        .padding = CLAY_PADDING_ALL(ui.theme.spacing.md),
                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                    },
                    .backgroundColor = ui.theme.surfaceColor,
                    .cornerRadius = CLAY_CORNER_RADIUS(ui.theme.radiusSm),
                    .border = {
                        .color = ui.theme.borderColor,
                        .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
                    },
                }) {
                    ClayWidgets_Label(&ui, CLAY_STRING("Right-click here for a context menu"));
                }
                if (ClayWidgets_RightClicked(&ui, CLAY_ID("GalleryCtxTarget"))) {
                    ClayWidgets_OpenContextMenu(&ui, CLAY_ID("GalleryMenu"), ui.input.mouseX, ui.input.mouseY);
                }
            }
            ClayWidgets_EndCard(&ui, CLAY_ID("OverlaysCard"));
        }
        ClayWidgets_EndScrollPanel(&ui, CLAY_ID("GalleryRight"));
    }
}

} // namespace

#endif // CLAY_WIDGETS_DEMO_GALLERY_H
