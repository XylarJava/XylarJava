#include "mesh.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(condition)                                                    \
    do {                                                                    \
        if (!(condition)) {                                                 \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
            exit(1);                                                        \
        }                                                                   \
    } while (0)
int main(void) {
    const int normals[6][3] = {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
    GxVertex face[6];
    for (int side = 0; side < 6; ++side) {
        CHECK(gx_make_face(face, 15, 7, 31, side, 1));
        for (int t = 0; t < 6; t += 3) {
            const GxVertex a = face[t], b = face[t + 1], c = face[t + 2];
            float ab[3] = {b.x - a.x, b.y - a.y, b.z - a.z}, ac[3] = {c.x - a.x, c.y - a.y, c.z - a.z};
            float n[3] = {ab[1] * ac[2] - ab[2] * ac[1], ab[2] * ac[0] - ab[0] * ac[2], ab[0] * ac[1] - ab[1] * ac[0]};
            for (int axis = 0; axis < 3; ++axis)
                CHECK(fabsf(n[axis] - normals[side][axis]) < 0.00001f);
        }
        for (int i = 0; i < 6; ++i) {
            CHECK(face[i].x >= 15 && face[i].x <= 16);
            CHECK(face[i].y >= 7 && face[i].y <= 8);
            CHECK(face[i].z >= 31 && face[i].z <= 32);
            CHECK(face[i].a == 255);
        }
    }
    CHECK(!gx_make_face(face, -1, 0, 0, 0, 1));
    CHECK(!gx_make_face(face, 64, 0, 0, 0, 1));
    CHECK(!gx_make_face(face, 0, 32, 0, 0, 1));
    CHECK(!gx_make_face(face, 0, 0, 64, 0, 1));
    CHECK(!gx_make_face(face, 0, 0, 0, 6, 1));
    CHECK(!gx_make_face(face, 0, 0, 0, 0, 0));
    CHECK(!gx_make_face(face, 0, 0, 0, 0, 7));
    GxMesh mesh = {0};
    for (int i = 0; i < GX_MAX_FACES; ++i)
        CHECK(gx_append_face(&mesh, 0, 0, 0, i % 6, 3));
    CHECK(mesh.count == (size_t)GX_MAX_FACES * 6);
    CHECK(mesh.capacity <= (size_t)GX_MAX_FACES * 6);
    CHECK(!gx_append_face(&mesh, 0, 0, 0, 0, 1));
    CHECK(mesh.count == (size_t)GX_MAX_FACES * 6);
    mesh.count = 0; /* Rebuild reuses the scratch arena. */
    CHECK(gx_append_face(&mesh, 63, 31, 63, 5, 6));
    CHECK(mesh.vertices[0].x == 64);
    gx_free_mesh(&mesh);
    CHECK(mesh.vertices == NULL && mesh.count == 0 && mesh.capacity == 0);
    gx_free_mesh(&mesh);
    puts("Native mesh: winding, coordinates, material bounds, bounded allocation and reuse passed.");
}
