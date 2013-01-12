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
#include "omxVideo.h"


int omx_video_initialized = 0;

#define omxLockMutex(component) \
	pthread_mutex_lock(&component->eventMutex); \

#define omxUnLockMutex(component) \
	pthread_mutex_unlock(&component->eventMutex); \


int omxInit()

{
	if (omx_video_initialized)  return 0;

	initializeBCM();

	OMX_ERRORTYPE omxError = OMX_Init();
	if(omxError != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error initializing OMX (Error=0x%08x).\n",omxError);
		return OMX_ERROR_OMX_INIT;
	}

	omx_video_initialized = 1;

	return 0;
}

int omxDeInit()
{
	if (!omx_video_initialized)  return 0;

	OMX_ERRORTYPE omxError = OMX_Deinit();
	if(omxError != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error deinitializing OMX (Error=0x%08x).\n",omxError);
		return OMX_ERROR_OMX_DEINIT;
	}

	omx_video_initialized = 0;

	return 0;
}

int omxAddEvent(struct OMX_COMPONENT_T *component, struct OMX_EVENT_T *event)
{
	if (component == NULL) return - 1;

	
	omxLockMutex(component);

	if (component->eventListStart == NULL) {
		logInfo(LOG_OMX_DEBUG, "empty eventlist going to create new list.\n");
		component->eventListStart = createSimpleListItem(event);
		logInfo(LOG_OMX_DEBUG, "After createSimpleListItem.(%s)\n", component->componentName);
		if (component->eventListStart == NULL) {
			logInfo(LOG_OMX, "Not enough memory to add event.\n");
			omxUnLockMutex(component);
			return -1;
		}
		component->eventListEnd = component->eventListStart;
	}
	else {
		logInfo(LOG_OMX_DEBUG, "Adding item to the end of list.\n");
		addObjectToSimpleList(component->eventListEnd, event);
		logInfo(LOG_OMX_DEBUG, "After addObjectToSimpleList.(%s)\n", component->componentName);
		if (component->eventListEnd->next == NULL) {
			logInfo(LOG_OMX, "Not enough memory to add event.\n");
			omxUnLockMutex(component);
			return -1;
		}
		component->eventListEnd = component->eventListEnd->next;
	}

	omxUnLockMutex(component);
	return 0;
}

/******************************
 * omxGetEvent
 *
 * When an event is available in the list it will point event to the event and
 * return 1 to indicate there are more events in list or 0 when end of list is reached.
 * Will return -1 when no events are in list.
 ***************************/

int omxGetEvent(struct OMX_COMPONENT_T *component, struct OMX_EVENT_T **event)
{
	event[0] = NULL;
	if (component == NULL) return -1;

	omxLockMutex(component);

	if (component->eventListStart == NULL) {
		omxUnLockMutex(component);
		return -1;
	}

	event[0] = component->eventListStart->object;
	logInfo(LOG_OMX_DEBUG, "Retreived event from queue nData1=0x%08x, nData2=0x%08x.(%s)\n", (uint32_t)event[0]->nData1, (uint32_t)event[0]->nData2, component->componentName);

	struct SIMPLELISTITEM_T *oldStart = component->eventListStart;
	deleteFromSimpleList(&component->eventListStart, 0);
	freeSimpleListItem(oldStart);

	if (component->eventListStart == NULL) {
		component->eventListEnd = NULL;
		omxUnLockMutex(component);
		return 0;
	}

	omxUnLockMutex(component);
	return 1;
}

/****************************************
 * WaitForCommandComplete
 * Will wait at least timeout microsecond to see if specified command complete event passes by.
 * if event found it will return OMX_ErrorNone
 * When timedout it will return OMX_ErrorTimeout
 * and on error it will return OMX_EventError;
 *******************************************/

