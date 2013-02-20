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

#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <libavcodec/avcodec.h>

#include "bcm.h"
#include "globalFunctions.h"
#include "lists.h"
#include "omxCore.h"
#include "omxVideo.h"

OMX_ERRORTYPE omxSetVideoSetExtraBuffers(struct OMX_COMPONENT_T *component)
{
	OMX_PARAM_U32TYPE extraBuffers;
	OMX_INIT_STRUCTURE(extraBuffers);
	extraBuffers.nU32 = 3;

	OMX_ERRORTYPE omxErr;
	omxErr = OMX_SetParameter(component->handle, OMX_IndexParamBrcmExtraBuffers, &extraBuffers);

	if(omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error SetParameter OMX_IndexParamBrcmExtraBuffers for component %s omxErr(0x%08x)\n", component->componentName, (int)omxErr);
		return omxErr;
	}

	return OMX_ErrorNone;
}

/*
 * type == 0 OMX_ImageFilterDeInterlaceLineDouble,
 * type == 1 OMX_ImageFilterDeInterlaceAdvanced,
 */

OMX_ERRORTYPE omxSetVideoDeInterlace(struct OMX_COMPONENT_T *component, int type)
{
	OMX_CONFIG_IMAGEFILTERPARAMSTYPE image_filter;
	OMX_INIT_STRUCTURE(image_filter);

	image_filter.nPortIndex = component->outputPort;
	image_filter.nNumParams = 1;
	image_filter.nParams[0] = 3; // Looks like this is OMX_CONFIG_INTERLACETYPE.eMode
//	image_filter.nParams[1] = 0; // Then this will probably be OMX_CONFIG_INTERLACETYPE.bRepeatFirstField
	image_filter.eImageFilter = (type == 1) ? OMX_ImageFilterDeInterlaceAdvanced : OMX_ImageFilterDeInterlaceLineDouble;

	OMX_ERRORTYPE omxErr;
	omxErr = OMX_SetConfig(component->handle, OMX_IndexConfigCommonImageFilterParameters, &image_filter);

	if(omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error SetConfig OMX_IndexConfigCommonImageFilterParameters for port %d status on component %s omxErr(0x%08x)\n", component->outputPort, component->componentName, (int)omxErr);
		return omxErr;
	}

	return OMX_ErrorNone;
}

OMX_ERRORTYPE omxGetVideoInterlace(struct OMX_COMPONENT_T *component, OMX_CONFIG_INTERLACETYPE *videoInterlace)
{
	OMX_INIT_STRUCTURE(*videoInterlace);

	videoInterlace->nPortIndex = component->outputPort;

	OMX_ERRORTYPE omxErr;
	omxErr = OMX_GetConfig(component->handle, OMX_IndexConfigCommonInterlace, videoInterlace);

	if(omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error SetConfig OMX_IndexConfigCommonInterlace for port %d status on component %s omxErr(0x%08x)\n", component->outputPort, component->componentName, (int)omxErr);
		return omxErr;
	}

	return OMX_ErrorNone;
}
	
OMX_ERRORTYPE omxShowVideoInterlace(struct OMX_COMPONENT_T *component)
{
	OMX_CONFIG_INTERLACETYPE videoInterlace;

	OMX_ERRORTYPE omxErr;
	omxErr = omxGetVideoInterlace(component, &videoInterlace); 
	if(omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error SetConfig OMX_IndexConfigCommonInterlace for port %d status on component %s omxErr(0x%08x)\n", component->outputPort, component->componentName, (int)omxErr);
		return omxErr;
	}

//   OMX_InterlaceProgressive,                    /**< The data is not interlaced, it is progressive scan */
//   OMX_InterlaceFieldSingleUpperFirst,          /**< The data is interlaced, fields sent
//                                                     separately in temporal order, with upper field first */
//   OMX_InterlaceFieldSingleLowerFirst,          /**< The data is interlaced, fields sent
//                                                     separately in temporal order, with lower field first */
//   OMX_InterlaceFieldsInterleavedUpperFirst,    /**< The data is interlaced, two fields sent together line
//                                                     interleaved, with the upper field temporally earlier */
//   OMX_InterlaceFieldsInterleavedLowerFirst,    /**< The data is interlaced, two fields sent together line
//                                                     interleaved, with the lower field temporally earlier */
//   OMX_InterlaceMixed,                          /**< The stream may contain a mixture of progressive

	switch (videoInterlace.eMode) {
	case OMX_InterlaceProgressive: logInfo(LOG_OMX, "VideoInterlace is OMX_InterlaceProgressive (%d).\n", videoInterlace.eMode); break;
	case OMX_InterlaceFieldSingleUpperFirst: logInfo(LOG_OMX, "VideoInterlace is OMX_InterlaceFieldSingleUpperFirst (%d).\n", videoInterlace.eMode); break;
	case OMX_InterlaceFieldSingleLowerFirst: logInfo(LOG_OMX, "VideoInterlace is OMX_InterlaceFieldSingleLowerFirst (%d).\n", videoInterlace.eMode); break;
	case OMX_InterlaceFieldsInterleavedUpperFirst: logInfo(LOG_OMX, "VideoInterlace is OMX_InterlaceFieldsInterleavedUpperFirst (%d).\n", videoInterlace.eMode); break;
	case OMX_InterlaceFieldsInterleavedLowerFirst: logInfo(LOG_OMX, "VideoInterlace is OMX_InterlaceFieldsInterleavedLowerFirst (%d).\n", videoInterlace.eMode); break;
	case OMX_InterlaceMixed: logInfo(LOG_OMX, "VideoInterlace is OMX_InterlaceMixed.\n"); break;
	default:
		logInfo(LOG_OMX, "VideoInterlace is UNKNOWN (%d).\n", videoInterlace.eMode);
	}

	logInfo(LOG_OMX, "VideoInterlace bRepeatFirstField = %d.\n", videoInterlace.bRepeatFirstField);

	return OMX_ErrorNone;
}

