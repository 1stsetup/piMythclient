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


#ifdef PI

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "vgfont.h"
#include "graphics_x_private.h"
#include "graphicsMain.h"
#include "graphicsMainPrivate.h"

static int inited = 0;

#define CHANGE_LAYER (1<<0)
#define CHANGE_OPACITY (1<<1)
#define CHANGE_DEST (1<<2)
#define CHANGE_SRC (1<<3)
#define CHANGE_MASK (1<<4)
#define CHANGE_XFORM (1<<5)

#define MAX_DISPLAY_HANDLES 4

static DISPMANX_DISPLAY_HANDLE_T screens[MAX_DISPLAY_HANDLES];

VCOS_LOG_CAT_T gx_log_cat; /*< Logging category for GraphicsX. */

static void showEGLErrorStr(EGLint egl_error, char *functionStr, char *userStr)
{
		if (egl_error == EGL_NO_SURFACE) printf("%s: %s: EGL_NO_SURFACE\n", functionStr, userStr);
		if (egl_error == EGL_BAD_DISPLAY) printf("%s: %s: EGL_BAD_DISPLAY\n", functionStr, userStr);
		if (egl_error == EGL_NOT_INITIALIZED) printf("%s: %s: EGL_NOT_INITIALIZED\n", functionStr, userStr);
		if (egl_error == EGL_BAD_CONFIG) printf("%s: %s: EGL_BAD_CONFIG\n", functionStr, userStr);
		if (egl_error == EGL_BAD_NATIVE_WINDOW) printf("%s: %s: EGL_BAD_NATIVE_WINDOW\n", functionStr, userStr);
		if (egl_error == EGL_BAD_ATTRIBUTE) printf("%s: %s: EGL_BAD_ATTRIBUTE\n", functionStr, userStr);
		if (egl_error == EGL_BAD_ALLOC) printf("%s: %s: EGL_BAD_ALLOC\n", functionStr, userStr);
		if (egl_error == EGL_BAD_MATCH) printf("%s: %s: EGL_BAD_MATCH\n", functionStr, userStr);
		if (egl_error == EGL_BAD_SURFACE) printf("%s: %s: EGL_BAD_SURFACE\n", functionStr, userStr);
		if (egl_error == EGL_CONTEXT_LOST) printf("%s: %s: EGL_CONTEXT_LOST\n", functionStr, userStr);
		if (egl_error == EGL_BAD_CONTEXT) printf("%s: %s: EGL_BAD_CONTEXT\n", functionStr, userStr);
		if (egl_error == EGL_BAD_ACCESS) printf("%s: %s: EGL_BAD_ACCESS\n", functionStr, userStr);
		if (egl_error == EGL_BAD_NATIVE_PIXMAP) printf("%s: %s: EGL_BAD_NATIVE_PIXMAP\n", functionStr, userStr);
		if (egl_error == EGL_BAD_CURRENT_SURFACE) printf("%s: %s: EGL_BAD_CURRENT_SURFACE\n", functionStr, userStr);
		if (egl_error == EGL_BAD_PARAMETER) printf("%s: %s: EGL_BAD_PARAMETER\n", functionStr, userStr);

		printf("%s: %s: 0x%x\n", functionStr, userStr, egl_error);
}

/** Convert graphics_x colour formats into EGL format. */
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


/* Create an EGLContext for a given GRAPHICS_RESOURCE_TYPE */
VCOS_STATUS_T createContext(struct DISPLAY_T *disp, struct CONTEXT_T *context)
{
   int n;
   EGLint nconfigs;//, attribs[32];
   n = graphicsEGLAttribColours(&(context->attribs[0]));

   // we want to be able to do OpenVG on this surface...
   context->attribs[n++] = EGL_RENDERABLE_TYPE; 
   context->attribs[n++] = EGL_OPENVG_BIT;
   context->attribs[n++] = EGL_SURFACE_TYPE; 
   context->attribs[n++] = EGL_WINDOW_BIT;

   context->attribs[n] = EGL_NONE;

   EGLBoolean egl_ret = eglChooseConfig(disp->display,
                                        &(context->attribs[0]), context->configs,
                                        countof(context->configs), &nconfigs);

   if (!egl_ret || !nconfigs)
   {
	EGLint egl_error = eglGetError();
	showEGLErrorStr(egl_error, "createContext", "eglChooseConfig");
      return VCOS_EINVAL;
   }

   EGLContext cxt = eglCreateContext(disp->display, context->configs[0], context->ctx, 0);
   if (!cxt)
   {
	EGLint egl_error = eglGetError();
	showEGLErrorStr(egl_error, "createContext", "eglCreateContext");
      return VCOS_ENOSPC;
   }
   
//   gx_configs[image_type] = configs[0];
   context->ctx = cxt;
   context->display = disp;

   return VCOS_SUCCESS;
}

