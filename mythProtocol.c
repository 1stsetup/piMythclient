#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <pthread.h>
#include "lists.h"
#include "connection.h"
#include "globalFunctions.h"
#include "mythProtocol.h"

#define MAX_HOSTNAME_LEN 125

ssize_t sendCommand(struct CONNECTION_T *connection, char *inCommand)
{
	char command[MAX_COMMAND_LENGTH];
	size_t len = strnlen(inCommand, MAX_COMMAND_LENGTH-COMMAND_LENGTH_PADDING);
	ssize_t sendLen;

	// Clear complete command with spaces (0x20)
	memset(&command[0], 0x20, MAX_COMMAND_LENGTH);

	// Create command to send to myth including command length and padding.
	snprintf(&command[0], MAX_COMMAND_LENGTH, "%-8zd%s", len, inCommand);

	logInfo( LOG_MYTHPROTOCOL_DEBUG,"%d->: %s\n", connection->socket, command); // For debugging.

	pthread_mutex_lock(&connection->readWriteLock);
	
	// Send the command to myth.
	sendLen = send(connection->socket, &command[0], len+COMMAND_LENGTH_PADDING, 0);

	logInfo( LOG_MYTHPROTOCOL_DEBUG,"sendCommand: send: sendLen=%zd\n", sendLen);

	pthread_mutex_unlock(&connection->readWriteLock);

	if (sendLen == -1) {
		logInfo(LOG_MYTHPROTOCOL, "sendCommand: send error");
		return sendLen;
	}
	
	if (sendLen != len+COMMAND_LENGTH_PADDING) {
		logInfo(LOG_MYTHPROTOCOL, "sendCommand: not all dat send");
		return -1;
	}

	return sendLen;
}

ssize_t readResponse(struct CONNECTION_T *connection, char *outResponse, size_t responseLength)
{
	logInfo( LOG_MYTHPROTOCOL_DEBUG,"readResponse: responseLength=%zd.\n", responseLength);

	if (fillConnectionBuffer(connection, responseLength, 0) == -1) {
		return -1;
	}

//	logInfo( LOG_MYTHPROTOCOL_DEBUG,"readResponse: connection.buffer=%s.\n", connection->buffer);
//	logInfo( LOG_MYTHPROTOCOL_DEBUG,"readResponse: Connection details. bufferEnd=%d, bufferPos=%d, bufferLen=%d.\n", connection->bufferEnd, connection->bufferPos, connection->bufferLen);

	// Get length of response. It is in the first 8 bytes.
	char *rlenStr = malloc(8);
	memset(rlenStr, 0, 8);
	readConnectionBuffer(connection, rlenStr, 8);

	int rlen = 0;
	int i = 0;
	char *tmpBuffer = rlenStr;

	while ((i < 8) && (*tmpBuffer != 0x20)) {
		if (rlen != 0) {
			rlen = rlen * 10;
		}
		rlen = rlen + (*tmpBuffer - 0x30);
		i++;
		tmpBuffer++;

		logInfo( LOG_MYTHPROTOCOL_DEBUG,"readResponse: 1. *tmpBuffer=%x.\n", *tmpBuffer);
	}
	free(rlenStr);
	
	logInfo( LOG_MYTHPROTOCOL_DEBUG,"readResponse: 2. rlen=%d, i=%d.\n", rlen, i);

	// Only return the amount requested max.
	if (rlen >= responseLength) {
		rlen = responseLength - 1;
	}

	// Clear complete command with spaces (0x00)
	memset(outResponse, 0x00, rlen+1);
	if (readConnectionBuffer(connection, outResponse, rlen) != rlen) {
		logInfo( LOG_MYTHPROTOCOL,"readResponse: WARNING: Not enough data in connection buffer.\n");
	}

	logInfo( LOG_MYTHPROTOCOL_DEBUG,"%d<-: %-7d %s\n", connection->socket, rlen, outResponse);

	return strlen(outResponse);	
}

