#include <arpa/inet.h>

struct CONNECTION_T{
	int	socket;
	char	*buffer;
	unsigned int	bufferPos;
	unsigned int	bufferLen;
	unsigned int	bufferEnd;
	pthread_mutex_t readWriteLock;

};

struct CONNECTION_T *createConnection(char *inHostname, uint16_t port);
unsigned long long int fillConnectionBuffer(struct CONNECTION_T *connection, unsigned long long int responseLength, int readFull);
unsigned long long int readConnectionBuffer(struct CONNECTION_T *connection, char *dstBuffer, int len);
void destroyConnection(struct CONNECTION_T *connection);
unsigned long long int getConnectionDataLen(struct CONNECTION_T *connection);