void destroyContext(struct CONTEXT_T *context)
{
   eglDestroyContext(context->display->display,context->ctx);
}

static VCOS_STATUS_T graphicsInitialize(struct DISPLAY_T *display, EGLenum api)
{
	EGLDisplay disp;
	EGLint egl_maj, egl_min;
	int32_t ret = VCOS_EINVAL;
	EGLBoolean result;

	if (inited != 0) {
		return ret;
	}

	int i;
	for(i=0;i<MAX_DISPLAY_HANDLES;i++) {
		screens[i] = DISPMANX_NO_HANDLE;
	}

	disp = eglGetDisplay(EGL_DEFAULT_DISPLAY);

	if (disp == EGL_NO_DISPLAY)
	{
		EGLint egl_error = eglGetError();
		showEGLErrorStr(egl_error, "graphicsInitialize", "eglGetDisplay");
		goto fail_disp;
	}

	result = eglInitialize(disp, &egl_maj, &egl_min);
	if (result == EGL_FALSE)
	{
		EGLint egl_error = eglGetError();
		showEGLErrorStr(egl_error, "graphicsInitialize", "eglInitialize");
		goto fail_egl_init;
	}

#ifdef DEBUG
	printf("graphicsInitialize: eglVersion :egl_maj=%d, egl_min=%d\n", egl_maj, egl_min);
#endif

	display->api = api;
	result = eglBindAPI(api);
	if (result == EGL_FALSE)
	{
		EGLint egl_error = eglGetError();
		showEGLErrorStr(egl_error, "graphicsInitialize", "eglBindAPI");
		goto fail_egl_init;
	}

	display->display = disp;

	inited = 1;

	ret = VCOS_SUCCESS;
#ifdef DEBUG
	printf("haha. %s.\n",eglQueryString(disp, EGL_CLIENT_APIS));
#endif
	return ret;

fail_egl_init:
fail_disp:
	free(disp);
	return ret;
}

VCOS_STATUS_T graphicsInit(struct DISPLAY_T *disp, EGLenum api, const char *font_dir)
{
	VCOS_STATUS_T rc;

	rc = graphicsInitialize(disp, api);
	if (rc == VCOS_SUCCESS)
		rc = gx_priv_font_init(font_dir);

	return rc;
}

/*   // create the available contexts
   EGLContext shared_context = EGL_NO_CONTEXT;
   ret = createContext(disp, &shared_context);

   if (ret != VCOS_SUCCESS)
      goto fail_cxt;
*/
void graphicsDestroy(struct DISPLAY_T *disp)
{
	eglTerminate(disp->display);
}

static int32_t graphicsOpenScreen(uint32_t index)
{
	if (screens[index] == DISPMANX_NO_HANDLE) {
		screens[index] = vc_dispmanx_display_open(index);
		if (screens[index] == DISPMANX_NO_HANDLE)
		{
			 printf("Could not open dispmanx display %d.\n", index);
			 return 0;
		}
#ifdef DEBUG
		printf("graphicsOpenScreen: opened screen %d (handle=0x%x).\n", index, screens[index]);
#endif
	}
#ifdef DEBUG
	else {
		printf("graphicsOpenScreen: screen %d is already open (handle=0x%x).\n", index, screens[index]);
	}
#endif
	return 1;
}

void graphicsCloseScreen(uint32_t index)
{
	if (screens[index] != DISPMANX_NO_HANDLE) {
		vc_dispmanx_display_close(screens[index]);
#ifdef DEBUG
		printf("graphicsCloseScreen: closed screen %d (handle=0x%x).\n", index, screens[index]);
#endif
		screens[index] = DISPMANX_NO_HANDLE;
	}
#ifdef DEBUG
	else {
		printf("graphicsCloseScreen: screen %d is not open.\n", index, screens[index]);
	}
#endif
}

int graphicsIsScreenOpen(uint32_t index)
{
	if (screens[index] == DISPMANX_NO_HANDLE) {
		return 0;
	}
	else {
		return 1;
	}
}

