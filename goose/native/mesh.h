#ifndef XYLAR_GOOSE_MESH_H
#define XYLAR_GOOSE_MESH_H
#include <stddef.h>
#include <stdint.h>

enum { GX_CHUNKS = 16, GX_MAX_FACES = 16 * 32 * 16 * 6, GX_VERTICES_PER_FACE = 6 };
typedef struct {
    float x, y, z;
    uint8_t r, g, b, a;
} GxVertex;
typedef struct {
    GxVertex* vertices;
    size_t count, capacity;
} GxMesh;
/* -X,+X,-Y,+Y,-Z,+Z; CCW winding, viewed from outside. */
int gx_make_face(GxVertex out[6], int64_t x, int64_t y, int64_t z, int64_t side, int32_t block);
int gx_append_face(GxMesh* mesh, int64_t x, int64_t y, int64_t z, int64_t side, int32_t block);
void gx_free_mesh(GxMesh* mesh);
#endif
