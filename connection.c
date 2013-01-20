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


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <errno.h>
#include "connection.h"
#include "globalFunctions.h"

int createSocketIPv6(char *inHostname, uint16_t port, struct hostent *hp)
{
	int	sd = -1;
	struct sockaddr_in6 pin;
	char *tmpStr = malloc(INET6_ADDRSTRLEN+1);
	memset(tmpStr, 0, INET6_ADDRSTRLEN+1);

	/* fill in the socket structure with host information */
	memset(&pin, 0, sizeof(pin));

//	pin.sin6_len = sizeof(pin);
	pin.sin6_family = hp->h_addrtype;
	memcpy((char *)&pin.sin6_addr, hp->h_addr, hp->h_length);
	pin.sin6_port = htons(port);
	logInfo( LOG_CONNECTION,"hp->h_length=%d\n", hp->h_length);

	logInfo( LOG_CONNECTION,"Using ipv6 address '%s'\n", inet_ntop(AF_INET6, &pin.sin6_addr, tmpStr, INET6_ADDRSTRLEN+1));

	/* grab an Internet domain socket */
	if ((sd = socket(AF_INET6, SOCK_STREAM, 0)) == -1) {
		logInfo( LOG_CONNECTION,"Could not create socket to host %s:%d.\n", inHostname, port);
		return sd;
	}

	/* connect to PORT on HOST */
	if (connect(sd,(struct sockaddr *)  &pin, sizeof(pin)) == -1) {
		logInfo( LOG_CONNECTION,"Could not connect to host %s:%d.(errno=%d)\n", inHostname, port, errno);
		sd = -1;
		return sd;
	}

	return sd;
}

int createSocketIPv4(char *inHostname, uint16_t port, struct hostent *hp)
{
	int	sd = -1;
	struct sockaddr_in pin;

	/* fill in the socket structure with host information */
	memset(&pin, 0, sizeof(pin));
	pin.sin_family = AF_INET;
	pin.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
	pin.sin_port = htons(port);
	logInfo( LOG_CONNECTION,"hp->h_length=%d\n", hp->h_length);

	/* grab an Internet domain socket */
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		logInfo( LOG_CONNECTION,"Could not create socket to host %s:%d.\n", inHostname, port);
		return sd;
	}

	/* connect to PORT on HOST */
	if (connect(sd,(struct sockaddr *)  &pin, sizeof(pin)) == -1) {
		logInfo( LOG_CONNECTION,"Could not connect to host %s:%d.\n", inHostname, port);
		sd = -1;
		return sd;
	}

	return sd;
}

int createSocket(char *inHostname, uint16_t port)
{
	struct hostent *hp;

	if ((hp = gethostbyname2(inHostname, AF_INET)) == NULL) {
		if ((hp = gethostbyname2(inHostname, AF_INET6)) == NULL) {
			logInfo( LOG_CONNECTION,"Error on gethostbyname.\n");
			return -1;
		}
	}

	if (hp->h_addrtype == AF_INET) {
		return createSocketIPv4(inHostname, port, hp);
	}
	else {
		if (hp->h_addrtype == AF_INET6) {
			return createSocketIPv6(inHostname, port, hp);
		}
	}

	return -1;
}

struct CONNECTION_T *createConnection(char *inHostname, uint16_t port)
{
	logInfo( LOG_CONNECTION_DEBUG,"start\n");

	struct CONNECTION_T *result = (struct CONNECTION_T *)malloc(sizeof(struct CONNECTION_T));

/*	if (indexOf(inHostname, ":") > -1) {
		result->socket = createSocketIPv6(inHostname, port);
	}
	else {
		result->socket = createSocketIPv4(inHostname, port);
	}
*/
	result->socket = createSocket(inHostname, port);

	if (result->socket < 0) {
		free(result);
		logInfo( LOG_CONNECTION,"result->socket < 0.\n");
		return NULL;
	}
	result->buffer = NULL;
	result->bufferPos = 0;
	result->bufferLen = 0;
	result->bufferEnd = 0;

	// Create mutexes
	if (pthread_mutex_init(&result->readWriteLock, NULL) != 0)
	{
		logInfo( LOG_CONNECTION,"createConnection: mutex init failed\n");
		close(result->socket);
		free(result);
		return NULL;
	}


	return result;
}

