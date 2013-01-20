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

int mythDataAvailableOnConnection(struct CONNECTION_T *connection)
{
	return (getConnectionDataLen(connection) > 0) ? 1 : 0;
}

ssize_t readResponse(struct CONNECTION_T *connection, char *outResponse, size_t responseLength, int doWait)
{
	logInfo( LOG_MYTHPROTOCOL_DEBUG,"readResponse: responseLength=%zd.\n", responseLength);

	int doFill = 1;

	char *rlenStr = malloc(8);
	memset(rlenStr, 0, 8);

	if (peekConnectionBuffer(connection, rlenStr, 8) == 8) {

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
		if ((rlen + 8) > getConnectionDataLen(connection)) {
			doFill = 0;
		}
	}

	free(rlenStr);

	unsigned long long int bytesFilled = 0;
	if (doFill == 1) {
		// First we are going check if data is waiting on the socket.
		// If so read it. Else return.
		int rc = 1;
		if (doWait == 0) {
			struct timeval timeout;
			fd_set working_fd_set;

			FD_ZERO(&working_fd_set);
			FD_SET(connection->socket, &working_fd_set);

			timeout.tv_sec  = 0;
			timeout.tv_usec = 1;

			rc = select(connection->socket+1, &working_fd_set, NULL, NULL, &timeout);
		}

		if (rc < 0) {
			return -1; // Interupt signal
		}

		if (rc > 0) {
			size_t stillToRead = responseLength - getConnectionDataLen(connection);
			bytesFilled = fillConnectionBuffer(connection, stillToRead, 0);
		}
		else {
			bytesFilled = -1;
		}

	}

	if ((doFill == 1) && (bytesFilled == -1)) {
		return -1;
	}

//	logInfo( LOG_MYTHPROTOCOL_DEBUG,"readResponse: connection.buffer=%s.\n", connection->buffer);
//	logInfo( LOG_MYTHPROTOCOL_DEBUG,"readResponse: Connection details. bufferEnd=%d, bufferPos=%d, bufferLen=%d.\n", connection->bufferEnd, connection->bufferPos, connection->bufferLen);

	if (getConnectionDataLen(connection) < 8) {
		return -1;
	}

	// Get length of response. It is in the first 8 bytes.
	rlenStr = malloc(8);
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
	unsigned long long int read = 0;
	unsigned long long int totalRead = 0;
	int tryCount = 0;
	do {
		read = readConnectionBuffer(connection, outResponse+totalRead, rlen);
		rlen -= read;
		totalRead += read;
		if (read == 0) {
			fillConnectionBuffer(connection, rlen, 1);
			tryCount++;
		}
	} while ((rlen > 0) && (tryCount < 10));
		
/*	if (read != rlen) {
		logInfo( LOG_MYTHPROTOCOL,"readResponse: WARNING: Not enough data in connection buffer.\n");
	}
*/
	logInfo( LOG_MYTHPROTOCOL_DEBUG,"%d<-: %-7lld %s\n", connection->socket, totalRead, outResponse);

	return strlen(outResponse);	
}

