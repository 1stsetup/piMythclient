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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <time.h>
#include <pthread.h>
//#include <vgfont.h>

#include "globalFunctions.h"
#include "lists.h"
#include "connection.h"
#include "mythProtocol.h"
#include "demuxer.h"
#include "omxCore.h"
#include "omxVideo.h"
#include "omxAudio.h"
#include "osd.h"
#include "licenseChecks.h"

#include <bcm_host.h>

int ffmpeg_inited = 0;
char *language = NULL;

int demuxerStartInterrupted = 0;

void demuxerSetLanguage(char *newLanguage)
{
	if (language) {
		free(language);
	}

	language = malloc(strlen(newLanguage));
	if (language) {
		strcpy(language, newLanguage);
	}
}

void demuxerInitialize()
{
	if (ffmpeg_inited == 1) return;

	av_register_all();
	avformat_network_init();

	logInfo( LOG_DEMUXER_DEBUG,"av_register_all.\n");

	ffmpeg_inited = 1;
}

AVInputFormat *demuxerGetStreamFormatFromConnection(unsigned char *buffer, int probeLen)
{
	AVProbeData probe_data;

	probe_data.filename = "";
	probe_data.buf_size = probeLen; // av_open_input_file tries this many times with progressively larger buffers each time, but this must be enough

	probe_data.buf = (unsigned char *) malloc(probe_data.buf_size+AVPROBE_PADDING_SIZE);
	// allocate memory to read the first bytes
	memset(probe_data.buf, 0, probe_data.buf_size+AVPROBE_PADDING_SIZE);
	memcpy(probe_data.buf, buffer, probeLen);

	// probe
	AVInputFormat *ret = av_probe_input_format(&probe_data, 1);
	// cleanup
	free(probe_data.buf);
	return ret;
}

int demuxerIsStopped(struct DEMUXER_T *demuxer)
{
	pthread_mutex_lock(&demuxer->threadLock);

	int result = demuxer->doStop;

	pthread_mutex_unlock(&demuxer->threadLock);

	return result;
}

int videoIsStopped(struct DEMUXER_T *demuxer)
{
	pthread_mutex_lock(&demuxer->videoThreadLock);

	int result = demuxer->doStopVideoThread;

	pthread_mutex_unlock(&demuxer->videoThreadLock);

	return result;
}

int audioIsStopped(struct DEMUXER_T *demuxer)
{
	pthread_mutex_lock(&demuxer->audioThreadLock);

	int result = demuxer->doStopAudioThread;

	pthread_mutex_unlock(&demuxer->audioThreadLock);

	return result;
}

int subtitleIsStopped(struct DEMUXER_T *demuxer)
{
	pthread_mutex_lock(&demuxer->subtitleThreadLock);

	int result = demuxer->doStopSubtitleThread;

	pthread_mutex_unlock(&demuxer->subtitleThreadLock);

	return result;
}

int mythIsStopped(struct DEMUXER_T *demuxer)
{
	pthread_mutex_lock(&demuxer->mythThreadLock);

	int result = demuxer->doStopMythThread;

	pthread_mutex_unlock(&demuxer->mythThreadLock);

	return result;
}

static int demuxerInterruptCallback(struct DEMUXER_T *demuxer)
{
	if(demuxer->demuxerThread == -1) {
		return demuxerStartInterrupted;
	}
	else {
		return demuxerIsStopped(demuxer);
	}
}

void demuxerSetNextFile(struct DEMUXER_T *demuxer, char *filename)
{
	demuxer->nextFile = malloc(strlen(filename)+1);
	memset(demuxer->nextFile, 0, strlen(filename)+1);
	memcpy(demuxer->nextFile, filename, strlen(filename));
	logInfo( LOG_DEMUXER,"Set next file to %s.\n", demuxer->nextFile);
}

int demuxerReadPacket(struct DEMUXER_T *demuxer, uint8_t *buffer, int bufferSize) 
{
	if (demuxerInterruptCallback(demuxer) == 1) {
		logInfo( LOG_DEMUXER_DEBUG,"Received request to read %d bytes but demuxer is asked to stop.\n", bufferSize);
		return -1;
	}

	logInfo( LOG_DEMUXER_DEBUG,"Received request to read %d bytes.\n", bufferSize);

	ssize_t readLen;

	ssize_t receivedLen;
	ssize_t readBufferLen = 0;

	readLen = mythFiletransferRequestBlock(demuxer->mythConnection, bufferSize);
	logInfo( LOG_DEMUXER_DEBUG,"Myth will send %zd bytes of the requested %d.\n", readLen, bufferSize);
	if (readLen > 0) {

		receivedLen = fillConnectionBuffer(demuxer->mythConnection->transferConnection->connection, readLen, 1);
		logInfo( LOG_DEMUXER_DEBUG,"Connection buffer was filled with %zd bytes of the requested %zd.\n", receivedLen, readLen);


	/*	struct timespec interval;
		struct timespec remainingInterval;

		while (getConnectionDataLen(demuxer->mythConnection->transferConnection->connection) < receivedLen) {
			interval.tv_sec = 0;
			interval.tv_nsec = 10;

			nanosleep(&interval, &remainingInterval);
		}
	*/

		while (readBufferLen == 0) {
			readBufferLen = readConnectionBuffer(demuxer->mythConnection->transferConnection->connection, (char *)buffer, receivedLen);
	//		logInfo( LOG_DEMUXER_DEBUG,"Received %zd bytes from connection.\n", readBufferLen);
		}
		logInfo( LOG_DEMUXER_DEBUG,"Received %zd bytes from connection.\n", readBufferLen);
	}

	logInfo( LOG_DEMUXER_DEBUG,"test %zd< %d && demuxer->nextFile = %s.\n", readLen, bufferSize, demuxer->nextFile);
//	if ((readLen < bufferSize) && (demuxer->nextFile != NULL)) {
	if (readLen < bufferSize) {

		if (demuxer->nextFile != NULL) {
			// Swap transferConnection on our mythConnection.
			logInfo( LOG_DEMUXER, "We received less bytes from myth than requested. We also have a next file %s. Probably due to a program change on the same channel.\n", demuxer->nextFile);
			struct MYTH_CONNECTION_T *newTransferConnection = mythPrepareNextProgram(demuxer->mythConnection, demuxer->nextFile);
			if (newTransferConnection == NULL) {
				logInfo(LOG_DEMUXER, "Error creating new transferConnection.\n");
			}
			else {
				mythSetNewTransferConnection(demuxer->mythConnection, newTransferConnection);
			}
			free(demuxer->nextFile);
			demuxer->nextFile = NULL;
			demuxer->newProgram = 1;
		}
		else {
			logInfo( LOG_DEMUXER, "We received less bytes from myth than requested. We do NOT have a next file.!\n");
		}		
	}

//	// We are prefilling for next request. The call to mythbackend and reception of data could slow down our demuxing.
//	readLen = mythFiletransferRequestBlock(demuxer->mythConnection, bufferSize);
//	fillConnectionBuffer(demuxer->mythConnection->transferConnection->connection, readLen, 0);

	return readBufferLen;
}

int64_t demuxerSeek(struct DEMUXER_T *demuxer, int64_t offset, int whence) 
{
	if (demuxerInterruptCallback(demuxer) == 1) {
		logInfo( LOG_DEMUXER_DEBUG,"Received request to seek offset=%" PRId64 ", whence=%d, currentpos=%lld but demuxer is asked to stop.\n", offset, whence, demuxer->mythConnection->position);
		return -1;
	}

	logInfo( LOG_DEMUXER_DEBUG,"Received request to seek offset=%" PRId64 ", whence=%d, currentpos=%lld.\n", offset, whence, demuxer->mythConnection->position);
	int realWhence = 0;
	if (whence & SEEK_CUR) realWhence = SEEK_CUR;
	if (whence & SEEK_SET) realWhence = SEEK_SET;
	if (whence & SEEK_END) realWhence = SEEK_END;

	logInfo( LOG_DEMUXER_DEBUG,"Received request to seek realWhence=%d.\n", realWhence);

	if (((realWhence == SEEK_CUR) && (offset == 0)) || ((realWhence == SEEK_SET) && (offset == demuxer->mythConnection->position))) {
		return demuxer->mythConnection->position;
	}

	int64_t result = mythFiletransferSeek(demuxer->mythConnection, offset, realWhence, demuxer->mythConnection->position);
	logInfo( LOG_DEMUXER_DEBUG,"Seeked to=%" PRId64 ", whence=%d.\n", result, realWhence);

	clearConnectionBuffer(demuxer->mythConnection->transferConnection->connection);

/*	while (getConnectionDataLen(demuxer->mythConnection->transferConnection->connection) < MIN_MYTH_QUEUE_LENGTH) {
		sleep(1);
	}
*/
	return result;
}

int showVideoPacket(struct DEMUXER_T *demuxer, AVPacket *packet, double pts)
{
#ifndef PI
	av_free_packet(packet);
	return 1;
#else
	logInfo(LOG_DEMUXER_DEBUG, "Start pts=%.15f, packet->size=%d.\n", pts, (unsigned int)packet->size);
	unsigned int demuxer_bytes = (unsigned int)packet->size;
	uint8_t *demuxer_content = packet->data;
	OMX_ERRORTYPE omxErr;

	while(demuxer_bytes) {
		// 500ms timeout
		logInfo(LOG_DEMUXER_DEBUG, "demuxer_bytes=%d. Before omxGetInputBuffer\n", demuxer_bytes);
		OMX_BUFFERHEADERTYPE *omx_buffer = omxGetInputBuffer(demuxer->videoDecoder, 500);
		logInfo(LOG_DEMUXER_DEBUG, "demuxer_bytes=%d. After omxGetInputBuffer\n", demuxer_bytes);
		if(omx_buffer == NULL)
		{
			logInfo(LOG_DEMUXER, "omxGetInputBuffer is empty\n");
			return 0;
		}

		logInfo(LOG_DEMUXER_DEBUG, "pts:%.15f, omx_buffer:%8p, buffer:%8p, buffer#:%d\n", 
		  pts, omx_buffer, omx_buffer->pBuffer, (int)omx_buffer->pAppPrivate);

		omx_buffer->nFlags = 0;
		omx_buffer->nOffset = 0;

		uint64_t val  = (uint64_t)(pts == DVD_NOPTS_VALUE) ? 0 : pts;

		if (demuxer->setVideoStartTime) {
			omx_buffer->nFlags = OMX_BUFFERFLAG_STARTTIME;
			demuxer->setVideoStartTime = 0;
		}
		else {
			if (pts == DVD_NOPTS_VALUE)
				omx_buffer->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN;
		}

		omx_buffer->nTimeStamp = ToOMXTime(val);
		//omx_buffer->nTickCount = (OMX_U32)val;

		omx_buffer->nFilledLen = (demuxer_bytes > omx_buffer->nAllocLen) ? omx_buffer->nAllocLen : demuxer_bytes;

		memcpy(omx_buffer->pBuffer, demuxer_content, omx_buffer->nFilledLen);
		logInfo(LOG_DEMUXER_DEBUG, "omx_buffer->nFilledLen=%d.\n", (uint32_t)omx_buffer->nFilledLen);

		demuxer_bytes -= omx_buffer->nFilledLen;
		demuxer_content += omx_buffer->nFilledLen;

		if(demuxer_bytes == 0)
			omx_buffer->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;

		logInfo(LOG_DEMUXER_DEBUG, "demuxer->videoDecoder->portSettingChanged=%d.\n", demuxer->videoDecoder->portSettingChanged);
		if (demuxer->videoDecoder->portSettingChanged == 1) {

			logInfo(LOG_DEMUXER, "demuxer->videoDecoder->portSettingChanged set to true.\n");
			demuxer->videoDecoder->portSettingChanged = 0;

			// Portsettings of the videoDecoder have changed. Normally this is when a frame is ready  and it has other dimensions than before.
			// This happens also for the very first frame.

			omxShowVideoInterlace(demuxer->videoDecoder);

			// Check if the stream is de interlaced.
			OMX_CONFIG_INTERLACETYPE videoInterlace;
			omxErr = omxGetVideoInterlace(demuxer->videoDecoder, &videoInterlace);
			if(omxErr != OMX_ErrorNone) {
				logInfo(LOG_DEMUXER, "Error omxGetVideoInterlace. (Error=0x%08x).\n", omxErr);
				av_free_packet(packet);
				return 1;
			}

			demuxer->streamIsInterlaced = (videoInterlace.eMode != OMX_InterlaceProgressive) ? 1 : 0;


			// Tear down any tunnels we might have. The video settingschanged normally only appears at the start and
			// When something changed like the dimensions or interlace or framerate (i think)..

			if (demuxer->videoStreamWasInterlaced == 0) {

				if (demuxer->videoDecoderToSchedulerTunnel->tunnelIsUp == 1) {
					logInfo(LOG_DEMUXER, "Destroying demuxer->videoDecoderToSchedulerTunnel.\n");
					omxErr = omxDisconnectTunnel(demuxer->videoDecoderToSchedulerTunnel);
					if(omxErr != OMX_ErrorNone) {
						logInfo(LOG_DEMUXER, "Error deestablishing tunnel between video decoder and video scheduler components. (Error=0x%08x).\n", omxErr);
						av_free_packet(packet);
						return 1;
					}

					omxChangeStateToLoaded(demuxer->videoScheduler, 0);
					logInfo(LOG_DEMUXER, "Changed state for video scheduler to loaded.\n");
				}
			}
			else {

				if (demuxer->videoDecoderToImageFXTunnel->tunnelIsUp == 1) {
					logInfo(LOG_DEMUXER, "Destroying demuxer->videoDecoderToImageFXTunnel.\n");
					omxErr = omxDisconnectTunnel(demuxer->videoDecoderToImageFXTunnel);
					if(omxErr != OMX_ErrorNone) {
						logInfo(LOG_DEMUXER, "Error deestablishing tunnel between video decoder and video imxge_fx components. (Error=0x%08x).\n", omxErr);
						av_free_packet(packet);
						return 1;
					}

					omxChangeStateToLoaded(demuxer->videoImageFX, 0);
					logInfo(LOG_DEMUXER, "Changed state for video image_fx to loaded.\n");
				}

				if (demuxer->videoImageFXToSchedulerTunnel->tunnelIsUp == 1) {
					logInfo(LOG_DEMUXER, "Destroying demuxer->videoImageFXToSchedulerTunnel.\n");
					omxErr = omxDisconnectTunnel(demuxer->videoImageFXToSchedulerTunnel);
					if(omxErr != OMX_ErrorNone) {
						logInfo(LOG_DEMUXER, "Error deestablishing tunnel between video decoder and video imxge_fx components. (Error=0x%08x).\n", omxErr);
						av_free_packet(packet);
						return 1;
					}

					omxChangeStateToLoaded(demuxer->videoScheduler, 0);
					logInfo(LOG_DEMUXER, "Changed state for video scheduler to loaded.\n");
				}

			}


			if (demuxer->videoSchedulerToRenderTunnel->tunnelIsUp == 1) {
				logInfo(LOG_DEMUXER, "Destroying demuxer->videoSchedulerToRenderTunnel.\n");
				omxErr = omxDisconnectTunnel(demuxer->videoSchedulerToRenderTunnel);
				if(omxErr != OMX_ErrorNone) {
					logInfo(LOG_DEMUXER, "Error deestablishing tunnel between video scheduler and video render components. (Error=0x%08x).\n", omxErr);
					av_free_packet(packet);
					return 1;
				}

				omxChangeStateToLoaded(demuxer->videoRender, 0);
				logInfo(LOG_DEMUXER, "Changed state for video render to loaded.\n");
			}

			if ((demuxer->deInterlace == 0) || (demuxer->streamIsInterlaced == 0)) {

				logInfo(LOG_DEMUXER, "The pi is NOT going to do the deinterlacing.\n");
				omxErr = omxEstablishTunnel(demuxer->videoDecoderToSchedulerTunnel);
				if(omxErr != OMX_ErrorNone) {
					logInfo(LOG_DEMUXER, "Error establishing tunnel between video decoder and video scheduler components. (Error=0x%08x).\n", omxErr);
					av_free_packet(packet);
					return 1;
				}

				logInfo(LOG_DEMUXER, "Setting state of videoScheduler to OMX_StateExecuting.\n");
				omxErr = omxSetStateForComponent(demuxer->videoScheduler, OMX_StateExecuting, 50000);
				if (omxErr != OMX_ErrorNone)
				{
					logInfo(LOG_DEMUXER, "video scheduler SetStateForComponent to OMX_StateExecuting. (Error=0x%08x).\n", omxErr);
					av_free_packet(packet);
					return 1;
				}

			}
			else {

				logInfo(LOG_DEMUXER, "The pi is going to do the deinterlacing.\n");
				// We read the video decoder output port format frame dimensions to set the image_fx input and output port
				OMX_PARAM_PORTDEFINITIONTYPE portImage;
				OMX_INIT_STRUCTURE(portImage);
				portImage.nPortIndex = demuxer->videoDecoder->outputPort;

				omxErr = OMX_GetParameter(demuxer->videoDecoder->handle, OMX_IndexParamPortDefinition, &portImage);
				if (omxErr != OMX_ErrorNone) {
					logInfo(LOG_DEMUXER, "Error OMX_IndexParamPortDefinition for video decoder components. (Error=0x%08x).\n", omxErr);
					return 1;
				}

				logInfo(LOG_DEMUXER, "Video stream frame dimensions (WxH) = %dx%d.\n", demuxer->formatContext->streams[demuxer->videoStream]->codec->width, demuxer->formatContext->streams[demuxer->videoStream]->codec->height);
				logInfo(LOG_DEMUXER, "Video decoder output frame dimensions (WxH) = %dx%d.\n", (uint32_t)portImage.format.video.nFrameWidth, (uint32_t)portImage.format.video.nFrameHeight);

				OMX_VIDEO_PARAM_PORTFORMATTYPE formatType;
				OMX_INIT_STRUCTURE(formatType);
				formatType.nPortIndex = demuxer->videoDecoder->outputPort;

				omxErr = OMX_GetParameter(demuxer->videoDecoder->handle, OMX_IndexParamVideoPortFormat, &formatType);
				if (omxErr != OMX_ErrorNone) {
					logInfo(LOG_DEMUXER, "Error OMX_IndexParamPortDefinition for video decoder components. (Error=0x%08x).\n", omxErr);
					return 1;
				}

				logInfo(LOG_DEMUXER, "Video decoder out frame rate = %d.\n", (uint32_t)formatType.xFramerate);

				omxDisablePort(demuxer->videoImageFX, demuxer->videoImageFX->outputPort, 0);
				omxDisablePort(demuxer->videoImageFX, demuxer->videoImageFX->inputPort, 0);

				portImage.nPortIndex = demuxer->videoImageFX->inputPort;
				omxErr = OMX_SetParameter(demuxer->videoImageFX->handle, OMX_IndexParamPortDefinition, &portImage);
				if (omxErr != OMX_ErrorNone) {
					logInfo(LOG_DEMUXER, "Error OMX_IndexParamPortDefinition for video image_fx components inputPort. (Error=0x%08x).\n", omxErr);
					return 1;
				}

				portImage.nPortIndex = demuxer->videoImageFX->outputPort;
				omxErr = OMX_SetParameter(demuxer->videoImageFX->handle, OMX_IndexParamPortDefinition, &portImage);
				if (omxErr != OMX_ErrorNone) {
					logInfo(LOG_DEMUXER, "Error OMX_IndexParamPortDefinition for video image_fx components outputPort. (Error=0x%08x).\n", omxErr);
					return 1;

				}

				omxErr = omxSetVideoDeInterlace(demuxer->videoImageFX, 1);
				if (omxErr != OMX_ErrorNone) {
					logInfo(LOG_DEMUXER, "Error omxSetVideoDeInterlace on video image_fx component. (Error=0x%08x).\n", omxErr);
					return omxErr;
				}

				omxErr = omxEstablishTunnel(demuxer->videoDecoderToImageFXTunnel);
				if(omxErr != OMX_ErrorNone) {
					logInfo(LOG_DEMUXER, "Error establishing tunnel between video decoder and image_fx scheduler components. (Error=0x%08x).\n", omxErr);
					av_free_packet(packet);
					return 1;
				}

				logInfo(LOG_DEMUXER, "Setting state of videoImageFX to OMX_StateExecuting.\n");
				omxErr = omxSetStateForComponent(demuxer->videoImageFX, OMX_StateExecuting, 50000);
				if (omxErr != OMX_ErrorNone)
				{
					logInfo(LOG_DEMUXER, "video image_fx SetStateForComponent to OMX_StateExecuting. (Error=0x%08x).\n", omxErr);
					av_free_packet(packet);
					return 1;
				}

				demuxer->videoStreamWasInterlaced = 1;
			}

		}

		if (demuxer->videoImageFX->portSettingChanged == 1) {

			logInfo(LOG_DEMUXER, "demuxer->videoImageFX->portSettingChanged set to true.\n");
			demuxer->videoImageFX->portSettingChanged = 0;

			omxErr = omxEstablishTunnel(demuxer->videoImageFXToSchedulerTunnel);
			if(omxErr != OMX_ErrorNone) {
				logInfo(LOG_DEMUXER, "Error establishing tunnel between video image_fx and video scheduler components. (Error=0x%08x).\n", omxErr);
				av_free_packet(packet);
				return 1;
			}

			logInfo(LOG_DEMUXER, "Setting state of videoScheduler to OMX_StateExecuting.\n");
			omxErr = omxSetStateForComponent(demuxer->videoScheduler, OMX_StateExecuting, 50000);
			if (omxErr != OMX_ErrorNone)
			{
				logInfo(LOG_DEMUXER, "video scheduler SetStateForComponent to OMX_StateExecuting. (Error=0x%08x).\n", omxErr);
				av_free_packet(packet);
				return 1;
			}
		}

		if (demuxer->videoScheduler->portSettingChanged == 1) {

			logInfo(LOG_DEMUXER, "demuxer->videoScheduler->portSettingChanged set to true.\n");
			demuxer->videoScheduler->portSettingChanged = 0;

			omxErr = omxEstablishTunnel(demuxer->videoSchedulerToRenderTunnel);
			if(omxErr != OMX_ErrorNone) {
				logInfo(LOG_DEMUXER, "Error establishing tunnel between video scheduler and video render components. (Error=0x%08x).\n", omxErr);
				av_free_packet(packet);
				return 1;
			}

			logInfo(LOG_DEMUXER, "Setting state of videoRender to OMX_StateExecuting.\n");
			omxErr = omxSetStateForComponent(demuxer->videoRender, OMX_StateExecuting, 50000);
			if (omxErr != OMX_ErrorNone)
			{
				logInfo(LOG_DEMUXER, "video render SetStateForComponent to OMX_StateExecuting. (Error=0x%08x).\n", omxErr);
				av_free_packet(packet);
				return 1;
			}

		}

		int nRetry = 0;
		while(1 == 1) {
			logInfo(LOG_DEMUXER_DEBUG, "demuxer_bytes=%d. Before OMX_EmptyThisBuffer\n", demuxer_bytes);
			omxErr = OMX_EmptyThisBuffer(demuxer->videoDecoder->handle, omx_buffer);
			logInfo(LOG_DEMUXER_DEBUG, "demuxer_bytes=%d. after OMX_EmptyThisBuffer\n", demuxer_bytes);
			if (omxErr == OMX_ErrorNone) {
				break;
			}
			else {
				logInfo(LOG_DEMUXER_DEBUG, "OMX_EmptyThisBuffer() failed with result(0x%x). Retry=%d\n", omxErr, nRetry);
				nRetry++;
			}
			if(nRetry == 5) {
				logInfo(LOG_DEMUXER, "OMX_EmptyThisBuffer() finaly failed\n");
				av_free_packet(packet);
				return 1;
			}
		}

	}

	av_free_packet(packet);
	return 1;
#endif
}

