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

int omxInitialized = 0;

int omxInit()

{
	if (omxInitialized)  return 0;

	initializeBCM();

	OMX_ERRORTYPE omxError = OMX_Init();
	if(omxError != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error initializing OMX (Error=0x%08x).\n",omxError);
		return OMX_ERROR_OMX_INIT;
	}

	omxInitialized = 1;

	return 0;
}

int omxDeInit()
{
	if (!omxInitialized)  return 0;

	OMX_ERRORTYPE omxError = OMX_Deinit();
	if(omxError != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error deinitializing OMX (Error=0x%08x).\n",omxError);
		return OMX_ERROR_OMX_DEINIT;
	}

	omxInitialized = 0;

	return 0;
}

int omxAddEvent(struct OMX_COMPONENT_T *component, struct OMX_EVENT_T *event)
{
	if (component == NULL) return - 1;

	
	pthread_mutex_lock(&component->eventMutex);

	if (component->eventListStart == NULL) {
		logInfo(LOG_OMX_DEBUG, "empty eventlist going to create new list.\n");
		component->eventListStart = createSimpleListItem(event);
		logInfo(LOG_OMX_DEBUG, "After createSimpleListItem.(%s)\n", component->componentName);
		if (component->eventListStart == NULL) {
			logInfo(LOG_OMX, "Not enough memory to add event.\n");
			pthread_mutex_unlock(&component->eventMutex);
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
			pthread_mutex_unlock(&component->eventMutex);
			return -1;
		}
		component->eventListEnd = component->eventListEnd->next;
	}

	pthread_mutex_unlock(&component->eventMutex);
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

	pthread_mutex_lock(&component->eventMutex);

	if (component->eventListStart == NULL) {
		pthread_mutex_unlock(&component->eventMutex);
		return -1;
	}

	event[0] = component->eventListStart->object;
	logInfo(LOG_OMX_DEBUG, "Retreived event from queue nData1=0x%08x, nData2=0x%08x.(%s)\n", (uint32_t)event[0]->nData1, (uint32_t)event[0]->nData2, component->componentName);

	struct SIMPLELISTITEM_T *oldStart = component->eventListStart;
	deleteFromSimpleList(&component->eventListStart, 0);
	freeSimpleListItem(oldStart);

	if (component->eventListStart == NULL) {
		component->eventListEnd = NULL;
		pthread_mutex_unlock(&component->eventMutex);
		return 0;
	}

	pthread_mutex_unlock(&component->eventMutex);
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
		case OMX_EventCmdComplete: 
			switch(nData1) {
			case OMX_CommandStateSet: eventStr = "OMX_EventCmdComplete: cmd=OMX_CommandStateSet"; break;
			case OMX_CommandFlush: eventStr = "OMX_EventCmdComplete: cmd=OMX_CommandFlush"; break;
			case OMX_CommandPortDisable: eventStr = "OMX_EventCmdComplete: cmd=OMX_CommandPortDisable"; break;
			case OMX_CommandPortEnable: eventStr = "OMX_EventCmdComplete: cmd=OMX_CommandPortEnable"; break;
			case OMX_CommandMarkBuffer: eventStr = "OMX_EventCmdComplete: cmd=OMX_CommandMarkBuffer"; break;
			default:
				eventStr = "OMX_EventCmdComplete: cmd=??";
			} 
			break;	
		case OMX_EventError: eventStr = "OMX_EventError"; 
				omxShowState(component);
				break;	
		case OMX_EventMark: eventStr = "OMX_EventMark"; break;	
		case OMX_EventPortSettingsChanged: 
			eventStr = "OMX_EventPortSettingsChanged";
			component->portSettingChanged = 1;
			logInfo(LOG_OMX, "%s: component->portSettingChanged = 1.\n", component->componentName);
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

	logInfo(LOG_OMX, "%s Event: %s, nData1=0x%08x, nData2=0x%08x.\n", component->componentName, eventStr, (uint32_t)nData1, (uint32_t)nData2);

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

		if (component->inputBufferHdr == NULL) {
			component->inputBufferHdr = createSimpleListItem(pBuffer);
			component->inputBufferHdrEnd = component->inputBufferHdr;
		}
		else {
			addObjectToSimpleList(component->inputBufferHdrEnd, pBuffer);
			component->inputBufferHdrEnd = component->inputBufferHdrEnd->next;
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

		if (component->inputBufferHdr == NULL) {
			component->inputBufferHdr = createSimpleListItem(pBuffer);
			component->inputBufferHdrEnd = component->inputBufferHdr;
		}
		else {
			addObjectToSimpleList(component->inputBufferHdrEnd, pBuffer);
			component->inputBufferHdrEnd = component->inputBufferHdrEnd->next;
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

	if (pthread_mutex_init(&newComponent->componentMutex, NULL) != 0) {
		logInfo(LOG_OMX, "Error initializing component mutex for component '%s'.\n", componentName);
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
	newComponent->inputBufferEnd = NULL;
	newComponent->inputBufferHdr = NULL;
	newComponent->inputBufferHdrEnd = NULL;
	newComponent->portSettingChanged = 0;
	newComponent->isClock = 0;
	newComponent->hasBuffers = 0;
	newComponent->changingStateToLoadedFromIdle = 0;

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
		
		result = omxWaitForCommandComplete(newComponent, OMX_CommandPortDisable, i, 5000);
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
	newComponent->clockPort = -1;

	if (strcmp(componentName, "OMX.broadcom.audio_mixer") == 0) {
		newComponent->inputPort  = port_param.nStartPortNumber + 2;
		newComponent->outputPort = port_param.nStartPortNumber + 1;
		newComponent->clockPort = port_param.nStartPortNumber;
	}

	if (strcmp(componentName, "OMX.broadcom.clock") == 0) {
		logInfo(LOG_OMX, "OMX.broadcom.clock port_param.nStartPortNumber=%d.\n", port_param.nStartPortNumber);
		newComponent->clockPort = port_param.nStartPortNumber;
		newComponent->inputPort = -1;
		newComponent->outputPort = -1;
	}

	if (strcmp(componentName, "OMX.broadcom.video_scheduler") == 0) {
		logInfo(LOG_OMX, "OMX.broadcom.video_scheduler port_param.nStartPortNumber=%d.\n", port_param.nStartPortNumber);
		newComponent->clockPort = port_param.nStartPortNumber + 2;
	}

	if (strcmp(componentName, "OMX.broadcom.video_render") == 0) {
		newComponent->outputPort = -1;
	}

	if (strcmp(componentName, "OMX.broadcom.audio_render") == 0) {
		newComponent->clockPort = port_param.nStartPortNumber + 1;
		newComponent->outputPort = -1;
	}

	if (newComponent->outputPort > port_param.nStartPortNumber+port_param.nPorts-1)
		newComponent->outputPort = port_param.nStartPortNumber+port_param.nPorts-1;
	
	return newComponent;
}

OMX_STATETYPE omxGetState(struct OMX_COMPONENT_T *component)
{
	pthread_mutex_lock(&component->componentMutex);

	OMX_STATETYPE state;

	if(component->handle) {
		OMX_GetState(component->handle, &state);
		pthread_mutex_unlock(&component->componentMutex);
		return state;
	}

	pthread_mutex_unlock(&component->componentMutex);

	return (OMX_STATETYPE)0;
}

OMX_ERRORTYPE omxSetStateForComponent(struct OMX_COMPONENT_T *component, OMX_STATETYPE state, uint64_t timeout)
{

	OMX_ERRORTYPE omx_err = OMX_ErrorNone;
	OMX_STATETYPE state_actual = OMX_StateMax;

	if (!component) {
		return OMX_ErrorNone;
	}

	if(!component->handle) {
		return OMX_ErrorUndefined;
	}

	state_actual = omxGetState(component);
	if(state == state_actual) {
		return OMX_ErrorNone;
	}


	logInfo(LOG_OMX_DEBUG, "before OMX_SendCommand (component=%s).\n", component->componentName);
	omx_err = OMX_SendCommand(component->handle, OMX_CommandStateSet, state, NULL);
	logInfo(LOG_OMX_DEBUG, "after OMX_SendCommand (component=%s).\n", component->componentName);
	
	if (omx_err != OMX_ErrorNone) {
		logInfo(LOG_OMX_DEBUG, "omx_err != OMX_ErrorNone (component=%s).\n", component->componentName);
		if(omx_err == OMX_ErrorSameState) {
			omx_err = OMX_ErrorNone;
		}
		else {
			logInfo(LOG_OMX, "%s failed with omx_err(0x%x)\n", component->componentName, omx_err);
		}
	}
	else {
		logInfo(LOG_OMX_DEBUG, "omx_err == OMX_ErrorNone (component=%s).\n", component->componentName);
		omx_err = omxWaitForCommandComplete(component, OMX_CommandStateSet, state, timeout);
		logInfo(LOG_OMX_DEBUG, "after omxWaitForCommandComplete (component=%s).\n", component->componentName);
		if (omx_err == OMX_ErrorTimeout) {
			logInfo(LOG_OMX, "Waiting for command complete timedout for component '%s' OMX_CommandStateSet.\n", component->componentName);
		}
		else {
			if (omx_err != OMX_ErrorNone) {
				logInfo(LOG_OMX, "Error Waiting for command complete for component '%s' OMX_CommandStateSet.\n", component->componentName);
			}
			else {
				logInfo(LOG_OMX_DEBUG, "Received valid command complete OMX_CommandStateSet for component '%s'.\n", component->componentName);
			}
		}

/*		if(omx_err != OMX_ErrorSameState) {
			logInfo(LOG_OMX, "%s ignore OMX_ErrorSameState\n", component->componentName);
			return OMX_ErrorNone;
		}*/
	}


	logInfo(LOG_OMX_DEBUG, "end of omxSetStateForComponent (component=%s).\n", component->componentName);
	return omx_err;
}

OMX_ERRORTYPE omxEnablePort(struct OMX_COMPONENT_T *component, unsigned int port,  int wait)
{
	if (port == -1) return OMX_ErrorNone;

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
		logInfo(LOG_OMX_DEBUG, "Going to enable port %d on component %s.\n", port, component->componentName);
		omx_err = OMX_SendCommand(component->handle, OMX_CommandPortEnable, port, NULL);
		if(omx_err != OMX_ErrorNone) {
			logInfo(LOG_OMX, "Error enable port %d on component %s omx_err(0x%08x).\n",  port, component->componentName, (int)omx_err);
			return omx_err;
		}
	}
	else {
		logInfo(LOG_OMX_DEBUG, "Port %d on component %s already enabled.\n", port, component->componentName);
		if(wait) {
			logInfo(LOG_OMX_DEBUG, "Going to wait for event enabled Port %d on component %s.\n", port, component->componentName);
			omx_err = omxWaitForCommandComplete(component, OMX_CommandPortEnable, port, 1000);
		}
	}

	return omx_err;
}

OMX_ERRORTYPE omxDisablePort(struct OMX_COMPONENT_T *component, unsigned int port, int wait)
{
	if (port == -1) return OMX_ErrorNone;

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
		if(wait) {
			logInfo(LOG_OMX_DEBUG, "Going to wait for event disabled Port %d on component %s.\n", port, component->componentName);
			omx_err = omxWaitForCommandComplete(component, OMX_CommandPortDisable, port, 1000);
		}
	}

	return omx_err;
}

struct OMX_TUNNEL_T *omxCreateTunnel(struct OMX_COMPONENT_T *sourceComponent,unsigned int sourcePort,struct OMX_COMPONENT_T *destComponent,unsigned int destPort)
{
	if ((sourcePort == -1) || (destPort == -1)) return NULL;

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
	newTunnel->tunnelIsUp = 0;

	logInfo(LOG_OMX, "Created tunnel between %s port %d and %s port %d.\n", newTunnel->sourceComponent->componentName, sourcePort, newTunnel->destComponent->componentName, destPort);

	return newTunnel;
}

void omxDestroyTunnel(struct OMX_TUNNEL_T *tunnel)
{
	if (tunnel == NULL) return;

	omxDisconnectTunnel(tunnel);

	free(tunnel);
}

OMX_ERRORTYPE omxEstablishTunnel(struct OMX_TUNNEL_T *tunnel)
{
	if (tunnel == NULL) {
		return -1;
	}

	if (tunnel->tunnelIsUp == 1) return OMX_ErrorNone;

	logInfo(LOG_OMX_DEBUG, "Going to establish tunnel between %s:%d and %s:%d.\n", tunnel->sourceComponent->componentName, tunnel->sourcePort, tunnel->destComponent->componentName, tunnel->destPort);

	if ((tunnel->sourceComponent == NULL) || (tunnel->destComponent == NULL)) {
		logInfo(LOG_OMX_DEBUG, "tunnel->sourceComponent == NULL || tunnel->destComponent == NULL");
		return -2;
	}

	OMX_ERRORTYPE omx_err;
	if(omxGetState(tunnel->sourceComponent) == OMX_StateLoaded) {
	logInfo(LOG_OMX_DEBUG, "omxGetState of source component == OMX_StateLoaded.\n");
		omx_err = omxSetStateForComponent(tunnel->sourceComponent, OMX_StateIdle, 5000);
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

	pthread_mutex_lock(&tunnel->sourceComponent->componentMutex);
	pthread_mutex_lock(&tunnel->destComponent->componentMutex);

	omx_err = OMX_SetupTunnel(tunnel->sourceComponent->handle, tunnel->sourcePort, tunnel->destComponent->handle, tunnel->destPort);
	if(omx_err != OMX_ErrorNone) {
		logInfo(LOG_OMX, "could not setup tunnel src %s port %d dst %s port %d omx_err(0x%08x)\n", 
	  tunnel->sourceComponent->componentName, tunnel->sourcePort, tunnel->destComponent->componentName, tunnel->destPort, (int)omx_err);
		pthread_mutex_unlock(&tunnel->sourceComponent->componentMutex);
		pthread_mutex_unlock(&tunnel->destComponent->componentMutex);
		return omx_err;
	}
	logInfo(LOG_OMX_DEBUG, "Created tunnel between components.\n");

	pthread_mutex_unlock(&tunnel->sourceComponent->componentMutex);
	pthread_mutex_unlock(&tunnel->destComponent->componentMutex);

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
			logInfo(LOG_OMX, "Error enabling destport for %s omx_err(0x%08x)\n", tunnel->destComponent->componentName, (int)omx_err);
			return omx_err;
		}

		omx_err = omxSetStateForComponent(tunnel->destComponent, OMX_StateIdle, 50000);
		if(omx_err != OMX_ErrorNone) {
			logInfo(LOG_OMX, "Error setting state to idle %s omx_err(0x%08x)\n", tunnel->destComponent->componentName, (int)omx_err);
			return omx_err;
		}
	}
	else {
		omx_err = omxWaitForCommandComplete(tunnel->destComponent, OMX_CommandPortEnable, tunnel->destPort, 50000);
		if(omx_err != OMX_ErrorNone) {
			logInfo(LOG_OMX, "Error enabling destport for %s omx_err(0x%08x)\n", tunnel->destComponent->componentName, (int)omx_err);
			return omx_err;
		}
	}
	logInfo(LOG_OMX_DEBUG, "destport is enabled destcomponent.\n");

	omx_err = omxWaitForCommandComplete(tunnel->sourceComponent, OMX_CommandPortEnable, tunnel->sourcePort, 50000);
	if(omx_err != OMX_ErrorNone) {
		return omx_err;
	}
	logInfo(LOG_OMX_DEBUG, "sourceport is enabled sourcecomponent.\n");

	logInfo(LOG_OMX_DEBUG, "Established tunnel between %s:%d and %s:%d.\n", tunnel->sourceComponent->componentName, tunnel->sourcePort, tunnel->destComponent->componentName, tunnel->destPort);

	tunnel->tunnelIsUp = 1;

	return OMX_ErrorNone;
}

