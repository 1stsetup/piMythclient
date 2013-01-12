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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdint.h>

#include "globalFunctions.h"
#include "lists.h"

#ifdef PI

#include "bcm.h"
#include "vgfont.h"
#include <VG/vgu.h>

#endif

#include "osd.h"

#ifdef PI

//	s = gx_graphics_init(".");

VGfloat currentColor;

static void showEGLErrorStr(EGLint egl_error, char *userStr)
{
		if (egl_error == EGL_NO_SURFACE) logInfo(LOG_OSD, "%s: EGL_NO_SURFACE\n", userStr);
		if (egl_error == EGL_BAD_DISPLAY) logInfo(LOG_OSD, "%s: EGL_BAD_DISPLAY\n", userStr);
		if (egl_error == EGL_NOT_INITIALIZED) logInfo(LOG_OSD, "%s: EGL_NOT_INITIALIZED\n", userStr);
		if (egl_error == EGL_BAD_CONFIG) logInfo(LOG_OSD, "%s: EGL_BAD_CONFIG\n", userStr);
		if (egl_error == EGL_BAD_NATIVE_WINDOW) logInfo(LOG_OSD, "%s: EGL_BAD_NATIVE_WINDOW\n", userStr);
		if (egl_error == EGL_BAD_ATTRIBUTE) logInfo(LOG_OSD, "%s: EGL_BAD_ATTRIBUTE\n", userStr);
		if (egl_error == EGL_BAD_ALLOC) logInfo(LOG_OSD, "%s: EGL_BAD_ALLOC\n", userStr);
		if (egl_error == EGL_BAD_MATCH) logInfo(LOG_OSD, "%s: EGL_BAD_MATCH\n", userStr);
		if (egl_error == EGL_BAD_SURFACE) logInfo(LOG_OSD, " %s: EGL_BAD_SURFACE\n", userStr);
		if (egl_error == EGL_CONTEXT_LOST) logInfo(LOG_OSD, "%s: EGL_CONTEXT_LOST\n", userStr);
		if (egl_error == EGL_BAD_CONTEXT) logInfo(LOG_OSD, "%s: EGL_BAD_CONTEXT\n", userStr);
		if (egl_error == EGL_BAD_ACCESS) logInfo(LOG_OSD, "%s: EGL_BAD_ACCESS\n", userStr);
		if (egl_error == EGL_BAD_NATIVE_PIXMAP) logInfo(LOG_OSD, "%s: EGL_BAD_NATIVE_PIXMAP\n", userStr);
		if (egl_error == EGL_BAD_CURRENT_SURFACE) logInfo(LOG_OSD, "%s: EGL_BAD_CURRENT_SURFACE\n", userStr);
		if (egl_error == EGL_BAD_PARAMETER) logInfo(LOG_OSD, "%s: EGL_BAD_PARAMETER\n", userStr);

		logInfo(LOG_OSD, "%s: 0x%x\n", userStr, egl_error);
}

static void showVGErrorStr(VGErrorCode vg_error, char *userStr)
{
		if (vg_error == VG_BAD_HANDLE_ERROR) logInfo(LOG_OSD, "%s: VG_BAD_HANDLE_ERROR\n", userStr);
		if (vg_error == VG_ILLEGAL_ARGUMENT_ERROR) logInfo(LOG_OSD, "%s: VG_ILLEGAL_ARGUMENT_ERROR\n", userStr);
		if (vg_error == VG_OUT_OF_MEMORY_ERROR) logInfo(LOG_OSD, "%s: VG_OUT_OF_MEMORY_ERROR\n", userStr);
		if (vg_error == VG_PATH_CAPABILITY_ERROR) logInfo(LOG_OSD, "%s: VG_PATH_CAPABILITY_ERROR\n", userStr);
		if (vg_error == VG_UNSUPPORTED_IMAGE_FORMAT_ERROR) logInfo(LOG_OSD, "%s: VG_UNSUPPORTED_IMAGE_FORMAT_ERROR\n", userStr);
		if (vg_error == VG_UNSUPPORTED_PATH_FORMAT_ERROR) logInfo(LOG_OSD, "%s: VG_UNSUPPORTED_PATH_FORMAT_ERROR\n", userStr);
		if (vg_error == VG_IMAGE_IN_USE_ERROR) logInfo(LOG_OSD, "%s: VG_IMAGE_IN_USE_ERROR\n", userStr);
		if (vg_error == VG_NO_CONTEXT_ERROR) logInfo(LOG_OSD, " %s: VG_NO_CONTEXT_ERROR\n", userStr);

		logInfo(LOG_OSD, "%s: 0x%x\n", userStr, vg_error);
}