int playAudioPacket(struct DEMUXER_T *demuxer, AVPacket *packet, double pts)
{
#ifndef PI
	av_free_packet(packet);
	return 1;
#else
	logInfo(LOG_DEMUXER_DEBUG, "Start pts=%.15f.\n", pts);
	unsigned int demuxer_bytes = (unsigned int)packet->size;
	uint8_t *demuxer_content = packet->data;
	OMX_ERRORTYPE omxErr;

	while(demuxer_bytes) {
		// 200ms timeout
		logInfo(LOG_DEMUXER_DEBUG, "demuxer_bytes=%d. Before omxGetInputBuffer\n", demuxer_bytes);
		OMX_BUFFERHEADERTYPE *omx_buffer;
		if (demuxer->swDecodeAudio == 0) {
			omx_buffer = omxGetInputBuffer(demuxer->audioDecoder, 200);
		}
		else {
			omx_buffer = omxGetInputBuffer(demuxer->audioRender, 200);
		}

		logInfo(LOG_DEMUXER_DEBUG, "demuxer_bytes=%d. After omxGetInputBuffer\n", demuxer_bytes);
		if(omx_buffer == NULL)
		{
			logInfo(LOG_DEMUXER_DEBUG, "omxGetInputBuffer is empty\n");
			return 0;
		}

		logInfo(LOG_DEMUXER_DEBUG, "pts:%.15f, omx_buffer:%8p, buffer:%8p, buffer#:%d\n", 
		  pts, omx_buffer, omx_buffer->pBuffer, (int)omx_buffer->pAppPrivate);

		omx_buffer->nFlags = 0;
		omx_buffer->nOffset = 0;

		uint64_t val  = (uint64_t)(pts == DVD_NOPTS_VALUE) ? 0 : pts;

		if (demuxer->setAudioStartTime) {
			omx_buffer->nFlags = OMX_BUFFERFLAG_STARTTIME;
			demuxer->setAudioStartTime = 0;
		}
		else {
			if (pts == DVD_NOPTS_VALUE)
				omx_buffer->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN;
		}

		omx_buffer->nTimeStamp = ToOMXTime(val);
		//omx_buffer->nTickCount = (OMX_U32)val;

		omx_buffer->nFilledLen = (demuxer_bytes > omx_buffer->nAllocLen) ? omx_buffer->nAllocLen : demuxer_bytes;

		memcpy(omx_buffer->pBuffer, demuxer_content, omx_buffer->nFilledLen);
		logInfo(LOG_DEMUXER_DEBUG, "omx_buffer->nFilledLen=%d.\n", (uint32_t)omx_buffer->nFilledLen);

		demuxer_bytes -= omx_buffer->nFilledLen;
		demuxer_content += omx_buffer->nFilledLen;

		if(demuxer_bytes == 0)
			omx_buffer->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;

		if (demuxer->swDecodeAudio == 0) {
			if ((demuxer->audioPortSettingChanged == 0) && (omxWaitForEvent(demuxer->audioDecoder, OMX_EventPortSettingsChanged, demuxer->audioDecoder->outputPort, 0) == OMX_ErrorNone)) {

				demuxer->audioPortSettingChanged = 1;

				logInfo(LOG_DEMUXER_DEBUG, "demuxer->audioPortSettingChanged set to true.\n");

				if (demuxer->audioPassthrough == 0) {
					omxErr = omxEstablishTunnel(demuxer->audioDecoderToMixerTunnel);
					if(omxErr != OMX_ErrorNone) {
						logInfo(LOG_DEMUXER, "Error establishing tunnel between audio decoder and audio mixer components. (Error=0x%08x).\n", omxErr);
//						av_free_packet(packet);
						return 1;
					}

					logInfo(LOG_DEMUXER, "Setting state of audioMixer to OMX_StateExecuting.\n");
					omxErr = omxSetStateForComponent(demuxer->audioMixer, OMX_StateExecuting, 50000);
					if (omxErr != OMX_ErrorNone)
					{
						logInfo(LOG_DEMUXER, "audio mixer SetStateForComponent to OMX_StateExecuting. (Error=0x%08x).\n", omxErr);
						return omxErr;
					}
				}
				else {
					omxShowAudioPortFormat(demuxer->audioDecoder, demuxer->audioDecoder->outputPort);
					omxShowAudioPortFormat(demuxer->audioRender, demuxer->audioRender->inputPort);

					omxErr = omxEstablishTunnel(demuxer->audioDecoderToRenderTunnel);
					if(omxErr != OMX_ErrorNone) {
						logInfo(LOG_DEMUXER, "Error establishing tunnel between audio decoder and audio render components. (Error=0x%08x).\n", omxErr);
						return omxErr;
					}
				}


				logInfo(LOG_DEMUXER, "Setting state of audioRender to OMX_StateExecuting.\n");
				omxErr = omxSetStateForComponent(demuxer->audioRender, OMX_StateExecuting, 50000);
				if (omxErr != OMX_ErrorNone)
				{
					logInfo(LOG_DEMUXER, "audio render SetStateForComponent to OMX_StateExecuting. (Error=0x%08x).\n", omxErr);
					return omxErr;
				}
			}
		}

		int nRetry = 0;
		while(1 == 1) {
			logInfo(LOG_DEMUXER_DEBUG, "demuxer_bytes=%d. Before OMX_EmptyThisBuffer\n", demuxer_bytes);

			if (demuxer->swDecodeAudio == 0) {
				omxErr = OMX_EmptyThisBuffer(demuxer->audioDecoder->handle, omx_buffer);
			}
			else {
				omxErr = OMX_EmptyThisBuffer(demuxer->audioRender->handle, omx_buffer);
			}
			logInfo(LOG_DEMUXER_DEBUG, "demuxer_bytes=%d. after OMX_EmptyThisBuffer\n", demuxer_bytes);
			if (omxErr == OMX_ErrorNone) {
				break;
			}
			else {
				logInfo(LOG_DEMUXER_DEBUG, "OMX_EmptyThisBuffer() failed with result(0x%x). Retry=%d\n", omxErr, nRetry);
				nRetry++;
			}
			if(nRetry == 5) {
				logInfo(LOG_DEMUXER, "OMX_EmptyThisBuffer() finaly failed\n");
//				av_free_packet(packet);
				return 1;
			}
		}

	}

//	av_free_packet(packet);
	return 1;
#endif
}

int decodeAudio(struct DEMUXER_T *demuxer, AVPacket *inPacket, double pts)
{
	int iBytesUsed, got_frame;
	if (!demuxer->audioCodecContext) return -1;

	if (inPacket->size == 0) {
		logInfo(LOG_DEMUXER, "Sizeof audio packet == 0. inpacket->data=%p\n", inPacket->data);
		return 1;
	}

	int m_iBufferSize1 = AVCODEC_MAX_AUDIO_FRAME_SIZE;

	AVPacket avpkt;
	av_init_packet(&avpkt);
	avpkt.data = inPacket->data;
	avpkt.size = inPacket->size;

	AVFrame *frame1 = avcodec_alloc_frame();

	iBytesUsed = avcodec_decode_audio4( demuxer->audioCodecContext
		                                 , frame1
		                                 , &got_frame
		                                 , &avpkt);


	logInfo(LOG_DEMUXER_DEBUG, "Decoded packet. iBytesUsed=%d from packet.size=%d.\n", iBytesUsed, inPacket->size);

	logInfo(LOG_DEMUXER_DEBUG, "frame1->format=%d, demuxer->audioCodecContext->sample_fmt=%d, demuxer->audioCodecContext->sample_rate=%d.\n", frame1->format, demuxer->audioCodecContext->sample_fmt,demuxer->audioCodecContext->sample_rate);

	if (!got_frame) {
		logInfo(LOG_DEMUXER, "Did not have a full frame in the packet.\n");
	}

	if (iBytesUsed < 0) {
		logInfo(LOG_DEMUXER, "iBytesUsed < 0.\n");
	}

	if (iBytesUsed != inPacket->size) {
		logInfo(LOG_DEMUXER, "iBytesUsed != inPacket.size  => %d = %d.\n", iBytesUsed, inPacket->size);
	}

	if (iBytesUsed < 0 || !got_frame) {
		m_iBufferSize1 = 0;

		// We are going to create silence for this frame
		if (demuxer->lastFrameSize > 0) {
			av_init_packet(&avpkt);
			uint8_t *silence = malloc(demuxer->lastFrameSize);
			memset(silence, 0, demuxer->lastFrameSize);

			avpkt.data = silence;
			avpkt.size = m_iBufferSize1;

			int ret = playAudioPacket(demuxer, &avpkt, pts);

			free(silence);

			avcodec_free_frame(&frame1);

/*			if (ret == 1) {
				av_free_packet(inPacket);
			}
*/
			return ret;
		}
		else {
			// We did not have any valid audio packet.
			avcodec_free_frame(&frame1);

//			av_free_packet(inPacket);

			return 1;
		}
	}

	m_iBufferSize1 = av_samples_get_buffer_size(NULL, demuxer->audioCodecContext->channels, frame1->nb_samples, demuxer->audioCodecContext->sample_fmt, 1);
	logInfo(LOG_DEMUXER_DEBUG, "decoded: m_iBufferSize1=%d, frame1->nb_samples=%d\n", m_iBufferSize1, frame1->nb_samples);

	/* some codecs will attempt to consume more data than what we gave */
	if (iBytesUsed > inPacket->size)
	{
		logInfo(LOG_DEMUXER, "audioDecoder attempted to consume more data than given");
		iBytesUsed = inPacket->size;
	}

//	fwrite(frame1->data[0], 1, m_iBufferSize1, demuxer->outfile);

	// Conversion to 2 channel AV_SAMPLE_FMT_S16 is done in software. Cannot be done for every format in HW so we do it in software.
	uint8_t *output = NULL;
	SwrContext *swr = NULL;
	if ((frame1->format != AV_SAMPLE_FMT_S16) || (demuxer->audioCodecContext->channel_layout != AV_CH_LAYOUT_STEREO)) {
		swr = swr_alloc();

		av_opt_set_int(swr, "in_channel_layout", demuxer->audioCodecContext->channel_layout /*AV_CH_LAYOUT_5POINT1*/, 0);
		//	av_opt_set_int(swr, "in_channel_layout", AV_CH_LAYOUT_5POINT1, 0);
		av_opt_set_int(swr, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0); // For now we do stereo.
		av_opt_set_int(swr, "in_sample_rate", demuxer->audioCodecContext->sample_rate, 0);
		av_opt_set_int(swr, "out_sample_rate", demuxer->audioCodecContext->sample_rate, 0);
		av_opt_set_sample_fmt(swr, "in_sample_fmt", frame1->format, 0);
		av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
		if (swr_init(swr) < 0) {
			logInfo(LOG_DEMUXER, "Error swr_init.\n");
			avcodec_free_frame(&frame1);
			return 0;
		}

		int in_samples = frame1->nb_samples;
		int out_samples = av_rescale_rnd(swr_get_delay(swr, demuxer->audioCodecContext->sample_rate) + in_samples, demuxer->audioCodecContext->sample_rate, demuxer->audioCodecContext->sample_rate, AV_ROUND_UP);
		av_samples_alloc(&output, NULL, 2, out_samples,	AV_SAMPLE_FMT_S16, 0);
		out_samples = swr_convert(swr, &output, out_samples, (const uint8_t **)&frame1->data[0], in_samples);
		if (out_samples < 0) {
			logInfo(LOG_DEMUXER, "Error swr_convert.\n");
			av_freep(&output);
			avcodec_free_frame(&frame1);
			return 0;
		}

		m_iBufferSize1 = av_samples_get_buffer_size(NULL, 2, out_samples, AV_SAMPLE_FMT_S16, 1);
		logInfo(LOG_DEMUXER_DEBUG, "converted: m_iBufferSize1=%d, out_samples=%d\n", m_iBufferSize1, out_samples);
		//fwrite(output, 1, m_iBufferSize1, demuxer->outfile);

		av_init_packet(&avpkt);
		avpkt.data = output;
		avpkt.size = m_iBufferSize1;
	}
	else {
		av_init_packet(&avpkt);
		avpkt.data = frame1->data[0];
		avpkt.size = m_iBufferSize1;
	}

	demuxer->lastFrameSize = avpkt.size;

	int ret = playAudioPacket(demuxer, &avpkt, pts);

	if (output != NULL) {
		av_freep(&output);
	}
	if (swr != NULL) {
		swr_free(&swr);
	}
	avcodec_free_frame(&frame1);

/*	if (ret == 1) {
		av_free_packet(inPacket);
	}
*/
	return ret;
}

