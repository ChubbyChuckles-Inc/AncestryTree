#ifndef DETAIL_VIEW_H
#define DETAIL_VIEW_H

#include <stdbool.h>

struct ExpansionState;
struct RenderConfig;
struct Camera3D;

typedef struct DetailViewSystem DetailViewSystem;

DetailViewSystem *detail_view_create(void);
void detail_view_destroy(DetailViewSystem *system);
void detail_view_reset(DetailViewSystem *system);
void detail_view_update(DetailViewSystem *system, float delta_seconds, const struct ExpansionState *expansion,
                        float timeline_phase, float panel_phase);
void detail_view_render(const DetailViewSystem *system, const struct ExpansionState *expansion,
                        const struct RenderConfig *config, const struct Camera3D *camera);

#endif /* DETAIL_VIEW_H */
