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

#include <IL/OMX_Core.h>
#include <IL/OMX_Component.h>
#include <IL/OMX_Index.h>
#include <IL/OMX_Image.h>
#include <IL/OMX_Video.h>
#include <IL/OMX_Broadcom.h>

#define OMX_INIT_STRUCTURE(a) \
  memset(&(a), 0, sizeof(a)); \
  (a).nSize = sizeof(a); \
  (a).nVersion.s.nVersionMajor = OMX_VERSION_MAJOR; \
  (a).nVersion.s.nVersionMinor = OMX_VERSION_MINOR; \
  (a).nVersion.s.nRevision = OMX_VERSION_REVISION; \
  (a).nVersion.s.nStep = OMX_VERSION_STEP

#ifdef OMX_SKIP64BIT
static inline OMX_TICKS ToOMXTime(uint64_t pts)
{
	OMX_TICKS ticks;
	ticks.nLowPart = pts;
	ticks.nHighPart = pts >> 32;
	return ticks;
}

static inline uint64_t FromOMXTime(OMX_TICKS ticks)
{
	uint64_t pts = ticks.nLowPart | ((uint64_t)ticks.nHighPart << 32);
	return pts;
}
#else
#define FromOMXTime(x) (x)
#define ToOMXTime(x) (x)
#endif


typedef enum {
	OMX_ERROR_OMX_INIT			= -1,
	OMX_ERROR_OMX_GETHANDLE			= -2,
	OMX_ERROR_OMX_GETPARAMETER_PORT		= -3,
	OMX_ERROR_OMX_DEINIT			= -4,
	OMX_ERROR_INIT_EVENT_MUTEX		= -5,
	OMX_ERROR_MALLOC			= -6,
} OMX_ERROR_T;

struct OMX_COMPONENT_T {
	OMX_HANDLETYPE handle;
	char *componentName;

	OMX_CALLBACKTYPE callbacks;
	pthread_mutex_t eventMutex;
	pthread_mutex_t componentMutex;
	struct SIMPLELISTITEM_T *eventListStart;
	struct SIMPLELISTITEM_T *eventListEnd;

	int inputPort;
	int outputPort;

	pthread_mutex_t   inputMutex;
	pthread_cond_t    inputBufferCond;

	struct SIMPLELISTITEM_T *inputBuffer;
	struct SIMPLELISTITEM_T *inputBufferEnd;
	unsigned int	inputAlignment;
	unsigned int	inputBufferSize;
	unsigned int	inputBufferCount;
	int		inputUseBuffers;

	int		flushInput;
	int		useHWDecode;

	int		portSettingChanged;
	int		isClock;

};

struct OMX_TUNNEL_T {
	struct OMX_COMPONENT_T *sourceComponent;
	unsigned int	sourcePort;
	struct OMX_COMPONENT_T *destComponent;
	unsigned int	destPort;
	int tunnelIsUp;
};

struct OMX_EVENT_T {
	OMX_EVENTTYPE eEvent;
	OMX_U32 nData1;
	OMX_U32 nData2;
	OMX_PTR pEventData;
};

struct OMX_CLOCK_T {
	struct OMX_COMPONENT_T *clockComponent;
};

uint32_t omx_error;


int omxInit();
int omxDeInit();
struct OMX_COMPONENT_T *omxCreateComponent(char *componentName, OMX_INDEXTYPE paramIndex);
void omxDestroyComponent(struct OMX_COMPONENT_T *component);
struct OMX_CLOCK_T *omxCreateClock(int hasVideo, int hasAudio, int hasText, int hdmiSync);
void omxDestroyClock(struct OMX_CLOCK_T *clock);
OMX_STATETYPE omxGetState(struct OMX_COMPONENT_T *component);
OMX_ERRORTYPE omxSetStateForComponent(struct OMX_COMPONENT_T *component, OMX_STATETYPE state);
struct OMX_TUNNEL_T *omxCreateTunnel(struct OMX_COMPONENT_T *sourceComponent,unsigned int sourcePort,struct OMX_COMPONENT_T *destComponent,unsigned int destPort);
void omxDestroyTunnel(struct OMX_TUNNEL_T *tunnel);
OMX_ERRORTYPE omxEstablishTunnel(struct OMX_TUNNEL_T *tunnel);
OMX_ERRORTYPE omxDisconnectTunnel(struct OMX_TUNNEL_T *tunnel);
OMX_ERRORTYPE omxEnablePort(struct OMX_COMPONENT_T *component, unsigned int port, int wait);
OMX_ERRORTYPE omxDisablePort(struct OMX_COMPONENT_T *component, unsigned int port, int wait);
OMX_ERRORTYPE omxAllocInputBuffers(struct OMX_COMPONENT_T *component, int useBuffers);
void omxFreeInputBuffers(struct OMX_COMPONENT_T *component);
OMX_BUFFERHEADERTYPE *omxGetInputBuffer(struct OMX_COMPONENT_T *component , long timeout);
OMX_ERRORTYPE omxWaitForEvent(struct OMX_COMPONENT_T *component, OMX_EVENTTYPE inEvent, OMX_U32 nData1, uint64_t timeout);
OMX_ERRORTYPE omxFlushTunnel(struct OMX_TUNNEL_T *tunnel);
void omxFlushPort(struct OMX_COMPONENT_T *component, unsigned int port);
void omxChangeStateToLoaded(struct OMX_COMPONENT_T *component);
void omxShowState(struct OMX_COMPONENT_T *component);

#endif
