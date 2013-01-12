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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>

#include "connection.h"
#include "globalFunctions.h"
#include "mythProtocol.h"
#include "demuxer.h"

#include "lists.h"

#include "bcm.h"

#ifdef PI

#include "vgfont.h"
#include "tvservice.h"

#endif

#include "osd.h"

#define PORT        6543
             /* REPLACE with your server machine name*/
#define HOST        "virtualbox.verbraak.thuis"

#define STDIN 0

struct MYTH_CONNECTION_T *masterConnection = NULL;
struct MYTH_CONNECTION_T *slaveConnection = NULL;
struct MYTH_CONNECTION_T *monitorConnection = NULL;

void changeSTDIN()
{
	// This will change the stdin in to non line editing mode.
	// So we can read each character/keyboardpress as it is done.
	struct termios term;

	if (tcgetattr(STDIN, &term) == 0) {
		term.c_lflag &= ~ICANON;
		term.c_cc[VMIN] = 0;
		term.c_cc[VTIME] = 0;
		if (tcsetattr(STDIN, TCSANOW, &term) != 0) {
			logInfo(LOG_CLIENT, "Error on tcsetattr. Error=%d\n", errno);
		}
	}
	else {
		logInfo(LOG_CLIENT, "Error on tcgetattr. Error=%d\n", errno);
	}
}