static void pgm_save(unsigned char *buf, int wrap, int xsize, int ysize, char *filename)
{
	FILE *f;
	int i;

	f=fopen(filename,"w");
	fprintf(f,"P5\n%d %d\n%d\n",xsize,ysize,255);
	for(i=0;i<ysize;i++)
	 fwrite(buf + i * wrap,1,xsize,f);
	fclose(f);
}

static void ppm_save(unsigned char *buf, int wrap, int xsize, int ysize, char *filename)
{
	FILE *f;
	int i;

	f=fopen(filename,"w");
	fprintf(f,"P6\n%d %d\n%d\n",xsize,ysize,255);
	for(i=0;i<ysize;i++) {
		fwrite(buf + i * wrap,1,xsize*3,f);
	}
	fclose(f);
}

struct SIMPLELISTITEM_T *demuxerAddDVBSubtitle(struct DEMUXER_T *demuxer, AVSubtitle *subtitle)
{
	if (demuxer->subtitlePacketsEnd == NULL) {
		demuxer->subtitlePackets = createSimpleListItem(subtitle);
		demuxer->subtitlePacketsEnd = demuxer->subtitlePackets;
	}
	else {
		demuxer->subtitlePacketsEnd->next = createSimpleListItem(subtitle);
		demuxer->subtitlePacketsEnd = demuxer->subtitlePacketsEnd->next;
	}
	
	return demuxer->subtitlePacketsEnd;
}

AVSubtitle *demuxerGetDVBSubtitle(struct DEMUXER_T *demuxer)
{
	if (demuxer->subtitlePacketsEnd == NULL) {
		return NULL;
	}
	else {
		AVSubtitle *result = (AVSubtitle *)demuxer->subtitlePackets->object;
		struct SIMPLELISTITEM_T *oldPacket = demuxer->subtitlePackets;
		demuxer->subtitlePackets = demuxer->subtitlePackets->next;
		freeSimpleListItem(oldPacket);

		if (demuxer->subtitlePackets == NULL) {
			demuxer->subtitlePacketsEnd = NULL;
		}

		return result;
	}
	
}

void paintDVBSubtitle(struct OSD_T *osd, void *opaque)
{
	struct DEMUXER_T *demuxer = (struct DEMUXER_T *)opaque;

	AVSubtitle *subtitle = demuxerGetDVBSubtitle(demuxer);

	if (subtitle == NULL) {
		return;
	}

	int counter;
	AVSubtitleRect *dvbsubRect;
	char *text;
	char buf[1024];

	uint32_t screen_width, screen_height;

	int s = graphics_get_display_size(0, &screen_width, &screen_height);
	if (s != 0) {
		logInfo(LOG_DEMUXER, "graphics_get_display_size");
		return;
	}


//	osdClear(osd);
	for (counter=0; counter < subtitle->num_rects; counter++) {
		dvbsubRect = subtitle->rects[counter];

		switch (dvbsubRect->type) {
		case SUBTITLE_NONE: text = "NONE"; break;
		case SUBTITLE_BITMAP: text = "BITMAP"; break;
		case SUBTITLE_TEXT: text = "TEXT"; break;
		case SUBTITLE_ASS: text = "ASS"; break;
		}

		logInfo(LOG_DEMUXER, "DVB Subtitle rect %d: x=%d, y=%d, %dx%d (wxh), nb_colors=%d, type=%s (%d)| screen: %dx%d (wxh).\n",
				counter+1,
				dvbsubRect->x, dvbsubRect->y,
				dvbsubRect->w, dvbsubRect->h,
				dvbsubRect->nb_colors, text, dvbsubRect->type,
				screen_width, screen_height);

//			snprintf(buf, sizeof(buf), "%.0f.%d.pgm", pts, counter);
//			pgm_save(dvbsubRect->pict.data[0], dvbsubRect->pict.linesize[0], dvbsubRect->w, dvbsubRect->h, buf);

		AVPicture outputPicture;
		AVPicture tmpPicture;

		int newWidth = screen_width;
		int newHeight = ((newWidth / dvbsubRect->w) * dvbsubRect->h);
		logInfo(LOG_DEMUXER, "DVB Subtitle rect %d: new %dx%d (wxh).\n", counter, newWidth, newHeight);

		if (avpicture_alloc(&outputPicture, AV_PIX_FMT_RGBA, newWidth, newHeight) != 0) {
			logInfo(LOG_DEMUXER, "Error in allocating memory for new picture.\n");
			return;
		}
		if (avpicture_alloc(&tmpPicture, AV_PIX_FMT_RGBA, dvbsubRect->w, dvbsubRect->h) != 0) {
			logInfo(LOG_DEMUXER, "Error in allocating memory for tmp picture.\n");
			return;
		}
		logInfo(LOG_DEMUXER_DEBUG, "going to sws_getContext.\n");

		struct SwsContext *sws = sws_getContext(dvbsubRect->w, dvbsubRect->h, AV_PIX_FMT_RGBA,
						newWidth, newHeight, AV_PIX_FMT_RGBA,
						 SWS_BICUBIC, NULL, NULL, NULL);
		if (sws == NULL) {
			logInfo(LOG_DEMUXER, "Error sws_getContext.\n");

			return;
		}

		// in pict.data[0] each byte is the color code of one pixel.
		// The color for the pixel can be found in the Colour Look-up table (CLUT) in pict.data[1]
		// also do a horizontal mirroring because the subtitle image and the openvg have a different
		// Coordinate system: subtitle image Y is from top to bottom whil openvg Y is from bottom to top.
		logInfo(LOG_DEMUXER_DEBUG, "going to brighten the grayscale and do horizontal mirror.\n");
		int x, y;
		uint8_t *oldPixel;
		uint32_t *pointer = (uint32_t *)tmpPicture.data[0];

		logInfo(LOG_DEMUXER, "dvbsubRect->pict.linesize[0]=%d\n", dvbsubRect->pict.linesize[0]);

		uint32_t *clut_table;

		for (y = (dvbsubRect->h - 1); y >= 0; y--) {
			for (x = 0; x < dvbsubRect->w; x++) {
				oldPixel = dvbsubRect->pict.data[0] + ( y * dvbsubRect->w) + x;

				clut_table = (uint32_t *)(dvbsubRect->pict.data[1]) + *oldPixel;
				//printf("%.2x", *oldPixel);
				*(pointer++) = *clut_table;
			}
				//printf("\n");
		}

		// Going to scale the subtitle image to fit the window size
		logInfo(LOG_DEMUXER_DEBUG, "going to sws_scale.\n");
		int finalHeight = sws_scale(sws, tmpPicture.data, tmpPicture.linesize, 0, dvbsubRect->h, outputPicture.data, outputPicture.linesize);
		logInfo(LOG_DEMUXER_DEBUG, "DVB Subtitle rect %d: finalHeight=%d.\n", counter, finalHeight);

/*		// We are going to from 1 byte grey to 4 byte RGBA
		uint8_t *newImage = malloc(newWidth*newHeight*4);
		uint8_t *channel;
		
		channel = newImage;
		oldPixel = outputPicture.data[0];
		for (y = 0; y < newHeight; y++) {
			for (x = 0; x < newWidth; x++) {
				if (oldPixel[0] == 0) {
					channel[3] = 0; // Alpha (clear)
				}
				else {
					channel[0] = oldPixel[0]; // Blue
					channel[1] = oldPixel[0]; // Green
					channel[2] = oldPixel[0]; // Red
					channel[3] = 0xFF; // Alpha (visible)
				}
				channel += 4;
				oldPixel++;
			}
		}
*/
		int xpos = ((newWidth / dvbsubRect->w) * dvbsubRect->x);

		int ypos = screen_height - ((newWidth / dvbsubRect->w) * dvbsubRect->y);

		// write out subtitle image
		vgWritePixels(outputPicture.data[0], outputPicture.linesize[0], VG_sARGB_8888, xpos, ypos, newWidth, newHeight);
//		vgWritePixels(newImage, newWidth*4, VG_sARGB_8888, xpos, ypos, newWidth, newHeight);

		VGErrorCode vg_error = vgGetError();
		if (vg_error != VG_NO_ERROR) {
			logInfo(LOG_DEMUXER, "Error vgWritePixels. %d.%d (x.y) xpos=%d, ypos=%d (error=%d).\n)", x, y, xpos, ypos, vg_error);
//			free(newImage);
			avpicture_free(&outputPicture);	
			sws_freeContext(sws);	
			avsubtitle_free(subtitle);
			return;
		}

//		free(newImage);

		logInfo(LOG_DEMUXER_DEBUG, "ypos=%d.\n", ypos);

		avpicture_free(&outputPicture);	
		avpicture_free(&tmpPicture);	
		sws_freeContext(sws);	
	}

	avsubtitle_free(subtitle);
}

AVSubtitle *decodeDVBSub(struct DEMUXER_T *demuxer, AVPacket *inPacket, double pts)
{
	if (!demuxer->dvbsubCodecContext) return NULL;

	if (inPacket->size == 0) {
		logInfo(LOG_DEMUXER, "Sizeof subtitle packet == 0.\n");
		return NULL;
	}

	AVPacket avpkt;
	av_init_packet(&avpkt);
	avpkt.data = inPacket->data;
	avpkt.size = inPacket->size;
	avpkt.pts = inPacket->pts;
	avpkt.dts = inPacket->dts;

	logInfo(LOG_DEMUXER, "Going to get memory for decoded packet.\n");
	AVSubtitle *subtitle = malloc(sizeof(AVSubtitle));

	int got_frame;

	logInfo(LOG_DEMUXER, "Going to decode dvb subtitle packet.\n");
	int iBytesUsed = avcodec_decode_subtitle2(demuxer->dvbsubCodecContext
						, subtitle
						, &got_frame
						, &avpkt);
                            
	logInfo(LOG_DEMUXER, "Decoded packet. iBytesUsed=%d from packet.size=%d. got_frame=%d, demuxer->dvbsubCodecContext->pix_fmt=%d.\n", iBytesUsed, inPacket->size, got_frame, demuxer->dvbsubCodecContext->pix_fmt);

	if (!got_frame) {
		logInfo(LOG_DEMUXER, "Did not have a full frame in the packet.\n");
	}

	if (iBytesUsed < 0) {
		logInfo(LOG_DEMUXER, "iBytesUsed < 0.\n");
	}

	if (iBytesUsed != inPacket->size) {
		logInfo(LOG_DEMUXER, "iBytesUsed != inPacket.size  => %d = %d.\n", iBytesUsed, inPacket->size);
	}

	if (iBytesUsed < 0 || !got_frame) {

		if (got_frame) {
			avsubtitle_free(subtitle);
		}

		free(subtitle);

		return NULL;
	}

	return subtitle;
}

double ffmpegConvertTimestamp(struct DEMUXER_T *demuxer, int64_t pts, AVRational *time_base)
{
	double new_pts = pts;

	if(demuxer == NULL)
		return 0;

	if (pts == (int64_t)AV_NOPTS_VALUE)
		return DVD_NOPTS_VALUE;

	new_pts -=demuxer->startPTS;

	new_pts *= av_q2d(*time_base);

	new_pts *= (double)DVD_TIME_BASE;

	return new_pts;
}

double ffmpegConvertTimeOut(int64_t pts, AVRational *time_base)
{
	double new_pts = pts;

	if (pts == (int64_t)AV_NOPTS_VALUE)
		return DVD_NOPTS_VALUE;

	new_pts *= av_q2d(*time_base);

	new_pts *= (double)DVD_TIME_BASE;

	return new_pts;
}

