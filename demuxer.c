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

#include "globalFunctions.h"
#include "lists.h"
#include "connection.h"
#include "mythProtocol.h"
#include "demuxer.h"
#include "omxCore.h"
#include "omxVideo.h"
#include "omxAudio.h"


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

	ssize_t readLen = mythFiletransferRequestBlock(demuxer->mythConnection, bufferSize);
	logInfo( LOG_DEMUXER_DEBUG,"Myth will send %zd bytes of the requested %d.\n", readLen, bufferSize);
	if (readLen == 0) {
		return 0;
	}

	ssize_t receivedLen = fillConnectionBuffer(demuxer->mythConnection->transferConnection->connection, readLen, 1);
	logInfo( LOG_DEMUXER_DEBUG,"Connection buffer was filled with %zd bytes of the requested %zd.\n", receivedLen, readLen);

	struct timespec interval;
	struct timespec remainingInterval;

	while (getConnectionDataLen(demuxer->mythConnection->transferConnection->connection) < receivedLen) {
		interval.tv_sec = 0;
		interval.tv_nsec = 10;

		nanosleep(&interval, &remainingInterval);
	}

	ssize_t readBufferLen = 0;
	while (readBufferLen == 0) {
		readBufferLen = readConnectionBuffer(demuxer->mythConnection->transferConnection->connection, (char *)buffer, receivedLen);
//		logInfo( LOG_DEMUXER_DEBUG,"Received %zd bytes from connection.\n", readBufferLen);
	}
	logInfo( LOG_DEMUXER_DEBUG,"Received %zd bytes from connection.\n", readBufferLen);

	logInfo( LOG_DEMUXER_DEBUG,"test %zd< %d && demuxer->nextFile = %s.\n", readLen, bufferSize, demuxer->nextFile);
	if ((readLen < bufferSize) && (demuxer->nextFile != NULL)) {
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

	int64_t result = mythFiletransferSeek(demuxer->mythConnection, offset, realWhence, demuxer->mythConnection->position);
	logInfo( LOG_DEMUXER_DEBUG,"Seeked to=%" PRId64 ", whence=%d.\n", result, realWhence);

	return result;
}