int sendCommandAndReadReply(struct MYTH_CONNECTION_T *mythConnection, char *inCommand, char *outResponse, size_t responseLength)
{

	pthread_mutex_lock(&mythConnection->readWriteLock);

	if (sendCommand(mythConnection->connection, inCommand) == -1) {
		logInfo(LOG_MYTHPROTOCOL, "sendCommandAndReadReply: sendCommand");
		pthread_mutex_unlock(&mythConnection->readWriteLock);
		return -1;
	}

        if (readResponse(mythConnection->connection, outResponse, responseLength) == -1) {
                logInfo(LOG_MYTHPROTOCOL, "sendCommandAndReadReply: readResponse");
		pthread_mutex_unlock(&mythConnection->readWriteLock);
                return -2;
        }

	pthread_mutex_unlock(&mythConnection->readWriteLock);

	return 1;
}

int checkResponse(char *response, char *needle)
{
	logInfo( LOG_MYTHPROTOCOL_DEBUG,"checkResponse: response=%s, needle=%s.\n", response, needle);

	int len = strlen(needle);

	if (strncmp(needle, response, len) == 0) {
		logInfo( LOG_MYTHPROTOCOL_DEBUG,"checkResponse: We have a match.\n");

		return 1;
	}

	logInfo( LOG_MYTHPROTOCOL_DEBUG,"checkResponse: No match.\n");

	return 0;
}

struct MYTH_CONNECTION_T *createMythConnection(char *inHostname, uint16_t port, int annType)
{
	struct MYTH_CONNECTION_T *backendConnection = (struct MYTH_CONNECTION_T *)malloc(sizeof(struct MYTH_CONNECTION_T));
	backendConnection->connection = (struct CONNECTION_T *)malloc(sizeof(struct CONNECTION_T));

	logInfo(LOG_MYTHPROTOCOL, "start.\n");

	char	response[RESPONSESIZE];
	char 	command[MAX_COMMAND_LENGTH];
	int 	error = 0;

	if (port == 0) port = 6543;

	backendConnection->connection = createConnection(inHostname, port);
	logInfo(LOG_MYTHPROTOCOL, "createdconnection.\n");
	if (backendConnection->connection == NULL) {
		logInfo(LOG_MYTHPROTOCOL, "createMythConnection: Error: createConnection\n");
		free(backendConnection);
		return NULL;
	}

	backendConnection->hostname = malloc(strlen(inHostname)+1);
	strcpy(backendConnection->hostname, inHostname);
	backendConnection->port = port;
	backendConnection->annType = annType;

	// Create mutexes
	if (pthread_mutex_init(&backendConnection->readWriteLock, NULL) != 0)
	{
		logInfo( LOG_CONNECTION,"mutex init failed\n");
		destroyMythConnection(backendConnection);
		return NULL;
	}

	snprintf(&command[0], MAX_COMMAND_LENGTH, "MYTH_PROTO_VERSION %s %s", CLIENT_PROTOCOL_VERSION, CLIENT_PROTOCOL_TOKEN);

	logInfo(LOG_MYTHPROTOCOL, "Going to sendCommandAndReadReply.\n");
	error = sendCommandAndReadReply(backendConnection, &command[0], &response[0], RESPONSESIZE);
	if (error >= 0) {
		if (checkResponse(&response[0], "ACCEPT[]:[]") == 0) {
			logInfo(LOG_MYTHPROTOCOL, "createMythConnection: MYTH_PROTO_VERSION of backend is not the same as we can handle.\n");
			destroyMythConnection(backendConnection);
			return NULL;
		}
	}
	logInfo(LOG_MYTHPROTOCOL, "done sendCommandAndReadReply.\n");