int32_t graphicsGetDisplaySize( const uint16_t display_number,
                                   uint32_t *width,
                                   uint32_t *height)
{
	DISPMANX_MODEINFO_T mode_info;
	int32_t success = -1;
	DISPMANX_DISPLAY_HANDLE_T disp;
	vcos_assert(width && height);
	*width = *height = 0;

	if(display_number < MAX_DISPLAY_HANDLES)
	{
		// TODO Shouldn't this close the display if it wasn't previously open?

		int screenWasOpen = graphicsIsScreenOpen(display_number);
		if (screenWasOpen == 0) {
			if (graphicsOpenScreen(display_number) == 0)
			{
				printf("graphicsGetDisplaySize: error graphicsOpenScreen.\n");
				return -1;
			}
		}
		success = vc_dispmanx_display_get_info(screens[display_number], &mode_info);

		if (screenWasOpen == 0) {
			graphicsCloseScreen(display_number);
		}

		if( success >= 0 )
		{
			*width = mode_info.width;
			*height = mode_info.height;
			vcos_assert(*height > 64);
		}
		else
		{
			printf("graphicsGetDisplaySize: error vc_dispmanx_display_get_info.\n");
		}
	}

	return success;
}

VCOS_STATUS_T graphicsCreateWindow(struct CONTEXT_T *ctx,
				uint32_t screen_id,
                                uint32_t xpos,
                                uint32_t ypos,
                                uint32_t width,
                                uint32_t height,
				struct WINDOW_T *window)
{
	VC_RECT_T dst_rect;
	VC_RECT_T src_rect;
	VCOS_STATUS_T status = VCOS_EINVAL;
	EGLBoolean egl_result;

	window->screen_id = screen_id;

	int screenWasOpen = graphicsIsScreenOpen(screen_id);
	if (screenWasOpen == 0) {
		if (graphicsOpenScreen(screen_id) == 0)
		{
			printf("graphicsCreateWindow: graphicsOpenScreen failed.\n");
			goto fail_screen;
		}
	}

	window->vc_dispmanx_update = vc_dispmanx_update_start(0);
	if (!window->vc_dispmanx_update)
	{
		printf("graphicsCreateWindow: Could not start update on screen %d.\n", screen_id);
		goto fail_update;
	}
	
	src_rect.x = 0;
	src_rect.y = 0;
	src_rect.width = width << 16;
	src_rect.height = height << 16;

	dst_rect.x = 0;
	dst_rect.y = 0;
	dst_rect.width = 1;
	dst_rect.height = 1;
/*
	dst_rect.x = xpos;
	dst_rect.y = ypos;
	dst_rect.width = width << 16;
	dst_rect.height = height << 16;
*/
	window->xpos = xpos;
	window->ypos = ypos;
	window->width = width;
	window->height = height;

	window->egl_win.width = width;
	window->egl_win.height = height;

	VC_DISPMANX_ALPHA_T alpha;
	memset(&alpha, 0x0, sizeof(VC_DISPMANX_ALPHA_T));
	alpha.flags = DISPMANX_FLAGS_ALPHA_FROM_SOURCE;

	DISPMANX_CLAMP_T clamp;
	memset(&clamp, 0x0, sizeof(DISPMANX_CLAMP_T));

	window->egl_win.element = vc_dispmanx_element_add(window->vc_dispmanx_update, screens[window->screen_id],
			0 /* layer */, &dst_rect,
			0 /* src */, &src_rect,
			DISPMANX_PROTECTION_NONE,
			&alpha /* alpha */,
			&clamp /* clamp */,
			0 /* transform */);

	if ( !window->egl_win.element )
	{
		printf("graphicsCreateWindow: Could not add element %dx%d\n",width,height);
		vc_dispmanx_update_submit_sync(window->vc_dispmanx_update);
		goto fail_screen;
	}

	window->ctx = ctx;
	window->surface = eglCreateWindowSurface(ctx->display->display, ctx->configs[0], &(window->egl_win), NULL);
	if (!window->surface)
	{
		EGLint egl_error = eglGetError();
		showEGLErrorStr(egl_error, "graphicsCreateWindow", "Could not create window surface");

		status = VCOS_ENOMEM;
		goto fail_win;
	}

	egl_result = eglSurfaceAttrib(ctx->display->display, window->surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED);

