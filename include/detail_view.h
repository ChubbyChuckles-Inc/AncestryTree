#ifndef DETAIL_VIEW_H
#define DETAIL_VIEW_H

#include <stdbool.h>
#include <stdint.h>

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raylib.h>
#endif

#include "detail_gallery.h"
#include "detail_timeline.h"

struct Person;
struct nk_context;

typedef struct DetailViewState
{
    bool initialized;
    uint32_t cached_person_id;
    int selected_certificate_index;
    DetailGallery gallery;
    DetailTimeline timeline;
    int gallery_hover_index;
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    Texture2D profile_texture;
    bool profile_texture_loaded;
    Texture2D certificate_preview_texture;
    bool certificate_preview_loaded;
    char certificate_preview_path[260];
#endif
} DetailViewState;

#ifdef __cplusplus
extern "C"
{
#endif

    void detail_view_state_reset(DetailViewState *state);
    bool detail_view_init(DetailViewState *state);
    void detail_view_cleanup(DetailViewState *state);
    bool detail_view_render(DetailViewState *state, struct nk_context *ctx, const struct Person *person,
                            int viewport_width, int viewport_height, bool *out_exit_requested);

#ifdef __cplusplus
}
#endif

#endif /* DETAIL_VIEW_H */
