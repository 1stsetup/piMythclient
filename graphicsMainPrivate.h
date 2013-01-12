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


