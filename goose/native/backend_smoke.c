/* Exercises only the C graphics/input backend. This is not the Goose client. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"
#define CHECK(condition)                                                    \
    do {                                                                    \
        if (!(condition)) {                                                 \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
            gx_close();                                                     \
            exit(1);                                                        \
        }                                                                   \
    } while (0)
int main(void) {
    CHECK(gx_open(1));
    CHECK(!gx_mesh_begin(-1));
    CHECK(!gx_mesh_begin(16));
    CHECK(gx_mesh_begin(0));
    CHECK(!gx_mesh_begin(1));
    /* Small fixture: a floor and colored pillars, not generated game terrain. */
    for (int z = 0; z < 16; ++z)
        for (int x = 0; x < 16; ++x)
            CHECK(gx_face(x, 0, z, 3, 1));
    for (int block = 1; block <= 5; ++block)
        for (int y = 1; y <= block; ++y)
            for (int side = 0; side < 6; ++side)
                CHECK(gx_face(2 + block * 2, y, 7, side, block));
    CHECK(gx_mesh_end());
    CHECK(!gx_mesh_end());
    gx_target(1, 8, 2, 7);
    for (int i = 0; i < 3; ++i)
        CHECK(gx_present(8.5, 7, 17.5, 0, -.35, 3));
    CHECK(gx_screenshot());
    /* A successful GL call is insufficient: verify visible fixture colors. */
    FILE* preview = fopen("goose-preview.ppm", "rb");
    CHECK(preview);
    char magic[3] = {0};
    int width = 0, height = 0, maximum = 0;
    CHECK(fscanf(preview, "%2s %d %d %d", magic, &width, &height, &maximum) == 4);
    CHECK(strcmp(magic, "P6") == 0 && width == 960 && height == 540 && maximum == 255);
    CHECK(fgetc(preview) == '\n');
    size_t green = 0, stone = 0;
    for (int pixel = 0; pixel < width * height; ++pixel) {
        unsigned char rgb[3];
        CHECK(fread(rgb, 1, sizeof(rgb), preview) == sizeof(rgb));
        green += rgb[1] > rgb[0] + 20 && rgb[1] > rgb[2] + 20;
        stone += abs(rgb[0] - rgb[1]) < 15 && abs(rgb[1] - rgb[2]) < 15 && rgb[0] > 50 && rgb[0] + rgb[1] + rgb[2] < 600;
    }
    CHECK(green > 1000 && stone > 1000);
    CHECK(fgetc(preview) == EOF);
    CHECK(fclose(preview) == 0);
    CHECK(gx_mesh_begin(0));
    CHECK(gx_mesh_end()); /* Empty mesh replaces the old GPU allocation. */
    CHECK(gx_present(8.5, 7, 17.5, 0, -.35, 3));
    gx_close();
    gx_close();
    puts("Backend smoke: OpenGL context, mesh upload/replacement, drawing, screenshot and cleanup passed.");
    return 0;
}