OMX_ERRORTYPE demuxerInitOMXVideo(struct DEMUXER_T *demuxer)
{
	OMX_ERRORTYPE omxErr;
	OMX_IMAGE_CODINGTYPE compressionFormat;

	if (demuxer->videoStream > -1) {

		// First check if we can handle the video type.
		switch (demuxer->videoCodec->id) {
		case AV_CODEC_ID_H264:
			logInfo(LOG_DEMUXER, "This video stream has a codecid of H264.\n");
/*			if (licenseH264IsInstalled() == 0) {
				logInfo(LOG_DEMUXER, "No hardware decoding license is avaialble for H264. Get the license if you want to play it. CodeID=%d.\n",demuxer->videoCodec->id);
				return OMX_ErrorNotImplemented;
			}
*/			compressionFormat = OMX_VIDEO_CodingAVC;
			break;
		case AV_CODEC_ID_MPEG1VIDEO:
			logInfo(LOG_DEMUXER, "This video stream has a codecid of MPEG1VIDEO.\n");
		case AV_CODEC_ID_MPEG2VIDEO:
			if (demuxer->videoCodec->id == AV_CODEC_ID_MPEG2VIDEO) {
				logInfo(LOG_DEMUXER, "This video stream has a codecid of MPEG2VIDEO.\n");
			}
/*			if (licenseMPEG2IsInstalled() == 0) {
				logInfo(LOG_DEMUXER, "No hardware decoding license is avaialble for MPEG2VIDEO. Get the license if you want to play it. CodeID=%d.\n",demuxer->videoCodec->id);
				return OMX_ErrorNotImplemented;
			}
*/			compressionFormat = OMX_VIDEO_CodingMPEG2;
			break;
		case AV_CODEC_ID_VC1:
			logInfo(LOG_DEMUXER, "This video stream has a codecid of VC1VIDEO.\n");
			if (licenseVC1IsInstalled() == 0) {
				logInfo(LOG_DEMUXER, "No hardware decoding license is avaialble for VC1VIDEO. Get the license if you want to play it. CodeID=%d.\n",demuxer->videoCodec->id);
				return OMX_ErrorNotImplemented;
			}
			compressionFormat = OMX_VIDEO_CodingMVC;
			break;
		default:
			logInfo(LOG_DEMUXER, "This video stream has a codecid I cannot handle yet. CodeID=%d.\n",demuxer->videoCodec->id);
			return OMX_ErrorNotImplemented;
		}

		// For a good video output we need the following component stream
		// 1. TV progressive or in interlaced mode: 
		// 		video_decoder -> video_scheduler -> video_render -> hdmi
		//			       clock -^
		// 2. TV progressive and deinterlace done in pi hardware: 
		//		video_decoder -> image_fx -> video_scheduler -> video_render -> hdmi
		//				           clock -^

		// Create video decoder
		demuxer->videoDecoder = omxCreateComponent("OMX.broadcom.video_decode" , OMX_IndexParamVideoInit);
		if (demuxer->videoDecoder == NULL) {
			logInfo(LOG_DEMUXER, "Error creating OMX video decoder component. (Error=0x%08x).\n", omx_error);
			return omx_error;
		}
		logInfo(LOG_DEMUXER_DEBUG, "Created OMX video decoder component.\n");

		// Create video image_fx
		demuxer->videoImageFX = omxCreateComponent("OMX.broadcom.image_fx" , OMX_IndexParamImageInit);
		if (demuxer->videoImageFX == NULL) {
			logInfo(LOG_DEMUXER, "Error creating OMX video image_fx component. (Error=0x%08x).\n", omx_error);
			return omx_error;
		}
		logInfo(LOG_DEMUXER_DEBUG, "Created OMX video image_fx component.\n");

		// Create video scheduler
		demuxer->videoScheduler = omxCreateComponent("OMX.broadcom.video_scheduler" , OMX_IndexParamVideoInit);
		if (demuxer->videoScheduler == NULL) {
			logInfo(LOG_DEMUXER, "Error creating OMX video scheduler component. (Error=0x%08x).\n", omx_error);
			return omx_error;
		}
		logInfo(LOG_DEMUXER_DEBUG, "Created OMX video scheduler component.\n");

		// Create video render
		demuxer->videoRender = omxCreateComponent("OMX.broadcom.video_render" , OMX_IndexParamVideoInit);
		if (demuxer->videoRender == NULL) {
			logInfo(LOG_DEMUXER, "Error creating OMX video render component. (Error=0x%08x).\n", omx_error);
			return omx_error;
		}
		logInfo(LOG_DEMUXER_DEBUG, "Created OMX video render component.\n");

		// Now we are going to create the tunnels.

		// Tunnel between Decoder and Scheduler when the pi is not going to deinterlace.
		demuxer->videoDecoderToSchedulerTunnel = omxCreateTunnel(demuxer->videoDecoder, demuxer->videoDecoder->outputPort, demuxer->videoScheduler, demuxer->videoScheduler->inputPort);
		if (demuxer->videoDecoderToSchedulerTunnel == NULL) {
			logInfo(LOG_DEMUXER, "Error creating tunnel between video decoder and video scheduler components. (Error=0x%08x).\n", omx_error);
			return omx_error;
		}

		// Tunnel between decoder and image_fx for when the pi is doing the deinterlace.
		demuxer->videoDecoderToImageFXTunnel = omxCreateTunnel(demuxer->videoDecoder, demuxer->videoDecoder->outputPort, demuxer->videoImageFX, demuxer->videoImageFX->inputPort);
		if (demuxer->videoDecoderToImageFXTunnel == NULL) {
			logInfo(LOG_DEMUXER, "Error creating tunnel between video decoder and video image_fx components. (Error=0x%08x).\n", omx_error);
			return omx_error;
		}

		// Tunnel between image_fx and scheduler for when the pi is doing the deinterlace.
		demuxer->videoImageFXToSchedulerTunnel = omxCreateTunnel(demuxer->videoImageFX, demuxer->videoImageFX->outputPort, demuxer->videoScheduler, demuxer->videoScheduler->inputPort);
		if (demuxer->videoImageFXToSchedulerTunnel == NULL) {
			logInfo(LOG_DEMUXER, "Error creating tunnel between video image_fx and video scheduler components. (Error=0x%08x).\n", omx_error);
			return omx_error;
		}

/*		if (demuxer->deInterlace == 1) {

			omxErr = omxSetVideoDeInterlace(demuxer->videoImageFX, 1);
			if (omxErr != OMX_ErrorNone) {
				logInfo(LOG_DEMUXER, "Error omxSetVideoDeInterlace on video image_fx component. (Error=0x%08x).\n", omxErr);
				return omxErr;
			}

		}
*/

		// Tunnel between Scheduler and Render.
		demuxer->videoSchedulerToRenderTunnel = omxCreateTunnel(demuxer->videoScheduler, demuxer->videoScheduler->outputPort, demuxer->videoRender, demuxer->videoRender->inputPort);
		if (demuxer->videoSchedulerToRenderTunnel == NULL) {
			logInfo(LOG_DEMUXER, "Error creating tunnel between video scheduler and video render components. (Error=0x%08x).\n", omx_error);
			return omx_error;
		}

		// Tunnel between Clock and Scheduler
		demuxer->clockToVideoSchedulerTunnel = omxCreateTunnel(demuxer->clock->clockComponent, demuxer->clock->clockComponent->clockPort+1, demuxer->videoScheduler, demuxer->videoScheduler->clockPort);
		if (demuxer->clockToVideoSchedulerTunnel == NULL) {
			logInfo(LOG_DEMUXER, "Error creating tunnel between clock and video scheduler components. (Error=0x%08x).\n", omx_error);
			return omx_error;
		}

		// Exsthablish tunnel between clock and scheduler.
		omxErr = omxEstablishTunnel(demuxer->clockToVideoSchedulerTunnel);
		if(omxErr != OMX_ErrorNone) {
			logInfo(LOG_DEMUXER, "Error establishing tunnel between clock and video scheduler components. (Error=0x%08x).\n", omxErr);
			return omxErr;
		}

		// Now we are going to configure the video decoder. It needs to be in the idle state for this.
		logInfo(LOG_DEMUXER, "Setting state of videoDecoder to OMX_StateIdle.\n");
		omxErr = omxSetStateForComponent(demuxer->videoDecoder, OMX_StateIdle, 50000);
		if (omxErr != OMX_ErrorNone)
		{
			logInfo(LOG_DEMUXER, "video decoder SetStateForComponent to OMX_StateIdle. (Error=0x%08x).\n", omxErr);
			return omxErr;
		}

//		logInfo(LOG_DEMUXER, "This video stream has a framerate of %f fps.\n", av_q2d(demuxer->formatContext->streams[demuxer->videoStream]->avg_frame_rate));
//		logInfo(LOG_DEMUXER, "This video stream has a framerate of %f fps.\n", av_q2d(demuxer->formatContext->streams[demuxer->videoStream]->r_frame_rate));

/*		switch (demuxer->videoCodec->id) {
		case AV_CODEC_ID_H264:
			logInfo(LOG_DEMUXER, "This video stream has a codecid of H264.\n");
			omxErr = omxSetVideoCompressionFormatAndFrameRate(demuxer->videoDecoder, OMX_VIDEO_CodingAVC, av_q2d(demuxer->formatContext->streams[demuxer->videoStream]->avg_frame_rate));
			break;
		case AV_CODEC_ID_MPEG2VIDEO:
			logInfo(LOG_DEMUXER, "This video stream has a codecid of MPEG2VIDEO.\n");
			omxErr = omxSetVideoCompressionFormatAndFrameRate(demuxer->videoDecoder, OMX_VIDEO_CodingMPEG2, av_q2d(demuxer->formatContext->streams[demuxer->videoStream]->avg_frame_rate));
			break;
		default:
			logInfo(LOG_DEMUXER, "This video stream has a codecid I cannot handle yet. CodeID=%d.\n",demuxer->videoCodec->id);
		}
*/
		omxErr = omxSetVideoCompressionFormat(demuxer->videoDecoder, compressionFormat);
		if (omxErr != OMX_ErrorNone)
		{
			logInfo(LOG_DEMUXER, "video decoder omxSetVideoCompressionFormat. (Error=0x%08x).\n", omxErr);
			return omxErr;
		}

		logInfo(LOG_DEMUXER, "This video stream has a framesize of %dx%d.\n", demuxer->formatContext->streams[demuxer->videoStream]->codec->width, demuxer->formatContext->streams[demuxer->videoStream]->codec->height);
		omxErr = omxVideoSetFrameSize(demuxer->videoDecoder, demuxer->formatContext->streams[demuxer->videoStream]->codec->width, demuxer->formatContext->streams[demuxer->videoStream]->codec->height);
		if (omxErr != OMX_ErrorNone)
		{
			logInfo(LOG_DEMUXER, "video decoder omxSetFrameSize. (Error=0x%08x).\n", omxErr);
			return omxErr;
		}

		omxErr = omxVideoStartWithValidFrame(demuxer->videoDecoder, 0);
		if (omxErr != OMX_ErrorNone)
		{
			logInfo(LOG_DEMUXER, "video decoder omxStartWithValidFrame. (Error=0x%08x).\n", omxErr);
			return omxErr;
		}

	//	if(1 == 2 /*m_hdmi_clock_sync*/)
	//	{
/*			OMX_CONFIG_LATENCYTARGETTYPE latencyTarget;
			OMX_INIT_STRUCTURE(latencyTarget);
			latencyTarget.nPortIndex = demuxer->videoRender->inputPort;
			latencyTarget.bEnabled = OMX_TRUE;
			latencyTarget.nFilter = 2;
			latencyTarget.nTarget = 4000;
			latencyTarget.nShift = 3;
			latencyTarget.nSpeedFactor = -135;
			latencyTarget.nInterFactor = 500;
			latencyTarget.nAdjCap = 20;

			omxErr = OMX_SetParameter(demuxer->videoRender->handle, OMX_IndexConfigLatencyTarget, &latencyTarget);
			if (omxErr != OMX_ErrorNone)
			{
				logInfo(LOG_DEMUXER, "video render OMX_IndexConfigLatencyTarget error (0%08x)\n", omxErr);
				return omxErr;
			}
*/	//	}

		omxErr = omxSetVideoSetExtraBuffers(demuxer->videoDecoder);
		if (omxErr != OMX_ErrorNone)
		{
			logInfo(LOG_DEMUXER, "video decoder omxSetVideoSetExtraBuffers. (Error=0x%08x).\n", omxErr);
			return omxErr;
		}

		omxErr = omxAllocInputBuffers(demuxer->videoDecoder, 0);
		if (omxErr != OMX_ErrorNone)
		{
			logInfo(LOG_DEMUXER, "video decoder omxAllocInputBuffers. (Error=%d).\n", omxErr);
			return omxErr;
		}

		logInfo(LOG_DEMUXER, "Setting state of videoDecoder to OMX_StateExecuting.\n");
		omxErr = omxSetStateForComponent(demuxer->videoDecoder, OMX_StateExecuting, 50000);
		if (omxErr != OMX_ErrorNone)
		{
			logInfo(LOG_DEMUXER, "video decoder SetStateForComponent to OMX_StateExecuting. (Error=0x%08x).\n", omxErr);
			return omxErr;
		}
	}

	return OMX_ErrorNone;
}

OMX_ERRORTYPE demuxerInitOMXAudio(struct DEMUXER_T *demuxer)
{
	OMX_ERRORTYPE omxErr;

	if (demuxer->audioStream > -1) {

		if (demuxer->swDecodeAudio == 0) {
			demuxer->audioDecoder = omxCreateComponent("OMX.broadcom.audio_decode" , OMX_IndexParamAudioInit);
			if (demuxer->audioDecoder == NULL) {
				logInfo(LOG_DEMUXER, "Error creating OMX audio decoder component. (Error=0x%08x).\n", omx_error);
				return omx_error;
			}
			logInfo(LOG_DEMUXER_DEBUG, "Created OMX audio decoder component.\n");

			omxErr = omxSetAudioPassthrough(demuxer->audioDecoder, demuxer->audioPassthrough);
			if (omxErr != OMX_ErrorNone)
			{
				logInfo(LOG_DEMUXER, "audio render omxSetAudioClockAsSourceReference. (Error=0x%08x).\n", omxErr);
				return omxErr;
			}
		}

		demuxer->audioRender = omxCreateComponent("OMX.broadcom.audio_render" , OMX_IndexParamAudioInit);
		if (demuxer->audioRender == NULL) {
			logInfo(LOG_DEMUXER, "Error creating OMX audio render component. (Error=0x%08x).\n", omx_error);
			return omx_error;
		}
		logInfo(LOG_DEMUXER_DEBUG, "Created OMX audio render component.\n");

		omxErr = omxSetAudioDestination(demuxer->audioRender, "hdmi");
		if (omxErr != OMX_ErrorNone)
		{
			logInfo(LOG_DEMUXER, "audio render omxSetAudioDestination. (Error=0x%08x).\n", omxErr);
			return omxErr;
		}
		
		omxErr = omxSetAudioClockAsSourceReference(demuxer->audioRender, 0);
		if (omxErr != OMX_ErrorNone)
		{
			logInfo(LOG_DEMUXER, "audio render omxSetAudioClockAsSourceReference. (Error=0x%08x).\n", omxErr);
			return omxErr;
		}
		
		demuxer->clockToAudioRenderTunnel = omxCreateTunnel(demuxer->clock->clockComponent, demuxer->clock->clockComponent->clockPort, demuxer->audioRender, demuxer->audioRender->clockPort);
		if (demuxer->clockToAudioRenderTunnel == NULL) {
			logInfo(LOG_DEMUXER, "Error creating tunnel between clock and audio render components. (Error=0x%08x).\n", omx_error);
			return omx_error;
		}

		omxErr = omxEstablishTunnel(demuxer->clockToAudioRenderTunnel);
		if(omxErr != OMX_ErrorNone) {
			logInfo(LOG_DEMUXER, "Error establishing tunnel between clock and audio scheduler components. (Error=0x%08x).\n", omxErr);
			return omxErr;
		}

		if (demuxer->swDecodeAudio == 1) {

			logInfo(LOG_DEMUXER_DEBUG, "Going to configure audio render for receiving software decoded stream.\n");

			omxErr = omxSetAudioRenderInput(demuxer->audioRender, 48000, 16, 2);
			if(omxErr != OMX_ErrorNone) {
				logInfo(LOG_DEMUXER, "Error omxSetAudioRenderInput on audio render. (Error=0x%08x).\n", omxErr);
				return omxErr;
			}

			logInfo(LOG_DEMUXER, "Setting state of audioRender to OMX_StateIdle.\n");
			omxErr = omxSetStateForComponent(demuxer->audioRender, OMX_StateIdle, 50000);
			if (omxErr != OMX_ErrorNone)
			{
				logInfo(LOG_DEMUXER, "audio render SetStateForComponent to OMX_StateIdle. (Error=0x%08x).\n", omxErr);
				return omxErr;
			}

			omxErr = omxAllocInputBuffers(demuxer->audioRender, 0);
			if (omxErr != OMX_ErrorNone)
			{
				logInfo(LOG_DEMUXER, "audio render omxAllocInputBuffers. (Error=0x%08x).\n", omxErr);
				return omxErr;
			}

			logInfo(LOG_DEMUXER, "Setting state of audioRender to OMX_StateExecuting.\n");
			omxErr = omxSetStateForComponent(demuxer->audioRender, OMX_StateExecuting, 50000);
			if (omxErr != OMX_ErrorNone)
			{
				logInfo(LOG_DEMUXER, "audio render SetStateForComponent to OMX_StateExecuting. (Error=0x%08x).\n", omxErr);
				return omxErr;
			}
		}

		if ((demuxer->audioPassthrough == 0) && (demuxer->swDecodeAudio == 0)) {
			demuxer->audioMixer = omxCreateComponent("OMX.broadcom.audio_mixer" , OMX_IndexParamAudioInit);
			if (demuxer->audioMixer == NULL) {
				logInfo(LOG_DEMUXER, "Error creating OMX audio mixer component. (Error=0x%08x).\n", omx_error);
				return omx_error;
			}
			logInfo(LOG_DEMUXER_DEBUG, "Created OMX audio mixer component.\n");

			demuxer->audioDecoderToMixerTunnel = omxCreateTunnel(demuxer->audioDecoder, demuxer->audioDecoder->outputPort, demuxer->audioMixer, demuxer->audioMixer->inputPort);
			if (demuxer->audioDecoderToMixerTunnel == NULL) {
				logInfo(LOG_DEMUXER, "Error creating tunnel between audio decoder and audio mixer components. (Error=0x%08x).\n", omx_error);
				return omx_error;
			}

			demuxer->audioMixerToRenderTunnel = omxCreateTunnel(demuxer->audioMixer, demuxer->audioMixer->outputPort, demuxer->audioRender, demuxer->audioRender->inputPort);
			if (demuxer->audioMixerToRenderTunnel == NULL) {
				logInfo(LOG_DEMUXER, "Error creating tunnel between audio mixer and audio render components. (Error=0x%08x).\n", omx_error);
				return omx_error;
			}
		}
		else {
			if (demuxer->swDecodeAudio == 0) {
				demuxer->audioDecoderToRenderTunnel = omxCreateTunnel(demuxer->audioDecoder, demuxer->audioDecoder->outputPort, demuxer->audioRender, demuxer->audioRender->inputPort);
				if (demuxer->audioDecoderToRenderTunnel == NULL) {
					logInfo(LOG_DEMUXER, "Error creating tunnel between audio decoder and audio render components. (Error=0x%08x).\n", omx_error);
					return omx_error;
				}
			}
		}

		if (demuxer->swDecodeAudio == 0) {
			logInfo(LOG_DEMUXER, "This audio stream has codec_id=%d, sample_rate=%d, bits_per_coded_sample=%d, channels=%d, passthrough=%d.\n", 
					demuxer->formatContext->streams[demuxer->audioStream]->codec->codec_id, 
					demuxer->formatContext->streams[demuxer->audioStream]->codec->sample_rate, 
					demuxer->formatContext->streams[demuxer->audioStream]->codec->bits_per_coded_sample,
					demuxer->formatContext->streams[demuxer->audioStream]->codec->channels,
					demuxer->audioPassthrough);
			omxErr = omxSetAudioCompressionFormatAndBuffer(demuxer->audioDecoder, demuxer->formatContext->streams[demuxer->audioStream]->codec->codec_id, 
					demuxer->formatContext->streams[demuxer->audioStream]->codec->sample_rate, 
					demuxer->formatContext->streams[demuxer->audioStream]->codec->bits_per_coded_sample,
					demuxer->formatContext->streams[demuxer->audioStream]->codec->channels,
					demuxer->audioPassthrough);
			if (omxErr != OMX_ErrorNone)
			{
				logInfo(LOG_DEMUXER, "audio decoder omxSetAudioCompressionFormatAndBuffer. (Error=0x%08x).\n", omxErr);
				return omxErr;
			}

			logInfo(LOG_DEMUXER, "Setting state of audioDecoder to OMX_StateIdle.\n");
			omxErr = omxSetStateForComponent(demuxer->audioDecoder, OMX_StateIdle, 50000);
			if (omxErr != OMX_ErrorNone)
			{
				logInfo(LOG_DEMUXER, "audio decoder SetStateForComponent to OMX_StateIdle. (Error=0x%08x).\n", omxErr);
				return omxErr;
			}

			omxErr = omxAllocInputBuffers(demuxer->audioDecoder, 0);
			if (omxErr != OMX_ErrorNone)
			{
				logInfo(LOG_DEMUXER, "audio decoder omxAllocInputBuffers. (Error=0x%08x).\n", omxErr);
				return omxErr;
			}

			logInfo(LOG_DEMUXER, "Setting state of audioDecoder to OMX_StateExecuting.\n");
			omxErr = omxSetStateForComponent(demuxer->audioDecoder, OMX_StateExecuting, 50000);
			if (omxErr != OMX_ErrorNone)
			{
				logInfo(LOG_DEMUXER, "audio decoder SetStateForComponent to OMX_StateExecuting. (Error=0x%08x).\n", omxErr);
				return omxErr;
			}

			if ((demuxer->audioDecoder->useHWDecode == 1) && (demuxer->audioPassthrough == 0)) {
				omxErr = omxSetAudioExtraData(demuxer->audioDecoder, demuxer->formatContext->streams[demuxer->audioStream]->codec->extradata, 
						demuxer->formatContext->streams[demuxer->audioStream]->codec->extradata_size);
				if (omxErr != OMX_ErrorNone)
				{
					logInfo(LOG_DEMUXER, "audio decoder omxSetAudioExtraData. (Error=0x%08x).\n", omxErr);
					return omxErr;
				}

			}

			if (demuxer->audioPassthrough == 0) {

				OMX_AUDIO_PARAM_PCMMODETYPE pcmOutput;
				OMX_AUDIO_PARAM_PCMMODETYPE pcmInput;

				OMX_INIT_STRUCTURE(pcmOutput);
				OMX_INIT_STRUCTURE(pcmInput);

				omxErr = OMX_GetParameter(demuxer->audioDecoder->handle, OMX_IndexParamAudioPcm, &pcmInput);
				if (omxErr != OMX_ErrorNone)
				{
					logInfo(LOG_DEMUXER, "audio decoder GetParameter OMX_IndexParamAudioPcm error (0%08x)\n", omxErr);
					return omxErr;
				}

				pcmInput.nPortIndex = demuxer->audioMixer->inputPort;
				omxErr = OMX_SetParameter(demuxer->audioMixer->handle, OMX_IndexParamAudioPcm, &pcmInput);
				if (omxErr != OMX_ErrorNone)
				{
					logInfo(LOG_DEMUXER, "audio mixer SetParameter OMX_IndexParamAudioPcm  on inputport error (0%08x)\n", omxErr);
					return omxErr;
				}

				pcmOutput.nPortIndex = demuxer->audioMixer->outputPort;
				omxErr = OMX_SetParameter(demuxer->audioMixer->handle, OMX_IndexParamAudioPcm, &pcmOutput);
				if (omxErr != OMX_ErrorNone)
				{
					logInfo(LOG_DEMUXER, "audio mixer SetParameter OMX_IndexParamAudioPcm on outputport error (0%08x)\n", omxErr);
					return omxErr;
				}

				omxErr = OMX_GetParameter(demuxer->audioMixer->handle, OMX_IndexParamAudioPcm, &pcmOutput);
				if (omxErr != OMX_ErrorNone)
				{
					logInfo(LOG_DEMUXER, "audio mixer GetParameter OMX_IndexParamAudioPcm outputport error (0%08x)\n", omxErr);
					return omxErr;
				}

				pcmOutput.nPortIndex = demuxer->audioRender->inputPort;
				omxErr = OMX_SetParameter(demuxer->audioRender->handle, OMX_IndexParamAudioPcm, &pcmOutput);
				if (omxErr != OMX_ErrorNone)
				{
					logInfo(LOG_DEMUXER, "audio render SetParameter OMX_IndexParamAudioPcm on inputport error (0%08x)\n", omxErr);
					return omxErr;
				}


			}
		}

		omxErr = omxSetAudioVolume(demuxer->audioRender, 100);
	}

	return OMX_ErrorNone;
}