	if (egl_result != EGL_TRUE) {

		EGLint egl_error = eglGetError();
		showEGLErrorStr(egl_error, "graphicsCreateWindow", "Could not set surface surface attrib");
		status = VCOS_ENOMEM;
		goto fail_win;
	}

	vc_dispmanx_update_submit_sync(window->vc_dispmanx_update);

/*	egl_result = eglSwapBuffers(ctx->display->display, window->surface);

	if (egl_result != EGL_TRUE) {
		printf("graphicsCreateWindow: Could not swap buffer: 0x%x\n", eglGetError());
		status = VCOS_ENOMEM;
		goto fail_win;
	}*/

	status = VCOS_SUCCESS;
	window->visible = 0;
	return status;

fail_update:
	if (screenWasOpen == 0) {
		graphicsCloseScreen(window->screen_id);
	}
fail_screen:
	return VCOS_EINVAL;
fail_win:
	graphicsDestroyWindow(window);
	return status;
}

void graphicsDestroyWindow(struct WINDOW_T *window)
{
	if(window->vc_dispmanx_update != 0)
	{
		int ret = vc_dispmanx_element_remove(window->vc_dispmanx_update, window->egl_win.element);
		vcos_assert(ret == 0);
		ret = vc_dispmanx_update_submit_sync(window->vc_dispmanx_update);
		vcos_assert(ret == 0);
	}

//	graphicsCloseScreen(window->screen_id);
}


EGLBoolean graphicSetCurrentWindow(struct WINDOW_T *window)
{
	EGLBoolean egl_result;

	egl_result = eglBindAPI(window->ctx->display->api);
	if (egl_result != EGL_TRUE) {
		EGLint egl_error = eglGetError();
		showEGLErrorStr(egl_error, "graphicSetCurrentWindow", "error eglBindAPI");
		return egl_result;
	}
	vcos_assert(egl_result); // really should succeed

	egl_result = eglMakeCurrent(window->ctx->display->display, window->surface,
                                  window->surface, window->ctx->ctx);
	if (egl_result != EGL_TRUE) {
		EGLint egl_error = eglGetError();
		showEGLErrorStr(egl_error, "graphicSetCurrentWindow", "error eglMakeCurrent");
	}

#ifdef DEBUG
		printf("graphicSetCurrentWindow: set current Window.\n");
#endif
	return egl_result;
}

void graphicSaveCurrentWindow(struct CURRENT_WINDOW_T *currentWindow)
{
	currentWindow->ctx = eglGetCurrentContext();
	currentWindow->display = eglGetCurrentDisplay();
	currentWindow->readSurface = eglGetCurrentSurface(EGL_READ);
	currentWindow->drawSurface = eglGetCurrentSurface(EGL_DRAW);
	currentWindow->api = eglQueryAPI();
#ifdef DEBUG
		printf("graphicSaveCurrentWindow: saved current Window.\n");
#endif
}

EGLBoolean graphicRestoreCurrentWindow(struct CURRENT_WINDOW_T *currentWindow)
{
	EGLBoolean egl_result = EGL_FALSE;

	if (currentWindow->api != EGL_NONE) {
		egl_result = eglBindAPI(currentWindow->api);
		if (egl_result != EGL_TRUE) {
			EGLint egl_error = eglGetError();
			showEGLErrorStr(egl_error, "graphicRestoreCurrentWindow", "error eglBindAPI");
			return egl_result;
		}
	}
#ifdef DEBUG
	else {
		printf("graphicRestoreCurrentWindow: currentWindow->api == EGL_NONE.\n");
	}
#endif

	if ((currentWindow->display != EGL_NO_DISPLAY) && (currentWindow->ctx != EGL_NO_CONTEXT) && (currentWindow->drawSurface != EGL_NO_SURFACE)) {
		egl_result = eglMakeCurrent(currentWindow->display, currentWindow->drawSurface,
		                          currentWindow->readSurface, currentWindow->ctx);
		if (egl_result != EGL_TRUE) {
			EGLint egl_error = eglGetError();
			showEGLErrorStr(egl_error, "graphicRestoreCurrentWindow", "error eglMakeCurrent");
		}
	}
#ifdef DEBUG
	else {
		printf("graphicRestoreCurrentWindow: could not restore oldCurrent window.\n");
	}
#endif

	return egl_result;
}

