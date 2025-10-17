typedef struct DetailViewSystem DetailViewSystem;
#ifndef DETAIL_VIEW_H
#define DETAIL_VIEW_H

#include <stdbool.h>
#include <stddef.h>

struct ExpansionState;
struct RenderConfig;
struct Camera3D;

typedef struct DetailViewSystem DetailViewSystem;

#define DETAIL_VIEW_MAX_FACTS 5U
#define DETAIL_VIEW_MAX_TIMELINE_SLOTS 12U
#define DETAIL_VIEW_MAX_PANEL_SLOTS 6U

typedef struct DetailViewContent
{
    char name[96];
    char lifespan[64];
    char facts[DETAIL_VIEW_MAX_FACTS][160];
    size_t fact_count;
} DetailViewContent;

typedef struct DetailViewTimelineInfo
{
    float base_angle;
    float height;
    float radius;
} DetailViewTimelineInfo;

typedef struct DetailViewPanelInfo
{
    float position[3];
    float normal[3];
    float scale;
} DetailViewPanelInfo;

DetailViewSystem *detail_view_create(void);
void detail_view_destroy(DetailViewSystem *system);
bool detail_view_init(DetailViewSystem *system);
void detail_view_cleanup(DetailViewSystem *system);
void detail_view_reset(DetailViewSystem *system);
bool detail_view_set_content(DetailViewSystem *system, const DetailViewContent *content);
bool detail_view_content_ready(const DetailViewSystem *system);
const DetailViewContent *detail_view_get_content(const DetailViewSystem *system);
bool detail_view_get_timeline_info(const DetailViewSystem *system, size_t index, DetailViewTimelineInfo *out_info);
bool detail_view_get_panel_info(const DetailViewSystem *system, size_t index, DetailViewPanelInfo *out_info);
float detail_view_get_timeline_phase(const DetailViewSystem *system);
float detail_view_get_panel_phase(const DetailViewSystem *system);
float detail_view_get_detail_phase(const DetailViewSystem *system);
void detail_view_update(DetailViewSystem *system, float delta_seconds, const struct ExpansionState *expansion,
                        float timeline_phase, float panel_phase);
void detail_view_render(const DetailViewSystem *system, const struct ExpansionState *expansion,
                        const struct RenderConfig *config, const struct Camera3D *camera);

#endif /* DETAIL_VIEW_H */
