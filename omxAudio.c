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

#define OMX_MAX_CHANNELS 10

static enum OMX_AUDIO_CHANNELTYPE OMXChannels[OMX_MAX_CHANNELS] =
{
  OMX_AUDIO_ChannelLF, OMX_AUDIO_ChannelRF,
  OMX_AUDIO_ChannelCF, OMX_AUDIO_ChannelLFE,
  OMX_AUDIO_ChannelLR, OMX_AUDIO_ChannelRR,
  OMX_AUDIO_ChannelLS, OMX_AUDIO_ChannelRS,
  OMX_AUDIO_ChannelCS, OMX_AUDIO_ChannelNone
};

OMX_ERRORTYPE omxSetAudioRenderInput(struct OMX_COMPONENT_T *component, int sample_rate, int bits_per_coded_sample, int channels)
{
	OMX_ERRORTYPE omxErr;

	OMX_AUDIO_PARAM_PCMMODETYPE pcmInput;

	OMX_INIT_STRUCTURE(pcmInput);
	pcmInput.nPortIndex = component->inputPort;

	pcmInput.eNumData		= OMX_NumericalDataSigned;
	pcmInput.eEndian		= OMX_EndianLittle;
	pcmInput.bInterleaved		= OMX_TRUE;
	pcmInput.nBitPerSample		= bits_per_coded_sample;
	pcmInput.ePCMMode		= OMX_AUDIO_PCMModeLinear;
	pcmInput.nChannels		= channels;
	pcmInput.nSamplingRate		= sample_rate;
	pcmInput.eChannelMapping[0]	= OMX_AUDIO_ChannelLF;
	pcmInput.eChannelMapping[1]	= OMX_AUDIO_ChannelRF;

	omxErr = OMX_SetParameter(component->handle, OMX_IndexParamAudioPcm, &pcmInput);
	if(omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error SetParameter OMX_IndexParamAudioPcm for port %d status on component %s omxErr(0x%08x)\n", component->inputPort, component->componentName, (int)omxErr);
		return omxErr;
	}

	OMX_AUDIO_PARAM_PORTFORMATTYPE formatType;
	OMX_INIT_STRUCTURE(formatType);
	formatType.nPortIndex = component->inputPort;

	formatType.eEncoding = OMX_AUDIO_CodingPCM;

	omxErr = OMX_SetParameter(component->handle, OMX_IndexParamAudioPortFormat, &formatType);
	if(omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error SetParameter OMX_IndexParamAudioPortFormat for port %d status on component %s omxErr(0x%08x)\n", component->inputPort, component->componentName, (int)omxErr);
		return omxErr;
	}

	OMX_PARAM_PORTDEFINITIONTYPE portDef;
	OMX_INIT_STRUCTURE(portDef);
	portDef.nPortIndex = component->inputPort;

	portDef.format.audio.eEncoding = OMX_AUDIO_CodingPCM;

	portDef.nBufferSize = ((6144 * 2) + 15) & ~15;
	portDef.nBufferCountActual = 60;

	omxErr = OMX_SetParameter(component->handle, OMX_IndexParamPortDefinition, &portDef);
	if(omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error SetParameter OMX_IndexParamPortDefinition for port %d status on component %s omxErr(0x%08x)\n", component->inputPort, component->componentName, (int)omxErr);
		return omxErr;
	}

	return OMX_ErrorNone;
}