	if (error >= 0) {
		switch (annType) 
		{
			case ANN_PLAYBACK:
				logInfo(LOG_MYTHPROTOCOL, "Going to mythAnnPlayback.\n");
				error = mythAnnPlayback(backendConnection);
				break;
			case ANN_FILETRANSFER:
				// We do not do anythin. mythAnnFileTransfer needs to be called after creation.
				logInfo(LOG_MYTHPROTOCOL, "Going to mythAnnFileTrasfer.\n");
				break;
			case ANN_MONITOR:
				logInfo(LOG_MYTHPROTOCOL, "Going to mythAnnMonitor.\n");
				error = mythAnnMonitor(backendConnection);
				break;
		}

		if (error < 0) {
			logInfo(LOG_MYTHPROTOCOL, "createMythConnection: mythAnnPlayback error.\n");
			destroyMythConnection(backendConnection);
			return NULL;
		}
	}

	return backendConnection;
}

void destroyMythConnection(struct MYTH_CONNECTION_T *mythConnection)
{
	if (mythConnection == NULL) return;

	if (mythConnection->connection) {
		destroyConnection(mythConnection->connection);
	}

	free(mythConnection->hostname);

	free(mythConnection);
}

int mythAnnPlayback(struct MYTH_CONNECTION_T *mythConnection)
{
	char	response[RESPONSESIZE];
	char 	command[MAX_COMMAND_LENGTH];
	int 	error = 0;

	char *hostname = malloc(MAX_HOSTNAME_LEN+1);

	if (gethostname(hostname, MAX_HOSTNAME_LEN) != 0) {
		free(hostname);
		error = -1;
	}

	if (error >= 0) {
		snprintf(&command[0], MAX_COMMAND_LENGTH, "ANN Playback %s %d", hostname, 0);
		free(hostname);
		error = sendCommandAndReadReply(mythConnection, &command[0], &response[0], RESPONSESIZE);
	}

	if (error >= 0) {
		if (checkResponse(&response[0], "OK") == 0) {
			error = -1;
		}
	}

	return error;
}

int mythAnnMonitor(struct MYTH_CONNECTION_T *mythConnection)
{
	char	response[RESPONSESIZE];
	char 	command[MAX_COMMAND_LENGTH];
	int 	error = 0;

	char *hostname = malloc(MAX_HOSTNAME_LEN+1);

	if (gethostname(hostname, MAX_HOSTNAME_LEN) != 0) {
		free(hostname);
		error = -1;
	}

	if (error >= 0) {
		snprintf(&command[0], MAX_COMMAND_LENGTH, "ANN Monitor %s %d", hostname, 1);
		free(hostname);
		error = sendCommandAndReadReply(mythConnection, &command[0], &response[0], RESPONSESIZE);
	}

	if (error >= 0) {
		if (checkResponse(&response[0], "OK") == 0) {
			error = -1;
		}
	}

	return error;
}

int mythAnnFileTransfer(struct MYTH_CONNECTION_T *mythConnection)
{
        char    response[RESPONSESIZE];
        char    command[MAX_COMMAND_LENGTH];
        int     error = 0;

        char *hostname = malloc(MAX_HOSTNAME_LEN+1);

        if (gethostname(hostname, MAX_HOSTNAME_LEN) != 0) {
                free(hostname);
               error = -1;
        }

        if (error >= 0) {
		char *filename = getStringAtListIndex(mythConnection->backendConnection->currentRecording,10);
		memset(&command[0], 0, MAX_COMMAND_LENGTH);
                snprintf(&command[0], MAX_COMMAND_LENGTH, "ANN FileTransfer %s %d %d %d[]:[]/%s[]:[][]:[][]:[][]:[]", hostname, 0, 1, 10000, filename);
                free(hostname);
                error = sendCommandAndReadReply(mythConnection, &command[0], &response[0], RESPONSESIZE);
        }

        if (error >= 0) {
                if (checkResponse(&response[0], "OK") == 0) {
                        error = -1;
                }
		else {
			//21      OK[]:[]173[]:[]116184
	                struct LISTITEM_T *details = convertStrToList(&response[0], "[]:[]");
			char *socketStr = getStringAtListIndex(details,1);
			if (socketStr != NULL) {
				mythConnection->transferSocket = atoi(socketStr);

		                logInfo( LOG_MYTHPROTOCOL,"mythAnnFileTransfer: transferSocket=%d.\n",mythConnection->transferSocket);
			}
			else {
				error = -1;
				logInfo( LOG_MYTHPROTOCOL,"mythAnnFileTransfer:Error getting transferSocket.\n");
			}
			freeList(details);
 
		}
        }

        return error;
}