int showVideoPacket(struct DEMUXER_T *demuxer, AVPacket *packet, double pts)
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
		// 500ms timeout
		logInfo(LOG_DEMUXER_DEBUG, "demuxer_bytes=%d. Before omxGetInputBuffer\n", demuxer_bytes);
		OMX_BUFFERHEADERTYPE *omx_buffer = omxGetInputBuffer(demuxer->videoDecoder, 500);
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

		if ((demuxer->videoPortSettingChanged == 0) && (omxWaitForEvent(demuxer->videoDecoder, OMX_EventPortSettingsChanged, demuxer->videoDecoder->outputPort, 0) == OMX_ErrorNone)) {

			demuxer->videoPortSettingChanged = 1;

			logInfo(LOG_DEMUXER_DEBUG, "demuxer->videoPortSettingChanged set to true.\n");


			if (demuxer->deInterlace == 0) {
				omxErr = omxEstablishTunnel(demuxer->videoDecoderToSchedulerTunnel);
				if(omxErr != OMX_ErrorNone) {
					logInfo(LOG_DEMUXER, "Error establishing tunnel between video decoder and video scheduler components. (Error=0x%08x).\n", omxErr);
					av_free_packet(packet);
					return 1;
				}
			}
			else {

				omxShowVideoInterlace(demuxer->videoDecoder);

				OMX_PARAM_PORTDEFINITIONTYPE portImage;
				OMX_INIT_STRUCTURE(portImage);
				portImage.nPortIndex = demuxer->videoDecoder->outputPort;

				omxErr = OMX_GetParameter(demuxer->videoDecoder->handle, OMX_IndexParamPortDefinition, &portImage);
				if (omxErr != OMX_ErrorNone) {
					logInfo(LOG_DEMUXER, "Error OMX_IndexParamPortDefinition for video decoder components. (Error=0x%08x).\n", omxErr);
					return 1;
				}

				if ((portImage.format.video.nFrameWidth == demuxer->formatContext->streams[demuxer->videoStream]->codec->width) && (portImage.format.video.nFrameHeight == demuxer->formatContext->streams[demuxer->videoStream]->codec->height)) {
					logInfo(LOG_DEMUXER, "Demuxer has first frame available for image_fx.\n");

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

				}
				else {
					logInfo(LOG_DEMUXER, "Demuxer does NOT have the first frame available for image_fx!!!!!!!!\n");
				}

				omxErr = omxEstablishTunnel(demuxer->videoDecoderToImageFXTunnel);
				if(omxErr != OMX_ErrorNone) {
					logInfo(LOG_DEMUXER, "Error establishing tunnel between video decoder and image_fx scheduler components. (Error=0x%08x).\n", omxErr);
					av_free_packet(packet);
					return 1;
				}

				omxErr = omxEstablishTunnel(demuxer->videoImageFXToSchedulerTunnel);
				if(omxErr != OMX_ErrorNone) {
					logInfo(LOG_DEMUXER, "Error establishing tunnel between video image_fx and video scheduler components. (Error=0x%08x).\n", omxErr);
					av_free_packet(packet);
					return 1;
				}

				omxErr = omxSetStateForComponent(demuxer->videoImageFX, OMX_StateExecuting);
				if (omxErr != OMX_ErrorNone)
				{
					logInfo(LOG_DEMUXER, "video image_fx SetStateForComponent to OMX_StateExecuting. (Error=0x%08x).\n", omxErr);
					av_free_packet(packet);
					return 1;
				}

			}

			omxErr = omxSetStateForComponent(demuxer->videoScheduler, OMX_StateExecuting);
			if (omxErr != OMX_ErrorNone)
			{
				logInfo(LOG_DEMUXER, "video scheduler SetStateForComponent to OMX_StateExecuting. (Error=0x%08x).\n", omxErr);
				av_free_packet(packet);
				return 1;
			}

			omxErr = omxEstablishTunnel(demuxer->videoSchedulerToRenderTunnel);
			if(omxErr != OMX_ErrorNone) {
				logInfo(LOG_DEMUXER, "Error establishing tunnel between video scheduler and video render components. (Error=0x%08x).\n", omxErr);
				av_free_packet(packet);
				return 1;
			}

			omxErr = omxSetStateForComponent(demuxer->videoRender, OMX_StateExecuting);
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

					omxErr = omxSetStateForComponent(demuxer->audioMixer, OMX_StateExecuting);
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


				omxErr = omxSetStateForComponent(demuxer->audioRender, OMX_StateExecuting);
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

			if (ret == 1) {
				av_free_packet(inPacket);
			}

			return ret;
		}
		else {
			// We did not have any valid audio packet.
			avcodec_free_frame(&frame1);

			av_free_packet(inPacket);

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

	if (ret == 1) {
		av_free_packet(inPacket);
	}

	return ret;
}

double ffmpegConvertTimestamp(struct DEMUXER_T *demuxer, int64_t pts, AVRational *time_base)
{
	double new_pts = pts;

	if(demuxer == NULL)
		return 0;

	if (pts == (int64_t)AV_NOPTS_VALUE)
		return DVD_NOPTS_VALUE;

//	if (demuxer->formatContext->start_time != (int64_t)AV_NOPTS_VALUE)
//	new_pts += demuxer->formatContext->start_time;

	new_pts -=demuxer->startPTS;

	new_pts *= av_q2d(*time_base);

	new_pts *= (double)DVD_TIME_BASE;

	return new_pts;
}

