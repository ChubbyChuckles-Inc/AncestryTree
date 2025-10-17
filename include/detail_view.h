typedef struct DetailViewSystem DetailViewSystem;
#ifndef DETAIL_VIEW_H
#define DETAIL_VIEW_H

#include <stdbool.h>
#include <stddef.h>

#include "timeline.h"

struct ExpansionState;
struct RenderConfig;
struct Camera3D;

typedef struct DetailViewSystem DetailViewSystem;

#define DETAIL_VIEW_MAX_FACTS 5U
#define DETAIL_VIEW_MAX_TIMELINE_SLOTS 12U
#define DETAIL_VIEW_MAX_PANEL_SLOTS 6U
#define DETAIL_VIEW_MAX_CERTIFICATES 6U
#define DETAIL_VIEW_MAX_TIMELINE_EVENTS 16U

typedef enum DetailViewCertificateType
{
    DETAIL_CERTIFICATE_BIRTH = 0,
    DETAIL_CERTIFICATE_MARRIAGE,
    DETAIL_CERTIFICATE_DEATH,
    DETAIL_CERTIFICATE_ARCHIVE
} DetailViewCertificateType;

typedef enum DetailViewTimelineType
{
    DETAIL_TIMELINE_BIRTH = 0,
    DETAIL_TIMELINE_MARRIAGE,
    DETAIL_TIMELINE_DEATH,
    DETAIL_TIMELINE_CUSTOM
} DetailViewTimelineType;

typedef struct DetailViewCertificate
{
    DetailViewCertificateType type;
    char heading[64];
    char summary[192];
    char media_path[256];
    bool has_media_asset;
    bool is_pdf;
} DetailViewCertificate;

typedef struct DetailViewTimelineEvent
{
    DetailViewTimelineType type;
    char title[64];
    char date[32];
    char description[192];
    float normalized_time;
} DetailViewTimelineEvent;

typedef struct DetailViewContent
{
    char name[96];
    char lifespan[64];
    char facts[DETAIL_VIEW_MAX_FACTS][160];
    size_t fact_count;
    DetailViewCertificate certificates[DETAIL_VIEW_MAX_CERTIFICATES];
    size_t certificate_count;
    size_t certificate_focus_index;
    DetailViewTimelineEvent timeline_events[DETAIL_VIEW_MAX_TIMELINE_EVENTS];
    size_t timeline_event_count;
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
void detail_view_focus_next_certificate(DetailViewSystem *system);
void detail_view_clear_certificate_zoom(DetailViewSystem *system);
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