unsigned long long int fillConnectionBuffer(struct CONNECTION_T *connection, unsigned long long int responseLength, int readFull)
{
	char *response = malloc(responseLength+1);

	logInfo( LOG_CONNECTION_DEBUG,"fillBuffer: responseLength=%lld\n", responseLength);
	
	ssize_t readLen = recv(connection->socket, response, responseLength, 0);

	logInfo( LOG_CONNECTION_DEBUG,"fillBuffer: readLen=%zd, response=%s\n", readLen, response);

        if (readLen == -1) {
                logInfo( LOG_CONNECTION,"recv");
                return readLen;
        }

	pthread_mutex_lock(&connection->readWriteLock);

	// Get memory for received data.
	if (connection->buffer == NULL) {

		logInfo( LOG_CONNECTION_DEBUG,"fillBuffer: Going to allocate memory for the first time.\n");

		connection->bufferLen = 0;
		connection->bufferPos = 0;
		connection->bufferEnd = 0;

		connection->buffer = malloc(readLen);
		if (connection->buffer == NULL) {
			logInfo( LOG_CONNECTION,"fillBuffer: connection buffer malloc. Tried to allocate %lld bytes.\n", (long long int)readLen);
			pthread_mutex_unlock(&connection->readWriteLock);
			return -1;
		}
		connection->bufferLen = readLen;
	}
	else {
		if ((connection->bufferLen - connection->bufferEnd) < responseLength) {
			// Will not fit in the buffer 

			logInfo( LOG_CONNECTION_DEBUG,"fillBuffer: Going to create new memory allocation for buffer. bufferEnd=%d, bufferPos=%d, bufferLen=%d.\n", connection->bufferEnd, connection->bufferPos, connection->bufferLen);

			unsigned int newSize = readLen + (connection->bufferEnd - connection->bufferPos);

			logInfo( LOG_CONNECTION_DEBUG,"fillBuffer: newSize=%d.\n", newSize);

			char *newBuffer = malloc(newSize);
			if (newBuffer == NULL) {
				logInfo( LOG_CONNECTION,"fillBuffer: newBuffer malloc. Tried to allocate %d bytes.\n", newSize);
				pthread_mutex_unlock(&connection->readWriteLock);
				return -1;
			}
			// Copy old buffer to new buffer
			unsigned int newBufferPos = 0;
			unsigned int oldBufferPos = connection->bufferPos;
			char *tmpNewBuffer;
			char *tmpOldBuffer;

			// Need to replace this with a memcpy
			while (oldBufferPos < connection->bufferEnd) {
				tmpNewBuffer = newBuffer + newBufferPos;
				tmpOldBuffer = connection->buffer + oldBufferPos;

				*tmpNewBuffer = *tmpOldBuffer;

				oldBufferPos++;
				newBufferPos++;
			}

			logInfo( LOG_CONNECTION_DEBUG,"fillBuffer: newBuffer=%s.\n", newBuffer);

			free(connection->buffer);

			connection->buffer = newBuffer;
			connection->bufferEnd = connection->bufferEnd - connection->bufferPos;
			connection->bufferPos = 0;
			connection->bufferLen = newSize;
		}
	}

	logInfo( LOG_CONNECTION_DEBUG,"fillBuffer: Buffer is ready. bufferEnd=%d, bufferPos=%d, bufferLen=%d.\n", connection->bufferEnd, connection->bufferPos, connection->bufferLen);

	// Add new received data to connection buffer
	char *tmpBuffer;
	tmpBuffer = connection->buffer + connection->bufferEnd;
	memcpy(tmpBuffer, response, readLen);
	connection->bufferEnd += readLen;
	
	free(response);

	logInfo( LOG_CONNECTION_DEBUG,"fillBuffer: new Buffer=%s.\n", connection->buffer);
	logInfo( LOG_CONNECTION_DEBUG,"fillBuffer: Buffer details. bufferEnd=%d, bufferPos=%d, bufferLen=%d.\n", connection->bufferEnd, connection->bufferPos, connection->bufferLen);

	pthread_mutex_unlock(&connection->readWriteLock);

	if ((readFull > 0) && (readLen != responseLength)) {
		readLen += fillConnectionBuffer(connection, responseLength - readLen, readFull);
	}

	return readLen;
}

unsigned long long int peekConnectionBuffer(struct CONNECTION_T *connection, char *dstBuffer, int len)
{
	unsigned long long int rlen = len;

	pthread_mutex_lock(&connection->readWriteLock);

	unsigned long long int tmpLen = getConnectionDataLen(connection);
	if (tmpLen < len) {
		rlen = tmpLen;
	}
	char *tmpBuffer = connection->buffer + connection->bufferPos;

	memcpy(dstBuffer, tmpBuffer, rlen);

	pthread_mutex_unlock(&connection->readWriteLock);

	return rlen;
}

unsigned long long int readConnectionBuffer(struct CONNECTION_T *connection, char *dstBuffer, int len)
{
	unsigned long long int rlen = len;

	pthread_mutex_lock(&connection->readWriteLock);

	unsigned long long int tmpLen = getConnectionDataLen(connection);
	if (tmpLen < len) {
		rlen = tmpLen;
	}

	char *tmpBuffer = connection->buffer + connection->bufferPos;

	memcpy(dstBuffer, tmpBuffer, rlen);
	connection->bufferPos += rlen;

	pthread_mutex_unlock(&connection->readWriteLock);

	return rlen;
}

void destroyConnection(struct CONNECTION_T *connection)
{
	if (connection == NULL) return;

	if (connection->socket > -1) {
		close(connection->socket);
	}
	if (connection->buffer) {
		free(connection->buffer);
	}

	// Destroy mutex
	pthread_mutex_destroy(&connection->readWriteLock);

	free(connection);
}

unsigned long long int getConnectionDataLen(struct CONNECTION_T *connection)
{
	unsigned long long int result = (connection->bufferEnd - connection->bufferPos);

	return result;
}
