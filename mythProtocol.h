
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
