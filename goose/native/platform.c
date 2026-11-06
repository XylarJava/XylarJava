#include "platform.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "mesh.h"

static SDL_Window* window;
static SDL_GLContext context;
static PFNGLGENBUFFERSPROC make_buffers;
static PFNGLDELETEBUFFERSPROC delete_buffers;
static PFNGLBINDBUFFERPROC bind_buffer;
static PFNGLBUFFERDATAPROC buffer_data;
static GLuint buffers[GX_CHUNKS];
static GLsizei counts[GX_CHUNKS];
static GxMesh scratch;
static int64_t building = -1;
static int32_t captured, running, active, mouse_x, mouse_y, actions, selected = 1;
static int32_t target_visible;
static int64_t target_x, target_y, target_z;
static double title_time;
static unsigned title_frames;

static void capture(int32_t enabled) {
    captured = enabled && SDL_SetRelativeMouseMode(SDL_TRUE) == 0;
    if (!captured)
        SDL_SetRelativeMouseMode(SDL_FALSE);
    mouse_x = mouse_y = actions = 0;
}

void gx_close(void) {
    if (context && delete_buffers)
        delete_buffers(GX_CHUNKS, buffers);
    gx_free_mesh(&scratch);
    if (context)
        SDL_GL_DeleteContext(context);
    if (window)
        SDL_DestroyWindow(window);
    window = NULL;
    context = NULL;
    for (int i = 0; i < GX_CHUNKS; ++i) {
        buffers[i] = 0;
        counts[i] = 0;
    }
    running = captured = active = 0;
    building = -1;
    SDL_Quit();
}

int32_t gx_open(int32_t hidden) {
    if (window)
        return 0;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
        goto fail;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    window = SDL_CreateWindow(
        "Xylar Goose | sandbox prototype", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 960, 540,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | (hidden ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN));
    if (!window)
        goto fail;
    context = SDL_GL_CreateContext(window);
    if (!context)
        goto fail;
    make_buffers = (PFNGLGENBUFFERSPROC)SDL_GL_GetProcAddress("glGenBuffers");
    delete_buffers = (PFNGLDELETEBUFFERSPROC)SDL_GL_GetProcAddress("glDeleteBuffers");
    bind_buffer = (PFNGLBINDBUFFERPROC)SDL_GL_GetProcAddress("glBindBuffer");
    buffer_data = (PFNGLBUFFERDATAPROC)SDL_GL_GetProcAddress("glBufferData");
    if (!make_buffers || !delete_buffers || !bind_buffer || !buffer_data) {
        SDL_SetError("OpenGL vertex buffers are unavailable");
        goto fail;
    }
    SDL_GL_SetSwapInterval(1);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 48.f);
    glFogf(GL_FOG_END, 110.f);
    const GLfloat sky[] = {.53f, .72f, .88f, 1.f};
    glFogfv(GL_FOG_COLOR, sky);
    glClearColor(sky[0], sky[1], sky[2], sky[3]);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    running = 1;
    active = !hidden;
    if (active)
        capture(1);
    title_time = gx_clock();
    title_frames = 0;
    fprintf(stderr, "Goose backend: %s / %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
    return 1;
fail:
    fprintf(stderr, "Goose graphics initialization failed: %s\n", SDL_GetError());
    gx_close();
    return 0;
}

int32_t gx_poll(void) {
    mouse_x = mouse_y = actions = 0;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT)
            running = 0;
        else if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST || event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                active = 0;
                capture(0);
            } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED || event.window.event == SDL_WINDOWEVENT_RESTORED)
                active = 1;
        } else if (event.type == SDL_KEYDOWN && !event.key.repeat) {
            if (event.key.keysym.sym == SDLK_ESCAPE && active)
                capture(!captured);
            if (event.key.keysym.sym >= SDLK_1 && event.key.keysym.sym <= SDLK_5)
                selected = event.key.keysym.sym - SDLK_1 + 1;
        } else if (event.type == SDL_MOUSEBUTTONDOWN && active) {
            if (!captured) {
                capture(1);
                continue; /* Resuming must not accidentally destroy a block. */
            }
            if (event.button.button == SDL_BUTTON_LEFT)
                actions |= 1;
            if (event.button.button == SDL_BUTTON_RIGHT)
                actions |= 2;
        } else if (event.type == SDL_MOUSEMOTION && captured) {
            mouse_x += SDL_clamp(event.motion.xrel, -1000, 1000);
            mouse_y += SDL_clamp(event.motion.yrel, -1000, 1000);
            mouse_x = SDL_clamp(mouse_x, -10000, 10000);
            mouse_y = SDL_clamp(mouse_y, -10000, 10000);
        } else if (event.type == SDL_MOUSEWHEEL && captured) {
            const int direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1;
            selected = 1 + ((selected - 1 - direction * (event.wheel.y % 5)) % 5 + 5) % 5;
        }
    }
    return running;
}