static int graphicsEGLAttribColours(EGLint *attribs)
{
   int i, n;
   static EGLint rgba[] = {EGL_RED_SIZE, EGL_GREEN_SIZE, EGL_BLUE_SIZE, EGL_ALPHA_SIZE};
   static uint8_t rgb32a[] = {8,8,8,8};

   for (n=0, i=0; i<countof(rgba); i++)
   {
      attribs[n++] = rgba[i];
      attribs[n++] = rgb32a[i];
   }
   return n;
}

struct OSD_T *osdCreate(int layer, uint32_t width, uint32_t height, uint32_t fill_colour)
{
	logInfo(LOG_OSD, "osdCreate start.\n");

	struct OSD_T *result = (struct OSD_T *)malloc(sizeof(struct OSD_T));
	result->visible = 0;
	result->layer = layer;
	result->fill_colour = fill_colour;

	uint32_t screen_width, screen_height;

	int s;

	// window init
	if ((width == 0) || (height == 0)) {
		s = graphics_get_display_size(0, &screen_width, &screen_height);
		if (s != 0) {
			logInfo(LOG_OSD, "graphics_get_display_size");
			return NULL;
		}
		if (width == 0) {
			width = screen_width;
		}
		if (height == 0) {
			height = screen_height;
		}
	}

	result->width = width;
	result->height = height;

	logInfo(LOG_OSD, "width=%d, height=%d\n", width, height);


	result->dispman_display = vc_dispmanx_display_open(0 /* LCD */);
	if (result->dispman_display == DISPMANX_NO_HANDLE) {
		logInfo(LOG_OSD, "Could not vc_dispmanx_display_open.\n");
		return NULL;
	}

	result->vc_dispmanx_update = vc_dispmanx_update_start(0);
	if (!result->vc_dispmanx_update) {
		logInfo(LOG_OSD, "Could not vc_dispmanx_update_start on screen.\n");
		return NULL;
	}

	VC_RECT_T dst_rect;
	dst_rect.x = 0;
	dst_rect.y = 0;
	dst_rect.width = screen_width;
	dst_rect.height = screen_height;

	VC_RECT_T src_rect;
	src_rect.x = 0;
	src_rect.y = 0;
	src_rect.width = result->width << 16;
	src_rect.height = result->height << 16;        

	result->dispman_element =
		vc_dispmanx_element_add(result->vc_dispmanx_update,
				        result->dispman_display,
				        result->layer,
				        &dst_rect,
				        0 /*src*/,
				        &src_rect,
				        DISPMANX_PROTECTION_NONE,
				        0 /*alpha*/,
				        0 /*clamp*/,
				        (DISPMANX_TRANSFORM_T) 0 /*transform*/);

	if (!vc_dispmanx_update_submit_sync(result->vc_dispmanx_update)) {
		logInfo(LOG_OSD, "vc_dispmanx_update_submit_sync\n");
	}

	// EGL init
	result->eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (result->eglDisplay == EGL_NO_DISPLAY) {
		EGLint egl_error = eglGetError();
		showEGLErrorStr(egl_error, "eglGetDisplay");
		return NULL;
	}

