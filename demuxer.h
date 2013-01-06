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

#ifndef FFMPEG_FILE_BUFFER_SIZE
#define FFMPEG_FILE_BUFFER_SIZE   32768 // default reading size for ffmpeg
#endif

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
} DEMUXER_ERROR_T;

struct DEMUXER_T{
	int videoStream;
	int audioStream;
	AVFormatContext *fFormatContext;
	struct MYTH_CONNECTION_T *mythConnection;
	pthread_t demuxerThread;
	int doStop;
	pthread_mutex_t threadLock;
	AVCodec *videoCodec;
	AVCodec *audioCodec;

	AVCodecContext *audioCodecContext;

	struct OMX_COMPONENT_T *videoDecoder;
	struct OMX_COMPONENT_T *videoRender;
	struct OMX_COMPONENT_T *videoScheduler;
	struct OMX_TUNNEL_T *videoDecoderToSchedulerTunnel;
	struct OMX_TUNNEL_T *videoSchedulerToRenderTunnel;
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

	int videoPortSettingChanged;
	int audioPortSettingChanged;

	int64_t startPTS;
	int64_t startDTS;
};

int demuxer_error;

void demuxerSetLanguage(char *newLanguage);
struct DEMUXER_T *demuxerStart(struct MYTH_CONNECTION_T *mythConnection, int showVideo, int playAudio, int audioPassthrough);
void demuxerStop(struct DEMUXER_T *demuxer);

