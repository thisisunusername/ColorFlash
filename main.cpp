#include "android_native_app_glue.h"
#include <EGL/egl.h>
#include <GLES/gl.h>
#include <time.h>
#include <stdint.h>
#include <android/log.h>

#define LOG_TAG "ColorFlash"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

struct Engine {
    struct android_app* app;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    int animating = 0;
    int64_t last_color_ms = 0;
    int color_index = 0;
};
//这是颜色
static const GLfloat COLORS[][3] = {
    // 基础色
    {1,0,0}, {0,1,0}, {0,0,1},
    // 二次色
    {1,1,0}, {1,0,1}, {0,1,1},
    // 三次色
    {1,0.5f,0}, {0.5f,1,0}, {0,1,0.5f},
    {0,0.5f,1}, {0.5f,0,1}, {1,0,0.5f},
    // 灰阶
    {0,0,0}, {0.5f,0.5f,0.5f}, {0.75f,0.75f,0.75f}, {1,1,1}
};
static const int COLOR_COUNT = sizeof(COLORS) / sizeof(COLORS[0]);
//这是颜色结束
//这是闪烁频率
static const int64_t DURATION_MS = 200;

static int64_t now_ms() {
    timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int engine_init_display(Engine* e) {
    LOGI("engine_init_display start");

    const EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_NONE
    };
    EGLint numConfigs = 0; EGLConfig config = nullptr; EGLint format = 0;

    e->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    LOGI("eglGetDisplay = %p", e->display);
    if (e->display == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay failed");
        return -1;
    }

    EGLBoolean ok = eglInitialize(e->display, 0, 0);
    LOGI("eglInitialize = %d", ok);
    if (!ok) {
        LOGE("eglInitialize failed, err=0x%x", eglGetError());
        return -1;
    }

    ok = eglChooseConfig(e->display, attribs, &config, 1, &numConfigs);
    LOGI("eglChooseConfig = %d, numConfigs=%d", ok, numConfigs);
    if (!ok || numConfigs < 1) {
        LOGE("eglChooseConfig failed, err=0x%x", eglGetError());
        return -1;
    }

    ok = eglGetConfigAttrib(e->display, config, EGL_NATIVE_VISUAL_ID, &format);
    LOGI("eglGetConfigAttrib format=%d ok=%d", format, ok);

    ANativeWindow_setBuffersGeometry(e->app->window, 0, 0, format);

    e->surface = eglCreateWindowSurface(e->display, config, e->app->window, 0);
    LOGI("eglCreateWindowSurface = %p", e->surface);
    if (e->surface == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface failed, err=0x%x", eglGetError());
        return -1;
    }

    e->context = eglCreateContext(e->display, config, 0, 0);
    LOGI("eglCreateContext = %p", e->context);
    if (e->context == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext failed, err=0x%x", eglGetError());
        return -1;
    }

    ok = eglMakeCurrent(e->display, e->surface, e->surface, e->context);
    LOGI("eglMakeCurrent = %d", ok);
    if (!ok) {
        LOGE("eglMakeCurrent failed, err=0x%x", eglGetError());
        return -1;
    }

    glDisable(GL_DEPTH_TEST);
    e->last_color_ms = now_ms();
    e->color_index = 0;
    LOGI("engine_init_display success");
    return 0;
}

static void engine_draw_frame(Engine* e) {
    if (e->display == EGL_NO_DISPLAY || e->surface == EGL_NO_SURFACE) {
        return;
    }

    int64_t now = now_ms();
    if (now - e->last_color_ms >= DURATION_MS) {
        e->color_index = (e->color_index + 1) % COLOR_COUNT;
        e->last_color_ms = now;
    }
//这里如果闪烁频率快了就要删了
   static int frame_count = 0;
  if (frame_count++ % 60 == 0) {
        LOGI("draw frame %d color_idx=%d", frame_count, e->color_index);
   }

    const GLfloat* c = COLORS[e->color_index];
    glClearColor(c[0], c[1], c[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(e->display, e->surface);
}

static void engine_term_display(Engine* e) {
    if (e->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(e->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (e->context != EGL_NO_CONTEXT) eglDestroyContext(e->display, e->context);
        if (e->surface != EGL_NO_SURFACE) eglDestroySurface(e->display, e->surface);
        eglTerminate(e->display);
    }
    e->animating = 0;
    e->display = EGL_NO_DISPLAY;
    e->context = EGL_NO_CONTEXT;
    e->surface = EGL_NO_SURFACE;
}

static void engine_handle_cmd(android_app* app, int32_t cmd) {
    Engine* e = (Engine*)app->userData;
    LOGI("handle cmd %d", cmd);
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window) {
                int ret = engine_init_display(e);
                LOGI("init_display ret=%d", ret);
                e->animating = (ret == 0) ? 1 : 0;
                if (e->animating) engine_draw_frame(e);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            engine_term_display(e);
            break;
        default: break;
    }
}

void android_main(struct android_app* state) {
    Engine engine;
    state->userData = &engine;
    state->onAppCmd = engine_handle_cmd;
    engine.app = state;

    LOGI("android_main entered");

    while (!state->destroyRequested) {
        android_poll_source* source = nullptr;
        int events = 0;
        int timeout = engine.animating ? 0 : -1;
        int ident = ALooper_pollOnce(timeout, nullptr, &events, (void**)&source);

        if (ident >= 0 && source) {
            source->process(state, source);
        }
        if (state->destroyRequested) break;

        if (engine.animating) {
            engine_draw_frame(&engine);
        }
    }

    LOGI("android_main exiting");
    engine_term_display(&engine);
}