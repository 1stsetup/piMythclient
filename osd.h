#include "EGL/egl.h"
#include "EGL/eglext.h"

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
	EGLSurface surface;
	EGLContext context;

	VGPath path;
	VGPaint paint;
	uint32_t fill_colour;

	struct SIMPLELISTITEM_T *actions;
#endif
};

#ifdef PI
struct OSD_T *osdCreate(int layer, uint32_t width, uint32_t height, uint32_t fill_colour);
#else
struct OSD_T *osdCreate(int layer, uint32_t width, uint32_t height);
#endif

void osdDestroy(struct OSD_T *osd);
void osdDraw(struct OSD_T *osd);
void osdHide(struct OSD_T *osd);
void osdClear(struct OSD_T *osd);
int osdDrawText(struct OSD_T *osd, uint32_t xpos, uint32_t ypos, char *text, const uint32_t text_size);
void osdSetColor(uint32_t color);
void osdDrawRect(struct OSD_T *osd, uint32_t xpos, uint32_t ypos, uint32_t width, uint32_t height, uint32_t fill_colour);