int mythFrontendReady(struct MYTH_CONNECTION_T *mythConnection)
{
	char	response[RESPONSESIZE];
	char 	command[MAX_COMMAND_LENGTH];
	int 	error = 0;

	snprintf(&command[0], MAX_COMMAND_LENGTH, "QUERY_RECORDER %d[]:[]FRONTEND_READY", mythConnection->recorderId);
	error = sendCommandAndReadReply(mythConnection, &command[0], &response[0], RESPONSESIZE);
	if (error >= 0) {
		if (checkResponse(&response[0], "OK") == 0) {
			error = -1;
		}
	}

	return error;
}

int mythGetInput(struct MYTH_CONNECTION_T *mythConnection)
{
	char	response[RESPONSESIZE];
	char 	command[MAX_COMMAND_LENGTH];
	int 	error = 0;

	snprintf(&command[0], MAX_COMMAND_LENGTH, "QUERY_RECORDER %d[]:[]GET_INPUT", mythConnection->recorderId);
	error = sendCommandAndReadReply(mythConnection, &command[0], &response[0], RESPONSESIZE);

	return error;
}

/*
49      QUERY_FILETRANSFER 173[]:[]REQUEST_BLOCK[]:[]2048
4       2048
49      QUERY_FILETRANSFER 173[]:[]SEEK[]:[]0[]:[]0[]:[]0
1       0
*/
unsigned long long int mythFiletransferRequestBlock(struct MYTH_CONNECTION_T *mythConnection, unsigned long long int blockLen)
{
        char    response[RESPONSESIZE];
        char    command[MAX_COMMAND_LENGTH];
        int     error = 0;

        snprintf(&command[0], MAX_COMMAND_LENGTH, "QUERY_FILETRANSFER %d[]:[]REQUEST_BLOCK[]:[]%lld", mythConnection->transferConnection->transferSocket, blockLen);
        error = sendCommandAndReadReply(mythConnection, &command[0], &response[0], RESPONSESIZE);
        if (error >= 0) {
		logInfo( LOG_MYTHPROTOCOL_DEBUG,"Backend will send %lld bytes.\n", atoll(&response[0]));

		pthread_mutex_lock(&mythConnection->readWriteLock);

		mythConnection->position += atoll(&response[0]);

		pthread_mutex_unlock(&mythConnection->readWriteLock);

		return atoll(&response[0]);
        }

        return error;
}

/* fseek values for whence */
//#define SEEK_SET	0	/* Seek from beginning of file.  */
//#define SEEK_CUR	1	/* Seek from current position.  */
//#define SEEK_END	2	/* Seek from end of file.  */


unsigned long long int mythFiletransferSeek(struct MYTH_CONNECTION_T *mythConnection, unsigned long long int position, int whence, unsigned long long int currentPos)
{
        char    response[RESPONSESIZE];
        char    command[MAX_COMMAND_LENGTH];
        int     error = 0;

        snprintf(&command[0], MAX_COMMAND_LENGTH, "QUERY_FILETRANSFER %d[]:[]SEEK[]:[]%lld[]:[]%d[]:[]%lld", mythConnection->transferConnection->transferSocket, position, whence, currentPos);
        error = sendCommandAndReadReply(mythConnection, &command[0], &response[0], RESPONSESIZE);
        if (error >= 0) {
		logInfo( LOG_MYTHPROTOCOL,"Backend seek result = %lld bytes.\n", atoll(&response[0]));

		pthread_mutex_lock(&mythConnection->readWriteLock);

		mythConnection->position = atoll(&response[0]);

		pthread_mutex_unlock(&mythConnection->readWriteLock);

		return atoll(&response[0]);
        }

	pthread_mutex_lock(&mythConnection->readWriteLock);

	unsigned long long int restult = mythConnection->position;

	pthread_mutex_unlock(&mythConnection->readWriteLock);

        return restult;
}

