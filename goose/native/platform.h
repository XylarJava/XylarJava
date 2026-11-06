#ifndef XYLAR_GOOSE_PLATFORM_H
#define XYLAR_GOOSE_PLATFORM_H
#include <stdint.h>
/* Scalar-only C ABI, shared by Goose and the native backend smoke test. */
int32_t gx_open(int32_t hidden);
void gx_close(void);
int32_t gx_poll(void);
int32_t gx_active(void);
int32_t gx_keys(void); /* W,S,A,D,jump,sprint: bits 0..5 */
int32_t gx_mouse_x(void);
int32_t gx_mouse_y(void);
int32_t gx_actions(void); /* break,place: bits 0..1; edges, not holds */
int32_t gx_selected(void);
double gx_clock(void);
void gx_idle(void);
int32_t gx_mesh_begin(int64_t chunk);
int32_t gx_face(int64_t x, int64_t y, int64_t z, int64_t side, int32_t block);
int32_t gx_mesh_end(void);
void gx_target(int32_t visible, int64_t x, int64_t y, int64_t z);
int32_t gx_present(double x, double y, double z, double yaw, double pitch, int32_t selected);
/* Writes the last presented frame as PPM; used by previews and smoke tests. */
int32_t gx_screenshot(void);
#endif