	EGLint egl_maj, egl_min;
	EGLBoolean egl_ret;

	egl_ret = eglInitialize(result->eglDisplay, &egl_maj, &egl_min);
	if (egl_ret == EGL_FALSE)
	{
		EGLint egl_error = eglGetError();
		showEGLErrorStr(egl_error, "eglInitialize");
		return NULL;
	}
	logInfo(LOG_OSD, "eglVersion :egl_maj=%d, egl_min=%d\n", egl_maj, egl_min);

	// get an appropriate EGL frame buffer configuration

	EGLConfig config[1];
	EGLint num_config;

	EGLint attribs[32];
	int n = 0;
	n = graphicsEGLAttribColours(&attribs[0]);
	attribs[n++] = EGL_RENDERABLE_TYPE; 
	attribs[n++] = EGL_OPENVG_BIT;
	attribs[n++] = EGL_SURFACE_TYPE; 
	attribs[n++] = EGL_WINDOW_BIT;
	attribs[n] = EGL_NONE;

	egl_ret = eglChooseConfig(result->eglDisplay, &attribs[0], config, 1, &num_config);
	if ((egl_ret == EGL_FALSE) || (num_config == 0))
	{
		EGLint egl_error = eglGetError();
		showEGLErrorStr(egl_error, "eglChooseConfig");
		return NULL;
	}

	egl_ret = eglBindAPI(EGL_OPENVG_API);
	if (egl_ret == EGL_FALSE)
	{
		EGLint egl_error = eglGetError();
		showEGLErrorStr(egl_error, "eglBindAPI");
		return NULL;
	}

	static EGL_DISPMANX_WINDOW_T nativewindow;
	nativewindow.element = result->dispman_element;
	nativewindow.width = result->width;
	nativewindow.height = result->height;

	result->surface = eglCreateWindowSurface(result->eglDisplay, config[0], &nativewindow, NULL);
	if (!result->surface)
	{
		EGLint egl_error = eglGetError();
		showEGLErrorStr(egl_error, "Could not create window surface");
		return NULL;
	}

	result->context = eglCreateContext(result->eglDisplay, config[0], EGL_NO_CONTEXT, NULL);
	if (!result->context)
	{
		EGLint egl_error = eglGetError();
		showEGLErrorStr(egl_error, "eglCreateContext");
		return NULL;
	}

	egl_ret = eglMakeCurrent(result->eglDisplay, result->surface, result->surface, result->context);
	if (egl_ret != EGL_TRUE) {
		EGLint egl_error = eglGetError();
		showEGLErrorStr(egl_error, "eglMakeCurrent");
		return NULL;
	}

	result->path = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F,
					1.0, 0.0,
					0, 0,
					VG_PATH_CAPABILITY_ALL);
	if (!result->path) {
		EGLint egl_error = eglGetError();
		showEGLErrorStr(egl_error, "vgCreatePath");
		return NULL;
	}

	result->paint = vgCreatePaint();
	if (!result->paint) {
		EGLint egl_error = eglGetError();
		showEGLErrorStr(egl_error, "vgCreatePaint");
		return NULL;
	}

	logInfo(LOG_OSD, "osdCreate end.\n");
	return result;
}