int mythFiletransferDone(struct MYTH_CONNECTION_T *mythConnection)
{
	char	response[RESPONSESIZE];
	char 	command[MAX_COMMAND_LENGTH];
	int 	error = 0;

	snprintf(&command[0], MAX_COMMAND_LENGTH, "QUERY_FILETRANSFER %d[]:[]DONE", mythConnection->transferConnection->transferSocket);
	error = sendCommandAndReadReply(mythConnection, &command[0], &response[0], RESPONSESIZE);
	if (error >= 0) {
		if (checkResponse(&response[0], "OK") == 0) {
			error = -1;
		}
	}

	return error;
}

int mythSpawnLiveTV(struct MYTH_CONNECTION_T *mythConnection, struct MYTH_CONNECTION_T *masterConnection, int recorderId, char *chainId, int channelNum, int pip)
{
	char	response[RESPONSESIZE];
	char 	command[MAX_COMMAND_LENGTH];
	int 	error = 0;

	// QUERY_RECORDER 1[]:[]SPAWN_LIVETV[]:[]live-seans-laptop-07-03-23T12:30:32[]:[]0[]:[]232
	// 94      QUERY_RECORDER 29[]:[]SPAWN_LIVETV[]:[]live-odi.verbraak.thuis-2012-12-16T18:43:40Z[]:[]0[]:[]
//	snprintf(&command[0], MAX_COMMAND_LENGTH, "QUERY_RECORDER %d[]:[]SPAWN_LIVETV[]:[]%s[]:[]%d[]:[]", recorderId, chainId, pip);
	snprintf(&command[0], MAX_COMMAND_LENGTH, "QUERY_RECORDER %d[]:[]SPAWN_LIVETV[]:[]%s[]:[]%d[]:[]%d", recorderId, chainId, pip, channelNum);
	error = sendCommandAndReadReply(mythConnection, &command[0], &response[0], RESPONSESIZE);

	logInfo( LOG_MYTHPROTOCOL_DEBUG,"mythSpawnLiveTV: sendCommandAndReadReply: error=%d\n", error);

	if (error >= 0) {
		if (checkResponse(&response[0], "OK") == 0) {
			error = -1;
		}
	}

	logInfo( LOG_MYTHPROTOCOL_DEBUG,"mythSpawnLiveTV: checkResponse: error=%d\n", error);

	if (error >= 0) {
		mythConnection->recorderId = recorderId;
		mythConnection->chainId = chainId;
		mythConnection->channelNum = channelNum;
		mythConnection->pip = pip;
		mythConnection->masterConnection = masterConnection;
	}

	return error;
}

int mythStopLiveTV(struct MYTH_CONNECTION_T *mythConnection)
{
	char	response[RESPONSESIZE];
	char 	command[MAX_COMMAND_LENGTH];
	int 	error = 0;

	snprintf(&command[0], MAX_COMMAND_LENGTH, "QUERY_RECORDER %d[]:[]STOP_LIVETV", mythConnection->recorderId);
	error = sendCommandAndReadReply(mythConnection, &command[0], &response[0], RESPONSESIZE);

	logInfo( LOG_MYTHPROTOCOL_DEBUG,"mythStopLiveTV: sendCommandAndReadReply: error=%d\n", error);

	if (error >= 0) {
		if (checkResponse(&response[0], "OK") == 0) {
			error = -1;
		}
	}

	logInfo( LOG_MYTHPROTOCOL_DEBUG,"mythStopLiveTV: checkResponse: error=%d\n", error);

	return error;
}

int mythLiveTVChainUpdate(struct MYTH_CONNECTION_T *mythConnection, char *chainId)
{
	char 	command[MAX_COMMAND_LENGTH];
	int 	error = 0;

	if (error >= 0) {
		snprintf(&command[0], MAX_COMMAND_LENGTH, "BACKEND_MESSAGE[]:[]LIVETV_CHAIN UPDATE %s[]:[]empty", chainId);
		error = sendCommand(mythConnection->masterConnection->connection, &command[0]);
	}

	return error;
}