OMX_ERRORTYPE omxSetAudioCompressionFormatAndBuffer(struct OMX_COMPONENT_T *component, enum AVCodecID codec, 
				int sample_rate, int bits_per_coded_sample, int channels, int audioPassthrough)
{
	OMX_AUDIO_CODINGTYPE encoding;

	switch(codec) { 
	/*
	case CODEC_ID_VORBIS:
	CLog::Log(LOGDEBUG, "COMXAudio::CanHWDecode OMX_AUDIO_CodingVORBIS\n");
	m_eEncoding = OMX_AUDIO_CodingVORBIS;
	m_HWDecode = true;
	break;
	case CODEC_ID_AAC:
	CLog::Log(LOGDEBUG, "COMXAudio::CanHWDecode OMX_AUDIO_CodingAAC\n");
	m_eEncoding = OMX_AUDIO_CodingAAC;
	m_HWDecode = true;
	break;
	*/
	case CODEC_ID_MP2:
	case CODEC_ID_MP3:
		logInfo(LOG_OMX_DEBUG, "OMX_AUDIO_CodingMP3\n");
		encoding = OMX_AUDIO_CodingMP3;
		component->useHWDecode = 1;
		break;
	case CODEC_ID_DTS:
		logInfo(LOG_OMX_DEBUG, "OMX_AUDIO_CodingDTS\n");
		encoding = OMX_AUDIO_CodingDTS;
		component->useHWDecode = 1;
		break;
	case CODEC_ID_AC3:
	case CODEC_ID_EAC3:
		logInfo(LOG_OMX_DEBUG, "OMX_AUDIO_CodingDDP\n");
		encoding = OMX_AUDIO_CodingDDP;
		component->useHWDecode = 1;
		break;
	default:
		logInfo(LOG_OMX_DEBUG, "OMX_AUDIO_CodingPCM\n");
		encoding = OMX_AUDIO_CodingPCM;
		component->useHWDecode = 0;
		break;
	} 

	if (bits_per_coded_sample == 0) {
		bits_per_coded_sample = 16;
	}

	unsigned int bufferLen = sample_rate * (bits_per_coded_sample >> 3) * channels;
	unsigned int chunkLen = 6144;

	// set up the number/size of buffers
	OMX_PARAM_PORTDEFINITIONTYPE portDef;
	OMX_INIT_STRUCTURE(portDef);
	portDef.nPortIndex = component->inputPort;

	OMX_ERRORTYPE omxErr;

	if ((component->useHWDecode == 1) && (audioPassthrough == 0)) {
		OMX_AUDIO_PARAM_PORTFORMATTYPE formatType;
		OMX_INIT_STRUCTURE(formatType);
		formatType.nPortIndex = component->inputPort;

		formatType.eEncoding = encoding;
		formatType.nIndex = 0;	

		omxErr = OMX_SetParameter(component->handle, OMX_IndexParamAudioPortFormat, &formatType);
		if(omxErr != OMX_ErrorNone) {
			logInfo(LOG_OMX, "Error SetParameter OMX_IndexParamAudioPortFormat for port %d status on component %s omxErr(0x%08x)\n", component->inputPort, component->componentName, (int)omxErr);
			return omxErr;
		}
	}

	omxErr = OMX_GetParameter(component->handle, OMX_IndexParamPortDefinition, &portDef);
	if (omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error GetParameter OMX_IndexParamPortDefinition for port %d status on component %s omxErr(0x%08x)\n", component->inputPort, component->componentName, (int)omxErr);
		return omxErr;
	}

	logInfo(LOG_OMX_DEBUG,"chunkLen=%d, bufferLen=%d.\n", chunkLen, bufferLen);

	portDef.format.audio.eEncoding = encoding;

	portDef.nBufferSize = chunkLen;
	portDef.nBufferCountActual = bufferLen / chunkLen;

	omxErr = OMX_SetParameter(component->handle, OMX_IndexParamPortDefinition, &portDef);
	if(omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error SetParameter OMX_IndexParamPortDefinition for port %d status on component %s omxErr(0x%08x)\n", component->inputPort, component->componentName, (int)omxErr);
		return omxErr;
	}

	if ((codec == CODEC_ID_AC3) || (codec == CODEC_ID_EAC3)) {

		OMX_AUDIO_PARAM_DDPTYPE ddpType;
		OMX_INIT_STRUCTURE(ddpType);
		ddpType.nPortIndex = component->inputPort;

		ddpType.nSampleRate = sample_rate;
		ddpType.nChannels = (channels == 6) ? 8 : channels;
		ddpType.nBitRate = 0;

		if (codec == CODEC_ID_AC3) {
			ddpType.eBitStreamId = OMX_AUDIO_DDPBitStreamIdAC3;
		}
		else {
			ddpType.eBitStreamId = OMX_AUDIO_DDPBitStreamIdEAC3;
		}

		unsigned int i;
		for(i = 0; i < OMX_MAX_CHANNELS; i++) {
			if (i < channels) {
				ddpType.eChannelMapping[i] = OMXChannels[i];
			}
			else {
				ddpType.eChannelMapping[i] = OMX_AUDIO_ChannelNone;
			}
		}

		omxErr = OMX_SetParameter(component->handle, OMX_IndexParamAudioDdp, &ddpType);
		if(omxErr != OMX_ErrorNone) {
			logInfo(LOG_OMX, "Error SetParameter OMX_IndexParamAudioDdp for port %d status on component %s omxErr(0x%08x)\n", component->inputPort, component->componentName, (int)omxErr);
			return omxErr;
		}
		logInfo(LOG_OMX_DEBUG, "Set sampeling_rate for AC3.\n");
	} 

	return OMX_ErrorNone;
}

OMX_ERRORTYPE omxShowAudioPortFormat(struct OMX_COMPONENT_T *component, unsigned int port)
{
	OMX_ERRORTYPE omxErr;

	OMX_PARAM_PORTDEFINITIONTYPE portDef;
	OMX_INIT_STRUCTURE(portDef);
	portDef.nPortIndex = port;

	omxErr = OMX_GetParameter(component->handle, OMX_IndexParamPortDefinition, &portDef);
	if (omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error GetParameter OMX_IndexParamPortDefinition for port %d status on component %s omxErr(0x%08x)\n", component->inputPort, component->componentName, (int)omxErr);
		return omxErr;
	}

	logInfo(LOG_OMX_DEBUG,"Audio format for port %d is %d.\n", port, portDef.format.audio.eEncoding);

	return OMX_ErrorNone;
}

