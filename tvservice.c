#include "bcm.h"
#include "vcos.h"
#include "globalFunctions.h"

#ifdef PI

#include <interface/vchiq_arm/vchiq_if.h> 

static int initialized = 0;

TV_GET_STATE_RESP_T *tvstate;

/**
 * Callback reason and arguments from HDMI middleware
 * Each callback comes with two optional uint32_t parameters.
 * Reason                     param1       param2      remark
 * VC_HDMI_UNPLUGGED            -            -         cable is unplugged
 * VC_HDMI_STANDBY            CEA/DMT      mode code   cable is plugged in and peripheral powered off (preferred mode sent back if available)
 * VC_HDMI_DVI                CEA/DMT      mode code   DVI mode is active at said resolution
 * VC_HDMI_HDMI               CEA(3D)/DMT  mode code   HDMI mode is active at said resolution (in 3D mode if CEA3D)
 * VC_HDMI_HDCP_UNAUTH        HDCP_ERROR_T  retry?     HDCP is inactive, the error can be none if we actively disable HDCP, if retry is non-zero, HDCP will attempt to reauthenticate
 * VC_HDMI_HDCP_AUTH            -            -         HDCP is active
 * VC_HDMI_HDCP_KEY_DOWNLOAD  success?       -         HDCP key download success (zero) or not (non zero)
 * VC_HDMI_HDCP_SRM_DOWNLOAD  no. of keys    -         HDCP revocation list download set no. of keys (zero means failure)
 * VC_HDMI_CHANGING_MODE        0            0         No information is supplied in this callback
 */

TVSERVICE_CALLBACK_T tvserviceCallback(void *callback_data, uint32_t reason, uint32_t param1, uint32_t param2)
{
	const char *reasonStr = vc_tv_notification_name((VC_HDMI_NOTIFY_T)reason);
	logInfo(LOG_TVSERVICE, "%s, param1=0x%x, param2=0x%x\n", reasonStr, param1, param2);
}

int tvserviceInit() 
{
	if (initialized) return 0;

	VCHI_INSTANCE_T vchi_instance;
	VCHI_CONNECTION_T *vchi_connections;

	// initialise bcm_host
	initializeBCM();
   
	// initialise vcos/vchi
	initializeVCOS();

	if (vchi_initialise(&vchi_instance) != VCHIQ_SUCCESS) {
		logInfo(LOG_TVSERVICE, "failed to open vchiq instance\n");
		return -2;
	}

	// create a vchi connection
	if ( vchi_connect( NULL, 0, vchi_instance ) != 0) {
		logInfo(LOG_TVSERVICE, "failed to connect to VCHI\n");
		return -3;
	}

	// connect to tvservice
	if ( vc_vchi_tv_init( vchi_instance, &vchi_connections, 1) != 0) {
		logInfo(LOG_TVSERVICE, "failed to connect to tvservice\n");
		return -4;
	}

	vc_tv_register_callback(&tvserviceCallback, NULL);

	tvstate = malloc(sizeof(TV_GET_STATE_RESP_T));

	vc_tv_get_state(tvstate);
	logInfo(LOG_TVSERVICE, "tvstate: %dx%d, %d fps, %s\n", tvstate->width, tvstate->height, tvstate->frame_rate, tvstate->scan_mode ? "interlaced" : "progressive");

	if (vc_tv_hdmi_set_spd("1st Setup", "piMythClient", HDMI_SPD_TYPE_PMP) != 0) {
		logInfo(LOG_TVSERVICE, "failed to vc_tv_hdmi_set_spd\n");
	}

	initialized = 1;

	return 0;
}

void tvserviceDestroy()
{
	if (!initialized) return;

	free(tvstate);

	vc_tv_unregister_callback(&tvserviceCallback);

	logInfo(LOG_TVSERVICE, "before vc_vchi_tv_stop\n");
	vc_vchi_tv_stop();
	logInfo(LOG_TVSERVICE, "afters vc_vchi_tv_stop\n");
}

int tvserviceHDMIPowerOn(int use3D)
{
	tvserviceInit();

	int result;

	if (use3D) {
		result = vc_tv_hdmi_power_on_preferred_3d();
	}
	else {
		result = vc_tv_hdmi_power_on_preferred();
	}

	if (result != 0) {
		logInfo(LOG_TVSERVICE, "tvserviceHDMIPowerOn failed\n");
	}

	return result;
}


//typedef enum {
//   HDMI_MODE_MATCH_NONE          = 0x0, /**<No mode*/
//   HDMI_MODE_MATCH_FRAMERATE     = 0x1, /**<Match based on frame rate */
//   HDMI_MODE_MATCH_RESOLUTION    = 0x2, /**<Match based on resolution */
//   HDMI_MODE_MATCH_SCANMODE      = 0x4  /**<Match based on scan mode */
// } EDID_MODE_MATCH_FLAG_T;

int tvserviceHDMIPowerOnBest(int use3D, uint32_t width, uint32_t height, uint32_t frame_rate, int interlaced)
{
	tvserviceInit();

	int result;

	if (use3D) {
		result = vc_tv_hdmi_power_on_best_3d(width, height, frame_rate, interlaced ? HDMI_INTERLACED : HDMI_NONINTERLACED, HDMI_MODE_MATCH_RESOLUTION | HDMI_MODE_MATCH_FRAMERATE | HDMI_MODE_MATCH_SCANMODE);
	}
	else {
		result = vc_tv_hdmi_power_on_best(width, height, frame_rate, interlaced ? HDMI_INTERLACED : HDMI_NONINTERLACED, HDMI_MODE_MATCH_RESOLUTION | HDMI_MODE_MATCH_FRAMERATE | HDMI_MODE_MATCH_SCANMODE);
	}

	if (result != 0) {
		logInfo(LOG_TVSERVICE, "tvserviceHDMIPowerOnBest failed\n");
	}

	return result;
}

int tvservicePowerOff()
{
	tvserviceInit();

	int result;

	result = vc_tv_power_off();

	if (result != 0) {
		logInfo(LOG_TVSERVICE, "tvservicePowerOff failed\n");
	}

	return result;
}

int tvserviceAVLatency()
{
	tvserviceInit();

	int result;

	result = vc_tv_hdmi_get_av_latency();

	if (result != 0) {
		logInfo(LOG_TVSERVICE, "tvserviceAVLatency failed\n");
	}

	return result;
}

#endif