void *videoLoop(struct DEMUXER_T *demuxer)
{
	while (videoIsStopped(demuxer) == 0) {
		sleep(1);
	}
	return NULL;
}

void audioAddPacketToList(struct DEMUXER_T *demuxer, AVPacket *pkt)
{
	logInfo(LOG_DEMUXER_DEBUG, "Adding packet to audio list.\n")
	
	pthread_mutex_lock(&demuxer->audioThreadLock);

	if (demuxer->audioPackets == NULL) {
		demuxer->audioPackets = createSimpleListItem(pkt);
		demuxer->audioPacketsEnd = demuxer->audioPackets;
	}
	else {
		addObjectToSimpleList(demuxer->audioPacketsEnd, pkt);
		demuxer->audioPacketsEnd = demuxer->audioPacketsEnd->next;
	}

	pthread_mutex_unlock(&demuxer->audioThreadLock);
	logInfo(LOG_DEMUXER_DEBUG, "Adding packet to audio list.\n")
}

AVPacket *audioGetPacketFromList(struct DEMUXER_T *demuxer)
{
	logInfo(LOG_DEMUXER_DEBUG, "Removing packet from audio list.\n")
	AVPacket *result = NULL;

	pthread_mutex_lock(&demuxer->audioThreadLock);

	if (demuxer->audioPackets == NULL) {
		pthread_mutex_unlock(&demuxer->audioThreadLock);
		logInfo(LOG_DEMUXER_DEBUG, "No available packet in audio list.\n")
		return NULL;
	}

	result = (AVPacket *)demuxer->audioPackets->object;
	struct SIMPLELISTITEM_T *oldItem = demuxer->audioPackets;
	demuxer->audioPackets = demuxer->audioPackets->next;
	if (demuxer->audioPackets == NULL) {
		demuxer->audioPacketsEnd = NULL;
	}
	freeSimpleListItem(oldItem);

	pthread_mutex_unlock(&demuxer->audioThreadLock);

	logInfo(LOG_DEMUXER_DEBUG, "Removed packet from audio list.\n")
	return result;
}

void *audioLoop(struct DEMUXER_T *demuxer)
{
	AVPacket *pkt = NULL;

	struct timespec interval;
	struct timespec remainingInterval;

	double tmpPTS = 0;

	while (audioIsStopped(demuxer) == 0) {
		if (pkt == NULL) {
			pkt = audioGetPacketFromList(demuxer);
		}

		if (pkt != NULL) {
			tmpPTS = ffmpegConvertTimestamp(demuxer, pkt->pts, &demuxer->formatContext->streams[pkt->stream_index]->time_base);
			if (decodeAudio(demuxer, pkt, tmpPTS) == 1) {
				av_free_packet(pkt);
				free(pkt);
				pkt = NULL;
			}
			// Else retry
		}

		interval.tv_sec = 0;
		interval.tv_nsec = 10000;

		nanosleep(&interval, &remainingInterval);
	}

	return NULL;
}

void subtitleAddAVPacketToList(struct DEMUXER_T *demuxer, AVPacket *pkt)
{
	logInfo(LOG_DEMUXER_DEBUG, "Adding packet to subtitle list.\n")
	pthread_mutex_lock(&demuxer->subtitleThreadLock);

	if (demuxer->subtitleAVPackets == NULL) {
		demuxer->subtitleAVPackets = createSimpleListItem(pkt);
		demuxer->subtitleAVPacketsEnd = demuxer->subtitleAVPackets;
	}
	else {
		addObjectToSimpleList(demuxer->subtitleAVPacketsEnd, pkt);
		demuxer->subtitleAVPacketsEnd = demuxer->subtitleAVPacketsEnd->next;
	}

	pthread_mutex_unlock(&demuxer->subtitleThreadLock);
	logInfo(LOG_DEMUXER_DEBUG, "Added packet to subtitle list.\n")
}

AVPacket *subtitleGetAVPacketFromList(struct DEMUXER_T *demuxer)
{
	logInfo(LOG_DEMUXER_DEBUG, "Removing packet from subtitle list.\n")
	AVPacket *result = NULL;

	pthread_mutex_lock(&demuxer->subtitleThreadLock);

	if (demuxer->subtitleAVPackets == NULL) {
		pthread_mutex_unlock(&demuxer->subtitleThreadLock);
		logInfo(LOG_DEMUXER_DEBUG, "No available packet in subtitle list.\n")
		return NULL;
	}

	result = (AVPacket *)demuxer->subtitleAVPackets->object;
	struct SIMPLELISTITEM_T *oldItem = demuxer->subtitleAVPackets;
	demuxer->subtitleAVPackets = demuxer->subtitleAVPackets->next;
	if (demuxer->subtitleAVPackets == NULL) {
		demuxer->subtitleAVPacketsEnd = NULL;
	}
	freeSimpleListItem(oldItem);

	pthread_mutex_unlock(&demuxer->subtitleThreadLock);

	logInfo(LOG_DEMUXER_DEBUG, "Removed packet from subtitle list.\n")
	return result;
}

void *subtitleLoop(struct DEMUXER_T *demuxer)
{
	AVPacket *pkt = NULL;

	struct timespec interval;
	struct timespec remainingInterval;

	double tmpPTS = 0;

	demuxer->subtitleOSD = osdCreate(2, 0, 0, GRAPHICS_RGBA32(0,0,0,0x0), &paintDVBSubtitle);

	AVSubtitle *subtitle = NULL;

	int showing = 0;
	uint64_t timeToShow = 0;

	while (subtitleIsStopped(demuxer) == 0) {
		if ((pkt == NULL) && (showing == 0)) {
			pkt = subtitleGetAVPacketFromList(demuxer);
		}

		if ((pkt != NULL) && (showing == 0)) {
			tmpPTS = ffmpegConvertTimestamp(demuxer, pkt->pts, &demuxer->formatContext->streams[pkt->stream_index]->time_base);
			subtitle = decodeDVBSub(demuxer, pkt, tmpPTS);
			if (subtitle != NULL) {
				logInfo(LOG_DEMUXER, "AVSubtitle.format=%ud.\n", subtitle->format);
				logInfo(LOG_DEMUXER, "AVSubtitle.start_display_time=%ud.\n", subtitle->start_display_time);
				logInfo(LOG_DEMUXER, "AVSubtitle.end_display_time=%ud.\n", subtitle->end_display_time);
				logInfo(LOG_DEMUXER, "AVSubtitle.num_rects=%ud.\n", subtitle->num_rects);
				logInfo(LOG_DEMUXER, "AVSubtitle.pts=%" PRId64 ", main PTS=%f.\n", subtitle->pts, tmpPTS);

				if (subtitle->num_rects > 0) {

					timeToShow = tmpPTS;
					showing = 1;
				}
				else {
					timeToShow = tmpPTS;
					showing = 2;
				}

				OMX_TICKS outTimeStamp;
				if ((demuxer->clock) && (omxGetClockCurrentMediaTime(demuxer->clock, demuxer->clock->clockComponent->clockPort, &outTimeStamp) == OMX_ErrorNone)) {
					uint64_t clockTime = FromOMXTime(outTimeStamp);
					if (((int64_t)clockTime - (int64_t)timeToShow) >= 1000000) {
						showing = 3; // Not showing because to old.
						avsubtitle_free(subtitle);
					}
				}
			}

			av_free_packet(pkt);
			free(pkt);
			pkt = NULL;
		}

		if ((showing > 0) && (showing != 3)) {
			// Get clock time
			OMX_TICKS outTimeStamp;
			if ((demuxer->clock) && (omxGetClockCurrentMediaTime(demuxer->clock, demuxer->clock->clockComponent->clockPort, &outTimeStamp) == OMX_ErrorNone)) {
				uint64_t clockTime = FromOMXTime(outTimeStamp);

				if (clockTime > timeToShow) {  // We only show subtitles which are not older than 1 second
					logInfo(LOG_DEMUXER, "clockTime=%" PRId64 ", timeToShow=%" PRId64 ".\n", clockTime, timeToShow);

					osdHide(demuxer->subtitleOSD);

					if ((showing == 1) & (((int64_t)clockTime - (int64_t)timeToShow) < 1000000)) {
						demuxerAddDVBSubtitle(demuxer, subtitle);
						logInfo(LOG_DEMUXER, "Showing with a timeout of '%" PRId64 "'.\n", (uint64_t)(subtitle->end_display_time * 1000));
						osdShowWithTimeoutMicroseconds(demuxer->subtitleOSD, (uint64_t)(subtitle->end_display_time * 1000), demuxer);
					}
					showing = 0; 
				}

			}
			else {
				// When we are not able to get a valid clock. Show subtitle. It will probably be out of sync.
				osdHide(demuxer->subtitleOSD);
				if (showing == 1) {
					demuxerAddDVBSubtitle(demuxer, subtitle);
					logInfo(LOG_DEMUXER, "Showing with a timeout of '%" PRId64 "'.\n", (uint64_t)(subtitle->end_display_time * 1000));
					osdShowWithTimeoutMicroseconds(demuxer->subtitleOSD, (uint64_t)(subtitle->end_display_time * 1000), demuxer);
				}
				showing = 0; 
			}
		
		}

		if (showing != 3) {
			interval.tv_sec = 0;
			interval.tv_nsec = 1000000;

			nanosleep(&interval, &remainingInterval);

			if ((demuxer->subtitleOSD->visible) && (demuxer->subtitleOSD->timeout > 0) && ((nowInMicroseconds() - demuxer->subtitleOSD->startTime) >= demuxer->subtitleOSD->timeout)) {
				logInfo(LOG_DEMUXER, "Hiding subtitleOSD.\n");
				osdHide(demuxer->subtitleOSD);
				showing = 0;
			}
		}
		else {
			showing = 0;
		}
	}

	osdHide(demuxer->subtitleOSD);
	osdDestroy(demuxer->subtitleOSD);
	return NULL;
}

/*void *mythLoop(struct DEMUXER_T *demuxer)
{
	ssize_t readLen;
	ssize_t receivedLen;
	unsigned long long int bytesAvailable;

	struct timespec interval;
	struct timespec remainingInterval;

	while (mythIsStopped(demuxer) == 0) {

		bytesAvailable = getConnectionDataLen(demuxer->mythConnection->transferConnection->connection);
		if (bytesAvailable < MIN_MYTH_QUEUE_LENGTH) {
			readLen = mythFiletransferRequestBlock(demuxer->mythConnection, 1024 * 16);
			logInfo( LOG_DEMUXER_DEBUG,"Myth will send %zd bytes of the requested %d.\n", readLen, 1024 * 16);
			receivedLen = fillConnectionBuffer(demuxer->mythConnection->transferConnection->connection, readLen, 1);
			logInfo( LOG_DEMUXER,"Connection buffer was filled with %zd bytes of the requested %zd.\n", receivedLen, readLen);
			
		}
		else {
			// Sleep
			interval.tv_sec = 0;
			interval.tv_nsec = 10;

			nanosleep(&interval, &remainingInterval);
		}
	}
	return NULL;
}
*/

void destroyPacket(struct AVPacket *pkt)
{
	logInfo(LOG_DEMUXER_DEBUG, "Going to destroy packet.\n");
	av_destruct_packet(pkt);
	logInfo(LOG_DEMUXER_DEBUG, "Destroyed packet.\n");
/*	av_free(pkt->data);
	pkt->data = NULL;
	pkt->size = 0;*/
}

