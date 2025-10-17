#ifndef CERTIFICATE_RENDER_H
#define CERTIFICATE_RENDER_H

#include <stdbool.h>
#include <stddef.h>

struct DetailViewContent;
struct Camera3D;

typedef struct CertificateRenderState
{
    float orbit_angle;
    int zoom_target;
    int active_zoom;
    float zoom_progress;
    float highlight_timer;
} CertificateRenderState;

void certificate_reset(CertificateRenderState *state);
void certificate_update(CertificateRenderState *state, float delta_seconds, float activation, size_t certificate_count,
                        size_t focus_index);
void certificate_zoom(CertificateRenderState *state, size_t index, bool enable);
void certificate_render(const CertificateRenderState *state, const struct DetailViewContent *content, float activation,
                        const float origin[3], float room_radius, const struct Camera3D *camera);

#endif /* CERTIFICATE_RENDER_H */
