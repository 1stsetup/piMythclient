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

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/crc.h>
#include <libavutil/fifo.h>
// for LIBAVCODEC_VERSION_INT:
#include <libavcodec/avcodec.h>
#include <libavutil/dict.h>
#include <libavformat/avio.h>
#include <libavutil/audioconvert.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>

#ifndef FFMPEG_FILE_BUFFER_SIZE
//#define FFMPEG_FILE_BUFFER_SIZE   32768 // default reading size for ffmpeg
#define FFMPEG_FILE_BUFFER_SIZE   24576 // default reading size for ffmpeg
#endif

#define MIN_MYTH_QUEUE_LENGTH	FFMPEG_FILE_BUFFER_SIZE * 100

#define DVD_NOPTS_VALUE    (-1LL<<52)
#define DVD_TIME_BASE 1000000

typedef enum {
	DEMUXER_ERROR_CREATE_IOCONTEXT		= -1,
	DEMUXER_ERROR_PROBE_INPUT_BUFFER	= -2,
	DEMUXER_ERROR_OPEN_INPUT		= -3,
	DEMUXER_ERROR_FIND_STREAM_INFO		= -4,
	DEMUXER_ERROR_CREATE_VIDEO_CONTEXT	= -5,
	DEMUXER_ERROR_FIND_VIDEO_DECODER	= -6,
	DEMUXER_ERROR_OPEN_VIDEO_CODEC		= -7,
	DEMUXER_ERROR_CREATE_AUDIO_CONTEXT	= -8,
	DEMUXER_ERROR_FIND_AUDIO_DECODER	= -9,
	DEMUXER_ERROR_OPEN_AUDIO_CODEC		= -10,
	DEMUXER_ERROR_START_THREAD		= -11,
	DEMUXER_ERROR_CREATE_FORMAT_CONTEXT	= -12,
	DEMUXER_ERROR_CREATE_DVBSUB_CONTEXT	= -8,
	DEMUXER_ERROR_FIND_DVBSUB_DECODER	= -9,
	DEMUXER_ERROR_OPEN_DVBSUB_CODEC		= -10,
} DEMUXER_ERROR_T;

struct DEMUXER_T{
	int videoStream;
	int audioStream;
	int dvbsubStream;
	AVFormatContext *formatContext;
	int formatContextIsOpen;
	struct MYTH_CONNECTION_T *mythConnection;
	pthread_t demuxerThread;
	int threadStarted;

	pthread_t videoThread;
	int videoThreadStarted;
	int doStopVideoThread;
	pthread_t audioThread;
	int audioThreadStarted;
	int doStopAudioThread;
	pthread_t subtitleThread;
	int subtitleThreadStarted;
	int doStopSubtitleThread;

	pthread_t mythThread;
	int mythThreadStarted;
	int doStopMythThread;

	int doStop;
	pthread_mutex_t threadLock;
	pthread_mutex_t videoThreadLock;
	pthread_mutex_t audioThreadLock;
	pthread_mutex_t subtitleThreadLock;
	pthread_mutex_t mythThreadLock;

	struct SIMPLELISTITEM_T *subtitleAVPackets;
	struct SIMPLELISTITEM_T *subtitleAVPacketsEnd;

	struct SIMPLELISTITEM_T *subtitlePackets;
	struct SIMPLELISTITEM_T *subtitlePacketsEnd;

	struct SIMPLELISTITEM_T *audioPackets;
	struct SIMPLELISTITEM_T *audioPacketsEnd;

	unsigned char *avioBuffer;
	AVIOContext *ioContext;
	AVCodec *videoCodec;
	AVCodec *audioCodec;
	AVCodec *dvbsubCodec;

	AVCodecContext *audioCodecContext;
	AVCodecContext *videoCodecContext;
	AVCodecContext *dvbsubCodecContext;

	char *nextFile;

	struct OMX_COMPONENT_T *videoDecoder;
	struct OMX_COMPONENT_T *videoImageFX;
	struct OMX_COMPONENT_T *videoRender;
	struct OMX_COMPONENT_T *videoScheduler;
	struct OMX_TUNNEL_T *videoDecoderToSchedulerTunnel;
	struct OMX_TUNNEL_T *videoSchedulerToRenderTunnel;
	struct OMX_TUNNEL_T *videoDecoderToImageFXTunnel;
	struct OMX_TUNNEL_T *videoImageFXToSchedulerTunnel;
	struct OMX_TUNNEL_T *clockToVideoSchedulerTunnel;

	struct OMX_COMPONENT_T *audioDecoder;
	struct OMX_COMPONENT_T *audioRender;
	struct OMX_COMPONENT_T *audioMixer;
	struct OMX_TUNNEL_T *audioDecoderToMixerTunnel;
	struct OMX_TUNNEL_T *audioMixerToRenderTunnel;
	struct OMX_TUNNEL_T *clockToAudioRenderTunnel;
	struct OMX_TUNNEL_T *audioDecoderToRenderTunnel;

	struct OMX_CLOCK_T *clock;

	FILE *f, *outfile;

	int isOpen;
	int dropState;
	int setVideoStartTime;
	int setAudioStartTime;
	int firstFrame;

	int showVideo;
	int playAudio;
	int audioPassthrough;
	int swDecodeAudio;

	int deInterlace;
	int streamIsInterlaced;

	int videoStreamWasInterlaced;
	int audioPortSettingChanged;

	int64_t startPTS;
	int64_t startDTS;

	int lastFrameSize;
	int newProgram;

	struct OSD_T *subtitleOSD;
};

int demuxer_error;

void demuxerSetNextFile(struct DEMUXER_T *demuxer, char *filename);
void demuxerSetLanguage(char *newLanguage);
struct DEMUXER_T *demuxerStart(struct MYTH_CONNECTION_T *mythConnection, int showVideo, int playAudio, int showDVBSub, int audioPassthrough);
void demuxerStop(struct DEMUXER_T *demuxer);

