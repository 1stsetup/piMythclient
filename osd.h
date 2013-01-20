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

#include "font.h"

#ifdef PI

#include <VG/vgu.h>

typedef enum {
	OBJECT_TYPE_RECTANGLE	= (1 << 0),
	OBJECT_TYPE_LINE	= (1 << 1)
} OBJECT_TYPE_T;

typedef struct {
	OBJECT_TYPE_T type;
} BASE_OBJECT_T;

typedef struct {
	OBJECT_TYPE_T type;
	uint32_t xpos;
	uint32_t ypos;
	uint32_t width;
	uint32_t height;
	uint32_t fill_colour;
	VGfloat color;
} OBJECT_RECTANGLE_T;

typedef struct {
	OBJECT_TYPE_T type;
	uint32_t xpos1;
	uint32_t ypos1;
	uint32_t xpos2;
	uint32_t ypos2;
	VGfloat color;
} OBJECT_LINE_T;
 
#endif

struct OSD_T{
	int visible;
	int xpos;
	int ypos;
	int width;
	int height;
	int layer;

#ifdef PI

	DISPMANX_DISPLAY_HANDLE_T dispman_display;
	DISPMANX_UPDATE_HANDLE_T vc_dispmanx_update;
	DISPMANX_ELEMENT_HANDLE_T dispman_element;
	EGLDisplay eglDisplay;
	EGLSurface finalSurface;
	EGLContext context;

	VGPath path;
	VGPaint fillPaint;
	VGPaint strokePaint;
	uint32_t fill_colour;

	struct SIMPLELISTITEM_T *actions;
#endif
	FT_Face fontFace;
	int useStatic;
	void (*doPaint)(struct OSD_T *osd);
	uint64_t timeout;
	uint64_t startTime;
};

#ifdef PI
struct OSD_T *osdCreate(int layer, uint32_t width, uint32_t height, uint32_t fill_colour, void (*doPaint)(struct OSD_T *osd));
#else
struct OSD_T *osdCreate(int layer, uint32_t width, uint32_t height);
#endif

void osdDestroy(struct OSD_T *osd);
void osdPaint(struct OSD_T *osd);
void osdShow(struct OSD_T *osd);
void osdHide(struct OSD_T *osd);
void osdClear(struct OSD_T *osd);
int osdDrawText(struct OSD_T *osd, uint32_t xpos, uint32_t ypos, char *text, const uint32_t text_size, uint32_t border_colour, uint32_t fill_colour, int doFill);
void osdSetColor(uint32_t color);
void osdDrawRect(struct OSD_T *osd, uint32_t xpos, uint32_t ypos, uint32_t width, uint32_t height, uint32_t border_colour, uint32_t fill_colour, int doFill);
int osdSetFont(struct OSD_T *osd, char *fontFile);
void osdShowWithTimeoutMicroseconds(struct OSD_T *osd, uint64_t timeout);
void osdShowWithTimeoutSeconds(struct OSD_T *osd, uint64_t timeout);
