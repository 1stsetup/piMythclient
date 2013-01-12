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


#define RESPONSESIZE     (64 * 1024)

#define CLIENT_PROTOCOL_VERSION "75"
#define CLIENT_PROTOCOL_TOKEN "SweetRock"
//#define CLIENT_PROTOCOL_VERSION "76"
//#define CLIENT_PROTOCOL_TOKEN "FireWilde"

#define COMMAND_LENGTH_PADDING 8
#define MAX_COMMAND_LENGTH 1024
#define MAX_RESPONSE_LENGTH 1024

struct MYTH_CONNECTION_T{
	struct	CONNECTION_T *connection;
	int connected;
	char *hostname;
	int port;
	int annType;
	int recorderId;
	char *chainId;
	int channelId;
	int channelNum;
	int pip;
	struct MYTH_CONNECTION_T *masterConnection;
	struct MYTH_CONNECTION_T *backendConnection;
	struct LISTITEM_T *currentRecording;
	char *fileName;
	int transferSocket;
	struct MYTH_CONNECTION_T *transferConnection;
	int streaming;
	pthread_mutex_t readWriteLock;
	unsigned long long int position;
};

struct MYTH_RECORDER_T{
	int recorderId;
	char *hostname;
	int port;
};

#define ANN_PLAYBACK		1
#define ANN_FILETRANSFER	2
#define ANN_MONITOR		3

ssize_t sendCommand(struct CONNECTION_T *connection, char *inCommand);
ssize_t readResponse(struct CONNECTION_T *connection, char *outResponse, size_t responseLength);
int sendCommandAndReadReply(struct MYTH_CONNECTION_T *connection, char *inCommand, char *outResponse, size_t responseLength);
int checkResponse(char *response, char *needle);
struct MYTH_CONNECTION_T *createMythConnection(char *inHostname, uint16_t port, int annType);
void destroyMythConnection(struct MYTH_CONNECTION_T *mythConnection);
int mythAnnPlayback(struct MYTH_CONNECTION_T *mythConnection);
int mythAnnFileTransfer(struct MYTH_CONNECTION_T *mythConnection, char *filename);
int mythSpawnLiveTV(struct MYTH_CONNECTION_T *mythConnection, struct MYTH_CONNECTION_T *masterConnection, int recorderId, char *chainId, int channelNum, int pip);
char *mythConvertToFilename(char *channelId, char *startTime);
struct MYTH_CONNECTION_T *startLiveTV(struct MYTH_CONNECTION_T *masterConnection, int channelNum);
int startLiveTVStream(struct MYTH_CONNECTION_T *mythConnection);
int stopLiveTVStream(struct MYTH_CONNECTION_T *mythConnection);
struct MYTH_CONNECTION_T *checkRecorderProgram(struct MYTH_CONNECTION_T *masterConnection, char *recordingFilename);
int playRecorderProgram(struct MYTH_CONNECTION_T *mythConnection);
void mythSetNewTransferConnection(struct MYTH_CONNECTION_T *slaveConnection, struct MYTH_CONNECTION_T *newTransferConnection);
struct MYTH_CONNECTION_T *mythPrepareNextProgram(struct MYTH_CONNECTION_T *mythConnection, char *newFilename);
unsigned long long int mythFiletransferRequestBlock(struct MYTH_CONNECTION_T *mythConnection, unsigned long long int blockLen);
unsigned long long int mythFiletransferSeek(struct MYTH_CONNECTION_T *mythConnection, unsigned long long int position, int whence, unsigned long long int currentPos);
int mythFiletransferDone(struct MYTH_CONNECTION_T *mythConnection);
int mythAnnMonitor(struct MYTH_CONNECTION_T *mythConnection);
struct LISTITEM_T *mythQueryRecordings(struct MYTH_CONNECTION_T *mythConnection, char *sort);
struct MYTH_RECORDER_T *mythGetNextFreeRecorder(struct MYTH_CONNECTION_T *mythConnection, int currentRecorderId);
int mythQueryRecorderCheckChannel(struct MYTH_CONNECTION_T *mythConnection, int recorderId, int channel);
int mythRefreshBackend(struct MYTH_CONNECTION_T *mythConnection);

