#ifndef UI_ICONS_H
#define UI_ICONS_H

#include <stdbool.h>

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raylib.h>
#endif

#if defined(ANCESTRYTREE_HAVE_NUKLEAR)
#include "external/nuklear.h"
#include "ui_nuklear_config.h"
#endif
#if !defined(ANCESTRYTREE_HAVE_NUKLEAR)
struct nk_context;
struct nk_rect;
struct nk_color;
#endif

typedef enum UIIconType
{
    UI_ICON_ADD = 0,
    UI_ICON_SEARCH,
    UI_ICON_SETTINGS,
    UI_ICON_HELP,
    UI_ICON_INFO,
    UI_ICON_SAVE,
    UI_ICON_CANCEL,
    UI_ICON_DELETE,
    UI_ICON_EXIT,
    UI_ICON_COUNT
} UIIconType;

typedef struct UIIconLibrary
{
    float density[UI_ICON_COUNT];
    int dimension;
    bool ready;
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    Texture2D atlas;
    struct nk_image glyphs[UI_ICON_COUNT];
#endif
} UIIconLibrary;

bool ui_icons_init(UIIconLibrary *library);
void ui_icons_shutdown(UIIconLibrary *library);
bool ui_icons_ready(const UIIconLibrary *library);

#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
bool ui_icons_draw(const UIIconLibrary *library, struct nk_context *ctx, UIIconType icon,
                   struct nk_rect bounds, struct nk_color color, float intensity);
#endif

bool ui_icons_density(UIIconType icon, float *out_density);
const char *ui_icons_name(UIIconType icon);
int ui_icons_dimension(void);

#endif /* UI_ICONS_H */