OMX_ERRORTYPE omxWaitForCommandComplete(struct OMX_COMPONENT_T *component, OMX_U32 command, OMX_U32 nData2, uint64_t timeout)
{
	if (component == NULL) return OMX_EventError;

	logInfo(LOG_OMX_DEBUG, "Going to wait for CommandComplete event command=0x%08x, nData2=0x%08x with %"  PRId64 " microseconds of timeout.\n", (uint32_t)command, (uint32_t)nData2, timeout);

	uint64_t startTime = nowInMicroseconds();

	struct OMX_EVENT_T *event = NULL;
	const char *eventStr;

	do {
//	while ((nowInMicroseconds() - startTime) < timeout) {
		if (event != NULL) {
			free(event);
		}
		if (omxGetEvent(component, &event) > -1) {
			logInfo(LOG_OMX_DEBUG, "Received event from queue.\n");
			if (event) {
				logInfo(LOG_OMX_DEBUG, "Received valid event from queue.\n");
				switch (event->eEvent) {
					case OMX_EventCmdComplete:
						logInfo(LOG_OMX_DEBUG, "Event OMX_EventCmdComplete.\n");
						if ((event->nData1 == command) && (event->nData2 == nData2)) {
							logInfo(LOG_OMX_DEBUG, "Found event we were waiting for.\n");
							free(event);
							return OMX_ErrorNone;
						}
					break;
					case OMX_EventError:
						if ((OMX_S32)event->nData1 == OMX_ErrorSameState) {
							logInfo(LOG_OMX_DEBUG, "Found event we were waiting for but OMX_ErrorSameState.\n");
							free(event);
							return OMX_ErrorNone;
						}
						free(event);
						logInfo(LOG_OMX_DEBUG, "Found Error event quiting wait.\n");
						return OMX_EventError;
					break;						
					default:

						switch (event->eEvent) {
							case OMX_EventCmdComplete: eventStr = "OMX_EventCmdComplete"; break;	
							case OMX_EventError: eventStr = "OMX_EventError"; break;	
							case OMX_EventMark: eventStr = "OMX_EventMark"; break;	
							case OMX_EventPortSettingsChanged: eventStr = "OMX_EventPortSettingsChanged"; break;	
							case OMX_EventBufferFlag: eventStr = "OMX_EventBufferFlag"; break;	
							case OMX_EventResourcesAcquired: eventStr = "OMX_EventResourcesAcquired"; break;	
							case OMX_EventComponentResumed: eventStr = "OMX_EventComponentResumed"; break;	
							case OMX_EventDynamicResourcesAvailable: eventStr = "OMX_EventDynamicResourcesAvailable"; break;	
							case OMX_EventPortFormatDetected: eventStr = "OMX_EventPortFormatDetected"; break;	
							case OMX_EventKhronosExtensions: eventStr = "OMX_EventKhronosExtensions"; break;	
							case OMX_EventVendorStartUnused: eventStr = "OMX_EventVendorStartUnused"; break;	
							case OMX_EventParamOrConfigChanged: eventStr = "OMX_EventParamOrConfigChanged"; break;
							default:	
								eventStr = "Unknown OMX event";
								break;
						}
						logInfo(LOG_OMX_DEBUG, "%s, Event: %s, nData1=0x%08x, nData2=0x%08x.\n", component->componentName, eventStr, (uint32_t)event->nData1, (uint32_t)event->nData2);
						break;
				}
//				free(event);
			}
		}
	} while (((nowInMicroseconds() - startTime) < timeout) || (event != NULL));


	logInfo(LOG_OMX_DEBUG, "Timedout waiting for event.\n");
	return OMX_ErrorTimeout;
}

/****************************************
 * WaitForEvent
 * Will wait at least timeout microsecond to see if specified event passes by.
 * if event found it will return OMX_ErrorNone
 * When timedout it will return OMX_ErrorTimeout
 * and on error it will return OMX_EventError;
 *******************************************/

OMX_ERRORTYPE omxWaitForEvent(struct OMX_COMPONENT_T *component, OMX_EVENTTYPE inEvent, OMX_U32 nData1, uint64_t timeout)
{
	if (component == NULL) return OMX_EventError;

	logInfo(LOG_OMX_DEBUG, "Going to wait for event=0x%08x, nData2=0x%08x with %"  PRId64 " microseconds of timeout.\n", (uint32_t)inEvent, (uint32_t)nData1, timeout);

	uint64_t startTime = nowInMicroseconds();

	struct OMX_EVENT_T *event = NULL;

	do {
//	while ((nowInMicroseconds() - startTime) < timeout) {
		if (event != NULL) {
			free(event);
		}
		if (omxGetEvent(component, &event) > -1) {
			logInfo(LOG_OMX_DEBUG, "Received event from queue.\n");
			if (event) {
				logInfo(LOG_OMX_DEBUG, "Received valid event from queue.\n");
				if ((event->eEvent == inEvent) && (event->nData1 = nData1)) {
					free(event);
					return OMX_ErrorNone;
				}
			}
		}
	} while (((nowInMicroseconds() - startTime) < timeout) || (event != NULL));


	logInfo(LOG_OMX_DEBUG, "Timedout waiting for event.\n");
	return OMX_ErrorTimeout;
}

OMX_ERRORTYPE omxEventHandlerCallback(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_EVENTTYPE eEvent,
  OMX_U32 nData1,
  OMX_U32 nData2,
  OMX_PTR pEventData)
{
	if (pAppData == NULL) {
		return OMX_ErrorNone;
	}

	struct OMX_COMPONENT_T *component = (struct OMX_COMPONENT_T *)pAppData;
	
	const char *eventStr;

	switch (eEvent) {
		case OMX_EventCmdComplete: eventStr = "OMX_EventCmdComplete"; break;	
		case OMX_EventError: eventStr = "OMX_EventError"; break;	
		case OMX_EventMark: eventStr = "OMX_EventMark"; break;	
		case OMX_EventPortSettingsChanged: 
			eventStr = "OMX_EventPortSettingsChanged";
			component->portSettingChanged = 1;
			break;	
		case OMX_EventBufferFlag: eventStr = "OMX_EventBufferFlag"; break;	
		case OMX_EventResourcesAcquired: eventStr = "OMX_EventResourcesAcquired"; break;	
		case OMX_EventComponentResumed: eventStr = "OMX_EventComponentResumed"; break;	
		case OMX_EventDynamicResourcesAvailable: eventStr = "OMX_EventDynamicResourcesAvailable"; break;	
		case OMX_EventPortFormatDetected: eventStr = "OMX_EventPortFormatDetected"; break;	
		case OMX_EventKhronosExtensions: eventStr = "OMX_EventKhronosExtensions"; break;	
		case OMX_EventVendorStartUnused: eventStr = "OMX_EventVendorStartUnused"; break;	
		case OMX_EventParamOrConfigChanged: eventStr = "OMX_EventParamOrConfigChanged"; break;
		default:	
			eventStr = "Unknown OMX event";
			break;
	}

	logInfo(LOG_OMX_DEBUG, "%s Event: %s, nData1=0x%08x, nData2=0x%08x.\n", component->componentName, eventStr, (uint32_t)nData1, (uint32_t)nData2);

	struct OMX_EVENT_T *event = malloc(sizeof(struct OMX_EVENT_T));
	if (event == NULL) {
		logInfo(LOG_OMX, "Not enough memory to create OMX_EVENT_T.\n");
		return OMX_ErrorNone;
	}
	event->eEvent = eEvent;
	event->nData1 = nData1;
	event->nData2 = nData2;
	event->pEventData = pEventData;

	logInfo(LOG_OMX_DEBUG, "Before omxAddEvent.\n");
	if (omxAddEvent(component, event) == 0) {
		logInfo(LOG_OMX_DEBUG, "Added event to queue.\n");
	}
	else {
		logInfo(LOG_OMX, "Error adding event to queue.\n");
	}

	return OMX_ErrorNone;
}

