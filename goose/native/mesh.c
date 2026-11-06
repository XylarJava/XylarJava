#include "mesh.h"
#include <stdlib.h>
#include <string.h>

int gx_make_face(GxVertex out[6], int64_t x, int64_t y, int64_t z, int64_t side, int32_t block) {
    if (!out || x < 0 || x >= 64 || y < 0 || y >= 32 || z < 0 || z >= 64 || side < 0 || side >= 6 || block < 1 || block > 6)
        return 0;
    static const uint8_t corners[6][4][3] = {
        {{0, 0, 1}, {0, 1, 1}, {0, 1, 0}, {0, 0, 0}}, {{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}},
        {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}, {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}},
        {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}}, {{1, 0, 1}, {1, 1, 1}, {0, 1, 1}, {0, 0, 1}},
    };
    static const uint8_t triangles[6] = {0, 1, 2, 0, 2, 3};
    static const uint8_t colors[7][3] = {{0, 0, 0},     {100, 164, 65}, {134, 95, 62}, {137, 143, 150},
                                         {115, 80, 48}, {66, 124, 57},  {61, 65, 70}};
    static const float shade[6] = {.78f, .86f, .52f, 1.f, .70f, .90f};
    int32_t material = block == 1 && side != 3 ? 2 : block;
    for (size_t i = 0; i < 6; ++i) {
        const uint8_t* corner = corners[side][triangles[i]];
        out[i] = (GxVertex){(float)x + corner[0],
                            (float)y + corner[1],
                            (float)z + corner[2],
                            (uint8_t)(colors[material][0] * shade[side]),
                            (uint8_t)(colors[material][1] * shade[side]),
                            (uint8_t)(colors[material][2] * shade[side]),
                            255};
    }
    return 1;
}

int gx_append_face(GxMesh* mesh, int64_t x, int64_t y, int64_t z, int64_t side, int32_t block) {
    GxVertex face[6];
    const size_t limit = (size_t)GX_MAX_FACES * GX_VERTICES_PER_FACE;
    if (!mesh || !gx_make_face(face, x, y, z, side, block) || mesh->count > limit - 6)
        return 0;
    if (mesh->count + 6 > mesh->capacity) {
        size_t capacity = mesh->capacity ? mesh->capacity * 2 : 1024;
        if (capacity > limit)
            capacity = limit;
        GxVertex* vertices = realloc(mesh->vertices, capacity * sizeof(*vertices));
        if (!vertices)
            return 0;
        mesh->vertices = vertices;
        mesh->capacity = capacity;
    }
    memcpy(mesh->vertices + mesh->count, face, sizeof(face));
    mesh->count += 6;
    return 1;
}

void gx_free_mesh(GxMesh* mesh) {
    if (!mesh)
        return;
    free(mesh->vertices);
    *mesh = (GxMesh){0};
}