OMX_ERRORTYPE omxSetAudioExtraData(struct OMX_COMPONENT_T *component, uint8_t *extraData, int extraSize)
{
	OMX_ERRORTYPE omxErr = OMX_ErrorNotReady;

	if ((extraSize <= 0) || (extraData == NULL)) {
		return OMX_ErrorNone;
	}

	OMX_BUFFERHEADERTYPE *omxBuffer = omxGetInputBuffer(component, 0);

	if(omxBuffer == NULL) {
		logInfo(LOG_OMX, "Error getting inputbuffer on component %s\n", component->componentName);
		return omxErr;
	}

	omxBuffer->nOffset = 0;
	omxBuffer->nFilledLen = extraSize;
	if(omxBuffer->nFilledLen > omxBuffer->nAllocLen) {
		logInfo(LOG_OMX, "Error omxBuffer->nFilledLen > omxBuffer->nAllocLen on component %s\n", component->componentName);
		return omxErr;
	}

	memset((unsigned char *)omxBuffer->pBuffer, 0x0, omxBuffer->nAllocLen);
	memcpy((unsigned char *)omxBuffer->pBuffer, extraData, omxBuffer->nFilledLen);
	omxBuffer->nFlags = OMX_BUFFERFLAG_CODECCONFIG | OMX_BUFFERFLAG_ENDOFFRAME;

	omxErr = OMX_EmptyThisBuffer(component->handle, omxBuffer);
	if (omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error OMX_EmptyThisBuffer on component %s (Error=0x%08x)\n", component->componentName, omxErr);
		return omxErr;
	}

	return OMX_ErrorNone;

}

OMX_ERRORTYPE omxSetAudioVolume(struct OMX_COMPONENT_T *component, long volume)
{
	OMX_ERRORTYPE omxErr;

	OMX_AUDIO_CONFIG_VOLUMETYPE volumeConfig;
	OMX_INIT_STRUCTURE(volumeConfig);
	volumeConfig.nPortIndex = component->inputPort;

	volumeConfig.bLinear = OMX_TRUE;
	volumeConfig.sVolume.nValue = volume;

	omxErr = OMX_SetConfig(component->handle, OMX_IndexConfigAudioVolume, &volumeConfig);
	if(omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error SetConfig OMX_IndexConfigAudioVolume on component %s (Error=0x%08x)\n", component->componentName, omxErr);
		return omxErr;
	}

	return OMX_ErrorNone;
}

OMX_ERRORTYPE omxSetAudioDestination(struct OMX_COMPONENT_T *component, const char *device)
{
	OMX_ERRORTYPE omxErr;

	OMX_CONFIG_BRCMAUDIODESTINATIONTYPE audioDest;
	OMX_INIT_STRUCTURE(audioDest);
	strncpy((char *)audioDest.sName, device, strlen(device));

	omxErr = OMX_SetConfig(component->handle, OMX_IndexConfigBrcmAudioDestination, &audioDest);
	if (omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error SetConfig OMX_IndexConfigBrcmAudioDestination on component %s (Error=0x%08x)\n", component->componentName, omxErr);
		return omxErr;
	}

	return OMX_ErrorNone;
}

OMX_ERRORTYPE omxSetAudioClockAsSourceReference(struct OMX_COMPONENT_T *component, int clockReferenceSource)
{
	OMX_ERRORTYPE omxErr;

	OMX_CONFIG_BOOLEANTYPE configBool;
	OMX_INIT_STRUCTURE(configBool);
	configBool.bEnabled = (clockReferenceSource == 1) ? OMX_TRUE : OMX_FALSE;

	omxErr = OMX_SetConfig(component->handle, OMX_IndexConfigBrcmClockReferenceSource, &configBool);
	if (omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error SetConfig OMX_IndexConfigBrcmClockReferenceSource on component %s (Error=0x%08x)\n", component->componentName, omxErr);
		return omxErr;
	}

	return OMX_ErrorNone;
}

OMX_ERRORTYPE omxSetAudioPassthrough(struct OMX_COMPONENT_T *component, int passthrough)
{
	OMX_ERRORTYPE omxErr;

	OMX_CONFIG_BOOLEANTYPE configBool;
	OMX_INIT_STRUCTURE(configBool);
	configBool.bEnabled = (passthrough == 1) ? OMX_TRUE : OMX_FALSE;

	omxErr = OMX_SetConfig(component->handle, OMX_IndexConfigBrcmDecoderPassThrough, &configBool);
	if (omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error SetConfig OMX_IndexConfigBrcmDecoderPassThrough on component %s (Error=0x%08x)\n", component->componentName, omxErr);
		return omxErr;
	}

	return OMX_ErrorNone;
}

#endif