OMX_ERRORTYPE omxEmptyBufferDoneCallback(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer)
{
	struct OMX_COMPONENT_T *component = (struct OMX_COMPONENT_T *)pAppData;

	logInfo(LOG_OMX_DEBUG, "Component %s Putting pBuffer#=%d back on queue.\n", component->componentName, (int)pBuffer->pAppPrivate);

	if (component) {

		pthread_mutex_lock(&component->inputMutex);

		pBuffer->nFilledLen      = 0;
		pBuffer->nOffset         = 0;

		if (component->inputBuffer == NULL) {
			component->inputBuffer = createSimpleListItem(pBuffer);
			component->inputBufferEnd = component->inputBuffer;
		}
		else {
			addObjectToSimpleList(component->inputBufferEnd, pBuffer);
			component->inputBufferEnd = component->inputBufferEnd->next;
		}

		pthread_cond_broadcast(&component->inputBufferCond);

		pthread_mutex_unlock(&component->inputMutex);
	}
	
	return OMX_ErrorNone;

}

OMX_ERRORTYPE omxFillBufferDoneCallback(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer)
{
	logInfo(LOG_OMX_DEBUG, "We are here omxFillBufferDoneCallback.\n");

	struct OMX_COMPONENT_T *component = (struct OMX_COMPONENT_T *)pAppData;

	if (component) {

		pthread_mutex_lock(&component->inputMutex);

		pBuffer->nFilledLen      = 0;
		pBuffer->nOffset         = 0;

		if (component->inputBuffer == NULL) {
			component->inputBuffer = createSimpleListItem(pBuffer);
			component->inputBufferEnd = component->inputBuffer;
		}
		else {
			addObjectToSimpleList(component->inputBufferEnd, pBuffer);
			component->inputBufferEnd = component->inputBufferEnd->next;
		}

		pthread_cond_broadcast(&component->inputBufferCond);

		pthread_mutex_unlock(&component->inputMutex);
	}

	return OMX_ErrorNone;

}

struct OMX_COMPONENT_T *omxCreateComponent(char *componentName, OMX_INDEXTYPE paramIndex)
{
	OMX_ERRORTYPE omxErr;

	struct OMX_COMPONENT_T *newComponent = malloc(sizeof(struct OMX_COMPONENT_T));
	if (newComponent == NULL) {
		logInfo(LOG_OMX, "Error creating component '%s'. Not enough memory available.\n", componentName);
		omx_error = OMX_ERROR_MALLOC;
		return NULL;
	}

	if (pthread_mutex_init(&newComponent->eventMutex, NULL) != 0) {
		logInfo(LOG_OMX, "Error initializing event mutex for component '%s'.\n", componentName);
		omx_error = OMX_ERROR_INIT_EVENT_MUTEX;
		free(newComponent);
		return NULL;
	}

	if (pthread_mutex_init(&newComponent->inputMutex, NULL) != 0) {
		logInfo(LOG_OMX, "Error initializing input mutex for component '%s'.\n", componentName);
		omx_error = OMX_ERROR_INIT_EVENT_MUTEX;
		free(newComponent);
		return NULL;
	}

	if (pthread_cond_init(&newComponent->inputBufferCond, NULL)) {
		logInfo(LOG_OMX, "Error initializing inputBuffer condition for component '%s'.\n", componentName);
		omx_error = OMX_ERROR_INIT_EVENT_MUTEX;
		free(newComponent);
		return NULL;
	}

	newComponent->eventListStart = NULL;
	newComponent->eventListEnd = NULL;
	newComponent->componentName = componentName;
	newComponent->inputBuffer = NULL;
	newComponent->portSettingChanged = 0;

	newComponent->callbacks.EventHandler    = &omxEventHandlerCallback;
	newComponent->callbacks.EmptyBufferDone = &omxEmptyBufferDoneCallback;
	newComponent->callbacks.FillBufferDone  = &omxFillBufferDoneCallback;