void graphicsColourToPaint(uint32_t col, VGfloat *rgba)
{
	// with OpenVG we use RGB order.
	rgba[0] = ((VGfloat)((col & R_888_MASK) >> 16 )) / 0xff;
	rgba[1] = ((VGfloat)((col & G_888_MASK) >> 8 )) / 0xff;
	rgba[2] = ((VGfloat)((col & B_888_MASK) >> 0 )) / 0xff;
	rgba[3] = ((VGfloat)((col & ALPHA_888_MASK) >> 24)) / 0xff;
}

/** Fill an area of a surface with a fixed colour.
*/
VCOS_STATUS_T graphicsFillSquare(struct WINDOW_T *window,
                               uint32_t x,
                               uint32_t y,
                               uint32_t width,
                               uint32_t height,
                               uint32_t fill_colour )
{
	struct CURRENT_WINDOW_T currentWindow;

	VGfloat vg_clear_colour[4];

	graphicsColourToPaint(fill_colour, vg_clear_colour);

	if (window->visible == 0) {
		graphicSaveCurrentWindow(&currentWindow);
	}

	graphicSetCurrentWindow(window);

	vgSeti(VG_SCISSORING, VG_FALSE);

	vgSetfv(VG_CLEAR_COLOR, 4, vg_clear_colour);
	vgClear(x, y, width, height);

	int err = vgGetError();

	if (window->visible == 0) {
		graphicRestoreCurrentWindow(&currentWindow);
	}

	if (err)
	{
		printf("vg error %x filling area.\n", err);
		return err;
	}

	return VCOS_SUCCESS;
}

/*
	if ((offset_x != res->dest.x) ||
	  (offset_y != res->dest.y) ||
	  (h != res->dest.height) ||
	  (w != res->dest.width))
	{
		change_flags |= CHANGE_DEST;
		res->dest.x = offset_x;
		res->dest.y = offset_y;
		res->dest.height = h;
		res->dest.width = w;
	}

*/

VCOS_STATUS_T graphicsShowWindow(struct WINDOW_T *window)
{
	if (window->visible == 1) return VCOS_SUCCESS;

	window->vc_dispmanx_update = vc_dispmanx_update_start(0);
	if (!window->vc_dispmanx_update)
	{
		printf("graphicsShowWindow: Could not start update .\n");
		return VCOS_EINVAL;
	}

	graphicsOpenScreen(window->screen_id);

	graphicSetCurrentWindow(window);

	VC_RECT_T dst_rect;
	VC_RECT_T src_rect;

	uint32_t change_flags = CHANGE_LAYER;

	vc_dispmanx_rect_set( &src_rect, 0, 0, window->width<<16, window->height<<16 );
	vc_dispmanx_rect_set( &dst_rect, window->xpos, window->ypos, window->width<<16, window->height<<16);

	int32_t rc = vc_dispmanx_element_change_attributes(window->vc_dispmanx_update,
			window->egl_win.element,
			change_flags,
			1, /* layer */
			0xff, /* opacity */
			&dst_rect,
			&src_rect,
			0, VC_DISPMAN_ROT0);

	if (rc != 0) {
		printf("graphicsShowWindow: error vc_dispmanx_element_change_attributes %d.\n", rc);
		return VCOS_EINVAL;
	}

	EGLBoolean result;
	result = eglSwapInterval(window->ctx->display->display, 1);
	if (result == EGL_FALSE)
	{
		EGLint egl_error = eglGetError();
		showEGLErrorStr(egl_error, "graphicsCreateWindow", "eglSwapInterval");
		return VCOS_EINVAL;
	}

	result = eglSwapBuffers(window->ctx->display->display, window->surface);
	if (result != EGL_TRUE) {
		EGLint egl_error = eglGetError();
		showEGLErrorStr(egl_error, "graphicsShowWindow", "Could not eglSwapBuffers");
		return VCOS_EINVAL;
	}

	result = eglWaitClient();
	if (result != EGL_TRUE) {
		EGLint egl_error = eglGetError();
		showEGLErrorStr(egl_error, "graphicsShowWindow", "Could not eglWaitClient");
		return VCOS_EINVAL;
	}

	if (vc_dispmanx_update_submit_sync( window->vc_dispmanx_update ) != 0) {
		printf("graphicsShowWindow: error vc_dispmanx_update_submit_sync.\n");
		return VCOS_EINVAL;
	}

	window->visible = 1;

	return VCOS_SUCCESS;
}

VCOS_STATUS_T graphicsHideWindow(struct WINDOW_T *window)
{
}


#endif
