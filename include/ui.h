#ifndef UI_H
#define UI_H

#include <stdbool.h>

typedef struct FamilyTree FamilyTree;
typedef struct LayoutResult LayoutResult;
typedef struct CameraController CameraController;

typedef struct UIContext
{
    bool available;
    void *impl;
    int width;
    int height;
} UIContext;

bool ui_init(UIContext *ui, int width, int height);
void ui_resize(UIContext *ui, int width, int height);
void ui_shutdown(UIContext *ui);

bool ui_begin_frame(UIContext *ui, float delta_seconds);
void ui_draw_overlay(UIContext *ui, const FamilyTree *tree, const LayoutResult *layout, CameraController *camera,
                     float fps);
void ui_render(UIContext *ui);
bool ui_is_available(const UIContext *ui);
bool ui_auto_orbit_enabled(const UIContext *ui);

#endif /* UI_H */