void omxShowState(struct OMX_COMPONENT_T *component)
{
	OMX_STATETYPE currentState = omxGetState(component);

	if (currentState == OMX_StateWaitForResources) {
		logInfo(LOG_OMX, "Component state == OMX_StateWaitForResources for component %s.\n", component->componentName);
	}
	if (currentState == OMX_StateExecuting) {
		logInfo(LOG_OMX, "Component state == OMX_StateExecuting for component %s.\n", component->componentName);
	}
	if (currentState == OMX_StateIdle) {
		logInfo(LOG_OMX, "Component state == OMX_StateIdle for component %s.\n", component->componentName);
	}
	if (currentState == OMX_StateLoaded) {
		logInfo(LOG_OMX, "Component state == OMX_StateLoaded for component %s.\n", component->componentName);
	}
	if (currentState == OMX_StatePause) {
		logInfo(LOG_OMX, "Component state == OMX_StatePause, for component %s.\n", component->componentName);
	}
	if (currentState == OMX_StateInvalid) {
		logInfo(LOG_OMX, "Component state == OMX_StateInvalid, for component %s.\n", component->componentName);
	}

}

OMX_ERRORTYPE omxDisconnectTunnel(struct OMX_TUNNEL_T *tunnel)
{
	OMX_ERRORTYPE omxErr = OMX_ErrorNone;

	if (tunnel == NULL) return OMX_ErrorNone;

	if(!tunnel->sourceComponent || !tunnel->destComponent)
		return OMX_ErrorUndefined;

	if (tunnel->tunnelIsUp == 0) return OMX_ErrorNone;

/*	if ((tunnel->sourceComponent->portSettingChanged == 0) && (tunnel->sourceComponent->isClock == 0)) {

		omxErr = omxWaitForEvent(tunnel->sourceComponent, OMX_EventPortSettingsChanged, tunnel->sourcePort, 0);
		if(omxErr != OMX_ErrorNone) {
			logInfo(LOG_OMX, "Component(%s) - timeout while waiting for event OMX_EventPortSettingsChanged of port %d. omxErr(0x%x)\n",
				tunnel->sourceComponent->componentName, tunnel->sourcePort, omxErr);
			return omxErr;
		}
	}
*/
	omxShowState(tunnel->sourceComponent);
	omxShowState(tunnel->destComponent);

	pthread_mutex_lock(&tunnel->sourceComponent->componentMutex);
	pthread_mutex_lock(&tunnel->destComponent->componentMutex);

	omxErr = omxDisablePort(tunnel->sourceComponent, tunnel->sourcePort, 0);
	if ((omxErr != OMX_ErrorNone) && (omxErr != OMX_ErrorSameState)) {
		logInfo(LOG_OMX, "Error omxDisablePort on port %d for component %s. (omxErr = 0x%08x).\n", tunnel->sourcePort, tunnel->sourceComponent->componentName, omxErr);
	}

	omxErr = omxDisablePort(tunnel->destComponent, tunnel->destPort, 0);
	if ((omxErr != OMX_ErrorNone) && (omxErr != OMX_ErrorSameState)) {
		logInfo(LOG_OMX, "Error omxDisablePort on port %d for component %s. (omxErr = 0x%08x).\n", tunnel->destPort, tunnel->destComponent->componentName, omxErr);
	}

	omxErr = OMX_SetupTunnel(tunnel->sourceComponent->handle, tunnel->sourcePort, 0x0, 0);
	if ((omxErr != OMX_ErrorNone) && (omxErr != OMX_ErrorSameState)) {
		logInfo(LOG_OMX, "Error OMX_SetupTunnel on port %d for component %s. (omxErr = 0x%08x).\n", tunnel->sourcePort, tunnel->sourceComponent->componentName, omxErr);
	}

	omxErr = OMX_SetupTunnel(0x0, 0, tunnel->destComponent->handle, tunnel->destPort);
	if ((omxErr != OMX_ErrorNone) && (omxErr != OMX_ErrorSameState)) {
		logInfo(LOG_OMX, "Error OMX_SetupTunnel on port %d for component %s. (omxErr = 0x%08x).\n", tunnel->destPort, tunnel->destComponent->componentName, omxErr);
	}

	tunnel->tunnelIsUp = 0;

	pthread_mutex_unlock(&tunnel->sourceComponent->componentMutex);
	pthread_mutex_unlock(&tunnel->destComponent->componentMutex);

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
		logInfo(LOG_OMX, "Error creating OMX clock component. (Error=0x%08x).\n", omx_error);
		goto theErrorEnd;
	}
	logInfo(LOG_OMX_DEBUG, "Created OMX clock component.\n");
	newClock->clockComponent->isClock = 1;

	OMX_TIME_CONFIG_CLOCKSTATETYPE clock;
	OMX_INIT_STRUCTURE(clock);

	clock.eState = OMX_TIME_ClockStateWaitingForStartTime;

	if (hasAudio == 1) {
		clock.nWaitMask |= OMX_CLOCKPORT0;
	}
	if (hasVideo == 1) {
		clock.nWaitMask |= OMX_CLOCKPORT1;
	}