int mythIsRecording(struct MYTH_CONNECTION_T *mythConnection)
{
	char	response[RESPONSESIZE];
	char 	command[MAX_COMMAND_LENGTH];
	int 	error = 0;

	if (error >= 0) {
		snprintf(&command[0], MAX_COMMAND_LENGTH, "QUERY_RECORDER %d[]:[]IS_RECORDING", mythConnection->recorderId);
		error = sendCommandAndReadReply(mythConnection, &command[0], &response[0], RESPONSESIZE);
	}

	if (error >= 0) {
		if (checkResponse(&response[0], "1") == 0) {
			error = -1;
		}
	}

	return error;
}

//QUERY_FILE_EXISTS[]:[]11035_20121216184343.mpg
int mythQueryFileExists(struct MYTH_CONNECTION_T *mythConnection)
{
        char    response[RESPONSESIZE];
        char    command[MAX_COMMAND_LENGTH];
        int     error = 0;

        if (error >= 0) {
                char *filename = getStringAtListIndex(mythConnection->currentRecording,10);
                //char *filename = getFilename(fullPath);

                snprintf(&command[0], MAX_COMMAND_LENGTH, "QUERY_FILE_EXISTS[]:[]%s", filename);
		//free(filename);
                error = sendCommandAndReadReply(mythConnection->masterConnection, &command[0], &response[0], RESPONSESIZE);
        }

        if (error >= 0) {
                if (checkResponse(&response[0], "1") == 0) {
                        error = -1;
                }
        }

        return error;
}

//688     QUERY_CHECKFILE[]:[]0[]:[]NOS Studio Sport Eredivisie[]:[][]:[] Samenvattingen - Samenvattingen van de gespeelde eredivisiewedstrijden:FC Utrecht - SC HeerenveenCommentaar: Jeroen GrueterFeyenoord - ADO Den HaagCommentaar: Jeroen ElshoffVitesse - RKC WaalwijkWillem II - AjaxCommentaar: Frank Snoeks []:[]0[]:[]0[]:[][]:[]11035[]:[]1[]:[]NED1 HD[]:[]NED1 HD[]:[]myth://192.168.1.60:6543/11035_20121216184343.mpg[]:[]0[]:[]1355680805[]:[]1355684405[]:[]0[]:[]xen01[]:[]0[]:[]0[]:[]0[]:[]0[]:[]-3[]:[]0[]:[]0[]:[]0[]:[]0[]:[]1355683423[]:[]1355684405[]:[]3145732[]:[]LiveTV[]:[][]:[]153652933[]:[][]:[][]:[]1355683423[]:[]0[]:[][]:[]Default[]:[]0[]:[]0[]:[]LiveTV[]:[]0[]:[]0[]:[]0[]:[]2012
int mythQueryCheckFile(struct MYTH_CONNECTION_T *mythConnection)
{
        char    response[RESPONSESIZE];
        char    command[MAX_COMMAND_LENGTH];
        int     error = 0;

        if (error >= 0) {
                char *proginfo = convertListToString(mythConnection->currentRecording, "[]:[]");

                snprintf(&command[0], MAX_COMMAND_LENGTH, "QUERY_CHECKFILE[]:[]0[]:[]%s", proginfo);
		free(proginfo);
                error = sendCommandAndReadReply(mythConnection->masterConnection, &command[0], &response[0], RESPONSESIZE);
        }

        if (error >= 0) {
                if (checkResponse(&response[0], "1") == 0) {
                        error = -1;
                }
        }

        return error;
}