	omxErr = OMX_GetHandle(&newComponent->handle, componentName, newComponent, &newComponent->callbacks);
	if (omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error creating component '%s' (Error=0x%08x).\n", componentName, omxErr);
		omx_error = OMX_ERROR_OMX_GETHANDLE;
		free(newComponent);
		return NULL;
	}

	OMX_PORT_PARAM_TYPE port_param;
	OMX_INIT_STRUCTURE(port_param);

	omxErr = OMX_GetParameter(newComponent->handle, paramIndex, &port_param);
	if (omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error getting port parameters for component '%s' (Error=0x%08x).\n", componentName, omxErr);
		omx_error = OMX_ERROR_OMX_GETPARAMETER_PORT;
		OMX_FreeHandle(newComponent->handle);
		free(newComponent);
		return NULL;
	}

	logInfo(LOG_OMX_DEBUG, "We received port info for component '%s'. There are %d ports on this component starting from port# %d.\n", componentName, (uint32_t)port_param.nPorts, (uint32_t)port_param.nStartPortNumber);

	logInfo(LOG_OMX_DEBUG, "Going to disable ports.\n");

	int i;
	int result;
	for (i = port_param.nStartPortNumber; i < (port_param.nStartPortNumber+port_param.nPorts); i++) {

		OMX_PARAM_PORTDEFINITIONTYPE portFormat;
		OMX_INIT_STRUCTURE(portFormat);
		portFormat.nPortIndex = i;

		omxErr = OMX_GetParameter(newComponent->handle, OMX_IndexParamPortDefinition, &portFormat);
		if(omxErr == OMX_ErrorNone) {
			if(portFormat.bEnabled == OMX_FALSE)
				continue;
		}

		omxErr = OMX_SendCommand(newComponent->handle, OMX_CommandPortDisable, i, NULL);
		if(omxErr != OMX_ErrorNone) {
			logInfo(LOG_OMX, "Error disabling port %d for component '%s' (Error=0x%08x).\n", i, componentName, omxErr);
		}
		
		result = omxWaitForCommandComplete(newComponent, OMX_CommandPortDisable, i, 1000);
		if (result == OMX_ErrorTimeout) {
			logInfo(LOG_OMX, "Waiting for command complete timedout for component '%s' and port %d.\n", componentName, i);
		}
		else {
			if (result != OMX_ErrorNone) {
				logInfo(LOG_OMX, "Error Waiting for command complete for component '%s' and port %d.\n", componentName, i);
			}
			else {
				logInfo(LOG_OMX_DEBUG, "Received valid command complete disabling port %d for component '%s'.\n", i, componentName);
			}
		}
	}

	newComponent->inputPort = port_param.nStartPortNumber;
	newComponent->outputPort = port_param.nStartPortNumber + 1;

	if (strcmp(componentName, "OMX.broadcom.audio_mixer") == 0) {
		newComponent->inputPort  = port_param.nStartPortNumber + 1;
		newComponent->outputPort = port_param.nStartPortNumber;
	}

	if (newComponent->outputPort > port_param.nStartPortNumber+port_param.nPorts-1)
		newComponent->outputPort = port_param.nStartPortNumber+port_param.nPorts-1;
	
	return newComponent;
}

OMX_STATETYPE omxGetState(struct OMX_COMPONENT_T *component)
{
	omxLockMutex(component);

	OMX_STATETYPE state;

	if(component->handle) {
		OMX_GetState(component->handle, &state);
		omxUnLockMutex(component);
		return state;
	}

	omxUnLockMutex(component);

	return (OMX_STATETYPE)0;
}

OMX_ERRORTYPE omxSetStateForComponent(struct OMX_COMPONENT_T *component, OMX_STATETYPE state)
{

	OMX_ERRORTYPE omx_err = OMX_ErrorNone;
	OMX_STATETYPE state_actual = OMX_StateMax;

	if(!component->handle) {
		return OMX_ErrorUndefined;
	}

	state_actual = omxGetState(component);
	if(state == state_actual) {
		return OMX_ErrorNone;
	}


	logInfo(LOG_OMX_DEBUG, "before OMX_SendCommand.\n");
	omx_err = OMX_SendCommand(component->handle, OMX_CommandStateSet, state, 0);
	logInfo(LOG_OMX_DEBUG, "after OMX_SendCommand.\n");
	
	if (omx_err != OMX_ErrorNone) {
		logInfo(LOG_OMX_DEBUG, "omx_err != OMX_ErrorNone.\n");
		if(omx_err == OMX_ErrorSameState) {
			omx_err = OMX_ErrorNone;
		}
		else {
			logInfo(LOG_OMX, "%s failed with omx_err(0x%x)\n", component->componentName, omx_err);
		}
	}
	else {
		logInfo(LOG_OMX_DEBUG, "omx_err == OMX_ErrorNone.\n");
		omx_err = omxWaitForCommandComplete(component, OMX_CommandStateSet, state, 1000);
		logInfo(LOG_OMX_DEBUG, "after omxWaitForCommandComplete.\n");
		if(omx_err == OMX_ErrorSameState) {
			logInfo(LOG_OMX, "%s ignore OMX_ErrorSameState\n", component->componentName);
			return OMX_ErrorNone;
		}
	}


	logInfo(LOG_OMX_DEBUG, "end of omxSetStateForComponent.\n");
	return omx_err;
}

