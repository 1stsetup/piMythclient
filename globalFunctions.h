#include <stdio.h>
#include <inttypes.h>

#define LOG_INFO		(1<<0)
#define LOG_CONNECTION		(1<<1)
#define LOG_MYTHPROTOCOL	(1<<2)
#define LOG_DEMUXER		(1<<3)
#define LOG_CLIENT		(1<<4)
#define LOG_LISTS		(1<<5)
#define LOG_GLOBALFUNCTIONS	(1<<6)
#define LOG_CONNECTION_DEBUG	(1<<7)
#define LOG_MYTHPROTOCOL_DEBUG	(1<<8)
#define LOG_DEMUXER_DEBUG	(1<<9)
#define LOG_OSD			(1<<10)
#define LOG_TVSERVICE		(1<<11)
#define LOG_CLIENT_DEBUG	(1<<12)
#define LOG_OMX			(1<<13)
#define LOG_OMX_DEBUG		(1<<14)
#define LOG_ALL			0xFFFFFFFF

void setLogLevel(uint32_t level) ;
uint32_t getLogLevel();

#define logInfo( level, fmt, arg... ) \
	if (getLogLevel() & level) { \
		printf( "%s:%d " fmt, __func__, __LINE__, ##arg); \
		fflush(NULL); \
	}

int indexOf(char *text, char *needle);
char *now(const char *format, int UTC);
uint64_t nowInMicroseconds();
char *getFilename(char *fullPath);
void setLogLevelByStr(char *level);
