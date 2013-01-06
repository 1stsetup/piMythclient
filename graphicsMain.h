#include "EGL/egl.h"
#include "EGL/eglext.h"

struct DISPLAY_T{
	EGLDisplay display;
	EGLenum api;
};

struct CONTEXT_T{
	EGLContext ctx;
	struct DISPLAY_T *display;
	EGLConfig configs[1];
	EGLint attribs[32];

};

struct WINDOW_T{
	uint32_t screen_id;
	uint32_t xpos;
	uint32_t ypos;
	uint32_t width;
	uint32_t height;
	DISPMANX_UPDATE_HANDLE_T vc_dispmanx_update;
	EGL_DISPMANX_WINDOW_T egl_win;
	EGLSurface surface;
	struct CONTEXT_T *ctx;
	int visible;
};

struct CURRENT_WINDOW_T{
	EGLContext ctx;
	EGLDisplay display;
	EGLSurface readSurface;
	EGLSurface drawSurface;
	EGLenum api;
};

VCOS_STATUS_T graphicsInit(struct DISPLAY_T *disp, EGLenum api, const char *font_dir);
void graphicsDestroy(struct DISPLAY_T *disp);
VCOS_STATUS_T createContext(struct DISPLAY_T *disp, struct CONTEXT_T *context);
void destroyContext(struct CONTEXT_T *context);
VCOS_STATUS_T graphicsCreateWindow(struct CONTEXT_T *ctx,
				uint32_t screen_id,
                                uint32_t xpos,
                                uint32_t ypos,
                                uint32_t width,
                                uint32_t height,
				struct WINDOW_T *window);
void graphicsDestroyWindow(struct WINDOW_T *window);
int32_t graphicsGetDisplaySize( const uint16_t display_number,
                                   uint32_t *width,
                                   uint32_t *height);
EGLBoolean graphicSetCurrentWindow(struct WINDOW_T *window);
VCOS_STATUS_T graphicsFillSquare(struct WINDOW_T *window,
                               uint32_t x,
                               uint32_t y,
                               uint32_t width,
                               uint32_t height,
                               uint32_t fill_colour );
VCOS_STATUS_T graphicsShowWindow(struct WINDOW_T *window);