int32_t gx_active(void) {
    return active && captured;
}
int32_t gx_mouse_x(void) {
    return mouse_x;
}
int32_t gx_mouse_y(void) {
    return mouse_y;
}
int32_t gx_actions(void) {
    return actions;
}
int32_t gx_selected(void) {
    return selected;
}
double gx_clock(void) {
    return (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
}
void gx_idle(void) {
    SDL_Delay(25);
}
int32_t gx_keys(void) {
    if (!gx_active())
        return 0;
    const Uint8* keys = SDL_GetKeyboardState(NULL);
    return (keys[SDL_SCANCODE_W] ? 1 : 0) | (keys[SDL_SCANCODE_S] ? 2 : 0) | (keys[SDL_SCANCODE_A] ? 4 : 0) |
           (keys[SDL_SCANCODE_D] ? 8 : 0) | (keys[SDL_SCANCODE_SPACE] ? 16 : 0) | (keys[SDL_SCANCODE_LSHIFT] ? 32 : 0);
}

int32_t gx_mesh_begin(int64_t chunk) {
    if (!context || building != -1 || chunk < 0 || chunk >= GX_CHUNKS)
        return 0;
    building = chunk;
    scratch.count = 0;
    return 1;
}
int32_t gx_face(int64_t x, int64_t y, int64_t z, int64_t side, int32_t block) {
    return building >= 0 && gx_append_face(&scratch, x, y, z, side, block);
}
int32_t gx_mesh_end(void) {
    if (building < 0)
        return 0;
    GLuint replacement = 0;
    make_buffers(1, &replacement);
    bind_buffer(GL_ARRAY_BUFFER, replacement);
    buffer_data(GL_ARRAY_BUFFER, (GLsizeiptr)(scratch.count * sizeof(GxVertex)), scratch.vertices, GL_STATIC_DRAW);
    bind_buffer(GL_ARRAY_BUFFER, 0);
    if (glGetError() != GL_NO_ERROR || !replacement) {
        delete_buffers(1, &replacement);
        building = -1;
        return 0;
    }
    delete_buffers(1, &buffers[building]);
    buffers[building] = replacement;
    counts[building] = (GLsizei)scratch.count;
    building = -1;
    return 1;
}
void gx_target(int32_t visible, int64_t x, int64_t y, int64_t z) {
    target_visible = visible && x >= 0 && x < 64 && y >= 0 && y < 32 && z >= 0 && z < 64;
    target_x = x;
    target_y = y;
    target_z = z;
}

static void draw_target(void) {
    if (!target_visible)
        return;
    const float e = .003f;
    const float corners[8][3] = {{-e, -e, -e},    {1 + e, -e, -e},    {1 + e, 1 + e, -e},    {-e, 1 + e, -e},
                                 {-e, -e, 1 + e}, {1 + e, -e, 1 + e}, {1 + e, 1 + e, 1 + e}, {-e, 1 + e, 1 + e}};
    const unsigned edges[24] = {0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7};
    glDisable(GL_FOG);
    glColor3f(.08f, .08f, .08f);
    glBegin(GL_LINES);
    for (size_t i = 0; i < 24; ++i) {
        const float* p = corners[edges[i]];
        glVertex3d(target_x + p[0], target_y + p[1], target_z + p[2]);
    }
    glEnd();
    glEnable(GL_FOG);
}

static void hud(int width, int height, int32_t block) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_FOG);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glColor3f(1.f, 1.f, 1.f);
    glBegin(GL_LINES);
    glVertex2i(width / 2 - 7, height / 2);
    glVertex2i(width / 2 + 7, height / 2);
    glVertex2i(width / 2, height / 2 - 7);
    glVertex2i(width / 2, height / 2 + 7);
    glEnd();
    for (int i = 1; i <= 5; ++i) {
        GxVertex face[6];
        gx_make_face(face, 0, 0, 0, 3, i);
        float x = width / 2.f + (i - 3) * 42.f - 17.f;
        float y = height - 48.f;
        glColor3f(i == block ? .3f : .12f, i == block ? .9f : .12f, .16f);
        glBegin(GL_QUADS);
        glVertex2f(x - 3, y - 3);
        glVertex2f(x + 37, y - 3);
        glVertex2f(x + 37, y + 37);
        glVertex2f(x - 3, y + 37);
        glEnd();
        glColor3ub(face[0].r, face[0].g, face[0].b);
        glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + 34, y);
        glVertex2f(x + 34, y + 34);
        glVertex2f(x, y + 34);
        glEnd();
    }
    glEnable(GL_FOG);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

