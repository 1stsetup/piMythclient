#define VCOS_LOG_CATEGORY (&gx_log_cat)

#include "EGL/egl.h"
#include "EGL/eglext.h"
#include "VG/openvg.h"
#include "VG/vgu.h"

#include "vgfont.h"
#include "bcm_host.h"

extern VCOS_LOG_CAT_T gx_log_cat;

#define LOG_ERR( fmt, arg... ) vcos_log_error( "%s:%d " fmt, __func__, __LINE__, ##arg)

#define GX_ERROR(format, arg...) if (1) {} else printf( format "\n", ##arg)
#define GX_LOG(format, arg...) if (1) {} else printf( format "\n", ##arg)
#define GX_TRACE(format, arg...) if (1) {} else printf( format "\n", ##arg)