OMX_ERRORTYPE demuxerInitOMXVideo(struct DEMUXER_T *demuxer)
{
	OMX_ERRORTYPE omxErr;

	if (demuxer->videoStream > -1) {
		demuxer->videoDecoder = omxCreateComponent("OMX.broadcom.video_decode" , OMX_IndexParamVideoInit);
		if (demuxer->videoDecoder == NULL) {
			logInfo(LOG_DEMUXER, "Error creating OMX video decoder component. (Error=0x%08x).\n", omx_error);
			return omx_error;
		}
		logInfo(LOG_DEMUXER_DEBUG, "Created OMX video decoder component.\n");

		if (demuxer->deInterlace == 1) {
			demuxer->videoImageFX = omxCreateComponent("OMX.broadcom.image_fx" , OMX_IndexParamImageInit);
			if (demuxer->videoImageFX == NULL) {
				logInfo(LOG_DEMUXER, "Error creating OMX video image_fx component. (Error=0x%08x).\n", omx_error);
				return omx_error;
			}
			logInfo(LOG_DEMUXER_DEBUG, "Created OMX video image_fx component.\n");

			omxErr = omxSetVideoDeInterlace(demuxer->videoImageFX, 1);
			if (omxErr != OMX_ErrorNone) {
				logInfo(LOG_DEMUXER, "Error omxSetVideoDeInterlace on video image_fx component. (Error=0x%08x).\n", omxErr);
				return omxErr;
			}

		}

		demuxer->videoRender = omxCreateComponent("OMX.broadcom.video_render" , OMX_IndexParamVideoInit);
		if (demuxer->videoRender == NULL) {
			logInfo(LOG_DEMUXER, "Error creating OMX video render component. (Error=0x%08x).\n", omx_error);
			return omx_error;
		}
		logInfo(LOG_DEMUXER_DEBUG, "Created OMX video render component.\n");

		demuxer->videoScheduler = omxCreateComponent("OMX.broadcom.video_scheduler" , OMX_IndexParamVideoInit);
		if (demuxer->videoScheduler == NULL) {
			logInfo(LOG_DEMUXER, "Error creating OMX video scheduler component. (Error=0x%08x).\n", omx_error);
			return omx_error;
		}
		logInfo(LOG_DEMUXER_DEBUG, "Created OMX video scheduler component.\n");

		if (demuxer->deInterlace == 0) {
			demuxer->videoDecoderToSchedulerTunnel = omxCreateTunnel(demuxer->videoDecoder, demuxer->videoDecoder->outputPort, demuxer->videoScheduler, demuxer->videoScheduler->inputPort);
			if (demuxer->videoDecoderToSchedulerTunnel == NULL) {
				logInfo(LOG_DEMUXER, "Error creating tunnel between video decoder and video scheduler components. (Error=0x%08x).\n", omx_error);
				return omx_error;
			}
		}
		else {
			demuxer->videoDecoderToImageFXTunnel = omxCreateTunnel(demuxer->videoDecoder, demuxer->videoDecoder->outputPort, demuxer->videoImageFX, demuxer->videoImageFX->inputPort);
			if (demuxer->videoDecoderToImageFXTunnel == NULL) {
				logInfo(LOG_DEMUXER, "Error creating tunnel between video decoder and video image_fx components. (Error=0x%08x).\n", omx_error);
				return omx_error;
			}

			demuxer->videoImageFXToSchedulerTunnel = omxCreateTunnel(demuxer->videoImageFX, demuxer->videoImageFX->outputPort, demuxer->videoScheduler, demuxer->videoScheduler->inputPort);
			if (demuxer->videoImageFXToSchedulerTunnel == NULL) {
				logInfo(LOG_DEMUXER, "Error creating tunnel between video image_fx and video scheduler components. (Error=0x%08x).\n", omx_error);
				return omx_error;
			}

/*			omxErr = omxSetStateForComponent(demuxer->videoImageFX, OMX_StateIdle);
			if (omxErr != OMX_ErrorNone)
			{
				logInfo(LOG_DEMUXER, "video image_fx SetStateForComponent to OMX_StateIdle. (Error=0x%08x).\n", omxErr);
				return omxErr;
			}
*/
		}

		demuxer->videoSchedulerToRenderTunnel = omxCreateTunnel(demuxer->videoScheduler, demuxer->videoScheduler->outputPort, demuxer->videoRender, demuxer->videoRender->inputPort);
		if (demuxer->videoSchedulerToRenderTunnel == NULL) {
			logInfo(LOG_DEMUXER, "Error creating tunnel between video scheduler and video render components. (Error=0x%08x).\n", omx_error);
			return omx_error;
		}

		demuxer->clockToVideoSchedulerTunnel = omxCreateTunnel(demuxer->clock->clockComponent, demuxer->clock->clockComponent->inputPort+1, demuxer->videoScheduler, demuxer->videoScheduler->outputPort + 1);
		if (demuxer->clockToVideoSchedulerTunnel == NULL) {
			logInfo(LOG_DEMUXER, "Error creating tunnel between clock and video scheduler components. (Error=0x%08x).\n", omx_error);
			return omx_error;
		}

		omxErr = omxEstablishTunnel(demuxer->clockToVideoSchedulerTunnel);
		if(omxErr != OMX_ErrorNone) {
			logInfo(LOG_DEMUXER, "Error establishing tunnel between clock and video scheduler components. (Error=0x%08x).\n", omxErr);
			return omxErr;
		}

		omxErr = omxSetStateForComponent(demuxer->videoDecoder, OMX_StateIdle);
		if (omxErr != OMX_ErrorNone)
		{
			logInfo(LOG_DEMUXER, "video decoder SetStateForComponent to OMX_StateIdle. (Error=0x%08x).\n", omxErr);
			return omxErr;
		}

//		logInfo(LOG_DEMUXER, "This video stream has a framerate of %f fps.\n", av_q2d(demuxer->formatContext->streams[demuxer->videoStream]->avg_frame_rate));
		logInfo(LOG_DEMUXER, "This video stream has a framerate of %f fps.\n", av_q2d(demuxer->formatContext->streams[demuxer->videoStream]->r_frame_rate));

		switch (demuxer->videoCodec->id) {
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

		if (omxErr != OMX_ErrorNone)
		{
			logInfo(LOG_DEMUXER, "video decoder omxSetVideoCompressionFormatAndFrameRate. (Error=0x%08x).\n", omxErr);
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
			OMX_CONFIG_LATENCYTARGETTYPE latencyTarget;
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
	//	}

		if (demuxer->deInterlace == 1) {
			omxErr = omxSetVideoSetExtraBuffers(demuxer->videoDecoder);
			if (omxErr != OMX_ErrorNone)
			{
				logInfo(LOG_DEMUXER, "video decoder omxSetVideoSetExtraBuffers. (Error=0x%08x).\n", omxErr);
				return omxErr;
			}
		}

		omxErr = omxAllocInputBuffers(demuxer->videoDecoder, 0);
		if (omxErr != OMX_ErrorNone)
		{
			logInfo(LOG_DEMUXER, "video decoder omxAllocInputBuffers. (Error=%d).\n", omxErr);
			return omxErr;
		}

		omxErr = omxSetStateForComponent(demuxer->videoDecoder, OMX_StateExecuting);
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
		
		demuxer->clockToAudioRenderTunnel = omxCreateTunnel(demuxer->clock->clockComponent, demuxer->clock->clockComponent->inputPort, demuxer->audioRender, demuxer->audioRender->outputPort + 1);
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

			omxErr = omxSetStateForComponent(demuxer->audioRender, OMX_StateIdle);
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

			omxErr = omxSetStateForComponent(demuxer->audioRender, OMX_StateExecuting);
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

			omxErr = omxSetStateForComponent(demuxer->audioDecoder, OMX_StateIdle);
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

			omxErr = omxSetStateForComponent(demuxer->audioDecoder, OMX_StateExecuting);
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

void *demuxerLoop(struct DEMUXER_T *demuxer)
{
	static AVPacket packet;
	int doneReading = 0;

#ifdef PI
	if (omxInit() != 0) {
		goto theEnd;
	}

	OMX_ERRORTYPE omxErr;

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
	int64_t audioDTS = -1;
	int64_t videoDTS = -1;
	double tmpPTS = 0;
	int validPTSValues = 0;

#ifdef PI
	omxErr = omxSetStateForComponent(demuxer->clock->clockComponent, OMX_StateExecuting);
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

	packet.data = NULL;

	int skipCount = 40;

	demuxer->videoPortSettingChanged = 0;
	demuxer->audioPortSettingChanged = 0;

	demuxer->startPTS = -1;
	demuxer->startDTS = -1;

	while ((doneReading == 0) && (demuxerIsStopped(demuxer) == 0)) {
		// Free old packet
		if (packet.data == NULL) {

			logInfo(LOG_DEMUXER_DEBUG, "Going to read packet.\n");

			// Read new packet
			if ((demuxerIsStopped(demuxer) == 0) && (av_read_frame(demuxer->formatContext, &packet)<0))
				doneReading = 1;

			logInfo(LOG_DEMUXER_DEBUG, "Read packet.\n");

			if (skipCount-- > 0) {
				logInfo(LOG_DEMUXER_DEBUG,"Skipping frame because they are the first. Still need to skip %d frames.\n", skipCount);
				av_free_packet(&packet);
				packet.data = NULL;
				continue;
			}

			if ((demuxerIsStopped(demuxer) == 0) && (demuxer->videoStream != -1) && (packet.data != NULL) && (packet.stream_index == demuxer->videoStream)) {
				// We used to save to a file in this part
			}
			else {
				if ((demuxerIsStopped(demuxer) == 0) && (demuxer->videoStream != -1) && (packet.data != NULL) && (packet.stream_index == demuxer->audioStream)) {
					// We used to save to a file in this part
				}
				else {
					logInfo(LOG_DEMUXER_DEBUG, "This is a packet we do not want to process. Going to ignore it. packet.stream_index=%d.\n", packet.stream_index);
					av_free_packet(&packet);
					packet.data = NULL;
					continue;
				}
			}

			if (packet.data != NULL) {
				if(packet.dts == 0)
					packet.dts = AV_NOPTS_VALUE;
				if(packet.pts == 0)
					packet.pts = AV_NOPTS_VALUE;
			}

			if (packet.stream_index == demuxer->audioStream) {
				audioPTS = packet.pts;
				audioDTS = packet.dts;
			}

			if (packet.stream_index == demuxer->videoStream) {
				videoPTS = packet.pts;
				videoDTS = packet.dts;
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

		if (validPTSValues) {


			// We need to drop the first few audio or video packets because the time difference is to big.

			if ((demuxerIsStopped(demuxer) == 0) && (demuxer->videoStream != -1) && (packet.data != NULL) && (packet.stream_index == demuxer->videoStream)) {
				tmpPTS = ffmpegConvertTimestamp(demuxer, videoPTS, &demuxer->formatContext->streams[packet.stream_index]->time_base);
				logInfo(LOG_DEMUXER_DEBUG, "VideoDTS=%" PRId64 ", VideoPTS=%" PRId64 ", demuxer->startPTS=%" PRId64 ", timestamp=%.15f\n", videoDTS, videoPTS, demuxer->startPTS, tmpPTS);
				if (showVideoPacket(demuxer, &packet, tmpPTS) == 1) {
					packet.data = NULL;
				}
				else {
					logInfo(LOG_DEMUXER_DEBUG, "Retrying video packet.\n");
				}
			}
			else {
				if ((demuxerIsStopped(demuxer) == 0) && (demuxer->audioStream != -1) && (packet.data != NULL) && (packet.stream_index == demuxer->audioStream)) {
					tmpPTS = ffmpegConvertTimestamp(demuxer, audioPTS, &demuxer->formatContext->streams[packet.stream_index]->time_base);
					logInfo(LOG_DEMUXER_DEBUG, "AudioDTS=%" PRId64 ", AudioPTS=%" PRId64 ", demuxer->startPTS=%" PRId64 ", timestamp=%.15f\n", audioDTS, audioPTS, demuxer->startPTS, tmpPTS);
					if (decodeAudio(demuxer, &packet, tmpPTS) == 1) {
//					if (playAudioPacket(demuxer, &packet, tmpPTS) == 1) {
						packet.data = NULL;
					}
					else {
						logInfo(LOG_DEMUXER_DEBUG, "Retrying audio packet.\n");
					}
				}
				else {
					// Drop other packets.
					av_free_packet(&packet);
					packet.data = NULL;
				}
			}
		}
		else {
			// Drop other packets.
			av_free_packet(&packet);
			packet.data = NULL;
		}

/*		interval.tv_sec = 0;
		interval.tv_nsec = 10;

		nanosleep(&interval, &remainingInterval);*/
	}

	if(packet.data!=NULL) av_free_packet(&packet);


theEnd:
	pthread_mutex_lock(&demuxer->threadLock);

	demuxer->doStop = 1;

	pthread_mutex_unlock(&demuxer->threadLock);

	logInfo(LOG_DEMUXER, "Stopping video tunnels.\n");
	omxFlushTunnel(demuxer->videoDecoderToSchedulerTunnel);
	omxFlushTunnel(demuxer->videoDecoderToImageFXTunnel);
	omxFlushTunnel(demuxer->videoImageFXToSchedulerTunnel);
	omxFlushTunnel(demuxer->clockToVideoSchedulerTunnel);
	omxFlushTunnel(demuxer->videoSchedulerToRenderTunnel);

	logInfo(LOG_DEMUXER, "Destroying video tunnels.\n");
	omxDestroyTunnel(demuxer->clockToVideoSchedulerTunnel);
	omxDestroyTunnel(demuxer->videoDecoderToSchedulerTunnel);
	omxDestroyTunnel(demuxer->videoDecoderToImageFXTunnel);
	omxDestroyTunnel(demuxer->videoImageFXToSchedulerTunnel);
	omxDestroyTunnel(demuxer->videoSchedulerToRenderTunnel);

	logInfo(LOG_DEMUXER, "Flushing video decoder input port.\n");
	omxFlushPort(demuxer->videoDecoder, demuxer->videoDecoder->inputPort);

	logInfo(LOG_DEMUXER, "Destroying video components.\n");
	omxDestroyComponent(demuxer->videoScheduler);
	omxDestroyComponent(demuxer->videoImageFX);
	omxDestroyComponent(demuxer->videoDecoder);
	omxDestroyComponent(demuxer->videoRender);

	logInfo(LOG_DEMUXER, "Stopping audio tunnels.\n");
	omxFlushTunnel(demuxer->clockToAudioRenderTunnel);
	omxFlushTunnel(demuxer->audioDecoderToMixerTunnel);
	omxFlushTunnel(demuxer->audioMixerToRenderTunnel);
	omxFlushTunnel(demuxer->audioDecoderToRenderTunnel);

	logInfo(LOG_DEMUXER, "Destroying audio tunnels.\n");
	omxDestroyTunnel(demuxer->audioDecoderToMixerTunnel);
	omxDestroyTunnel(demuxer->audioMixerToRenderTunnel);
	omxDestroyTunnel(demuxer->clockToAudioRenderTunnel);
	omxDestroyTunnel(demuxer->audioDecoderToRenderTunnel);

	logInfo(LOG_DEMUXER, "Flushing audio render input port.\n");
	omxFlushPort(demuxer->audioRender, demuxer->audioRender->inputPort);

	logInfo(LOG_DEMUXER, "Destroying audio components.\n");
	omxDestroyComponent(demuxer->audioDecoder);
	omxDestroyComponent(demuxer->audioMixer);
	omxDestroyComponent(demuxer->audioRender);

	omxDestroyClock(demuxer->clock);
	
	logInfo(LOG_DEMUXER, "Done stopping demuxer loop.\n");
	return NULL;
}

struct DEMUXER_T *demuxerStart(struct MYTH_CONNECTION_T *mythConnection, int showVideo, int playAudio, int audioPassthrough)
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
	demuxer->deInterlace = 0;
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

	demuxer->formatContext = NULL;

	demuxer->ioContext = NULL;
	demuxer->avioBuffer = NULL;

	demuxer->formatContextIsOpen = 0;
	demuxer->threadStarted = 0;

	if (pthread_mutex_init(&demuxer->threadLock, NULL) != 0)
	{
		logInfo( LOG_DEMUXER_DEBUG,"  ----------> could init mutex for thread.\n");
		demuxer_error = DEMUXER_ERROR_START_THREAD;
		free(demuxer);
		return NULL;
	}
	demuxer->demuxerThread = -1;

	demuxer->avioBuffer = (unsigned char*)av_malloc(FFMPEG_FILE_BUFFER_SIZE);

	// Init io module for input
	demuxer->ioContext = avio_alloc_context(demuxer->avioBuffer, FFMPEG_FILE_BUFFER_SIZE, 0, (void *)demuxer, &demuxerReadPacket, 0, &demuxerSeek);

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
	int firstVideoStream=-1;
	int firstAudioStream=-1;

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

	demuxer->formatContext = formatContext;

	logInfo( LOG_DEMUXER_DEBUG," *********************> starting demuxer thread.\n");
	int status = pthread_create( &demuxer->demuxerThread, NULL, &demuxerLoop, demuxer);
	if (status == 0) {
		logInfo( LOG_DEMUXER_DEBUG,"  ++++++++++> started thread ok.\n");
		demuxer->threadStarted = 1;
	}
	else {
		logInfo( LOG_DEMUXER_DEBUG,"  ----------> could not start thread.\n");
		demuxer_error = DEMUXER_ERROR_START_THREAD;
		demuxerStop(demuxer);
		return NULL;
	}

	demuxer_error = 0;
	return demuxer;
}

void demuxerStop(struct DEMUXER_T *demuxer)
{
	if (demuxer == NULL) {
		demuxerStartInterrupted = 1;
		return;
	}

	if (demuxer->threadStarted == 1) {

		if (demuxerIsStopped(demuxer) == 1) return;

		pthread_mutex_lock(&demuxer->threadLock);

		demuxer->doStop = 1;

		pthread_mutex_unlock(&demuxer->threadLock);

		logInfo(LOG_DEMUXER_DEBUG, "Waiting for demuxer thread to stop.\n");
		pthread_join(demuxer->demuxerThread, NULL);
		logInfo(LOG_DEMUXER_DEBUG, "Demuxer thread stopped.\n");
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

/*	if (demuxer->avioBuffer != NULL) {
		logInfo(LOG_DEMUXER, "Releasing memory of demuxer->avioBuffer.\n");
		av_free(demuxer->avioBuffer);
		demuxer->avioBuffer = NULL;
	}
*/ // Releasing this one gives and error. Is porbably nicely done by the ioContect freeing.

	if (demuxer->ioContext != NULL) {
		logInfo(LOG_DEMUXER, "Releasing memory of demuxer->ioContext.\n");
		av_free(demuxer->ioContext);
		demuxer->ioContext = NULL;
	}

	logInfo(LOG_DEMUXER, "demuxerStop finished.\n");

}