OMX_ERRORTYPE omxEnablePort(struct OMX_COMPONENT_T *component, unsigned int port,  int wait)
{
	OMX_ERRORTYPE omx_err = OMX_ErrorNone;

	OMX_PARAM_PORTDEFINITIONTYPE portFormat;
	OMX_INIT_STRUCTURE(portFormat);
	portFormat.nPortIndex = port;

	logInfo(LOG_OMX_DEBUG, "Port %d on component %s.\n", port, component->componentName);

	omx_err = OMX_GetParameter(component->handle, OMX_IndexParamPortDefinition, &portFormat);
	if(omx_err != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error get port %d status on component %s omx_err(0x%08x).\n", port, component->componentName, (int)omx_err);
	}

	if(portFormat.bEnabled == OMX_FALSE) {
		omx_err = OMX_SendCommand(component->handle, OMX_CommandPortEnable, port, NULL);
		if(omx_err != OMX_ErrorNone) {
			logInfo(LOG_OMX, "Error enable port %d on component %s omx_err(0x%08x).\n",  port, component->componentName, (int)omx_err);
			return omx_err;
		}
	}
	else {
		logInfo(LOG_OMX_DEBUG, "Port %d on component %s already enabled.\n", port, component->componentName);
		if(wait)
			omx_err = omxWaitForCommandComplete(component, OMX_CommandPortEnable, port, 1000);
	}

	return omx_err;
}

OMX_ERRORTYPE omxDisablePort(struct OMX_COMPONENT_T *component, unsigned int port, int wait)
{
	OMX_ERRORTYPE omx_err = OMX_ErrorNone;

	OMX_PARAM_PORTDEFINITIONTYPE portFormat;
	OMX_INIT_STRUCTURE(portFormat);
	portFormat.nPortIndex = port;

	logInfo(LOG_OMX_DEBUG, "Port %d on component %s.\n", port, component->componentName);

	omx_err = OMX_GetParameter(component->handle, OMX_IndexParamPortDefinition, &portFormat);
	if(omx_err != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error get port %d status on component %s omx_err(0x%08x)", port, component->componentName, (int)omx_err);
	}

	if(portFormat.bEnabled == OMX_TRUE) {
		omx_err = OMX_SendCommand(component->handle, OMX_CommandPortDisable, port, NULL);
		if(omx_err != OMX_ErrorNone) {
			logInfo(LOG_OMX, "Error disable port %d on component %s omx_err(0x%08x)",  port, component->componentName, (int)omx_err);
			return omx_err;
		}
	}
	else {
		logInfo(LOG_OMX_DEBUG, "Port %d on component %s already disabled.\n", port, component->componentName);
		if(wait)
			omx_err = omxWaitForCommandComplete(component, OMX_CommandPortDisable, port, 1000);
	}

	return omx_err;
}

struct OMX_TUNNEL_T *omxCreateTunnel(struct OMX_COMPONENT_T *sourceComponent,unsigned int sourcePort,struct OMX_COMPONENT_T *destComponent,unsigned int destPort)
{
	struct OMX_TUNNEL_T *newTunnel = malloc(sizeof(struct OMX_TUNNEL_T));

	if (newTunnel == NULL) {
		logInfo(LOG_OMX, "Error creating tunnel. Not enough memory available.\n");
		omx_error = OMX_ERROR_MALLOC;
		return NULL;
	}

	newTunnel->sourceComponent = sourceComponent;
	newTunnel->sourcePort = sourcePort;
	newTunnel->destComponent = destComponent;
	newTunnel->destPort = destPort;

	return newTunnel;
}