void *demuxerLoop(struct DEMUXER_T *demuxer)
{
	AVPacket *packet = NULL;
	int doneReading = 0;

#ifdef PI
	if (omxInit() != 0) {
		goto theEnd;
	}

	OMX_ERRORTYPE omxErr;

//	demuxer->clock = omxCreateClock(demuxer->videoStream > -1? 1 : 0, demuxer->audioStream > -1? 1 : 0, demuxer->dvbsubStream > -1? 1 : 0, 1);
	demuxer->clock = omxCreateClock(demuxer->videoStream > -1? 1 : 0, demuxer->audioStream > -1? 1 : 0, 0, 1);
	if (demuxer->clock == NULL) {
		logInfo(LOG_DEMUXER, "Error creating Clock. (Error=0x%08x).\n", omx_error);
		goto theEnd;
	}
	logInfo(LOG_DEMUXER_DEBUG, "Created Clock.\n");


	omxErr = demuxerInitOMXVideo(demuxer);
	if (omxErr != OMX_ErrorNone) {
		goto theEnd;
	}

	omxErr = demuxerInitOMXAudio(demuxer);
	if (omxErr != OMX_ErrorNone) {
		goto theEnd;
	}


#endif

	demuxer->isOpen = 1;
	demuxer->dropState = 0;
	demuxer->setVideoStartTime = 1;
	demuxer->setAudioStartTime = 1;
	demuxer->firstFrame = 1;

//	struct timespec interval;
//	struct timespec remainingInterval;
	int64_t audioPTS = -1;
	int64_t videoPTS = -1;
	int64_t dvbsubPTS = -1;
	int64_t audioDTS = -1;
	int64_t videoDTS = -1;
	int64_t dvbsubDTS = -1;
	double tmpPTS = 0;
	int validPTSValues = 0;

#ifdef PI
	logInfo(LOG_DEMUXER, "Setting state of clock to OMX_StateExecuting.\n");
	omxErr = omxSetStateForComponent(demuxer->clock->clockComponent, OMX_StateExecuting, 50000);
	if (omxErr != OMX_ErrorNone)
	{
		logInfo(LOG_DEMUXER, "clock SetStateForComponent to OMX_StateExecuting. (Error=0x%08x).\n", omxErr);
		goto theEnd;
	}

	OMX_TIME_CONFIG_CLOCKSTATETYPE clockConfig;
	OMX_INIT_STRUCTURE(clockConfig);

	clockConfig.eState = OMX_TIME_ClockStateRunning;

	omxErr = OMX_SetConfig(demuxer->clock->clockComponent->handle, OMX_IndexConfigTimeClockState, &clockConfig);
	if(omxErr != OMX_ErrorNone) {
		logInfo(LOG_DEMUXER, "Error during starting of clock. (Error=0x%08x).\n", omxErr);
		goto theEnd;
	}

#endif
	logInfo(LOG_DEMUXER_DEBUG, "Started clock entering loop.\n");

//	packet.data = NULL;

	int skipCount = 40;

	demuxer->audioPortSettingChanged = 0;

	demuxer->startPTS = -1;
	demuxer->startDTS = -1;

	int packetProcessed = 1;

	while ((doneReading == 0) && (demuxerIsStopped(demuxer) == 0)) {
		// Free old packet
		if (packetProcessed == 1) {
			packetProcessed = 0;
//		if (packet.data == NULL) {

			logInfo(LOG_DEMUXER_DEBUG, "Going to read packet.\n");

			// Read new packet
			packet = malloc(sizeof(AVPacket));
			if (packet == NULL) {
				logInfo(LOG_DEMUXER, "Could not allocated memory for next packet.\n");
				doneReading = 1;
				continue;
			}

			av_init_packet(packet);
			packet->destruct = &destroyPacket;

			if (demuxerIsStopped(demuxer) != 0) {
				logInfo(LOG_DEMUXER, "Thread is asked to stop.\n");
				//av_free_packet(packet);
				free(packet);
				doneReading = 1;
				continue;
			}

			if (av_read_frame(demuxer->formatContext, packet)<0) {
				logInfo(LOG_DEMUXER, "Reading packet failed.\n");
				av_free_packet(packet);
				free(packet);
				packetProcessed = 1;
				continue;
			}

			logInfo(LOG_DEMUXER_DEBUG, "Read packet.\n");

			if (skipCount-- > 0) {
				logInfo(LOG_DEMUXER_DEBUG,"Skipping frame because they are the first. Still need to skip %d frames.\n", skipCount);
				av_free_packet(packet);
				free(packet);
				packetProcessed = 1;
				continue;
			}

			if ((demuxerIsStopped(demuxer) == 1) || (packet->data == NULL) || ((packet->stream_index != demuxer->videoStream) && (packet->stream_index != demuxer->dvbsubStream) && (packet->stream_index != demuxer->audioStream))) {
				logInfo(LOG_DEMUXER_DEBUG, "This is a packet we do not want to process. Going to ignore it. packet.stream_index=%d.\n", packet->stream_index);
				if (packet->data != NULL) {
					av_free_packet(packet);
				}
				else {
					logInfo(LOG_DEMUXER, "packet->data == NULL.\n");
				}
				free(packet);
				packetProcessed = 1;
				continue;
			}

			if (packet->data != NULL) {
				if(packet->dts == 0)
					packet->dts = AV_NOPTS_VALUE;
				if(packet->pts == 0)
					packet->pts = AV_NOPTS_VALUE;
			}

			if (packet->stream_index == demuxer->audioStream) {
				audioPTS = packet->pts;
				audioDTS = packet->dts;
			}

			if (packet->stream_index == demuxer->videoStream) {
				videoPTS = packet->pts;
				videoDTS = packet->dts;
			}

			if (packet->stream_index == demuxer->dvbsubStream) {
				logInfo(LOG_DEMUXER, "Packet is a dvb subtitle packet.\n");
				dvbsubPTS = packet->pts;
				dvbsubDTS = packet->dts;
			}

			if ((validPTSValues == 0) && (((audioPTS != -1) || (demuxer->audioStream == -1)) && ((videoPTS != -1) || (demuxer->videoStream == -1)))) {
				validPTSValues = 1;
				logInfo(LOG_DEMUXER, "We found valid start PTS values.\n");

				if (audioPTS != -1) {
					demuxer->startPTS = audioPTS;
				}

				if ((videoPTS != -1) && ((videoPTS < demuxer->startPTS) || (demuxer->startPTS == -1))) {
					demuxer->startPTS = videoPTS;
				}

				if (audioDTS != -1) {
					demuxer->startDTS = audioDTS;
				}

				if ((videoDTS != -1) && ((videoDTS < demuxer->startDTS) || (demuxer->startDTS == -1))) {
					demuxer->startDTS = videoDTS;
				}
			}

		}

		if ((validPTSValues) && (demuxerIsStopped(demuxer) == 0) && (packet->data != NULL)) {


			// We need to drop the first few audio or video packets because the time difference is to big.

			if ((demuxer->videoStream != -1) && (packet->stream_index == demuxer->videoStream)) {
				tmpPTS = ffmpegConvertTimestamp(demuxer, videoPTS, &demuxer->formatContext->streams[packet->stream_index]->time_base);
				logInfo(LOG_DEMUXER_DEBUG, "VideoDTS=%" PRId64 ", VideoPTS=%" PRId64 ", demuxer->startPTS=%" PRId64 ", timestamp=%.15f\n", videoDTS, videoPTS, demuxer->startPTS, tmpPTS);
				if (showVideoPacket(demuxer, packet, tmpPTS) == 1) {
					free(packet);
					packetProcessed = 1;
				}
				else {
					logInfo(LOG_DEMUXER_DEBUG, "Retrying video packet.\n");
				}
			}
			else {
				if ((demuxer->audioStream != -1) && (packet->stream_index == demuxer->audioStream)) {
					tmpPTS = ffmpegConvertTimestamp(demuxer, audioPTS, &demuxer->formatContext->streams[packet->stream_index]->time_base);
					logInfo(LOG_DEMUXER_DEBUG, "AudioDTS=%" PRId64 ", AudioPTS=%" PRId64 ", demuxer->startPTS=%" PRId64 ", timestamp=%.15f\n", audioDTS, audioPTS, demuxer->startPTS, tmpPTS);
					audioAddPacketToList(demuxer, packet);
					packetProcessed = 1;
/*					if (decodeAudio(demuxer, packet, tmpPTS) == 1) {
						free(packet);
						packetProcessed = 1;
					}
					else {
						logInfo(LOG_DEMUXER_DEBUG, "Retrying audio packet.\n");
					}
*/				}
				else {
					if ((demuxer->dvbsubStream != -1) && (packet->stream_index == demuxer->dvbsubStream)) {
						tmpPTS = ffmpegConvertTimestamp(demuxer, dvbsubPTS, &demuxer->formatContext->streams[packet->stream_index]->time_base);
						logInfo(LOG_DEMUXER, "dvbsubDTS=%" PRId64 ", dvbsubPTS=%" PRId64 ", demuxer->startPTS=%" PRId64 ", timestamp=%.15f\n", dvbsubDTS, dvbsubPTS, demuxer->startPTS, tmpPTS);
						subtitleAddAVPacketToList(demuxer, packet);
						packetProcessed = 1;
/*						if (decodeDVBSub(demuxer, &packet, tmpPTS) == 1) {
							packet.data = NULL;
						}
						else {
							logInfo(LOG_DEMUXER_DEBUG, "Retrying dvb subtitle packet.\n");
						}
*/					}
					else {
						// Drop other packets.
						av_free_packet(packet);
						free(packet);
						packetProcessed = 1;
					}
				}
			}
		}
		else {
			// Drop other packets.
			av_free_packet(packet);
			free(packet);
			packetProcessed = 1;
		}

/*		interval.tv_sec = 0;
		interval.tv_nsec = 10;

		nanosleep(&interval, &remainingInterval);*/
	}

//	if(packet.data!=NULL) av_free_packet(&packet);

theEnd:
	pthread_mutex_lock(&demuxer->threadLock);

	demuxer->doStop = 1;

	pthread_mutex_unlock(&demuxer->threadLock);

	logInfo(LOG_DEMUXER, "Flushing video decoder input port.\n");
	omxFlushPort(demuxer->videoDecoder, demuxer->videoDecoder->inputPort);

	logInfo(LOG_DEMUXER, "Flushing audio render input port.\n");
	omxFlushPort(demuxer->audioRender, demuxer->audioRender->inputPort);

	int clockIdle = 0;
	logInfo(LOG_DEMUXER, "Setting state of clock component to idle.\n");
	omxErr = omxSetStateForComponent(demuxer->clock->clockComponent, OMX_StateIdle, 5000);
	if (omxErr != OMX_ErrorNone) {
		if (omxErr == OMX_ErrorTimeout) {
			logInfo(LOG_DEMUXER, "Error changing state to OMX_StateIdle for %s. Timedout waiting for state change.\n", demuxer->clock->clockComponent->componentName);
		}
		else {
			logInfo(LOG_DEMUXER, "Error changing state to OMX_StateLoaded for %s. (omxErr = 0x%08x)\n", demuxer->clock->clockComponent->componentName, omxErr);
		}
	}
	else {
		clockIdle = 1;
	}
	
	int videoDecoderIdle = 0;
	logInfo(LOG_DEMUXER, "Setting state of videoDecoder component to idle.\n");
	omxErr = omxSetStateForComponent(demuxer->videoDecoder, OMX_StateIdle, 5000);
	if (omxErr != OMX_ErrorNone) {
		if (omxErr == OMX_ErrorTimeout) {
			logInfo(LOG_DEMUXER, "Error changing state to OMX_StateIdle for %s. Timedout waiting for state change.\n", demuxer->videoDecoder->componentName);
		}
		else {
			logInfo(LOG_DEMUXER, "Error changing state to OMX_StateLoaded for %s. (omxErr = 0x%08x)\n", demuxer->videoDecoder->componentName, omxErr);
		}
	}
	else {
		videoDecoderIdle = 1;
	}
	int videoImageFXIdle = 0;
	logInfo(LOG_DEMUXER, "Setting state of videoImageFX component to idle.\n");
	omxErr = omxSetStateForComponent(demuxer->videoImageFX, OMX_StateIdle, 5000);
	if (omxErr != OMX_ErrorNone) {
		if (omxErr == OMX_ErrorTimeout) {
			logInfo(LOG_DEMUXER, "Error changing state to OMX_StateIdle for %s. Timedout waiting for state change.\n", demuxer->videoImageFX->componentName);
		}
		else {
			logInfo(LOG_DEMUXER, "Error changing state to OMX_StateLoaded for %s. (omxErr = 0x%08x)\n", demuxer->videoImageFX->componentName, omxErr);
		}
	}
	else {
		videoImageFXIdle = 1;
	}
	int videoSchedulerIdle = 0;
	logInfo(LOG_DEMUXER, "Setting state of videoScheduler component to idle.\n");
	omxErr = omxSetStateForComponent(demuxer->videoScheduler, OMX_StateIdle, 5000);
	if (omxErr != OMX_ErrorNone) {
		if (omxErr == OMX_ErrorTimeout) {
			logInfo(LOG_DEMUXER, "Error changing state to OMX_StateIdle for %s. Timedout waiting for state change.\n", demuxer->videoScheduler->componentName);
		}
		else {
			logInfo(LOG_DEMUXER, "Error changing state to OMX_StateLoaded for %s. (omxErr = 0x%08x)\n", demuxer->videoScheduler->componentName, omxErr);
		}
	}
	else {
		videoSchedulerIdle = 1;
	}
	int videoRenderIdle = 0;
	logInfo(LOG_DEMUXER, "Setting state of videoRender component to idle.\n");
	omxErr = omxSetStateForComponent(demuxer->videoRender, OMX_StateIdle, 5000);
	if (omxErr != OMX_ErrorNone) {
		if (omxErr == OMX_ErrorTimeout) {
			logInfo(LOG_DEMUXER, "Error changing state to OMX_StateIdle for %s. Timedout waiting for state change.\n", demuxer->videoRender->componentName);
		}
		else {
			logInfo(LOG_DEMUXER, "Error changing state to OMX_StateLoaded for %s. (omxErr = 0x%08x)\n", demuxer->videoRender->componentName, omxErr);
		}
	}
	else {
		videoRenderIdle = 1;
	}

	int audioDecoderIdle = 0;
	logInfo(LOG_DEMUXER, "Setting state of audioDecoder component to idle.\n");
	omxErr = omxSetStateForComponent(demuxer->audioDecoder, OMX_StateIdle, 5000);
	if (omxErr != OMX_ErrorNone) {
		if (omxErr == OMX_ErrorTimeout) {
			logInfo(LOG_DEMUXER, "Error changing state to OMX_StateIdle for %s. Timedout waiting for state change.\n", demuxer->audioDecoder->componentName);
		}
		else {
			logInfo(LOG_DEMUXER, "Error changing state to OMX_StateLoaded for %s. (omxErr = 0x%08x)\n", demuxer->audioDecoder->componentName, omxErr);
		}
	}
	else {
		audioDecoderIdle = 1;
	}
	int audioMixerIdle = 0;
	logInfo(LOG_DEMUXER, "Setting state of audioMixer component to idle.\n");
	omxErr = omxSetStateForComponent(demuxer->audioMixer, OMX_StateIdle, 5000);
	if (omxErr != OMX_ErrorNone) {
		if (omxErr == OMX_ErrorTimeout) {
			logInfo(LOG_DEMUXER, "Error changing state to OMX_StateIdle for %s. Timedout waiting for state change.\n", demuxer->audioMixer->componentName);
		}
		else {
			logInfo(LOG_DEMUXER, "Error changing state to OMX_StateLoaded for %s. (omxErr = 0x%08x)\n", demuxer->audioMixer->componentName, omxErr);
		}
	}
	else {
		audioMixerIdle = 1;
	}
	int audioRenderIdle = 0;
	logInfo(LOG_DEMUXER, "Setting state of audioRender component to idle.\n");
	omxErr = omxSetStateForComponent(demuxer->audioRender, OMX_StateIdle, 5000);
	if (omxErr != OMX_ErrorNone) {
		if (omxErr == OMX_ErrorTimeout) {
			logInfo(LOG_DEMUXER, "Error changing state to OMX_StateIdle for %s. Timedout waiting for state change.\n", demuxer->audioRender->componentName);
		}
		else {
			logInfo(LOG_DEMUXER, "Error changing state to OMX_StateLoaded for %s. (omxErr = 0x%08x)\n", demuxer->audioRender->componentName, omxErr);
		}
	}
	else {
		audioRenderIdle = 1;
	}

	if (clockIdle == 0) {
		logInfo(LOG_DEMUXER, "waitingForCommandComplete of clock component.\n");
		omxErr = omxWaitForCommandComplete(demuxer->clock->clockComponent, OMX_CommandStateSet, OMX_StateIdle, 5000000);
		if (omxErr == OMX_ErrorTimeout) {
			logInfo(LOG_OMX, "Waiting for command complete timedout for component '%s' OMX_CommandStateSet to OMX_StateIdle.\n", demuxer->clock->clockComponent->componentName);
		}
		else {
			if (omxErr != OMX_ErrorNone) {
				logInfo(LOG_OMX, "Error Waiting for command complete for component '%s' OMX_CommandStateSet to OMX_StateIdle.\n", demuxer->clock->clockComponent->componentName);
			}
			else {
				logInfo(LOG_OMX, "Received valid command complete OMX_CommandStateSet to OMX_StateIdle for component '%s'.\n", demuxer->clock->clockComponent->componentName);
			}
		}
	}
	if (videoDecoderIdle == 0) {
		logInfo(LOG_DEMUXER, "waitingForCommandComplete of videoDecoder component.\n");
		omxErr = omxWaitForCommandComplete(demuxer->videoDecoder, OMX_CommandStateSet, OMX_StateIdle, 5000000);
		if (omxErr == OMX_ErrorTimeout) {
			logInfo(LOG_OMX, "Waiting for command complete timedout for component '%s' OMX_CommandStateSet to OMX_StateIdle.\n", demuxer->videoDecoder->componentName);
		}
		else {
			if (omxErr != OMX_ErrorNone) {
				logInfo(LOG_OMX, "Error Waiting for command complete for component '%s' OMX_CommandStateSet to OMX_StateIdle.\n", demuxer->videoDecoder->componentName);
			}
			else {
				logInfo(LOG_OMX, "Received valid command complete OMX_CommandStateSet to OMX_StateIdle for component '%s'.\n", demuxer->videoDecoder->componentName);
			}
		}
	}
	if (videoImageFXIdle == 0) {
		logInfo(LOG_DEMUXER, "waitingForCommandComplete of videoImageFX component.\n");
		omxErr = omxWaitForCommandComplete(demuxer->videoImageFX, OMX_CommandStateSet, OMX_StateIdle, 5000000);
		if (omxErr == OMX_ErrorTimeout) {
			logInfo(LOG_OMX, "Waiting for command complete timedout for component '%s' OMX_CommandStateSet to OMX_StateIdle.\n", demuxer->videoImageFX->componentName);
		}
		else {
			if (omxErr != OMX_ErrorNone) {
				logInfo(LOG_OMX, "Error Waiting for command complete for component '%s' OMX_CommandStateSet to OMX_StateIdle.\n", demuxer->videoImageFX->componentName);
			}
			else {
				logInfo(LOG_OMX, "Received valid command complete OMX_CommandStateSet to OMX_StateIdle for component '%s'.\n", demuxer->videoImageFX->componentName);
			}
		}
	}
	if (videoSchedulerIdle == 0) {
		logInfo(LOG_DEMUXER, "waitingForCommandComplete of videoScheduler component.\n");
		omxErr = omxWaitForCommandComplete(demuxer->videoScheduler, OMX_CommandStateSet, OMX_StateIdle, 5000000);
		if (omxErr == OMX_ErrorTimeout) {
			logInfo(LOG_OMX, "Waiting for command complete timedout for component '%s' OMX_CommandStateSet to OMX_StateIdle.\n", demuxer->videoScheduler->componentName);
		}
		else {
			if (omxErr != OMX_ErrorNone) {
				logInfo(LOG_OMX, "Error Waiting for command complete for component '%s' OMX_CommandStateSet to OMX_StateIdle.\n", demuxer->videoScheduler->componentName);
			}
			else {
				logInfo(LOG_OMX, "Received valid command complete OMX_CommandStateSet to OMX_StateIdle for component '%s'.\n", demuxer->videoScheduler->componentName);
			}
		}
	}
	if (videoRenderIdle == 0) {
		logInfo(LOG_DEMUXER, "waitingForCommandComplete of videoRender component.\n");
		omxErr = omxWaitForCommandComplete(demuxer->videoRender, OMX_CommandStateSet, OMX_StateIdle, 5000000);
		if (omxErr == OMX_ErrorTimeout) {
			logInfo(LOG_OMX, "Waiting for command complete timedout for component '%s' OMX_CommandStateSet to OMX_StateIdle.\n", demuxer->videoRender->componentName);
		}
		else {
			if (omxErr != OMX_ErrorNone) {
				logInfo(LOG_OMX, "Error Waiting for command complete for component '%s' OMX_CommandStateSet to OMX_StateIdle.\n", demuxer->videoRender->componentName);
			}
			else {
				logInfo(LOG_OMX, "Received valid command complete OMX_CommandStateSet to OMX_StateIdle for component '%s'.\n", demuxer->videoRender->componentName);
			}
		}
	}

	if (audioDecoderIdle == 0) {
		logInfo(LOG_DEMUXER, "waitingForCommandComplete of audioDecoder component.\n");
		omxErr = omxWaitForCommandComplete(demuxer->audioDecoder, OMX_CommandStateSet, OMX_StateIdle, 5000000);
		if (omxErr == OMX_ErrorTimeout) {
			logInfo(LOG_OMX, "Waiting for command complete timedout for component '%s' OMX_CommandStateSet to OMX_StateIdle.\n", demuxer->audioDecoder->componentName);
		}
		else {
			if (omxErr != OMX_ErrorNone) {
				logInfo(LOG_OMX, "Error Waiting for command complete for component '%s' OMX_CommandStateSet to OMX_StateIdle.\n", demuxer->audioDecoder->componentName);
			}
			else {
				logInfo(LOG_OMX, "Received valid command complete OMX_CommandStateSet to OMX_StateIdle for component '%s'.\n", demuxer->audioDecoder->componentName);
			}
		}
	}
	if (audioMixerIdle == 0) {
		logInfo(LOG_DEMUXER, "waitingForCommandComplete of audioMixer component.\n");
		omxErr = omxWaitForCommandComplete(demuxer->audioMixer, OMX_CommandStateSet, OMX_StateIdle, 5000000);
		if (omxErr == OMX_ErrorTimeout) {
			logInfo(LOG_OMX, "Waiting for command complete timedout for component '%s' OMX_CommandStateSet to OMX_StateIdle.\n", demuxer->audioMixer->componentName);
		}
		else {
			if (omxErr != OMX_ErrorNone) {
				logInfo(LOG_OMX, "Error Waiting for command complete for component '%s' OMX_CommandStateSet to OMX_StateIdle.\n", demuxer->audioMixer->componentName);
			}
			else {
				logInfo(LOG_OMX, "Received valid command complete OMX_CommandStateSet to OMX_StateIdle for component '%s'.\n", demuxer->audioMixer->componentName);
			}
		}
	}
	if (audioRenderIdle == 0) {
		logInfo(LOG_DEMUXER, "waitingForCommandComplete of audioRender component.\n");
		omxErr = omxWaitForCommandComplete(demuxer->audioRender, OMX_CommandStateSet, OMX_StateIdle, 5000000);
		if (omxErr == OMX_ErrorTimeout) {
			logInfo(LOG_OMX, "Waiting for command complete timedout for component '%s' OMX_CommandStateSet to OMX_StateIdle.\n", demuxer->audioRender->componentName);
		}
		else {
			if (omxErr != OMX_ErrorNone) {
				logInfo(LOG_OMX, "Error Waiting for command complete for component '%s' OMX_CommandStateSet to OMX_StateIdle.\n", demuxer->audioRender->componentName);
			}
			else {
				logInfo(LOG_OMX, "Received valid command complete OMX_CommandStateSet to OMX_StateIdle for component '%s'.\n", demuxer->audioRender->componentName);
			}
		}
	}

	logInfo(LOG_DEMUXER, "Setting state of clock components to loaded.\n");
	omxChangeStateToLoaded(demuxer->clock->clockComponent, 1);
	logInfo(LOG_DEMUXER, "Setting state of videoDecoder components to loaded.\n");
	omxChangeStateToLoaded(demuxer->videoDecoder, 1);
	logInfo(LOG_DEMUXER, "Setting state of videoImageFX components to loaded.\n");
	omxChangeStateToLoaded(demuxer->videoImageFX, 1);
	logInfo(LOG_DEMUXER, "Setting state of videoScheduler components to loaded.\n");
	omxChangeStateToLoaded(demuxer->videoScheduler, 1);
	logInfo(LOG_DEMUXER, "Setting state of videoRender components to loaded.\n");
	omxChangeStateToLoaded(demuxer->videoRender, 1);

	logInfo(LOG_DEMUXER, "Setting state of audioMixer components to loaded.\n");
	omxChangeStateToLoaded(demuxer->audioMixer, 1);
	logInfo(LOG_DEMUXER, "Setting state of audioDecoder components to loaded.\n");
	omxChangeStateToLoaded(demuxer->audioDecoder, 1);
	logInfo(LOG_DEMUXER, "Setting state of audioRender components to loaded.\n");
	omxChangeStateToLoaded(demuxer->audioRender, 1);

	logInfo(LOG_DEMUXER, "Destroying clockToVideoSchedulerTunnel.\n");
	omxDestroyTunnel(demuxer->clockToVideoSchedulerTunnel);
	logInfo(LOG_DEMUXER, "Destroying videoDecoderToSchedulerTunnel.\n");
	omxDestroyTunnel(demuxer->videoDecoderToSchedulerTunnel);
	logInfo(LOG_DEMUXER, "Destroying videoDecoderToImageFXTunnel.\n");
	omxDestroyTunnel(demuxer->videoDecoderToImageFXTunnel);
	logInfo(LOG_DEMUXER, "Destroying videoImageFXToSchedulerTunnel.\n");
	omxDestroyTunnel(demuxer->videoImageFXToSchedulerTunnel);
	logInfo(LOG_DEMUXER, "Destroying videoSchedulerToRenderTunnel.\n");
	omxDestroyTunnel(demuxer->videoSchedulerToRenderTunnel);

	logInfo(LOG_DEMUXER, "Destroying audioDecoderToMixerTunnel.\n");
	omxDestroyTunnel(demuxer->audioDecoderToMixerTunnel);
	logInfo(LOG_DEMUXER, "Destroying audioMixerToRenderTunnel.\n");
	omxDestroyTunnel(demuxer->audioMixerToRenderTunnel);
	logInfo(LOG_DEMUXER, "Destroying clockToAudioRenderTunnel.\n");
	omxDestroyTunnel(demuxer->clockToAudioRenderTunnel);
	logInfo(LOG_DEMUXER, "Destroying audioDecoderToRenderTunnel.\n");
	omxDestroyTunnel(demuxer->audioDecoderToRenderTunnel);

	logInfo(LOG_DEMUXER, "Destroying video components.\n");
	omxDestroyComponent(demuxer->videoDecoder);
	omxDestroyComponent(demuxer->videoImageFX);
	omxDestroyComponent(demuxer->videoScheduler);
	omxDestroyComponent(demuxer->videoRender);

	logInfo(LOG_DEMUXER, "Destroying audio components.\n");
	omxDestroyComponent(demuxer->audioDecoder);
	omxDestroyComponent(demuxer->audioMixer);
	omxDestroyComponent(demuxer->audioRender);

	logInfo(LOG_DEMUXER, "Destroying clock components.\n");
	omxDestroyClock(demuxer->clock);
	demuxer->clock = NULL;
	
	logInfo(LOG_DEMUXER, "Done stopping demuxer loop.\n");
	return NULL;
}