/*	if (hasText == 1) {
		clock.nWaitMask |= OMX_CLOCKPORT2;
	}
*/
	OMX_ERRORTYPE omxErr;

	omxErr = OMX_SetConfig(newClock->clockComponent->handle, OMX_IndexConfigTimeClockState, &clock);
	if (omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error setting clockport config for clock. (Error=0x%08x).\n", omxErr);
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
		logInfo(LOG_OMX, "Error setting referenceclock config for clock. (Error=0x%08x).\n", omxErr);
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
			logInfo(LOG_OMX, "Error setting hdmiSync config for clock. (Error=0x%08x).\n", omxErr);
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

void omxDestroyClock(struct OMX_CLOCK_T *clock)
{
	if (clock == NULL) return;

	omxDestroyComponent(clock->clockComponent);

	free(clock);
}

OMX_ERRORTYPE omxGetClockCurrentMediaTime(struct OMX_CLOCK_T *clock, int port, OMX_TICKS *outTimeStamp)
{
	OMX_ERRORTYPE omxErr = OMX_ErrorNone;

	OMX_TIME_CONFIG_TIMESTAMPTYPE timeStampConfig;
	OMX_INIT_STRUCTURE(timeStampConfig);
	timeStampConfig.nPortIndex = port;

	omxErr = OMX_GetConfig(clock->clockComponent->handle, OMX_IndexConfigTimeCurrentMediaTime, &timeStampConfig);
	if(omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error on getting OMX_IndexConfigTimeCurrentMediaTime. (omxErr=0x%08x)\n", omxErr);
		return omxErr;
	}

	*outTimeStamp = timeStampConfig.nTimestamp;

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
			omxSetStateForComponent(component, OMX_StateLoaded, 5000);

		omxSetStateForComponent(component, OMX_StateIdle, 5000);
	}

	omxErr = omxEnablePort(component, component->inputPort, 0);
	if(omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX_DEBUG, "Component(%s) - omxEnablePort failed. (error:0x%08x)\n", component->componentName,omxErr);
		return omxErr;
	}

	component->inputAlignment     = portFormat.nBufferAlignment;
	component->inputBufferCount  = portFormat.nBufferCountActual;
	component->inputBufferSize   = portFormat.nBufferSize;

	logInfo(LOG_OMX, "Component(%s) - port(%d), nBufferCountMin(%lu), nBufferCountActual(%lu), nBufferSize(%lu), nBufferAlignmen(%lu)\n",
	    component->componentName, component->inputPort, (long unsigned int)portFormat.nBufferCountMin,
	    (long unsigned int)portFormat.nBufferCountActual, (long unsigned int)portFormat.nBufferSize, (long unsigned int)portFormat.nBufferAlignment);

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

		if (component->inputUseBuffers) {
			if (component->inputBuffer == NULL) {
				component->inputBuffer = createSimpleListItem(data);
				component->inputBufferEnd = component->inputBuffer;
			}
			else {
				addObjectToSimpleList(component->inputBufferEnd, data);
				component->inputBufferEnd = component->inputBufferEnd->next;
			}
		}

		if (component->inputBufferHdr == NULL) {
			component->inputBufferHdr = createSimpleListItem(buffer);
			component->inputBufferHdrEnd = component->inputBufferHdr;
		}
		else {
			addObjectToSimpleList(component->inputBufferHdrEnd, buffer);
			component->inputBufferHdrEnd = component->inputBufferHdrEnd->next;
		}
	}

	component->hasBuffers = 1;

	omxErr = omxWaitForCommandComplete(component, OMX_CommandPortEnable, component->inputPort, 50000); 
	if(omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Component(%s) - timeout while waiting for completion of port enable command. omxErr(0x%x)\n",
			component->componentName, omxErr);
		return omxErr;
	}

	component->flushInput = 0;

	return omxErr;
}