OMX_ERRORTYPE omxEstablishTunnel(struct OMX_TUNNEL_T *tunnel)
{
	if (tunnel == NULL) {
		return -1;
	}

	logInfo(LOG_OMX_DEBUG, "Going to establish tunnel between %s:%d and %s:%d.\n", tunnel->sourceComponent->componentName, tunnel->sourcePort, tunnel->destComponent->componentName, tunnel->destPort);

	if ((tunnel->sourceComponent == NULL) || (tunnel->destComponent == NULL)) {
		logInfo(LOG_OMX_DEBUG, "tunnel->sourceComponent == NULL || tunnel->destComponent == NULL");
		return -2;
	}

	OMX_ERRORTYPE omx_err;
	if(omxGetState(tunnel->sourceComponent) == OMX_StateLoaded) {
	logInfo(LOG_OMX_DEBUG, "omxGetState of source component == OMX_StateLoaded.\n");
		omx_err = omxSetStateForComponent(tunnel->sourceComponent, OMX_StateIdle);
		if(omx_err != OMX_ErrorNone) {
			logInfo(LOG_OMX, "Error setting state to idle %s omx_err(0x%08x)", tunnel->sourceComponent->componentName, (int)omx_err);
			return omx_err;
		}
	}
	logInfo(LOG_OMX_DEBUG, "Set source component to OMX_StateIdle.\n");

	omx_err = omxDisablePort(tunnel->sourceComponent, tunnel->sourcePort, 0);
	if(omx_err != OMX_ErrorNone && omx_err != OMX_ErrorSameState) {
		logInfo(LOG_OMX, "Error disable port %d on component %s omx_err(0x%08x)", tunnel->sourcePort, tunnel->sourceComponent->componentName, (int)omx_err);
	}
	logInfo(LOG_OMX_DEBUG, "Disabled sourceport %d on sourcecomponent %s.\n", tunnel->sourcePort, tunnel->sourceComponent->componentName);

	omx_err = omxDisablePort(tunnel->destComponent, tunnel->destPort, 0);
	if(omx_err != OMX_ErrorNone && omx_err != OMX_ErrorSameState) {
		logInfo(LOG_OMX, "Error disable port %d on component %s omx_err(0x%08x)", tunnel->destPort, tunnel->destComponent->componentName, (int)omx_err);
	}
	logInfo(LOG_OMX_DEBUG, "Disabled destport %d on destcomponent %s.\n", tunnel->destPort, tunnel->destComponent->componentName);

	omxLockMutex(tunnel->sourceComponent);
	omxLockMutex(tunnel->destComponent);

	omx_err = OMX_SetupTunnel(tunnel->sourceComponent->handle, tunnel->sourcePort, tunnel->destComponent->handle, tunnel->destPort);
	if(omx_err != OMX_ErrorNone) {
		logInfo(LOG_OMX, "could not setup tunnel src %s port %d dst %s port %d omx_err(0x%08x)\n", 
	  tunnel->sourceComponent->componentName, tunnel->sourcePort, tunnel->destComponent->componentName, tunnel->destPort, (int)omx_err);
		omxUnLockMutex(tunnel->sourceComponent);
		omxUnLockMutex(tunnel->destComponent);
		return omx_err;
	}
	logInfo(LOG_OMX_DEBUG, "Created tunnel between components.\n");

	omxUnLockMutex(tunnel->sourceComponent);
	omxUnLockMutex(tunnel->destComponent);

	omx_err = omxEnablePort(tunnel->sourceComponent, tunnel->sourcePort, 0);
	if(omx_err != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error enable port %d on component %s omx_err(0x%08x)", tunnel->sourcePort, tunnel->sourceComponent->componentName, (int)omx_err);
		return omx_err;
	}
	logInfo(LOG_OMX_DEBUG, "Enabled sourceport %d on sourcecomponent %s.\n", tunnel->sourcePort, tunnel->sourceComponent->componentName);

	omx_err = omxEnablePort(tunnel->destComponent, tunnel->destPort, 0);
	if(omx_err != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error enable port %d on component %s omx_err(0x%08x)", tunnel->destPort, tunnel->destComponent->componentName, (int)omx_err);
		return omx_err;
	}
	logInfo(LOG_OMX_DEBUG, "Enabled destport %d on destcomponent %s.\n", tunnel->destPort, tunnel->destComponent->componentName);

	if(omxGetState(tunnel->destComponent) == OMX_StateLoaded) {
		omx_err = omxWaitForCommandComplete(tunnel->destComponent, OMX_CommandPortEnable, tunnel->destPort, 50000);
		if(omx_err != OMX_ErrorNone) {
			return omx_err;
		}

		omx_err = omxSetStateForComponent(tunnel->destComponent, OMX_StateIdle);
		if(omx_err != OMX_ErrorNone) {
			logInfo(LOG_OMX, "Error setting state to idle %s omx_err(0x%08x)", tunnel->destComponent->componentName, (int)omx_err);
			return omx_err;
		}
	}
	else {
		omx_err = omxWaitForCommandComplete(tunnel->destComponent, OMX_CommandPortEnable, tunnel->destPort, 50000);
		if(omx_err != OMX_ErrorNone) {
			return omx_err;
		}
	}
	logInfo(LOG_OMX_DEBUG, "destport is enabled destcomponent.\n");

	omx_err = omxWaitForCommandComplete(tunnel->sourceComponent, OMX_CommandPortEnable, tunnel->sourcePort, 1000);
	if(omx_err != OMX_ErrorNone) {
		return omx_err;
	}
	logInfo(LOG_OMX_DEBUG, "sourceport is enabled sourcecomponent.\n");

	logInfo(LOG_OMX_DEBUG, "Established tunnel between %s:%d and %s:%d.\n", tunnel->sourceComponent->componentName, tunnel->sourcePort, tunnel->destComponent->componentName, tunnel->destPort);
	return OMX_ErrorNone;
}

struct OMX_CLOCK_T *omxCreateClock(int hasVideo, int hasAudio, int hasText, int hdmiSync)
{
	struct OMX_CLOCK_T *newClock = malloc(sizeof(struct OMX_CLOCK_T));

	if (newClock == NULL) {
		logInfo(LOG_OMX, "Error creating clock. Not enough memory available.\n");
		omx_error = OMX_ERROR_MALLOC;
		return NULL;
	}

	newClock->clockComponent = omxCreateComponent("OMX.broadcom.clock" , OMX_IndexParamOtherInit);
	if (newClock->clockComponent == NULL) {
		logInfo(LOG_OMX, "Error creating OMX clock component. (Error=%d).\n", omx_error);
		goto theErrorEnd;
	}
	logInfo(LOG_OMX_DEBUG, "Created OMX clock component.\n");

