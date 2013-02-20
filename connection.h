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

#include <arpa/inet.h>

struct CONNECTION_T{
	int	socket;
	char	*buffer;
	unsigned int	bufferPos;
	unsigned int	bufferLen;
	unsigned int	bufferEnd;
	pthread_mutex_t readWriteLock;
	int isLocked;

};

struct CONNECTION_T *createConnection(char *inHostname, uint16_t port);
unsigned long long int fillConnectionBuffer(struct CONNECTION_T *connection, unsigned long long int responseLength, int readFull);
unsigned long long int peekConnectionBuffer(struct CONNECTION_T *connection, char *dstBuffer, int len);
unsigned long long int readConnectionBuffer(struct CONNECTION_T *connection, char *dstBuffer, int len);
void clearConnectionBuffer(struct CONNECTION_T *connection);
void destroyConnection(struct CONNECTION_T *connection);
unsigned long long int getConnectionDataLen(struct CONNECTION_T *connection);