struct DEMUXER_T *demuxerStart(struct MYTH_CONNECTION_T *mythConnection, int showVideo, int playAudio, int showDVBSub, int audioPassthrough)
{
	demuxerInitialize();

	demuxerStartInterrupted = 0;

	logInfo( LOG_DEMUXER, "Version of avFormat: %s.\n", LIBAVFORMAT_IDENT);

	struct DEMUXER_T *demuxer = malloc(sizeof(struct DEMUXER_T));
	demuxer->mythConnection = mythConnection;
	demuxer->doStop = 0;
	demuxer->isOpen = 0;
	demuxer->showVideo = showVideo;
	demuxer->playAudio = playAudio;
	demuxer->audioPassthrough = audioPassthrough;
	demuxer->swDecodeAudio = 1; // For now hardcoded.
	demuxer->nextFile = NULL;
	demuxer->lastFrameSize = 0;
	demuxer->deInterlace = 1;
	demuxer->videoStreamWasInterlaced = 0;
	demuxer->firstFrame = 0;
	demuxer->newProgram = 0;
	
	demuxer->videoDecoder = NULL;
	demuxer->videoImageFX = NULL;
	demuxer->videoRender = NULL;
	demuxer->videoScheduler = NULL;
	demuxer->videoDecoderToSchedulerTunnel = NULL;
	demuxer->videoSchedulerToRenderTunnel = NULL;
	demuxer->videoDecoderToImageFXTunnel = NULL;
	demuxer->videoImageFXToSchedulerTunnel = NULL;
	demuxer->clockToVideoSchedulerTunnel = NULL;

	demuxer->audioDecoder = NULL;
	demuxer->audioRender = NULL;
	demuxer->audioMixer = NULL;
	demuxer->audioDecoderToMixerTunnel = NULL;
	demuxer->audioMixerToRenderTunnel = NULL;
	demuxer->clockToAudioRenderTunnel = NULL;
	demuxer->audioDecoderToRenderTunnel = NULL;

	demuxer->videoCodecContext = NULL;
	demuxer->audioCodecContext = NULL;
	demuxer->dvbsubCodecContext = NULL;

	demuxer->formatContext = NULL;

	demuxer->ioContext = NULL;
	demuxer->avioBuffer = NULL;

	demuxer->formatContextIsOpen = 0;
	demuxer->threadStarted = 0;
	demuxer->videoThreadStarted = 0;
	demuxer->audioThreadStarted = 0;
	demuxer->subtitleThreadStarted = 0;
	demuxer->doStopVideoThread = 0;
	demuxer->doStopAudioThread = 0;
	demuxer->doStopSubtitleThread = 0;

	demuxer->mythThreadStarted = 0;
	demuxer->doStopMythThread = 0;

	demuxer->subtitlePackets = NULL;
	demuxer->subtitlePacketsEnd = NULL;

	demuxer->subtitleAVPackets = NULL;
	demuxer->subtitleAVPacketsEnd = NULL;

	demuxer->audioPackets = NULL;
	demuxer->audioPacketsEnd = NULL;

	demuxer->demuxerThread = -1;

	demuxer->streamIsInterlaced = -1;

/*	if (pthread_mutex_init(&demuxer->mythThreadLock, NULL) != 0)
	{
		logInfo( LOG_DEMUXER_DEBUG,"  ----------> could not init mutex for myth thread.\n");
		demuxer_error = DEMUXER_ERROR_START_THREAD;
		demuxerStop(demuxer);
		return NULL;
	}
	int status = pthread_create( &demuxer->mythThread, NULL, (void * (*)(void *))&mythLoop, demuxer);
	if (status != 0) {
		logInfo( LOG_DEMUXER_DEBUG,"  ----------> could not start myth thread.\n");
		demuxer_error = DEMUXER_ERROR_START_THREAD;
		demuxerStop(demuxer);
		return NULL;
	}
	logInfo( LOG_DEMUXER_DEBUG,"  ++++++++++> started myth thread ok.\n");
	demuxer->mythThreadStarted = 1;

	while (getConnectionDataLen(demuxer->mythConnection->transferConnection->connection) < MIN_MYTH_QUEUE_LENGTH) {
		sleep(1);
	}

	logInfo( LOG_DEMUXER,"  Prefilled buffer from Myth.\n");
*/
	demuxer->avioBuffer = (unsigned char*)av_malloc(FFMPEG_FILE_BUFFER_SIZE);

	// Init io module for input
	demuxer->ioContext = avio_alloc_context(demuxer->avioBuffer, FFMPEG_FILE_BUFFER_SIZE, 0, (void *)demuxer, (int (*)(void *, uint8_t *, int))&demuxerReadPacket, 0, (int64_t (*)(void *, int64_t,  int))&demuxerSeek);

	if (demuxer->ioContext == NULL) {
		logInfo( LOG_DEMUXER," - init_put_byte() failed!\n");
		demuxer_error = DEMUXER_ERROR_CREATE_IOCONTEXT;
		demuxerStop(demuxer);
		return NULL;
	}

	demuxer->ioContext->max_packet_size = 6144;
	if(demuxer->ioContext->max_packet_size)
		demuxer->ioContext->max_packet_size *= FFMPEG_FILE_BUFFER_SIZE / demuxer->ioContext->max_packet_size;

	demuxer->ioContext->must_flush = 1;

	AVInputFormat *format = NULL;
	if (av_probe_input_buffer(demuxer->ioContext, &format, "", NULL, 0, 0) < -1) {
		logInfo( LOG_DEMUXER," - av_probe_input_buffer() failed!\n");
		demuxer_error = DEMUXER_ERROR_PROBE_INPUT_BUFFER;
		demuxerStop(demuxer);
		return NULL;
	}

	logInfo( LOG_DEMUXER,"Detected stream Format = %s.\n", format->name); 

	AVFormatContext *formatContext = avformat_alloc_context();
	if (formatContext == NULL) {
		logInfo( LOG_DEMUXER," - avformat_alloc_context() failed!\n");
		demuxer_error = DEMUXER_ERROR_CREATE_FORMAT_CONTEXT;
		demuxerStop(demuxer);
		return NULL;
	}

	formatContext->pb = demuxer->ioContext;

	formatContext->interrupt_callback.callback = &demuxerInterruptCallback;
	formatContext->interrupt_callback.opaque = demuxer;

	if (avformat_open_input(&formatContext, "", format, NULL)  <  0) {
		logInfo( LOG_DEMUXER," - av_open_input_file() failed!\n");
		demuxer_error = DEMUXER_ERROR_OPEN_INPUT;
		demuxerStop(demuxer);
		return NULL;
	}

	demuxer->formatContextIsOpen = 1;

	logInfo( LOG_DEMUXER_DEBUG," - av_open_input_file() success!\n");

	// if format can be nonblocking, let's use that
//	formatContext->flags |= AVFMT_FLAG_NONBLOCK;

	// Make sure that when we start the demuxer thread it will start with an empty buffer and not 
	// first with the part which was used to find the stream info. Otherwise we will get the
	// part used for finding the stream info double.
	formatContext->flags |= AVFMT_FLAG_NOBUFFER;
	
	logInfo(LOG_DEMUXER, "formatContext->probesize=%d.\n",formatContext->probesize);
 
	// Retrieve stream information
	if (avformat_find_stream_info(formatContext, NULL) < 0) {
		logInfo( LOG_DEMUXER," - av_find_stream_info() failed!\n");
		demuxer_error = DEMUXER_ERROR_FIND_STREAM_INFO;
		demuxerStop(demuxer);
		return NULL;
	}

	if (demuxerStartInterrupted == 1) {
		logInfo( LOG_DEMUXER," - demuxerStartInterrupted!\n");
		demuxerStop(demuxer);
		return NULL;
	}


	logInfo( LOG_DEMUXER_DEBUG," - av_find_stream_info() success!\n");

	// Dump information about stream onto standard error
	av_dump_format(formatContext, 0, "", 0);

	unsigned int i;

	// Find the first video stream
	int videoStream=-1;
	int audioStream=-1;
	int dvbsubStream = -1;
	int firstVideoStream=-1;
	int firstAudioStream=-1;
	int firstDVBSubStream = -1;

	AVDictionaryEntry *streamLang;
	int languageMatch = 0;

	for(i=0; i<formatContext->nb_streams; i++) {

		logInfo( LOG_DEMUXER_DEBUG, "Researching info in stream %d.\n", i);

		languageMatch = 0;
		if (formatContext->streams != NULL) {
			if (formatContext->streams[i]->metadata != NULL) {
				streamLang = av_dict_get(formatContext->streams[i]->metadata, "language", NULL, 0);
				if (streamLang) {
					logInfo( LOG_DEMUXER,"Language of stream is '%s'.\n", streamLang->value);
					if (language != NULL) {
						if (strcmp(language, streamLang->value) == 0) {
							languageMatch = 1;
						}
					}
				}
			}
			else {
				logInfo( LOG_DEMUXER_DEBUG, "formatContext->streams[%d]->metadata is NULL.\n",i);
			}
		}
		else {
			logInfo( LOG_DEMUXER_DEBUG, "formatContext->streams is NULL.\n");
		}

		if((formatContext->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) && (firstVideoStream == -1)) {
			firstVideoStream = i;
		}

		if((formatContext->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) && (videoStream == -1) && (languageMatch == 1)) {
			videoStream=i;
			logInfo( LOG_DEMUXER," Found a videostream on index=%d with stream.id=%d!\n", i, formatContext->streams[i]->id);
		}

		if((formatContext->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO) && (firstAudioStream == -1)) {
			firstAudioStream = i;
		}

		if((formatContext->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO) && (audioStream == -1) && (languageMatch == 1)) {
			audioStream=i;
			logInfo( LOG_DEMUXER," Found a audioStream on index=%d with stream.id=%d!\n", i, formatContext->streams[i]->id);
		}

		if((formatContext->streams[i]->codec->codec_type==AVMEDIA_TYPE_SUBTITLE) && (formatContext->streams[i]->codec->codec_id==AV_CODEC_ID_DVB_SUBTITLE) && (firstDVBSubStream == -1)) {
			firstDVBSubStream = i;
		}

		if((formatContext->streams[i]->codec->codec_type==AVMEDIA_TYPE_SUBTITLE) && (formatContext->streams[i]->codec->codec_id==AV_CODEC_ID_DVB_SUBTITLE) && (dvbsubStream == -1) && (languageMatch == 1)) {
			dvbsubStream=i;
			logInfo( LOG_DEMUXER," Found a dvb subtitle stream on index=%d with stream.id=%d!\n", i, formatContext->streams[i]->id);
		}

	}

	logInfo( LOG_DEMUXER_DEBUG, "Done: Retreived stream info.\n");

	if ((videoStream == -1) && (firstVideoStream != -1)) {
		videoStream = firstVideoStream;
		logInfo( LOG_DEMUXER," Found a videostream on index=%d with stream.id=%d!\n", videoStream, formatContext->streams[videoStream]->id);
	}