//NOS Het vragenuurtje[]:[][]:[][]:[]0[]:[]0[]:[][]:[]11035[]:[]1[]:[]NED1 HD[]:[]NED1 HD[]:[]/mnt/sda1/mythtv/livetv/11035_20121218132435.mpg[]:[]0[]:[]1355835605[]:[]1355839205[]:[]0[]:[]xen01[]:[]0[]:[]31[]:[]0[]:[]0[]:[]-2[]:[]0[]:[]0[]:[]15[]:[]6[]:[]1355837075[]:[]1355839205[]:[]1048580[]:[]LiveTV[]:[][]:[]104861685[]:[][]:[][]:[]1355835605[]:[]0[]:[][]:[]Default[]:[]0[]:[]0[]:[]LiveTV[]:[]0[]:[]0[]:[]0[]:[]2012

struct LISTITEM_T *mythGetCurrentRecording(struct MYTH_CONNECTION_T *mythConnection)
{
	char	response[RESPONSESIZE];
	char 	command[MAX_COMMAND_LENGTH];
	int 	error = 0;

	if (error >= 0) {
		snprintf(&command[0], MAX_COMMAND_LENGTH, "QUERY_RECORDER %d[]:[]GET_CURRENT_RECORDING", mythConnection->recorderId);
		error = sendCommandAndReadReply(mythConnection->masterConnection, &command[0], &response[0], RESPONSESIZE);
	}

	if (error < 0) {
		return NULL;
	}

	return convertStrToList(&response[0], "[]:[]");
}


int mythMasterUpdateProgInfo(struct MYTH_CONNECTION_T *mythConnection)
{
	char 	command[MAX_COMMAND_LENGTH];
	int 	error = 0;

	if (error >= 0) {
		char *tmpTime = now("%FT%TZ", 1);
		snprintf(&command[0], MAX_COMMAND_LENGTH, "BACKEND_MESSAGE[]:[]MASTER_UPDATE_PROG_INFO %d %s", mythConnection->channelId, tmpTime);
		free(tmpTime);
		error = sendCommand(mythConnection->masterConnection->connection, &command[0]);
	}

	return error;
}

struct MYTH_CONNECTION_T *startLiveTV(struct MYTH_CONNECTION_T *masterConnection, int channelNum)
{
	struct MYTH_CONNECTION_T *result = NULL;

	char	response[RESPONSESIZE];
	char 	command[MAX_COMMAND_LENGTH];
	int 	error = 0;
	char	*chainId = NULL;
	struct LISTITEM_T *tmpList;

	if (error >= 0) {
		snprintf(&command[0], MAX_COMMAND_LENGTH, "GET_NEXT_FREE_RECORDER[]:[]-1");
		error = sendCommandAndReadReply(masterConnection, &command[0], &response[0], RESPONSESIZE);
		if (error >= 0) {
			if (checkResponse(&response[0], "-1[]:[]nohost[]:[]-1") == 1) {
				logInfo( LOG_MYTHPROTOCOL,"No free recorder available.\n");
				error = -1;
			}
		}

	}

	if (error >=0) {
		tmpList = convertStrToList(&response[0], "[]:[]");

		logInfo( LOG_MYTHPROTOCOL,"Recorder=%s\n", getStringAtListIndex(tmpList, 0));
		logInfo( LOG_MYTHPROTOCOL,"ip-address=%s\n", getStringAtListIndex(tmpList, 1));
		logInfo( LOG_MYTHPROTOCOL,"port=%s\n", getStringAtListIndex(tmpList, 2));

		result = createMythConnection(getStringAtListIndex(tmpList, 1), atoi(getStringAtListIndex(tmpList, 2)), ANN_PLAYBACK);
		if (result == NULL) {
			error = -1;
		}
		else {
			chainId = malloc(37);
			memset(chainId, 0, sizeof(chainId));
			uuid_t tmpUUID;
			uuid_generate(tmpUUID);
			uuid_unparse(tmpUUID, chainId);
			uuid_clear(tmpUUID);
			logInfo( LOG_MYTHPROTOCOL,"uuid=%s\n", chainId);

			result->streaming = 0;
		}
	}

