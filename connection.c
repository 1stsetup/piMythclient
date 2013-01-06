
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include "connection.h"
#include "globalFunctions.h"


int createSocket(char *inHostname, uint16_t port)
{
        char hostname[100];
	int	sd = -1;
	//struct sockaddr_in sin;
	struct sockaddr_in pin;
	struct hostent *hp;

        strncpy(hostname,inHostname, 100);

	/* go find out about the desired host machine */
	if ((hp = gethostbyname(hostname)) == 0) {
		logInfo( LOG_CONNECTION,"gethostbyname");
		return sd;
	}

	/* fill in the socket structure with host information */
	memset(&pin, 0, sizeof(pin));
	pin.sin_family = AF_INET;
	pin.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
	pin.sin_port = htons(port);

	/* grab an Internet domain socket */
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		logInfo( LOG_CONNECTION,"socket");
		return sd;
	}

	/* connect to PORT on HOST */
	if (connect(sd,(struct sockaddr *)  &pin, sizeof(pin)) == -1) {
		logInfo( LOG_CONNECTION,"connect");
		sd = -1;
		return sd;
	}

	return sd;
}

struct CONNECTION_T *createConnection(char *inHostname, uint16_t port)
{
	logInfo( LOG_CONNECTION_DEBUG,"start\n");

	struct CONNECTION_T *result = (struct CONNECTION_T *)malloc(sizeof(struct CONNECTION_T));

	result->socket = createSocket(inHostname, port);
	if (result->socket < 0) {
		free(result);
		return NULL;
	}
	result->buffer = NULL;
	result->bufferPos = 0;
	result->bufferLen = 0;

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

		connection->buffer = malloc(readLen);
		if (connection->buffer == NULL) {
			logInfo( LOG_CONNECTION,"fillBuffer: connection buffer malloc");
			pthread_mutex_unlock(&connection->readWriteLock);
			return -1;
		}
		connection->bufferLen = readLen;
		connection->bufferPos = 0;
		connection->bufferEnd = 0;
	}
	else {
		if ((connection->bufferLen - connection->bufferEnd) < responseLength) {
			// Will not fit in the buffer 

			logInfo( LOG_CONNECTION_DEBUG,"fillBuffer: Going to create new memory allocation for buffer. bufferEnd=%d, bufferPos=%d, bufferLen=%d.\n", connection->bufferEnd, connection->bufferPos, connection->bufferLen);

			unsigned int newSize = readLen + (connection->bufferEnd - connection->bufferPos);

			logInfo( LOG_CONNECTION_DEBUG,"fillBuffer: newSize=%d.\n", newSize);

			char *newBuffer = malloc(newSize);
			if (newBuffer == NULL) {
				logInfo( LOG_CONNECTION,"fillBuffer: newBuffer malloc");
				pthread_mutex_unlock(&connection->readWriteLock);
				return -1;
			}
			// Copy old buffer to new buffer
			unsigned int newBufferPos = 0;
			unsigned int oldBufferPos = connection->bufferPos;
			char *tmpNewBuffer;
			char *tmpOldBuffer;

			while (oldBufferPos<connection->bufferEnd) {
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
	unsigned int responsePos = 0;
	char *tmpBuffer;
	while (responsePos < readLen) {
		tmpBuffer = connection->buffer + connection->bufferEnd;
		*tmpBuffer = response[responsePos];
		responsePos++;
		connection->bufferEnd++;
	}

	logInfo( LOG_CONNECTION_DEBUG,"fillBuffer: new Buffer=%s.\n", connection->buffer);
	logInfo( LOG_CONNECTION_DEBUG,"fillBuffer: Buffer details. bufferEnd=%d, bufferPos=%d, bufferLen=%d.\n", connection->bufferEnd, connection->bufferPos, connection->bufferLen);

	pthread_mutex_unlock(&connection->readWriteLock);

	if ((readFull > 0) && (readLen != responseLength)) {
		readLen += fillConnectionBuffer(connection, responseLength - readLen, readFull);
	}

	return readLen;
}

unsigned long long int readConnectionBuffer(struct CONNECTION_T *connection, char *dstBuffer, int len)
{
	unsigned long long int rlen = len;
	char *tmpBuffer = connection->buffer + connection->bufferPos;

	char *tmpOutBuffer = dstBuffer;

	pthread_mutex_lock(&connection->readWriteLock);

	while ((rlen > 0) && (connection->bufferPos < connection->bufferEnd)) {
		tmpBuffer = connection->buffer + connection->bufferPos;
		*tmpOutBuffer = *tmpBuffer;
		rlen--;
		connection->bufferPos++;
		tmpOutBuffer++;
	}

	pthread_mutex_unlock(&connection->readWriteLock);

	return (len - rlen);
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
	return (connection->bufferEnd - connection->bufferPos);
}