OMX_ERRORTYPE omxSetVideoCompressionFormat(struct OMX_COMPONENT_T *component, OMX_IMAGE_CODINGTYPE compressionFormat)
{
	pthread_mutex_lock(&component->componentMutex);

	OMX_VIDEO_PARAM_PORTFORMATTYPE formatType;
	OMX_INIT_STRUCTURE(formatType);
	formatType.nPortIndex = component->inputPort;
	formatType.eCompressionFormat = compressionFormat;

	OMX_ERRORTYPE omxErr;
	omxErr = OMX_SetParameter(component->handle, OMX_IndexParamVideoPortFormat, &formatType);
	if(omxErr != OMX_ErrorNone)
	{
		logInfo(LOG_OMX, "error OMX_IndexParamVideoPortFormat omxErr(0x%08x)\n", omxErr);
		return omxErr;
	}

	pthread_mutex_unlock(&component->componentMutex);

	return omxErr;
}

OMX_ERRORTYPE omxSetVideoCompressionFormatAndFrameRate(struct OMX_COMPONENT_T *component, OMX_IMAGE_CODINGTYPE compressionFormat,OMX_U32 framerate)
{
	pthread_mutex_lock(&component->componentMutex);

	OMX_VIDEO_PARAM_PORTFORMATTYPE formatType;
	OMX_INIT_STRUCTURE(formatType);
	formatType.nPortIndex = component->inputPort;
	formatType.eCompressionFormat = compressionFormat;

//	framerate = 50;

	if (framerate > 0) {
		formatType.xFramerate = framerate * (1<<16);
	}
	else {
		formatType.xFramerate = 25 * (1<<16);
	}

	OMX_ERRORTYPE omxErr;
	omxErr = OMX_SetParameter(component->handle, OMX_IndexParamVideoPortFormat, &formatType);
	if(omxErr != OMX_ErrorNone)
	{
		logInfo(LOG_OMX, "error OMX_IndexParamVideoPortFormat omxErr(0x%08x)\n", omxErr);
		return omxErr;
	}

	pthread_mutex_unlock(&component->componentMutex);

	return omxErr;
}

OMX_ERRORTYPE omxVideoSetFrameSize(struct OMX_COMPONENT_T *component, unsigned int width, unsigned int height)
{
	OMX_PARAM_PORTDEFINITIONTYPE portParam;
	OMX_INIT_STRUCTURE(portParam);
	portParam.nPortIndex = component->inputPort;

	OMX_ERRORTYPE omxErr;
	omxErr = OMX_GetParameter(component->handle, OMX_IndexParamPortDefinition, &portParam);
	if(omxErr != OMX_ErrorNone)
	{
		logInfo(LOG_OMX, "error OMX_GetParameter of OMX_IndexParamPortDefinition omxErr(0x%08x)\n", omxErr);
		return omxErr;
	}

	portParam.nPortIndex = component->inputPort;
	portParam.nBufferCountActual = OMX_VIDEO_BUFFERS;

	portParam.format.video.nFrameWidth  = width;
	portParam.format.video.nFrameHeight = height;

	omxErr = OMX_SetParameter(component->handle, OMX_IndexParamPortDefinition, &portParam);
	if(omxErr != OMX_ErrorNone)
	{
		logInfo(LOG_OMX, "error OMX_SetParameter OMX_IndexParamPortDefinition omxErr(0x%08x)\n", omxErr);
		return omxErr;
	}

	return omxErr;
}

OMX_ERRORTYPE omxVideoStartWithValidFrame(struct OMX_COMPONENT_T *component, int startWithValidFrame)
{
	OMX_PARAM_BRCMVIDEODECODEERRORCONCEALMENTTYPE concanParam;
	OMX_INIT_STRUCTURE(concanParam);
	concanParam.bStartWithValidFrame = (startWithValidFrame == 1) ? OMX_TRUE : OMX_FALSE;

	OMX_ERRORTYPE omxErr;
	omxErr = OMX_SetParameter(component->handle, OMX_IndexParamBrcmVideoDecodeErrorConcealment, &concanParam);

	return omxErr;
}

#endif