	OMX_TIME_CONFIG_CLOCKSTATETYPE clock;
	OMX_INIT_STRUCTURE(clock);

	clock.eState = OMX_TIME_ClockStateWaitingForStartTime;

	if (hasAudio == 1) {
		clock.nWaitMask |= OMX_CLOCKPORT0;
	}
	if (hasVideo == 1) {
		clock.nWaitMask |= OMX_CLOCKPORT1;
	}
	if (hasText == 1) {
		clock.nWaitMask |= OMX_CLOCKPORT2;
	}

	OMX_ERRORTYPE omxErr;

	omxErr = OMX_SetConfig(newClock->clockComponent->handle, OMX_IndexConfigTimeClockState, &clock);
	if (omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error setting clockport config for clock. (Error=%d).\n", omxErr);
		goto theErrorEnd;
	}
	logInfo(LOG_OMX_DEBUG, "Set clock ports.\n");

	OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE refClock;
	OMX_INIT_STRUCTURE(refClock);

	if(hasAudio == 1)
		refClock.eClock = OMX_TIME_RefClockAudio;
	else
		refClock.eClock = OMX_TIME_RefClockVideo;

	omxErr = OMX_SetConfig(newClock->clockComponent->handle, OMX_IndexConfigTimeActiveRefClock, &refClock);
	if (omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error setting referenceclock config for clock. (Error=%d).\n", omxErr);
		goto theErrorEnd;
	}
	logInfo(LOG_OMX_DEBUG, "Set the clock to reference audio or video.\n");

	if (hdmiSync) {
		OMX_CONFIG_LATENCYTARGETTYPE latencyTarget;
		OMX_INIT_STRUCTURE(latencyTarget);

		latencyTarget.nPortIndex = OMX_ALL;
		latencyTarget.bEnabled = OMX_TRUE;
		latencyTarget.nFilter = 10;
		latencyTarget.nTarget = 0;
		latencyTarget.nShift = 3;
		latencyTarget.nSpeedFactor = -200;
		latencyTarget.nInterFactor = 100;
		latencyTarget.nAdjCap = 100;

		omxErr = OMX_SetConfig(newClock->clockComponent->handle, OMX_IndexConfigLatencyTarget, &latencyTarget);
		if (omxErr != OMX_ErrorNone) {
			logInfo(LOG_OMX, "Error setting hdmiSync config for clock. (Error=%d).\n", omxErr);
			goto theErrorEnd;
		}
		logInfo(LOG_OMX_DEBUG, "Set the clock to sync to hdmi.\n");
	}

	logInfo(LOG_OMX_DEBUG, "Created the clock.\n");

	return newClock;

theErrorEnd:
	logInfo(LOG_OMX_DEBUG, "Error creating OMX clock component.\n");
	free(newClock);
	return NULL;
}

OMX_ERRORTYPE omxSetVideoCompressionFormatAndFrameRate(struct OMX_COMPONENT_T *component, OMX_IMAGE_CODINGTYPE compressionFormat,OMX_U32 framerate)
{
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

	omxLockMutex(component);

	OMX_ERRORTYPE omxErr;
	omxErr = OMX_SetParameter(component->handle, OMX_IndexParamVideoPortFormat, &formatType);

	omxUnLockMutex(component);

	return omxErr;
}

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

OMX_ERRORTYPE omxSetFrameSize(struct OMX_COMPONENT_T *component, unsigned int width, unsigned int height)
{
	OMX_PARAM_PORTDEFINITIONTYPE portParam;
	OMX_INIT_STRUCTURE(portParam);
	portParam.nPortIndex = component->inputPort;

	OMX_ERRORTYPE omxErr;
	omxErr = OMX_GetParameter(component->handle, OMX_IndexParamPortDefinition, &portParam);
	if(omxErr != OMX_ErrorNone)
	{
		logInfo(LOG_OMX, "error OMX_IndexParamPortDefinition omxErr(0x%08x)\n", omxErr);
		return omxErr;
	}

	portParam.nPortIndex = component->inputPort;
	portParam.nBufferCountActual = OMX_VIDEO_BUFFERS;

	portParam.format.video.nFrameWidth  = width;
	portParam.format.video.nFrameHeight = height;

	omxErr = OMX_SetParameter(component->handle, OMX_IndexParamPortDefinition, &portParam);

	return omxErr;
}

OMX_ERRORTYPE omxStartWithValidFrame(struct OMX_COMPONENT_T *component, int startWithValidFrame)
{
	OMX_PARAM_BRCMVIDEODECODEERRORCONCEALMENTTYPE concanParam;
	OMX_INIT_STRUCTURE(concanParam);
	concanParam.bStartWithValidFrame = startWithValidFrame ? OMX_TRUE : OMX_FALSE;

	OMX_ERRORTYPE omxErr;
	omxErr = OMX_SetParameter(component->handle, OMX_IndexParamBrcmVideoDecodeErrorConcealment, &concanParam);

	return omxErr;
}