int sendCommandAndReadReply(struct MYTH_CONNECTION_T *mythConnection, char *inCommand, char *outResponse, size_t responseLength)
{

	pthread_mutex_lock(&mythConnection->readWriteLock);

	if (sendCommand(mythConnection->connection, inCommand) == -1) {
		logInfo(LOG_MYTHPROTOCOL, "sendCommandAndReadReply: sendCommand.\n");
		pthread_mutex_unlock(&mythConnection->readWriteLock);
		return -1;
	}

        if (readResponse(mythConnection->connection, outResponse, responseLength, 1) == -1) {
                logInfo(LOG_MYTHPROTOCOL, "sendCommandAndReadReply: readResponse.\n");
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
	backendConnection->connected = 0;

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

	backendConnection->connected = 1;

	return backendConnection;
}

void destroyMythConnection(struct MYTH_CONNECTION_T *mythConnection)
{
	if (mythConnection == NULL) return;

	if (mythConnection->connection) {
		if (mythConnection->connected == 1) {
			sendCommand(mythConnection->connection, "DONE");
		}

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

int mythAnnFileTransfer(struct MYTH_CONNECTION_T *mythConnection, char *filename)
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
//		char *filename = getStringAtListIndex(mythConnection->backendConnection->currentRecording,10);
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

int mythRefreshBackend(struct MYTH_CONNECTION_T *mythConnection)
{
	char	response[RESPONSESIZE];
	char 	command[MAX_COMMAND_LENGTH];
	int 	error = 0;

	snprintf(&command[0], MAX_COMMAND_LENGTH, "REFRESH_BACKEND");
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

char *mythConvertToFilename(char *channelId, char *startTime)
{
	//channel = 11010  STARTTIME = 2013-01-09T20:00:05Z
	char *result = malloc(strlen(channelId)+1+14+4+1);
	memset(result, 0, strlen(channelId)+1+14+4+1);

	char *tmpPtr1 = result;
	char *tmpPtr2 = startTime;

	// First copy channelId into new string.
	memcpy(tmpPtr1, channelId, strlen(channelId));
	tmpPtr1 += strlen(channelId);
	tmpPtr1[0] = 0x5F; //"_";
	tmpPtr1++;
	memcpy(tmpPtr1, tmpPtr2, 4);
	tmpPtr1 += 4; tmpPtr2 +=5;
	int i;
	for(i = 0; i < 5; i++) {
		memcpy(tmpPtr1, tmpPtr2, 2);
		tmpPtr1 += 2; tmpPtr2 +=3;
	}
	memcpy(tmpPtr1, ".mpg", 4);

	return result;
}

int mythQueryRecorderCheckChannel(struct MYTH_CONNECTION_T *mythConnection, int recorderId, int channel)
{
	char	response[RESPONSESIZE];
	char 	command[MAX_COMMAND_LENGTH];
	int 	error = 0;

	if (error >= 0) {
		snprintf(&command[0], MAX_COMMAND_LENGTH, "QUERY_RECORDER %d[]:[]CHECK_CHANNEL[]:[]%d", recorderId, channel);
		error = sendCommandAndReadReply(mythConnection, &command[0], &response[0], RESPONSESIZE);
	}

	if ((error < 0) || (checkResponse(&response[0], "bad") == 1)) {
		return -1;
	}

	return atoi(&response[0]);
}

struct MYTH_RECORDER_T *mythGetNextFreeRecorder(struct MYTH_CONNECTION_T *mythConnection, int currentRecorderId)
{
	char	response[RESPONSESIZE];
	char 	command[MAX_COMMAND_LENGTH];
	int 	error = 0;
	struct MYTH_RECORDER_T *result = NULL;

	struct LISTITEM_T *details;

	if (error >= 0) {
		snprintf(&command[0], MAX_COMMAND_LENGTH, "GET_NEXT_FREE_RECORDER[]:[]%d", currentRecorderId);
		error = sendCommandAndReadReply(mythConnection, &command[0], &response[0], RESPONSESIZE);
	}

	if (error >=0) {
		if (checkResponse(&response[0], "-1[]:[]nohost[]:[]-1") == 1) {
			logInfo( LOG_MYTHPROTOCOL,"No free recorder available after %d.\n", currentRecorderId);
			error = -1;
		}
	}

	if (error >=0) {
		details = convertStrToList(&response[0], "[]:[]");
		result = malloc(sizeof(struct MYTH_RECORDER_T));

		result->recorderId = atoi(getStringAtListIndex(details, 0));
		result->hostname = malloc(strlen(getStringAtListIndex(details, 1))+1);
		memset(result->hostname, 0, strlen(getStringAtListIndex(details, 1))+1);

		strncpy(result->hostname, getStringAtListIndex(details, 1), strlen(getStringAtListIndex(details, 1)));

		result->port = atoi(getStringAtListIndex(details, 2));
		freeList(details);
	}

	return result; //convertStrToList(&response[0], "[]:[]");
}

struct MYTH_CONNECTION_T *startLiveTV(struct MYTH_CONNECTION_T *masterConnection, int channelNum)
{
	struct MYTH_CONNECTION_T *result = NULL;

	int 	error = 0;
	char	*chainId = NULL;
	struct MYTH_RECORDER_T *recorder;

	recorder = mythGetNextFreeRecorder(masterConnection, -1);
	int recorderFound = 0;
	int lastRecorderId = -1;
	while ((recorderFound == 0) && (recorder != NULL)) {

		logInfo( LOG_MYTHPROTOCOL,"Trying Recorder=%d, ip-address=%s, port=%d\n", recorder->recorderId, recorder->hostname, recorder->port);

		result = createMythConnection(recorder->hostname, recorder->port, ANN_PLAYBACK);

		if ((result != NULL) && (mythQueryRecorderCheckChannel(result, recorder->recorderId, channelNum) == 1)) {
			recorderFound = 1;
		}
		else {
			if (result != NULL) {
				lastRecorderId = recorder->recorderId;
				destroyMythConnection(result);
			}
			recorder = mythGetNextFreeRecorder(masterConnection, recorder->recorderId);
		}

		if ((recorder != NULL) && (recorder->recorderId < lastRecorderId)) {
			// We have looped the list. Exit.
			return NULL;
		}

	}

	if (recorderFound == 0) {
		logInfo( LOG_MYTHPROTOCOL,"No free recorder available.\n");
		error = -1;
	}

	if ((error >=0) && (result != NULL)) {
		logInfo( LOG_MYTHPROTOCOL,"Going to use Recorder=%d, ip-address=%s, port=%d\n", recorder->recorderId, recorder->hostname, recorder->port);

		chainId = malloc(37);
		memset(chainId, 0, sizeof(chainId));
		uuid_t tmpUUID;
		uuid_generate(tmpUUID);
		uuid_unparse(tmpUUID, chainId);
		uuid_clear(tmpUUID);
		logInfo( LOG_MYTHPROTOCOL,"uuid=%s\n", chainId);

		result->streaming = 0;
	}

	if ((result != NULL) && (error >=0)) {

		//struct MYTH_CONNECTION_T *mythConnection, int recorderId, char *chainId, int channelNum, int pip
		if (mythSpawnLiveTV(result, masterConnection, recorder->recorderId, chainId, channelNum, 0) < 0) {
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
		logInfo( LOG_MYTHPROTOCOL,"startLiveTV: mythGetCurrentRecording: channelId=%d, %d items in list.\n", result->channelId, listCount(result->currentRecording));
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

int mythGetRecordingDetails(struct MYTH_CONNECTION_T *mythConnection, char *recordingFilename)
{
	char	response[RESPONSESIZE];
	char 	command[MAX_COMMAND_LENGTH];
	int error = 0;
	struct LISTITEM_T *tmpList;

	snprintf(&command[0], MAX_COMMAND_LENGTH, "QUERY_RECORDING BASENAME %s", recordingFilename);
	error = sendCommandAndReadReply(mythConnection->masterConnection, &command[0], &response[0], RESPONSESIZE);
	if (error >= 0) {
		if (checkResponse(&response[0], "OK[]:[]") != 1) {
			logInfo( LOG_MYTHPROTOCOL,"could not get details for file %s.\n",recordingFilename);
			return -1;
		}
	}

	if (error >=0) {
		tmpList = convertStrToList(&response[0], "[]:[]");

		mythConnection->currentRecording = tmpList->next;

		freeListItem(tmpList);
	}

	return error;
}

struct MYTH_CONNECTION_T *checkRecorderProgram(struct MYTH_CONNECTION_T *masterConnection, char *recordingFilename)
{
	struct MYTH_CONNECTION_T *result = NULL;

	char	response[RESPONSESIZE];
	char 	command[MAX_COMMAND_LENGTH];
	int 	error = 0;
	char	*fileURL = NULL;
	char 	*hostURIFull = NULL;
	char	*hostURIReal = NULL;
	struct LISTITEM_T *tmpList;
	struct LISTITEM_T *URI;
	struct LISTITEM_T *hostURI;

	if (error >= 0) {
		snprintf(&command[0], MAX_COMMAND_LENGTH, "QUERY_FILE_EXISTS[]:[]%s", recordingFilename);
		error = sendCommandAndReadReply(masterConnection, &command[0], &response[0], RESPONSESIZE);
		if (error >= 0) {
			if (checkResponse(&response[0], "1[]:[]") != 1) {
				logInfo( LOG_MYTHPROTOCOL,"File recorder file %s does not exist.\n",recordingFilename);
				error = -1;
			}
		}

	}

	if (error >= 0) {
		snprintf(&command[0], MAX_COMMAND_LENGTH, "QUERY_RECORDING BASENAME %s", recordingFilename);
		error = sendCommandAndReadReply(masterConnection, &command[0], &response[0], RESPONSESIZE);
		if (error >= 0) {
			if (checkResponse(&response[0], "OK[]:[]") != 1) {
				logInfo( LOG_MYTHPROTOCOL,"could not get details for file %s.\n",recordingFilename);
				error = -1;
			}
		}

	}

	if (error >=0) {
		// We need the myth://[Default]@hostname:6543/file.mpg part
		tmpList = convertStrToList(&response[0], "[]:[]");

		fileURL = getStringAtListIndex(tmpList, 11);
		URI = convertStrToList(fileURL, "/");
		
		if (listCount(URI) == 1) {
			result = createMythConnection(masterConnection->hostname, masterConnection->port, ANN_PLAYBACK);
		}
		else {
			hostURIFull = getStringAtListIndex(URI, 2);
			hostURI = convertStrToList(hostURIFull, "@");
			hostURIReal = getStringAtListIndex(hostURI, listCount(hostURI)-1);
			freeList(hostURI);
			hostURI = convertStrToList(hostURIReal, ":");
			result = createMythConnection(getStringAtListIndex(hostURI, 0), atoi(getStringAtListIndex(hostURI, 1)), ANN_PLAYBACK);
			freeList(hostURI);
		}

		if (result == NULL) {
			error = -1;
		}
		else {
			result->currentRecording = tmpList->next;
			result->streaming = 0;
		}

		freeListItem(tmpList);
		freeList(URI);

	}


	if (error >= 0) {
		return result;
	}
	return NULL;

}

int playRecorderProgram(struct MYTH_CONNECTION_T *mythConnection)
{
	int error = 0;

	logInfo( LOG_MYTHPROTOCOL_DEBUG,"playRecorderProgram.\n");

	if (error >= 0) {
		mythConnection->transferConnection = createMythConnection(mythConnection->hostname, mythConnection->port, ANN_FILETRANSFER);
		if (mythConnection->transferConnection == NULL) {
			logInfo( LOG_MYTHPROTOCOL,"error createMythConnection filetransfer.\n");
			error = -1;
		}
	}

	logInfo( LOG_MYTHPROTOCOL_DEBUG,"created transferConnection.\n");

	if (error >= 0) {
		mythConnection->transferConnection->backendConnection = mythConnection;
		error = mythAnnFileTransfer(mythConnection->transferConnection, getStringAtListIndex(mythConnection->currentRecording,10));
		if (error < 0) {
			logInfo( LOG_MYTHPROTOCOL,"Error mythAnnFileTransfer error.\n");
			error = -1;
		}
	}

	if (error >= 0) {
		logInfo( LOG_MYTHPROTOCOL,"Started stream for recorded program.\n");
		mythConnection->streaming = 1;
	}

	return error;
}

void mythSetNewTransferConnection(struct MYTH_CONNECTION_T *slaveConnection, struct MYTH_CONNECTION_T *newTransferConnection)
{
	pthread_mutex_lock(&slaveConnection->readWriteLock);

	struct MYTH_CONNECTION_T *oldTransferConnection = slaveConnection->transferConnection;

	if (oldTransferConnection) {
		pthread_mutex_lock(&slaveConnection->transferConnection->readWriteLock);
	}

	logInfo( LOG_MYTHPROTOCOL,"Switching transferConnection.\n");
	slaveConnection->transferConnection = newTransferConnection;
	if (oldTransferConnection) {
		destroyMythConnection(oldTransferConnection);
		pthread_mutex_unlock(&slaveConnection->transferConnection->readWriteLock);
	}

	pthread_mutex_unlock(&slaveConnection->readWriteLock);
}

struct MYTH_CONNECTION_T *mythPrepareNextProgram(struct MYTH_CONNECTION_T *mythConnection, char *newFilename)
{
	int error = 0;
	struct MYTH_CONNECTION_T *result = NULL;

	logInfo( LOG_MYTHPROTOCOL,"mythPrepareNextProgram.\n");

	if (error >= 0) {
		result = createMythConnection(mythConnection->hostname, mythConnection->port, ANN_FILETRANSFER);
		if (result == NULL) {
			logInfo( LOG_MYTHPROTOCOL,"error createMythConnection filetransfer.\n");
			return NULL;
		}
	}

	logInfo( LOG_MYTHPROTOCOL,"created transferConnection.\n");

	if (error >= 0) {
		result->backendConnection = mythConnection;
		error = mythAnnFileTransfer(result, newFilename);
		if (error < 0) {
			logInfo( LOG_MYTHPROTOCOL,"Error mythAnnFileTransfer error.\n");
			return NULL;
		}
	}

	return result;
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
		error = mythAnnFileTransfer(mythConnection->transferConnection, getStringAtListIndex(mythConnection->currentRecording,10));
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

struct LISTITEM_T *mythQueryRecordings(struct MYTH_CONNECTION_T *mythConnection, char *sort)
{
        char    response[RESPONSESIZE*15];
        char    command[MAX_COMMAND_LENGTH];
        int     error = 0;

        if (error >= 0) {
                snprintf(&command[0], MAX_COMMAND_LENGTH, "QUERY_RECORDINGS %s", sort);
                error = sendCommandAndReadReply(mythConnection, &command[0], &response[0], RESPONSESIZE*5);
        }

/*        if (error >= 0) {
                if (checkResponse(&response[0], "1") == 0) {
                        error = -1;
                }
        }
*/
        return NULL;

}