	if ((result != NULL) && (error >=0)) {

		//struct MYTH_CONNECTION_T *mythConnection, int recorderId, char *chainId, int channelNum, int pip
		if (mythSpawnLiveTV(result, masterConnection, atoi(getStringAtListIndex(tmpList, 0)), chainId, channelNum, 0) < 0) {
			logInfo( LOG_MYTHPROTOCOL,"startLiveTV: mythSpawnLiveTV failed.\n");
			error = -1;
		}
	}


/*	if (error >= 0) {
		if (mythLiveTVChainUpdate(result, result->chainId) < 0) {
			logInfo( LOG_MYTHPROTOCOL,"startLiveTV: mythLiveTVChainUpdate failed.\n");
			error = -1;
		}
	}*/

	if (error >= 0) {
		result->currentRecording = mythGetCurrentRecording(result);
		logInfo( LOG_MYTHPROTOCOL,"startLiveTV: mythGetCurrentRecording: %d items in list.\n",listCount(result->currentRecording));
	}

	if (chainId != NULL) {
		free(chainId);
	}

	if (error >= 0) {
		return result;
	}
	return NULL;


/*	if (error >= 0) {
		result->channelId = atoi(getStringAtListIndex(result->currentRecording,6));
		if (mythMasterUpdateProgInfo(result) < 0) {
			logInfo( LOG_MYTHPROTOCOL,"startLiveTV: mythMasterUpdateProgInfo failed.\n");
			error = -1;
		}
	}*/

}

int startLiveTVStream(struct MYTH_CONNECTION_T *mythConnection)
{
	int error = 0;

	if (mythQueryCheckFile(mythConnection) < 0) {
		logInfo( LOG_MYTHPROTOCOL,"startLiveTV: mythQueryCheckFile == false.\n");
		error = -1;
	}

	if (error >= 0) {
		if (mythQueryFileExists(mythConnection) < 0) {
			logInfo( LOG_MYTHPROTOCOL,"startLiveTV: mythQueryFileExists == false.\n");
			error = -1;
		}
	}

	if (error >= 0) {
		if (mythIsRecording(mythConnection) < 0) {
			logInfo( LOG_MYTHPROTOCOL,"startLiveTV: mythIsRecording = 0.\n");
			error = -1;
		}
	}
        
	if (error >= 0) {
                if (mythFrontendReady(mythConnection) < 0) {
                        logInfo( LOG_MYTHPROTOCOL,"startLiveTV: mythFrontendReady = 0.\n");
                        error = -1;
                }
        }

	if (error >= 0) {
                if (mythGetInput(mythConnection) < 0) {
                        logInfo( LOG_MYTHPROTOCOL,"startLiveTV: mythGetInput = 0.\n");
                        error = -1;
                }
        }

	logInfo( LOG_MYTHPROTOCOL_DEBUG,"startLiveTV: mythGetInput >= 0.\n");

	if (error >= 0) {
		mythConnection->transferConnection = createMythConnection(mythConnection->hostname, mythConnection->port, ANN_FILETRANSFER);
		if (mythConnection == NULL) {
			logInfo( LOG_MYTHPROTOCOL,"startLiveTV: createMythConnection filetransfer.\n");
			error = -1;
		}
	}

	logInfo( LOG_MYTHPROTOCOL_DEBUG,"startLiveTV: create transferConnection.\n");

	if (error >= 0) {
		mythConnection->transferConnection->backendConnection = mythConnection;
		error = mythAnnFileTransfer(mythConnection->transferConnection);
		if (error < 0) {
			logInfo( LOG_MYTHPROTOCOL,"startLiveTV: mythAnnFileTransfer error.\n");
			error = -1;
		}
	}

	if (error >= 0) {
		mythConnection->streaming = 1;
	}

	return error;
}

int stopLiveTVStream(struct MYTH_CONNECTION_T *mythConnection)
{
	int error = 0;

	if (mythConnection->streaming == 1) {
		mythFiletransferDone(mythConnection);
		mythStopLiveTV(mythConnection);
	}

	mythConnection->streaming = 0;

	destroyMythConnection(mythConnection->transferConnection);

	return error;
}