int main(int argc, char *argv[])
{
	int error = 0;
	char response[6000];
	struct LISTITEM_T *tmpDetails;
	fd_set our_fd_set, working_fd_set;
	int maxSocket;
	struct timeval timeout;
	int rc;
	char *hostname = HOST;
	int port = PORT;
	int startChannelNum = 1;
	struct DEMUXER_T *demuxer = NULL;
	uint8_t stdinBuffer;
	uint32_t newChannel = 0;
	uint32_t newChannel2 = 0;
	uint32_t currentChannel = 0;
	int doStop = 0;
	int channelChanged = 0;
	int audioPassthrough = 1;
	int showVideo = 1;
	int playAudio = 1;
	char *recordingFilename = NULL;

	int c;
	opterr = 0;

/*
	-h <hostname>
	-p <port>
	-c <channelnumber>
	-l <comma separated list of logattributes to turn on>  "client, mythprotocol, demuxer, omx, client-debug, mythprotocol-debug, demuxer-debug, omx-debug"
	-t <language code> "e.g. dut for dutch"
	-a <0|1> "Set audio on or off. Default 1 (on)"
	-v <0|1> "Set video on or off. Default 1 (on)"
	-e <0|1> "Set audio passthrough on. Decoding is done externally. Default 0 (off)"
	-r <recording filename | list>  list will show details of all recordings while a "recording name" will start the playback of a the specified recording.
			e.g.: "recording filename" == 20024_20121201183000.mpg  <-  file name can be found in recording directory of mythtvbackend. 
*/

	while ((c = getopt(argc, argv, "h:p:c:l:t:e:a:v:r:")) != -1)
		switch (c) {
		case 'h':
			hostname = optarg;
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 'c':
			startChannelNum = atoi(optarg);
			break;
		case 'l':
			setLogLevelByStr(optarg);
			break;
		case 't':
			demuxerSetLanguage(optarg);
			break;
		case 'e':
			audioPassthrough = atoi(optarg);
			break;
		case 'a':
			playAudio = atoi(optarg);
			break;
		case 'v':
			showVideo = atoi(optarg);
			break;
		case 'r':
			recordingFilename = optarg;
			break;
		}

	changeSTDIN();

	initializeBCM();

#ifdef PI
	tvserviceInit();

//	tvservicePowerOff();
//	sleep(1);
//	tvserviceHDMIPowerOnBest(0, 1080, 720, 50, 0);
//	sleep(1);

 	struct OSD_T *osd2 = osdCreate(1, 0, 0, GRAPHICS_RGBA32(255,0,0,0xff));
	if (osd2 == NULL) {
		printf("osdCreate osd2 error.\n");
		exit(1);
	}

	osdClear(osd2);
	osdDrawRect(osd2, 10, 10, 110, 110, GRAPHICS_RGBA32(0,255,0,125));
	osdDrawRect(osd2, 20, 20, 80, 80, GRAPHICS_RGBA32(0,0,255,255));

//	osdDrawText(osd2, 200, 200, "DIT IS EEN TEST BERICHT", 50);
	osdDraw(osd2);

/*	struct OSD_T *osd1 = osdCreate(2, 500, 500, 0x80);
	if (osd1 == NULL) {
		printf("osdCreate osd1 error.\n");
		exit(1);
	}
	osdDraw(osd1);
*/
/*	int i;
	osdDrawText(osd, 0, 0, "Hello", 30);
	for(i=0; i<1000; i=i+100) {
		osdShow(osd, i, i);
		sleep(5);
	}*/

	logInfo( LOG_CLIENT,"the end.\n");
	sleep(1);

//	osdDestroy(osd1);
	osdDestroy(osd2);

//	tvserviceDestroy();
	bcm_host_deinit();

//	exit(1);
#else
	struct OSD_T *img = osdCreate(1, 0, 0);
	osdDrawText(img, 0, 0, "Hello", 11);
	osdDestroy(img);
#endif

	masterConnection = createMythConnection(hostname, port, ANN_PLAYBACK);
	if (masterConnection == NULL) {
		error = -1;
		logInfo( LOG_CLIENT,"Could not create master Connection.\n");
		return -1;
	}

#ifdef DEBUG
	logInfo( LOG_CLIENT,"main: We have a valid connection to the master backend.\n");
#endif

	if (error >= 0) {
		error = mythRefreshBackend(masterConnection);
	}

	if ((error >= 0) && (recordingFilename != NULL) && (strcmp(recordingFilename, "list") == 0)) {
		logInfo( LOG_CLIENT,"Going to show a list of recordings.\n");

		mythQueryRecordings(masterConnection, "Descending");
		destroyMythConnection(masterConnection);
		return 0;
	}

	if (error >= 0) {
		monitorConnection = createMythConnection(hostname, port, ANN_MONITOR);
		if (monitorConnection == NULL) {
			error = -1;
		}
	}

	if (error >= 0) {
		if (recordingFilename == NULL) {
			slaveConnection = startLiveTV(masterConnection, startChannelNum);
		}
		else {
			slaveConnection = checkRecorderProgram(masterConnection, recordingFilename);
		}
		if (slaveConnection == NULL) {
			error = -1;
		}
		newChannel2 = startChannelNum;
		currentChannel = startChannelNum;
	}

	FD_ZERO(&our_fd_set);

	FD_SET(monitorConnection->connection->socket, &our_fd_set);
	maxSocket = monitorConnection->connection->socket;

	FD_SET(STDIN, &our_fd_set);
	if (STDIN > maxSocket) {
		maxSocket = STDIN;
	}
	
	int changingChannel = 0;

	if ((error >=0) && (recordingFilename != NULL)) {
		error = playRecorderProgram(slaveConnection);
		if (error >= 0) {
			demuxer = demuxerStart(slaveConnection, showVideo, playAudio, audioPassthrough);
		}
	}
	
	if (error >= 0) {
		int firstStartRecording = 0;
		while (doStop == 0) {
			memcpy(&working_fd_set, &our_fd_set, sizeof(our_fd_set));

			timeout.tv_sec  = 1;
			timeout.tv_usec = 0;

			rc = select(maxSocket+1, &working_fd_set, NULL, NULL, &timeout);
			if (rc < 0) {
				perror("main: select error");
				exit(1);
			}

			if (rc == 0) {
				//logInfo( LOG_CLIENT," __ main: select timeout.\n");
			}

			if (rc > 0) {

				if ((FD_ISSET(monitorConnection->connection->socket, &working_fd_set)) && (changingChannel == 0)) {

					readResponse(monitorConnection->connection, &response[0], 6000);
					// We need to trigger in BACKEND_MESSAGE[]:[]SYSTEM_EVENT REC_STARTED CARDID 29 CHANID 11010 STARTTIME 2013-01-09T20:00:05Z RECSTATUS 0 SENDER xen01[]:[]empty
					if ((checkResponse(&response[0], "BACKEND_MESSAGE[]:[]SYSTEM_EVENT REC_STARTED CARDID") != 0) && (recordingFilename == NULL)) {
						// We have an update see if it is for us.
						tmpDetails = convertStrToList(&response[0], " ");
						if (slaveConnection->recorderId == atoi(getStringAtListIndex(tmpDetails,3))) {  // Check recorderId
							if (firstStartRecording == 1) {
								char *newFileName = mythConvertToFilename( getStringAtListIndex(tmpDetails,5), getStringAtListIndex(tmpDetails,7));
								if (slaveConnection->streaming == 0) {
									logInfo(LOG_CLIENT, "Going to request file=%s.\n", newFileName);
									slaveConnection->channelId = atoi(getStringAtListIndex(tmpDetails,5));
									slaveConnection->transferConnection = mythPrepareNextProgram(slaveConnection, newFileName); 
									if (slaveConnection->transferConnection != NULL) {
										logInfo( LOG_CLIENT," *********************> starting demuxer thread.\n");
										sleep(5); // We let it sleep for 5 seconds so mythtv can buffer up.
										demuxer = demuxerStart(slaveConnection, showVideo, playAudio, audioPassthrough);
										if (demuxer == NULL) {
											logInfo( LOG_CLIENT," *********************> Error starting demuxer thread.\n");
										}
										else {
											slaveConnection->streaming = 1;
										}
									}
								}
								else {
									logInfo(LOG_CLIENT, "Change of program on same channel. Will tell demuxer it needs to stream from a new file %s.\n", newFileName);
									demuxerSetNextFile(demuxer, newFileName);
								}
								free(newFileName);
							}
							else {
								// We do this because when the first start recording is seen mythtv is still tuning in on the channel. 
								// And will stop this recording when tuned. Then it will start the real recording. 
								logInfo(LOG_CLIENT, "First startrecording we see. We wait for the next one.\n");
								firstStartRecording++;
							}
						}
						freeList(tmpDetails);

/*						if ((strcmp(getStringAtListIndex(tmpDetails,6), getStringAtListIndex(slaveConnection->currentRecording,6)) == 0) &&
							(strcmp(getStringAtListIndex(tmpDetails,25), getStringAtListIndex(slaveConnection->currentRecording,25)) > 0)){
							// it is for us update currentRecording details. pos 53
							if (slaveConnection->streaming == 1) {
								// we are streaming. Close file and we will reopen it when data is available.
								logInfo( LOG_CLIENT,"main: We need to start a new stream.\n");
								if (p != NULL) {
									fclose(p);
								}
								// Stop stream.
								demuxerStop(demuxer);
								demuxer = NULL;

								stopLiveTVStream(slaveConnection);
								logInfo( LOG_CLIENT,"main: Stopped stream.\n");
							}
							freeList(slaveConnection->currentRecording);
							slaveConnection->currentRecording = tmpDetails;
							logInfo( LOG_CLIENT," it is for us update currentRecording details. data=%s\n",convertListToString(slaveConnection->currentRecording, "[]:[]"));
						}
*/					}

/*					if ((slaveConnection->streaming == 0) && (checkResponse(&response[0], "BACKEND_MESSAGE[]:[]UPDATE_FILE_SIZE") != 0) && (recordingFilename == NULL)) {
						// We have an update_file_size see if it is for us.
						tmpDetails = convertStrToList(&response[0], " ");
						logInfo( LOG_CLIENT," We have an update_file_size see if it is for us. newChannelId=%s, oldChannelId=%s\n", getStringAtListIndex(tmpDetails,1),getStringAtListIndex(slaveConnection->currentRecording,6));
						if (strcmp(getStringAtListIndex(tmpDetails,1), getStringAtListIndex(slaveConnection->currentRecording,6)) == 0) {
							// it is for us. Start receiving data.
							logInfo( LOG_CLIENT," it is for us. Start receiving data.\n");
							if (startLiveTVStream(slaveConnection) >= 0) {
								logInfo( LOG_CLIENT," *********************> starting demuxer thread.\n");
								demuxer = demuxerStart(slaveConnection, showVideo, playAudio, audioPassthrough);
								if (demuxer == NULL) {
									logInfo( LOG_CLIENT," *********************> Error starting demuxer thread.\n");
								}
							}
						}
						freeList(tmpDetails);
					}
*/				}

				if (FD_ISSET(STDIN, &working_fd_set)) {
					if (read(STDIN, &stdinBuffer, 1) == 1) {
						logInfo(LOG_CLIENT_DEBUG, "Received character on stdin: %d\n", stdinBuffer);
						if ((stdinBuffer >= 48) && (stdinBuffer <= 57)) { // digit keys 0..9
							newChannel = (newChannel * 10) + (stdinBuffer - 48);
							logInfo(LOG_CLIENT, "newChannel=%d.\n", newChannel);
						}
						if ((stdinBuffer == 10) && (recordingFilename != NULL)) {
							channelChanged = 0;
							if (newChannel != 0) { // enter key
								// Received enter. Switch to new channel when it is not 0.

								if (newChannel != currentChannel) {
									channelChanged = 1;
								}
								newChannel2 = newChannel;
								newChannel = 0;
							}
							else {
								if (newChannel2 != currentChannel) {
									channelChanged = 1;
								}
							}

							if (channelChanged) {
								// Received enter. Switch to new channel2 .
								logInfo(LOG_CLIENT, "Going to switch to channel %d\n", newChannel2);

								changingChannel = 1;

								demuxerStop(demuxer);
								demuxer = NULL;
								if (slaveConnection->streaming == 1) {
									stopLiveTVStream(slaveConnection);
								}

								destroyMythConnection(slaveConnection);
								slaveConnection = startLiveTV(masterConnection, newChannel2);
								if (slaveConnection == NULL) {
									error = -1;
									logInfo(LOG_CLIENT, "Error switching to channel %d\n", newChannel2);
								}
								else {
									currentChannel = newChannel2;
									logInfo(LOG_CLIENT, "Switched to channel %d\n", newChannel2);
								}

								logInfo( LOG_CLIENT," slaveConnection ChannelId=%s\n",getStringAtListIndex(slaveConnection->currentRecording,6));
								logInfo( LOG_CLIENT," slaveConnection StartTime=%s\n", getStringAtListIndex(slaveConnection->currentRecording,25));
								changingChannel = 0;
							}
						}

						if (stdinBuffer == 27) {  // ESC key
							newChannel = 0;
						}

						if (stdinBuffer == 43) {  // + key
							newChannel2 += 1;
							logInfo(LOG_CLIENT, "newChannel2=%d.\n", newChannel2);
						}
						if (stdinBuffer == 45) {  // - key
							newChannel2 -= 1;
							logInfo(LOG_CLIENT, "newChannel2=%d.\n", newChannel2);
						}

						if (stdinBuffer == 113) { // q key for quit
							logInfo(LOG_CLIENT, "Received quit from userconsole.\n");
							demuxerStop(demuxer);
							if (slaveConnection->streaming == 1) {
								stopLiveTVStream(slaveConnection);
							}
							doStop = 1;
						}
					}
				}			
			}
		}
	}


	destroyMythConnection(masterConnection);
	destroyMythConnection(slaveConnection);

	return 0;
}

 