OMX_ERRORTYPE omxAllocInputBuffers(struct OMX_COMPONENT_T *component, int useBuffers)
{
	OMX_ERRORTYPE omxErr = OMX_ErrorNone;

	component->inputUseBuffers = useBuffers; 

	if(!component->handle)
		return OMX_ErrorUndefined;

	OMX_PARAM_PORTDEFINITIONTYPE portFormat;
	OMX_INIT_STRUCTURE(portFormat);
	portFormat.nPortIndex = component->inputPort;

	omxErr = OMX_GetParameter(component->handle, OMX_IndexParamPortDefinition, &portFormat);
	if(omxErr != OMX_ErrorNone)
		return omxErr;

	if(omxGetState(component) != OMX_StateIdle) {
		if(omxGetState(component) != OMX_StateLoaded)
			omxSetStateForComponent(component, OMX_StateLoaded);

		omxSetStateForComponent(component, OMX_StateIdle);
	}

	omxErr = omxEnablePort(component, component->inputPort, 0);
	if(omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX_DEBUG, "Component(%s) - omxEnablePort failed. (error:0x%08x)\n", component->componentName,omxErr);
		return omxErr;
	}

	component->inputAlignment     = portFormat.nBufferAlignment;
	component->inputBufferCount  = portFormat.nBufferCountActual;
	component->inputBufferSize   = portFormat.nBufferSize;

	logInfo(LOG_OMX_DEBUG, "Component(%s) - port(%d), nBufferCountMin(%lu), nBufferCountActual(%lu), nBufferSize(%lu), nBufferAlignmen(%lu)\n",
	    component->componentName, component->inputPort, portFormat.nBufferCountMin,
	    portFormat.nBufferCountActual, portFormat.nBufferSize, portFormat.nBufferAlignment);

	size_t i;
	for (i = 0; i < portFormat.nBufferCountActual; i++) {

		logInfo(LOG_OMX_DEBUG, "Component(%s) - creating buffer %d.\n", component->componentName, i);

		OMX_BUFFERHEADERTYPE *buffer = NULL;
		OMX_U8* data = NULL;

		if(component->inputUseBuffers) {
			posix_memalign((void *)&data, component->inputAlignment, portFormat.nBufferSize); 
			//data = (OMX_U8*)_aligned_malloc(portFormat.nBufferSize, component->inputAlignment);
			omxErr = OMX_UseBuffer(component->handle, &buffer, component->inputPort, NULL, portFormat.nBufferSize, data);
		}
		else {
			omxErr = OMX_AllocateBuffer(component->handle, &buffer, component->inputPort, NULL, portFormat.nBufferSize);
		}

		if(omxErr != OMX_ErrorNone) {
			logInfo(LOG_OMX, "Component(%s) - OMX_UseBuffer/OMX_AllocateBuffer failed with omxErr(0x%x)\n",
				component->componentName, omxErr);

			if(component->inputUseBuffers && data)
				free(data);

			return omxErr;
		}
		buffer->nInputPortIndex = component->inputPort;
		buffer->nFilledLen      = 0;
		buffer->nOffset         = 0;
		buffer->pAppPrivate     = (void*)i; 

		if (component->inputBuffer == NULL) {
			component->inputBuffer = createSimpleListItem(buffer);
			component->inputBufferEnd = component->inputBuffer;
		}
		else {
			addObjectToSimpleList(component->inputBufferEnd, buffer);
			component->inputBufferEnd = component->inputBufferEnd->next;
		}
	}

	omxErr = omxWaitForCommandComplete(component, OMX_CommandPortEnable, component->inputPort, 10000); 
	if(omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Component(%s) - timeout while waiting for completion of port enable command. omxErr(0x%x)\n",
			component->componentName, omxErr);
		return omxErr;
	}

	component->flushInput = 0;

	return omxErr;
}

static void add_timespecs(struct timespec *time, long millisecs)
{
	time->tv_sec  += millisecs / 1000;
	time->tv_nsec += (millisecs % 1000) * 1000000;
	if (time->tv_nsec > 1000000000) {
		time->tv_sec  += 1;
		time->tv_nsec -= 1000000000;
	}
}

OMX_BUFFERHEADERTYPE *omxGetInputBuffer(struct OMX_COMPONENT_T *component , long timeout)
{
	pthread_mutex_lock(&component->inputMutex);

	if (component->inputBuffer == NULL) {

		struct timespec endtime;
		clock_gettime(CLOCK_REALTIME, &endtime);
		add_timespecs(&endtime, timeout);

		int retcode = pthread_cond_timedwait(&component->inputBufferCond, &component->inputMutex, &endtime);
		if (retcode != 0) {
			logInfo(LOG_OMX_DEBUG, "%s pthread_cond_timedwait timeout\n", component->componentName);
			pthread_mutex_unlock(&component->inputMutex);
			return NULL;
		}


	}

	OMX_BUFFERHEADERTYPE *result = component->inputBuffer->object;
	struct SIMPLELISTITEM_T *oldItem = component->inputBuffer;
	component->inputBuffer = component->inputBuffer->next;
	if (component->inputBuffer == NULL) {
		component->inputBufferEnd = NULL;
	}

	pthread_mutex_unlock(&component->inputMutex);
	
	if (oldItem != NULL) {
		freeSimpleListItem(oldItem);
	}

	return result;
}

#endif