int32_t gx_present(double x, double y, double z, double yaw, double pitch, int32_t block) {
    if (!context || !isfinite(x) || !isfinite(y) || !isfinite(z) || !isfinite(yaw) || !isfinite(pitch))
        return 0;
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
        gx_idle();
        return 1;
    }
    int width, height;
    SDL_GL_GetDrawableSize(window, &width, &height);
    if (width <= 0 || height <= 0) {
        gx_idle();
        return 1;
    }
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    const double near = .05, top = near * tan(35.0 * 3.141592653589793 / 180.0);
    glFrustum(-top * width / height, top * width / height, -top, top, near, 128.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotated(-pitch * 180.0 / 3.141592653589793, 1, 0, 0);
    glRotated(yaw * 180.0 / 3.141592653589793, 0, 1, 0);
    glTranslated(-x, -y, -z);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    size_t faces = 0;
    for (int i = 0; i < GX_CHUNKS; ++i) {
        if (!counts[i])
            continue;
        bind_buffer(GL_ARRAY_BUFFER, buffers[i]);
        glVertexPointer(3, GL_FLOAT, sizeof(GxVertex), (const void*)offsetof(GxVertex, x));
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(GxVertex), (const void*)offsetof(GxVertex, r));
        glDrawArrays(GL_TRIANGLES, 0, counts[i]);
        faces += counts[i] / 6;
    }
    bind_buffer(GL_ARRAY_BUFFER, 0);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    draw_target();
    hud(width, height, block);
    ++title_frames;
    const double now = gx_clock();
    if (now - title_time >= 1.0) {
        char title[320];
        snprintf(title, sizeof(title),
                 "Xylar Goose | sandbox prototype | %.0f FPS | %zu faces | %s | WASD Space Shift | L/R click | 1-5 | Esc",
                 title_frames / (now - title_time), faces, gx_active() ? "playing" : "paused - click to resume");
        SDL_SetWindowTitle(window, title);
        title_frames = 0;
        title_time = now;
    }
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        fprintf(stderr, "Goose OpenGL error: 0x%x\n", error);
        return 0;
    }
    SDL_GL_SwapWindow(window);
    return 1;
}

int32_t gx_screenshot(void) {
    if (!context)
        return 0;
    int width, height;
    SDL_GL_GetDrawableSize(window, &width, &height);
    if (width <= 0 || height <= 0 || width > 8192 || height > 8192)
        return 0;
    const size_t stride = (size_t)width * 3;
    unsigned char* pixels = malloc(stride * height);
    if (!pixels)
        return 0;
    glReadBuffer(GL_FRONT);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    if (glGetError() != GL_NO_ERROR) {
        free(pixels);
        return 0;
    }
    FILE* file = fopen("goose-preview.ppm", "wb");
    int ok = file != NULL;
    if (file) {
        if (fprintf(file, "P6\n%d %d\n255\n", width, height) < 0)
            ok = 0;
        for (int y = height - 1; y >= 0 && ok; --y)
            ok = fwrite(pixels + (size_t)y * stride, 1, stride, file) == stride;
        if (fclose(file) != 0)
            ok = 0;
    }
    free(pixels);
    return ok;
}