void osdDestroy(struct OSD_T *osd)
{
	// destroy egl
	if ((osd) && (osd->eglDisplay != EGL_NO_DISPLAY)) {
		EGLBoolean egl_ret;

		egl_ret = eglMakeCurrent(osd->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (egl_ret != EGL_TRUE) {
			EGLint egl_error = eglGetError();
			showEGLErrorStr(egl_error, "eglMakeCurrent");
		}

		egl_ret = eglTerminate(osd->eglDisplay);
		if (egl_ret != EGL_TRUE) {
			EGLint egl_error = eglGetError();
			showEGLErrorStr(egl_error, "eglTerminate");
		}

		osd->eglDisplay = EGL_NO_DISPLAY;
		osd->context = EGL_NO_CONTEXT;
		osd->surface = EGL_NO_SURFACE;
	}

	// destroy winow.
	if ((osd) && (osd->dispman_element)) {
		osd->vc_dispmanx_update = vc_dispmanx_update_start(0);
		if (osd->vc_dispmanx_update)
		{
      			vc_dispmanx_element_remove(osd->vc_dispmanx_update, osd->dispman_element);
			vc_dispmanx_update_submit_sync(osd->vc_dispmanx_update);
			osd->dispman_element = 0;
		}
	}

	if ((osd) && (osd->dispman_display != DISPMANX_NO_HANDLE)) {
		vc_dispmanx_display_close(osd->dispman_display);

		osd->dispman_display = DISPMANX_NO_HANDLE;
	}

	if (osd) {
		vgDestroyPath(osd->path);
		vgDestroyPaint(osd->paint);
		free(osd);
	}
	logInfo(LOG_OSD, "osdDestroy end.\n");

}

void graphicsColourToPaint(uint32_t col, VGfloat *rgba)
{
	// with OpenVG we use RGB order.
	rgba[0] = ((VGfloat)((col & R_888_MASK) >> 16 )) / 0xff;
	rgba[1] = ((VGfloat)((col & G_888_MASK) >> 8 )) / 0xff;
	rgba[2] = ((VGfloat)((col & B_888_MASK) >> 0 )) / 0xff;
	rgba[3] = ((VGfloat)((col & ALPHA_888_MASK) >> 24)) / 0xff;
}

void osdSetColor(uint32_t color)
{
	graphicsColourToPaint(color, &currentColor); 
}

void osdDrawRect(struct OSD_T *osd, uint32_t xpos, uint32_t ypos, uint32_t width, uint32_t height, uint32_t fill_colour)
{
	EGLBoolean egl_ret = eglMakeCurrent(osd->eglDisplay, osd->surface, osd->surface, osd->context);
	if (egl_ret != EGL_TRUE) {
		EGLint egl_error = eglGetError();
		showEGLErrorStr(egl_error, "eglMakeCurrent");
		return;
	}

	VGfloat rectColour[4];

	graphicsColourToPaint(fill_colour, rectColour);

	vgSeti(VG_SCISSORING, VG_FALSE);

	vgSetfv(VG_CLEAR_COLOR, 4, rectColour);

	vgSetParameteri(osd->paint, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
	vgSetParameterfv(osd->paint, VG_PAINT_COLOR, 4, rectColour);

	VGErrorCode vg_error = vguRect(osd->path, xpos, ypos, width, height);
	if (vg_error) {
		showVGErrorStr(vg_error, "vguRect");
	}
	
}

void osdClear(struct OSD_T *osd)
{
	EGLBoolean egl_ret = eglMakeCurrent(osd->eglDisplay, osd->surface, osd->surface, osd->context);
	if (egl_ret != EGL_TRUE) {
		EGLint egl_error = eglGetError();
		showEGLErrorStr(egl_error, "eglMakeCurrent");
		return;
	}

	VGfloat vg_clear_colour[4];

	graphicsColourToPaint(osd->fill_colour, vg_clear_colour);

	vgSeti(VG_SCISSORING, VG_FALSE);

	vgSetfv(VG_CLEAR_COLOR, 4, vg_clear_colour);
	vgClear(0, 0, osd->width, osd->width);
}

void osdDraw(struct OSD_T *osd)
{
	if (osd == NULL) return;

	EGLBoolean egl_ret;
	egl_ret = eglMakeCurrent(osd->eglDisplay, osd->surface, osd->surface, osd->context);
	if (egl_ret != EGL_TRUE) {
		EGLint egl_error = eglGetError();
		showEGLErrorStr(egl_error, "eglMakeCurrent");
		return;
	}

//	osd->vc_dispmanx_update = vc_dispmanx_update_start(10);
/*	if (osd->vc_dispmanx_update)
	{
		vc_dispmanx_element_remove(osd->vc_dispmanx_update, osd->dispman_element);
		vc_dispmanx_update_submit_sync(osd->vc_dispmanx_update);
		osd->dispman_element = 0;
	}*/

	osdClear(osd);

	VGErrorCode vg_error;

	vgSetPaint(osd->paint, VG_FILL_PATH);
	vg_error = vgGetError();
	if (vg_error) {
		showVGErrorStr(vg_error, "vgSetPaint");
	}

	vgDrawPath(osd->path, VG_FILL_PATH);
	vg_error = vgGetError();
	if (vg_error) {
		showVGErrorStr(vg_error, "vgDrawPath");
	}

/*	if ((osd->visible == 1) && ((xpos != osd->xpos) || (ypos != osd->ypos))) {
		osdHide(osd);
	}

	osd->xpos = xpos;
	osd->ypos = ypos;

	graphics_display_resource(osd->img, 0, osd->layer, xpos, ypos, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, VC_DISPMAN_ROT0, 1);*/
	osd->visible = 1;

	vgFinish();
	logInfo(LOG_OSD, "vgFinish.\n");

	egl_ret = eglSwapBuffers(osd->eglDisplay, osd->surface);
	if (egl_ret != EGL_TRUE) {
		EGLint egl_error = eglGetError();
		showEGLErrorStr(egl_error, "eglSwapBuffers");
		return;
	}
	logInfo(LOG_OSD, "eglSwapBuffers.\n");

}

void osdHide(struct OSD_T *osd)
{
	if ((osd == NULL) || (osd->visible == 0)) return;

	//graphics_display_resource(osd->img, 0, osd->layer, osd->xpos, osd->ypos, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, VC_DISPMAN_ROT0, 0);
	osd->visible = 0;
}

int osdDrawText(struct OSD_T *osd, uint32_t xpos, uint32_t ypos, char *text, const uint32_t text_size)
{
/*	if (osd == NULL) return -1;
	uint32_t text_length = strlen(text);
	//uint32_t width=0, height=0;
	int32_t s=0;
	//int len = 0; // length of pre-subtitle
	//uint32_t img_w, img_h;

//	graphics_get_resource_size(img, &img_w, &img_h);

//	s = graphics_resource_text_dimensions_ext(img, text, text_length, &width, &height, text_size);
//	if (s != 0) return -1;

	s = graphics_resource_render_text_ext(osd->img, xpos, ypos,
		                     GRAPHICS_RESOURCE_WIDTH,
		                     GRAPHICS_RESOURCE_HEIGHT,
		                     GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), 
		                     GRAPHICS_RGBA32(0,0,0,0x80), 
		                     text, text_length, text_size);
	if (s != 0) return -1;
	
	if (osd->visible == 1) {
		graphics_update_displayed_resource(osd->img, osd->xpos, osd->ypos, osd->width, osd->height);
	}
*/
	return 0;

}

#else

int osdInit(int screen)
{
	return 0;
}

struct OSD_T *osdCreate(int layer, uint32_t width, uint32_t height)
{
	return (struct OSD_T *)malloc(sizeof(struct OSD_T));
}

void osdDestroy(struct OSD_T *osd)
{
	free(osd);
}

void osdDrawRect(struct OSD_T *osd, uint32_t xpos, uint32_t ypos, uint32_t width, uint32_t height, uint32_t fill_colour)
{
}

void osdDraw(struct OSD_T *osd)
{
}

void osdHide(struct OSD_T *osd)
{
}

void osdClear(struct OSD_T *osd)
{
}

int osdDrawText(struct OSD_T *osd, uint32_t xpos, uint32_t ypos, char *text, const uint32_t text_size)
{
	printf("text=%s,x=%d,y=%d,text_size=%d\n", text, xpos, ypos, text_size);

	return 0;
}

#endif

