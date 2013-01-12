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