void omxFreeInputBuffers(struct OMX_COMPONENT_T *component)
{
	if (component == NULL) return;

	if (component->inputBufferHdr == NULL) return;

	pthread_mutex_lock(&component->inputMutex);
	pthread_cond_broadcast(&component->inputBufferCond);

	if (component->changingStateToLoadedFromIdle == 0) {
		omxDisablePort(component, component->inputPort, 0);
	}

	// Wait until all buffers have returned by component
	struct timespec interval;
	struct timespec remainingInterval;
	uint64_t counter = 0;
	if (simpleListCount(component->inputBufferHdr) != component->inputBufferCount) {
		logInfo(LOG_OMX, "Component(%s) - Not all buffers (%d of %d) have been returned by component. Waiting for component.\n",component->componentName, simpleListCount(component->inputBufferHdr), component->inputBufferCount);
	}

	while ((simpleListCount(component->inputBufferHdr) != component->inputBufferCount) && (counter < 1000000)) {
		interval.tv_sec = 0;
		interval.tv_nsec = 10000;

		nanosleep(&interval, &remainingInterval);
		counter++;
	}
	if (simpleListCount(component->inputBufferHdr) != component->inputBufferCount) {
		logInfo(LOG_OMX, "Component(%s) - Not all buffers (%d of %d) have been returned by component. Timedout.\n",component->componentName, simpleListCount(component->inputBufferHdr), component->inputBufferCount);
	}

	counter = 0;
	struct SIMPLELISTITEM_T *tmpItem = component->inputBufferHdr;
	while (tmpItem != NULL) {
		OMX_BUFFERHEADERTYPE *bufferHdr = tmpItem->object;

		OMX_FreeBuffer(component->handle, component->inputPort, bufferHdr);
		counter++;

		tmpItem = tmpItem->next;
	}
	freeSimpleList(component->inputBufferHdr);
	component->inputBufferHdr = NULL;
	component->inputBufferHdrEnd = NULL;
	logInfo(LOG_OMX, "Component(%s) - Freed %"  PRId64 " buffers.\n",component->componentName, counter);

	if(component->inputUseBuffers) {
		tmpItem = component->inputBuffer;
		while (tmpItem != NULL) {
			OMX_U8 *buffer = tmpItem->object;

			free(buffer); 

			tmpItem = tmpItem->next;
		}
		freeSimpleList(component->inputBuffer);
		component->inputBuffer = NULL;
		component->inputBufferEnd = NULL;
	}

	component->hasBuffers = 0;

	if (component->changingStateToLoadedFromIdle == 0) {
		omxWaitForCommandComplete(component, OMX_CommandPortDisable, component->inputPort, 2000); 
	}

	component->inputAlignment    = 0;
	component->inputBufferCount  = 0;
	component->inputBufferSize   = 0;

	pthread_mutex_unlock(&component->inputMutex);

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

	if (component->inputBufferHdr == NULL) {

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

	OMX_BUFFERHEADERTYPE *result = component->inputBufferHdr->object;
	struct SIMPLELISTITEM_T *oldItem = component->inputBufferHdr;
	component->inputBufferHdr = component->inputBufferHdr->next;
	if (component->inputBufferHdr == NULL) {
		component->inputBufferHdrEnd = NULL;
	}

	pthread_mutex_unlock(&component->inputMutex);
	
	if (oldItem != NULL) {
		freeSimpleListItem(oldItem);
	}
	logInfo(LOG_OMX_DEBUG, "Got buffer from %s\n", component->componentName);

	return result;
}

OMX_ERRORTYPE omxFlushTunnel(struct OMX_TUNNEL_T *tunnel)
{
	OMX_ERRORTYPE omxErr = OMX_ErrorNone;

	if (tunnel == NULL) return OMX_ErrorNone;

	if(!tunnel->sourceComponent || !tunnel->destComponent)
		return OMX_ErrorUndefined;

	pthread_mutex_lock(&tunnel->sourceComponent->componentMutex);
	pthread_mutex_lock(&tunnel->destComponent->componentMutex);

	omxErr = OMX_SendCommand(tunnel->sourceComponent->handle, OMX_CommandFlush, tunnel->sourcePort, NULL);
	if ((omxErr != OMX_ErrorNone) && (omxErr != OMX_ErrorSameState)) {
		logInfo(LOG_OMX, "Error OMX_CommandFlush on port %d for component %s. (omxErr = 0x%08x).\n", tunnel->sourcePort, tunnel->sourceComponent->componentName, omxErr);
	}

	omxErr = OMX_SendCommand(tunnel->destComponent->handle, OMX_CommandFlush, tunnel->destPort, NULL);
	if ((omxErr != OMX_ErrorNone) && (omxErr != OMX_ErrorSameState)) {
		logInfo(LOG_OMX, "Error OMX_CommandFlush on port %d for component %s. (omxErr = 0x%08x).\n", tunnel->destPort, tunnel->destComponent->componentName, omxErr);
	}

	omxErr = omxWaitForCommandComplete(tunnel->sourceComponent, OMX_CommandFlush, tunnel->sourcePort, 2000);

	omxErr = omxWaitForCommandComplete(tunnel->destComponent, OMX_CommandFlush, tunnel->destPort, 2000);

	pthread_mutex_unlock(&tunnel->sourceComponent->componentMutex);
	pthread_mutex_unlock(&tunnel->destComponent->componentMutex);

	return OMX_ErrorNone;
}

void omxFlushPort(struct OMX_COMPONENT_T *component, unsigned int port)
{
	if ((component == NULL) || (port == -1)) return;

	pthread_mutex_lock(&component->componentMutex);

	OMX_ERRORTYPE omxErr = OMX_ErrorNone;

	omxErr = OMX_SendCommand(component->handle, OMX_CommandFlush, port, NULL);

	if(omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error OMX_CommandFlush on port %d for component %s. (omxErr = 0x%08x).\n", port, component->componentName, omxErr);
	}

	omxErr = omxWaitForCommandComplete(component, OMX_CommandFlush, port, 50000);
	if(omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error omxWaitForCommandComplete event OMX_CommandFlush on port %d for component %s. (omxErr = 0x%08x).\n", port, component->componentName, omxErr);
	}

	pthread_mutex_unlock(&component->componentMutex);
}

void omxChangeStateToLoaded(struct OMX_COMPONENT_T *component, uint8_t freeBuffers)
{
	if (component == NULL) return;

	if (omxGetState(component) == OMX_StateLoaded) {
		logInfo(LOG_OMX, "%s has state OMX_StateLoaded not going to change state.\n", component->componentName);
		return;
	}

	OMX_ERRORTYPE omxErr = OMX_ErrorNone;
	int counter = 0;
	while ((omxGetState(component) != OMX_StateLoaded) && (omxErr == OMX_ErrorNone) && (counter < 5)) {
		if (omxGetState(component) == OMX_StateIdle) {
			logInfo(LOG_OMX, "%s has state OMX_StateIdle going to change to OMX_StateLoaded.\n", component->componentName);
			
			if (component->hasBuffers) {
				omxErr = omxSetStateForComponent(component, OMX_StateLoaded, 1);
				logInfo(LOG_OMX, "%s Going to free buffers so state can change to OMX_StateLoaded.\n", component->componentName);
				component->changingStateToLoadedFromIdle = 1;
				omxFreeInputBuffers(component);
				component->changingStateToLoadedFromIdle = 0;
				logInfo(LOG_OMX, "%s Waiting for event OMX_StateLoaded.\n", component->componentName);
				omxErr = omxWaitForCommandComplete(component, OMX_CommandStateSet, OMX_StateLoaded, 5000000);
				if (omxErr == OMX_ErrorTimeout) {
					logInfo(LOG_OMX, "Waiting for command complete timedout for component '%s' OMX_CommandStateSet to OMX_StateLoaded.\n", component->componentName);
				}
				else {
					if (omxErr != OMX_ErrorNone) {
						logInfo(LOG_OMX, "Error Waiting for command complete for component '%s' OMX_CommandStateSet to OMX_StateLoaded.\n", component->componentName);
					}
					else {
						logInfo(LOG_OMX, "Received valid command complete OMX_CommandStateSet to OMX_StateLoaded for component '%s'.\n", component->componentName);
					}
				}
			}
			else {
				omxErr = omxSetStateForComponent(component, OMX_StateLoaded, 5000000);
				if (omxErr != OMX_ErrorNone) {
					if (omxErr == OMX_ErrorTimeout) {
						logInfo(LOG_OMX, "Error changing state from !OMX_StateLoaded to OMX_StateLoaded for %s. Timedout waiting for state change.\n", component->componentName);
					}
					else {
						logInfo(LOG_OMX, "Error changing state from !OMX_StateLoaded to OMX_StateLoaded for %s. (omxErr = 0x%08x)\n", component->componentName, omxErr);
					}
				}
			}
		}
		else {			
			omxShowState(component);
			logInfo(LOG_OMX, "%s has state !OMX_StateIdle going to change to OMX_StateIdle.\n", component->componentName);
			OMX_ERRORTYPE omxErr = omxSetStateForComponent(component, OMX_StateIdle, 5000000);
			if (omxErr != OMX_ErrorNone) {
				logInfo(LOG_OMX, "Error changing state from !OMX_StateIdle to OMX_StateIdle for %s. (omxErr = 0x%08x)\n", component->componentName, omxErr);
			}
		}
		counter++;
	}

	if (omxGetState(component) != OMX_StateLoaded) {
		omxShowState(component);
		logInfo(LOG_OMX, "Failed changing state from !OMX_StateLoaded to OMX_StateLoaded for %s. (omxErr = 0x%08x)\n", component->componentName, omxErr);
	}

/*	if (omxGetState(component) == OMX_StateExecuting) {
		logInfo(LOG_OMX, "%s has state OMX_StateExecuting going to change to OMX_StatePause.\n", component->componentName);
		OMX_ERRORTYPE omxErr = omxSetStateForComponent(component, OMX_StatePause, 5000000);
		if (omxErr != OMX_ErrorNone) {
			logInfo(LOG_OMX, "Error changing state from OMX_StateExecuting to OMX_StatePause for %s. (omxErr = 0x%08x)\n", component->componentName, omxErr);
		}
	}

	if (omxGetState(component) != OMX_StateIdle) {
		logInfo(LOG_OMX, "%s has state !OMX_StateIdle going to change to OMX_StateIdle.\n", component->componentName);
		OMX_ERRORTYPE omxErr = omxSetStateForComponent(component, OMX_StateIdle, 5000000);
		if (omxErr != OMX_ErrorNone) {
			logInfo(LOG_OMX, "Error changing state from !OMX_StateIdle to OMX_StateIdle for %s. (omxErr = 0x%08x)\n", component->componentName, omxErr);
		}
	}

	if (omxGetState(component) != OMX_StateLoaded) {
		logInfo(LOG_OMX, "%s has state !OMX_StateLoaded going to change to OMX_StateLoaded.\n", component->componentName);
		OMX_ERRORTYPE omxErr;
		if (component->hasBuffers) {
			omxErr = omxSetStateForComponent(component, OMX_StateLoaded, 5000);
		}
		else {
			omxErr = omxSetStateForComponent(component, OMX_StateLoaded, 5000000);
		}
		if (omxErr != OMX_ErrorNone) {
			if (omxErr == OMX_ErrorTimeout) {
				if (component->hasBuffers) {
					logInfo(LOG_OMX, "%s Going to free buffers so state can change to OMX_StateLoaded.\n", component->componentName);
					component->changingStateToLoadedFromIdle = 1;
					omxFreeInputBuffers(component);
					component->changingStateToLoadedFromIdle = 0;
					logInfo(LOG_OMX, "%s Waiting for event OMX_StateLoaded.\n", component->componentName);
					omxErr = omxWaitForCommandComplete(component, OMX_CommandStateSet, OMX_StateLoaded, 5000000);
					if (omxErr == OMX_ErrorTimeout) {
						logInfo(LOG_OMX, "Waiting for command complete timedout for component '%s' OMX_CommandStateSet to OMX_StateLoaded.\n", component->componentName);
					}
					else {
						if (omxErr != OMX_ErrorNone) {
							logInfo(LOG_OMX, "Error Waiting for command complete for component '%s' OMX_CommandStateSet to OMX_StateLoaded.\n", component->componentName);
						}
						else {
							logInfo(LOG_OMX_DEBUG, "Received valid command complete OMX_CommandStateSet to OMX_StateLoaded for component '%s'.\n", component->componentName);
						}
					}
				}
				else {
					logInfo(LOG_OMX, "%s No buffers to free.\n", component->componentName);
				}
			}
			else {
				logInfo(LOG_OMX, "Error changing state from !OMX_StateLoaded to OMX_StateLoaded for %s. (omxErr = 0x%08x)\n", component->componentName, omxErr);
			}
		}
	}
*/
}

void omxDestroyComponent(struct OMX_COMPONENT_T *component)
{
	if (component == NULL) return;


	if (omxGetState(component) == OMX_StateWaitForResources) {
		logInfo(LOG_OMX, "Component state == OMX_StateWaitForResources for component %s.\n", component->componentName);
	}

	if (component->isClock == 0) {
		omxFlushPort(component, component->inputPort);
		omxFlushPort(component, component->outputPort);
	}

/*	if (component->isClock == 0) {
		omxFreeInputBuffers(component);
	}
*/
	omxChangeStateToLoaded(component, 1);

	OMX_ERRORTYPE omxErr = OMX_FreeHandle(component->handle);
	if(omxErr != OMX_ErrorNone) {
		logInfo(LOG_OMX, "Error OMX_FreeHandle for component %s. (omxErr = 0x%08x).\n", component->componentName, omxErr);
	}

	component->handle = NULL;
	component->inputPort = 0;
	component->outputPort = 0;
	component->componentName = "";
	
	pthread_mutex_destroy(&component->eventMutex);
	pthread_mutex_destroy(&component->componentMutex);
	pthread_mutex_destroy(&component->inputMutex);
	pthread_cond_destroy(&component->inputBufferCond);

	free(component);
}

#endif
