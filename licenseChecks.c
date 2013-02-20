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
#include <interface/vmcs_host/vcgencmd.h>

#include "bcm.h"
#include "licenseChecks.h"

#define MAX_COMMAND_RESPONSE_SIZE 1024

char *sendVCGenCmd(char *command)
{
	initializeBCM();

	int gencmdHandle = vc_gencmd_init();

	if (gencmdHandle != 0) {
		printf("Error vc_gencmd_init. Error=%d.\n", gencmdHandle);
		return NULL;
	}

	char *response = malloc(MAX_COMMAND_RESPONSE_SIZE);
	memset(response, 0, MAX_COMMAND_RESPONSE_SIZE);

	int vc_error = vc_gencmd(response, MAX_COMMAND_RESPONSE_SIZE - 1, command);

	vc_gencmd_stop();

	if (vc_error != 0) {
		printf("Error vc_gencmd. Error=%d.\n", vc_error);
		free(response);
		return NULL;
	}

	printf("vc_gencmd response: %s.\n", response);
	return response;
}

int sendVCGenCmdAndCheckToExpectedResponse(char *command, char *expectedResponse)
{
	char *check = sendVCGenCmd(command);

	if (check == NULL) {
		return 0;
	}

	int result = (strcmp(check, expectedResponse) == 0) ? 1 : 0;
	free(check);

	return result;
}

int licenseH264Available = -1;

int licenseH264IsInstalled()
{
	if (licenseH264Available > -1) return licenseH264Available;

	licenseH264Available = sendVCGenCmdAndCheckToExpectedResponse("codec_enabled H264", "H264=enabled");
	return licenseH264Available;
}

int licenseMPEG2Available = -1;

int licenseMPEG2IsInstalled()
{
	if (licenseMPEG2Available > -1) return licenseMPEG2Available;

	licenseMPEG2Available = sendVCGenCmdAndCheckToExpectedResponse("codec_enabled MPG2", "MPG2=enabled");
	return licenseMPEG2Available;
}

int licenseVC1Available = -1;

int licenseVC1IsInstalled()
{
	if (licenseVC1Available > -1) return licenseVC1Available;

	licenseVC1Available = sendVCGenCmdAndCheckToExpectedResponse("codec_enabled WVC1", "WVC1=enabled");
	return licenseVC1Available;
}