	if ((audioStream == -1) && (firstAudioStream != -1)) {
		audioStream = firstAudioStream;
		logInfo( LOG_DEMUXER," Found a audioStream on index=%d with stream.id=%d!\n", audioStream, formatContext->streams[audioStream]->id);
	}

	if ((dvbsubStream == -1) && (firstDVBSubStream != -1)) {
		dvbsubStream = firstDVBSubStream;
		logInfo( LOG_DEMUXER," Found a dvb subtitle stream on index=%d with stream.id=%d!\n", dvbsubStream, formatContext->streams[dvbsubStream]->id);
	}

	if(videoStream!=-1) {

		// Determine if we need to deinterlace
		if (formatContext->streams[videoStream]->r_frame_rate.den && formatContext->streams[videoStream]->r_frame_rate.num) {
			// We have enough information to determine if we need to deintelace or not.
			double realFrameRate = av_q2d(formatContext->streams[videoStream]->r_frame_rate);
			double avgFrameRate = av_q2d(formatContext->streams[videoStream]->avg_frame_rate);
			if (realFrameRate != avgFrameRate) {
				logInfo( LOG_DEMUXER,"We need to deinterlace this stream.\n");
				demuxer->deInterlace = 1;
			}
			else {
				logInfo( LOG_DEMUXER,"This is a progressive stream. No deinterlace required.\n");
			}
		}

		// Get a pointer to the codec context for the video stream
		demuxer->videoCodecContext = avcodec_alloc_context3(formatContext->streams[videoStream]->codec->codec);

		if (demuxer->videoCodecContext == NULL) {
			logInfo( LOG_DEMUXER," fVideoCodecContext == NULL!\n");
			demuxer_error = DEMUXER_ERROR_CREATE_VIDEO_CONTEXT;
			demuxerStop(demuxer);
			return NULL;
		}

		logInfo( LOG_DEMUXER,"We are going to use videostream %d with codecname '%s', codec_id=%d, width=%d, height=%d, profile=%d, timebase=%.15f.\n", 
				videoStream, formatContext->streams[videoStream]->codec->codec_name, formatContext->streams[videoStream]->codec->codec_id,
				formatContext->streams[videoStream]->codec->width, formatContext->streams[videoStream]->codec->height, formatContext->streams[videoStream]->codec->profile,
				av_q2d(formatContext->streams[videoStream]->time_base));

		AVCodec *fVideoCodec;

		// Find the decoder for the video stream
		fVideoCodec=avcodec_find_decoder(formatContext->streams[videoStream]->codec->codec_id);
		if(fVideoCodec == NULL) {
			logInfo( LOG_DEMUXER," Video Codec not found!\n");
			demuxer_error = DEMUXER_ERROR_FIND_VIDEO_DECODER;
			demuxerStop(demuxer);
			return NULL;
		}

		demuxer->videoCodec = fVideoCodec;

		logInfo( LOG_DEMUXER,"Codec of videostream is '%s'.\n", fVideoCodec->name);

		AVDictionaryEntry *videoLang = av_dict_get(formatContext->streams[videoStream]->metadata, "language", NULL, 0);
		if (videoLang) {
			logInfo( LOG_DEMUXER,"Language of videostream is '%s'.\n", videoLang->value);
		}

		// Inform the codec that we can handle truncated bitstreams -- i.e.,
		// bitstreams where frame boundaries can fall in the middle of packets
		if(fVideoCodec->capabilities & CODEC_CAP_TRUNCATED)
		    demuxer->videoCodecContext->flags|=CODEC_FLAG_TRUNCATED;

		// Open codec
		AVDictionary *videoDict = NULL;
		if(avcodec_open2(demuxer->videoCodecContext, fVideoCodec, &videoDict)<0) {
			logInfo( LOG_DEMUXER," Could not video open codec!\n");
			demuxer_error = DEMUXER_ERROR_OPEN_VIDEO_CODEC;
			demuxerStop(demuxer);
			return NULL;
		}
	}

	if(audioStream!=-1) {

		// Get a pointer to the codec context for the audio stream
		demuxer->audioCodecContext = avcodec_alloc_context3(formatContext->streams[audioStream]->codec->codec);

		if (demuxer->audioCodecContext == NULL) {
			logInfo( LOG_DEMUXER," demuxer->audioCodecContext == NULL!\n");
			demuxer_error = DEMUXER_ERROR_CREATE_AUDIO_CONTEXT;
			demuxerStop(demuxer);
			return NULL;
		}

		logInfo( LOG_DEMUXER_DEBUG,"We are going to use audiostream %d with codecname '%s', codec_id=%d, width=%d, height=%d.\n", audioStream, formatContext->streams[audioStream]->codec->codec_name, formatContext->streams[audioStream]->codec->codec_id,demuxer->audioCodecContext->width, demuxer->audioCodecContext->height);

		AVCodec *fAudioCodec;

		// Find the decoder for the video stream
		fAudioCodec=avcodec_find_decoder(formatContext->streams[audioStream]->codec->codec_id);
		if(fAudioCodec == NULL) {
			logInfo( LOG_DEMUXER," Audio Codec not found!\n");
			demuxer_error = DEMUXER_ERROR_FIND_AUDIO_DECODER;
			demuxerStop(demuxer);
			return NULL;
		}

		demuxer->audioCodec = fAudioCodec;

		logInfo( LOG_DEMUXER,"Codec of audiostream is '%s'.\n", fAudioCodec->name);

		AVDictionaryEntry *audioLang = av_dict_get(formatContext->streams[audioStream]->metadata, "language", NULL, 0);
		if (audioLang) {
			logInfo( LOG_DEMUXER,"Language of audiostream is '%s'.\n", audioLang->value);
		}

		// Inform the codec that we can handle truncated bitstreams -- i.e.,
		// bitstreams where frame boundaries can fall in the middle of packets
		if(fAudioCodec->capabilities & CODEC_CAP_TRUNCATED)
		    demuxer->audioCodecContext->flags|=CODEC_FLAG_TRUNCATED;

		// Open codec
		AVDictionary *audioDict = NULL;
		if(avcodec_open2(demuxer->audioCodecContext, fAudioCodec, &audioDict)<0) {
			logInfo( LOG_DEMUXER," Could not audio open codec!\n");
			demuxer_error = DEMUXER_ERROR_OPEN_AUDIO_CODEC;
			demuxerStop(demuxer);
			return NULL;
		}

	}

	if(dvbsubStream!=-1) {

		// Get a pointer to the codec context for the dvbsub stream
		demuxer->dvbsubCodecContext = avcodec_alloc_context3(formatContext->streams[dvbsubStream]->codec->codec);

		if (demuxer->dvbsubCodecContext == NULL) {
			logInfo( LOG_DEMUXER," demuxer->dvbsubCodecContext == NULL!\n");
			demuxer_error = DEMUXER_ERROR_CREATE_DVBSUB_CONTEXT;
			demuxerStop(demuxer);
			return NULL;
		}

		logInfo( LOG_DEMUXER_DEBUG,"We are going to use dvb subtitle stream %d with codecname '%s', codec_id=%d, width=%d, height=%d.\n", dvbsubStream, formatContext->streams[dvbsubStream]->codec->codec_name, formatContext->streams[dvbsubStream]->codec->codec_id,demuxer->dvbsubCodecContext->width, demuxer->dvbsubCodecContext->height);

		AVCodec *fDVBSubCodec;

		// Find the decoder for the video stream
		fDVBSubCodec=avcodec_find_decoder(formatContext->streams[dvbsubStream]->codec->codec_id);
		if(fDVBSubCodec == NULL) {
			logInfo( LOG_DEMUXER," DVB Subtitle Codec not found!\n");
			demuxer_error = DEMUXER_ERROR_FIND_DVBSUB_DECODER;
			demuxerStop(demuxer);
			return NULL;
		}

		demuxer->dvbsubCodec = fDVBSubCodec;

		logInfo( LOG_DEMUXER,"Codec of dvb subtitle stream is '%s'.\n", fDVBSubCodec->name);

		AVDictionaryEntry *dvbsubLang = av_dict_get(formatContext->streams[dvbsubStream]->metadata, "language", NULL, 0);
		if (dvbsubLang) {
			logInfo( LOG_DEMUXER,"Language of dvb subtitle stream is '%s'.\n", dvbsubLang->value);
		}

		// Inform the codec that we can handle truncated bitstreams -- i.e.,
		// bitstreams where frame boundaries can fall in the middle of packets
		if(fDVBSubCodec->capabilities & CODEC_CAP_TRUNCATED)
		    demuxer->dvbsubCodecContext->flags|=CODEC_FLAG_TRUNCATED;

		// Open codec
		AVDictionary *dvbsubDict = NULL;
		if(avcodec_open2(demuxer->dvbsubCodecContext, fDVBSubCodec, &dvbsubDict)<0) {
			logInfo( LOG_DEMUXER," Could not open dvb subtitle codec!\n");
			demuxer_error = DEMUXER_ERROR_OPEN_DVBSUB_CODEC;
			demuxerStop(demuxer);
			return NULL;
		}

	}

	if (showVideo == 1) {
		demuxer->videoStream = videoStream;
	}
	else {
		demuxer->videoStream = -1;
	}

	if (playAudio == 1) {
		demuxer->audioStream = audioStream;
	}
	else {
		demuxer->audioStream = -1;
	}

	if (showDVBSub == 1) {
		demuxer->dvbsubStream = dvbsubStream;
	}
	else {
		demuxer->dvbsubStream = -1;
	}


	demuxer->formatContext = formatContext;

	if (demuxer->videoStream != -1) {

		if (pthread_mutex_init(&demuxer->videoThreadLock, NULL) != 0)
		{
			logInfo( LOG_DEMUXER_DEBUG,"  ----------> could not init mutex for video thread.\n");
			demuxer_error = DEMUXER_ERROR_START_THREAD;
			demuxerStop(demuxer);
			return NULL;
		}

		int status = pthread_create( &demuxer->videoThread, NULL, (void * (*)(void *))&videoLoop, demuxer);
		if (status != 0) {
			logInfo( LOG_DEMUXER_DEBUG,"  ----------> could not start video thread.\n");
			demuxer_error = DEMUXER_ERROR_START_THREAD;
			demuxerStop(demuxer);
			return NULL;
		}
		demuxer->videoThreadStarted = 1;
	}

	if (demuxer->audioStream != -1) {

		if (pthread_mutex_init(&demuxer->audioThreadLock, NULL) != 0)
		{
			logInfo( LOG_DEMUXER_DEBUG,"  ----------> could not init mutex for audio thread.\n");
			demuxer_error = DEMUXER_ERROR_START_THREAD;
			demuxerStop(demuxer);
			return NULL;
		}

		int status = pthread_create( &demuxer->audioThread, NULL, (void * (*)(void *))&audioLoop, demuxer);
		if (status != 0) {
			logInfo( LOG_DEMUXER_DEBUG,"  ----------> could not start audio thread.\n");
			demuxer_error = DEMUXER_ERROR_START_THREAD;
			demuxerStop(demuxer);
			return NULL;
		}
		demuxer->audioThreadStarted = 1;
	}

	if (demuxer->dvbsubStream != -1) {

		if (pthread_mutex_init(&demuxer->subtitleThreadLock, NULL) != 0)
		{
			logInfo( LOG_DEMUXER_DEBUG,"  ----------> could not init mutex for subtitle thread.\n");
			demuxer_error = DEMUXER_ERROR_START_THREAD;
			demuxerStop(demuxer);
			return NULL;
		}

		int status = pthread_create( &demuxer->subtitleThread, NULL, (void * (*)(void *))&subtitleLoop, demuxer);
		if (status != 0) {
			logInfo( LOG_DEMUXER_DEBUG,"  ----------> could not start subtitle thread.\n");
			demuxer_error = DEMUXER_ERROR_START_THREAD;
			demuxerStop(demuxer);
			return NULL;
		}
		demuxer->subtitleThreadStarted = 1;
	}

	logInfo( LOG_DEMUXER_DEBUG," *********************> starting demuxer thread.\n");
	if (pthread_mutex_init(&demuxer->threadLock, NULL) != 0)
	{
		logInfo( LOG_DEMUXER_DEBUG,"  ----------> could not init mutex for thread.\n");
		demuxer_error = DEMUXER_ERROR_START_THREAD;
		demuxerStop(demuxer);
		return NULL;
	}
	int status = pthread_create( &demuxer->demuxerThread, NULL, (void * (*)(void *))&demuxerLoop, demuxer);
	if (status != 0) {
		logInfo( LOG_DEMUXER_DEBUG,"  ----------> could not start thread.\n");
		demuxer_error = DEMUXER_ERROR_START_THREAD;
		demuxerStop(demuxer);
		return NULL;
	}
	logInfo( LOG_DEMUXER_DEBUG,"  ++++++++++> started thread ok.\n");
	demuxer->threadStarted = 1;

	demuxer_error = 0;
	return demuxer;
}

void demuxerStop(struct DEMUXER_T *demuxer)
{
	if (demuxer == NULL) {
		demuxerStartInterrupted = 1;
		return;
	}

/*	if (demuxer->mythThreadStarted == 1) {

		if (mythIsStopped(demuxer) == 0) {

			pthread_mutex_lock(&demuxer->mythThreadLock);

			demuxer->doStopMythThread = 1;

			pthread_mutex_unlock(&demuxer->mythThreadLock);

			logInfo(LOG_DEMUXER, "Waiting for myth thread to stop.\n");
			pthread_join(demuxer->mythThread, NULL);
			logInfo(LOG_DEMUXER, "Myth thread stopped.\n");
		}
		demuxer->mythThreadStarted = 0;
	}
*/
	if (demuxer->subtitleThreadStarted == 1) {

		if (subtitleIsStopped(demuxer) == 0) {

			pthread_mutex_lock(&demuxer->subtitleThreadLock);

			demuxer->doStopSubtitleThread = 1;

			pthread_mutex_unlock(&demuxer->subtitleThreadLock);

			logInfo(LOG_DEMUXER, "Waiting for subtitle thread to stop.\n");
			pthread_join(demuxer->subtitleThread, NULL);
			logInfo(LOG_DEMUXER, "subtitle thread stopped.\n");
			pthread_mutex_destroy(&demuxer->subtitleThreadLock);
		}
		demuxer->subtitleThreadStarted = 0;
	}

	if (demuxer->videoThreadStarted == 1) {

		if (videoIsStopped(demuxer) == 0) {

			pthread_mutex_lock(&demuxer->videoThreadLock);

			demuxer->doStopVideoThread = 1;

			pthread_mutex_unlock(&demuxer->videoThreadLock);

			logInfo(LOG_DEMUXER, "Waiting for video thread to stop.\n");
			pthread_join(demuxer->videoThread, NULL);
			logInfo(LOG_DEMUXER, "video thread stopped.\n");
			pthread_mutex_destroy(&demuxer->videoThreadLock);
		}
		demuxer->videoThreadStarted = 0;
	}

	if (demuxer->audioThreadStarted == 1) {

		if (audioIsStopped(demuxer) == 0) {

			pthread_mutex_lock(&demuxer->audioThreadLock);

			demuxer->doStopAudioThread = 1;

			pthread_mutex_unlock(&demuxer->audioThreadLock);

			logInfo(LOG_DEMUXER, "Waiting for audio thread to stop.\n");
			pthread_join(demuxer->audioThread, NULL);
			logInfo(LOG_DEMUXER, "audio thread stopped.\n");
			pthread_mutex_destroy(&demuxer->audioThreadLock);
		}
		demuxer->audioThreadStarted = 0;
	}

	if (demuxer->threadStarted == 1) {

		if (demuxerIsStopped(demuxer) == 0) {

			pthread_mutex_lock(&demuxer->threadLock);

			demuxer->doStop = 1;

			pthread_mutex_unlock(&demuxer->threadLock);

			logInfo(LOG_DEMUXER, "Waiting for demuxer thread to stop.\n");
			pthread_join(demuxer->demuxerThread, NULL);
			logInfo(LOG_DEMUXER, "Demuxer thread stopped.\n");
		}
		demuxer->threadStarted = 0;
	}

	if (demuxer->videoCodecContext != NULL) {
		logInfo(LOG_DEMUXER, "Releasing memory of demuxer->videoCodecContext.\n");
		avcodec_close(demuxer->videoCodecContext);
		av_free(demuxer->videoCodecContext);
		demuxer->videoCodecContext = NULL;
	}
	if (demuxer->audioCodecContext != NULL) {
		logInfo(LOG_DEMUXER, "Releasing memory of demuxer->audioCodecContext.\n");
		avcodec_close(demuxer->audioCodecContext);
		av_free(demuxer->audioCodecContext);
		demuxer->audioCodecContext = NULL;
	}

	if (demuxer->formatContext != NULL) {
		logInfo(LOG_DEMUXER, "Releasing memory of demuxer->formatContext.\n");

		if (demuxer->formatContextIsOpen == 1) {
			avformat_close_input(&demuxer->formatContext);
		}
		else {
			avformat_free_context(demuxer->formatContext);
		}
		demuxer->formatContext = NULL;
	}

	if (demuxer->avioBuffer != NULL) {
		logInfo(LOG_DEMUXER, "Releasing memory of demuxer->avioBuffer.\n");
//		av_free(demuxer->avioBuffer); // Releasing this one gives and error. It is done by avformat_close_input.
		demuxer->avioBuffer = NULL;
	}
 

	if (demuxer->ioContext != NULL) {
		logInfo(LOG_DEMUXER, "Releasing memory of demuxer->ioContext.\n");
		av_free(demuxer->ioContext);
		demuxer->ioContext = NULL;
	}

	logInfo(LOG_DEMUXER, "demuxerStop finished.\n");

}


