#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "globalFunctions.h"
#include "lists.h"

uint32_t logLevel = 0;

void setLogLevel(uint32_t level) 
{
	logLevel = level;
}

uint32_t getLogLevel()
{
	return logLevel;
}

void setLogLevelByStr(char *level)
{
	struct LISTITEM_T *levels;
	struct LISTITEM_T *item;

	levels = convertStrToList(level, ",");
	item = levels;

	logLevel = 0;

	while (item != NULL) {
		if (strcmp(item->string, "demuxer") == 0) logLevel |= LOG_DEMUXER;
		if (strcmp(item->string, "mythprotocol") == 0) logLevel |= LOG_MYTHPROTOCOL;
		if (strcmp(item->string, "client") == 0) logLevel |= LOG_CLIENT;
		if (strcmp(item->string, "connection") == 0) logLevel |= LOG_CONNECTION;
		if (strcmp(item->string, "lists") == 0) logLevel |= LOG_LISTS;
		if (strcmp(item->string, "demuxer-debug") == 0) logLevel |= LOG_DEMUXER | LOG_DEMUXER_DEBUG;
		if (strcmp(item->string, "mythprotocol-debug") == 0) logLevel |= LOG_MYTHPROTOCOL | LOG_MYTHPROTOCOL_DEBUG;
		if (strcmp(item->string, "connection-debug") == 0) logLevel |= LOG_CONNECTION | LOG_CONNECTION_DEBUG;
		if (strcmp(item->string, "osd") == 0) logLevel |= LOG_OSD;
		if (strcmp(item->string, "tvservice") == 0) logLevel |= LOG_TVSERVICE;
		if (strcmp(item->string, "client-debug") == 0) logLevel |= LOG_CLIENT_DEBUG | LOG_CLIENT;
		if (strcmp(item->string, "omx") == 0) logLevel |= LOG_OMX;
		if (strcmp(item->string, "omx-debug") == 0) logLevel |= LOG_OMX_DEBUG | LOG_OMX;
		if (strcmp(item->string, "all") == 0) logLevel |= LOG_ALL;
		if (strcmp(item->string, "none") == 0) logLevel = 0;
		item = item->next;
	}
}

int indexOf(char *text, char *needle)
{
	char *tmpText = text;
	char *tmpNeedle = needle;
	int textLength = strlen(text);
	int needleLength = strlen(needle);

#ifdef DEBUG
	logInfo( LOG_GLOBALFUNCTIONS,"indexOf: 1. textLength=%d, needleLength=%d.\n", textLength, needleLength);
#endif
	if ((textLength == 0) || (needleLength == 0)) {
		return -1;
	}

	int needlePos = 0;
	int textPos = 0;

	int index = -1;

	int matchLen = 0;

	while ((textPos < textLength) && (matchLen < needleLength)) {
#ifdef DEBUG
		logInfo( LOG_GLOBALFUNCTIONS,"indexOf: 2. *tmpText=%x, *tmpNeedle=%x, textPos=%d, needlePos=%d, matchLen=%d\n", *tmpText, *tmpNeedle, textPos, needlePos, matchLen);
#endif
		if (*tmpText == *tmpNeedle) {
			matchLen++;
			if (index == -1) {
				index = textPos;
			}
			needlePos++;
			tmpNeedle++;
		}
		else {
			tmpNeedle = needle;
			needlePos = 0;
			matchLen = 0;
		}
		textPos++;
		tmpText++;
	}	

	if (matchLen == needleLength) {
#ifdef DEBUG
		logInfo( LOG_GLOBALFUNCTIONS,"indexOf: 3. We have a match. index=%d\n", index);
#endif
		return index;
	}

#ifdef DEBUG
		logInfo( LOG_GLOBALFUNCTIONS,"indexOf: 3. No match\n", index);
#endif
	return -1;
}

char *getFilename(char *fullPath)
{
	int filenameLen = 0;
	int fullPathLen = strlen(fullPath);
	char *tmpChar;

	tmpChar = fullPath + fullPathLen;
	while ((filenameLen <= fullPathLen) && (*tmpChar != 0x2F)) {
		filenameLen++;
		tmpChar--;
	}

	if (filenameLen < fullPathLen) {
		char *result = malloc(filenameLen+1);
		tmpChar++;
#ifdef DEBUG
		logInfo( LOG_GLOBALFUNCTIONS,"getFilename: filenameLen=%d, tmpChar=%s.\n", filenameLen, tmpChar);
#endif
		memset(result, 0, filenameLen+1);
		strncpy(result, tmpChar, filenameLen);
		return result;
	}
	else {
		return NULL;
	}
}

#define TIME_STR_MAX_SIZE 200

char *now(const char *format, int UTC)
{
	char *outstr = malloc(TIME_STR_MAX_SIZE);
	time_t t;
	struct tm *tmp;

	t = time(NULL);

	if (!UTC) {
		tmp = localtime(&t);
	}
	else {
		tmp = gmtime(&t);
	}
	if (tmp == NULL) {
		perror("now: localtime");
		free(outstr);
		return NULL;
	}

	if (strftime(outstr, TIME_STR_MAX_SIZE, format, tmp) == 0) {
		perror("now: strftime");
		free(outstr);
		return NULL;
	}

	return outstr;
}

uint64_t nowInMicroseconds()
{
	struct timespec now;
	if (clock_gettime(CLOCK_REALTIME, &now) == -1) return 0;

	return (now.tv_sec * 1000000) + (now.tv_nsec / 1000); // microseconds.
}

