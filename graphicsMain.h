/* ***** BEGIN MIV LICENSE BLOCK *****
 * Version: MIV 1.0
 *
 * This file is part of the "MIV" license.
 *
 * Rules of this license:
 * - This code may be reused in other free software projects (free means the end user does not have to pay anything to use it).
 * - This code may be reused in other non free software projects. 
 *     !! For this rule to apply you will grant or provide the below mentioned author unlimited free access/license to the:
 *         - binary program of the non free software project which uses this code. By this we mean a full working version.
 *         - One piece of the hardware using this code. For free at no costs to the author. 
 *         - 1% of the netto world wide sales.
 * - When you use this code always leave this complete license block in the file.
 * - When you create binaries (executable or library) based on this source you 
 *     need to provide a copy of this source online publicaly accessable.
 * - When you make modifications to this source file you will keep this license block complete.
 * - When you make modifications to this source file you will send a copy of the new file to 
 *     the author mentioned in this license block. These rules will also apply to the new file.
 * - External packages used by this source might have a different license which you should comply with.
 *
 * Latest version of this license can be found at http://www.1st-setup.nl
 *
 * Author: Michel Verbraak (info@1st-setup.nl)
 * Website: http://www.1st-setup.nl
 * email: info@1st-setup.nl
 *
 *
 * ***** END MIV LICENSE BLOCK *****/

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
