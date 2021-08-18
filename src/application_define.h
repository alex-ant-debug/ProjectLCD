/***********************************************************************************************************************
* Copyright [2016] Renesas Electronics Corporation and/or its licensors. All Rights Reserved.
* 
 * This file is part of Renesas SynergyTM Software Package (SSP)
*
* The contents of this file (the "contents") are proprietary and confidential to Renesas Electronics Corporation
* and/or its licensors ("Renesas") and subject to statutory and contractual protections.
*
* This file is subject to a Renesas SSP license agreement. Unless otherwise agreed in an SSP license agreement with
* Renesas: 1) you may not use, copy, modify, distribute, display, or perform the contents; 2) you may not use any name
* or mark of Renesas for advertising or publicity purposes or in connection with your use of the contents; 3) RENESAS
* MAKES NO WARRANTY OR REPRESENTATIONS ABOUT THE SUITABILITY OF THE CONTENTS FOR ANY PURPOSE; THE CONTENTS ARE PROVIDED
* "AS IS" WITHOUT ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
* PARTICULAR PURPOSE, AND NON-INFRINGEMENT; AND 4) RENESAS SHALL NOT BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, OR
* CONSEQUENTIAL DAMAGES, INCLUDING DAMAGES RESULTING FROM LOSS OF USE, DATA, OR PROJECTS, WHETHER IN AN ACTION OF
* CONTRACT OR TORT, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THE CONTENTS. Third-party contents
* included in this file may be subject to different terms.
**********************************************************************************************************************/

#ifndef APPLICATION_DEFINE_H_
#define APPLICATION_DEFINE_H_

#include "fx_api.h"
#include "ux_api.h"
#include "ux_hcd_synergy.h"
#include "ux_host_class_storage.h"

#include "application_payloads.h"

#include "hal_data.h"
#include "gui_thread.h"
#include "touch_thread.h"
#include "audio_thread.h"
#include "usb_thread.h"

#define APP_VERSION "v1.2.0"

#define ENTRIES_PER_DIR 100
#define AUDIO_BUFFER_SIZE \
    SF_AUDIO_PLAYBACK_CFG_BUFFER_SIZE_BYTES

extern TX_THREAD usb_thread;
extern TX_THREAD gui_thread;
extern TX_THREAD audio_thread;
extern TX_THREAD touch_thread;

typedef struct st_app_player_status
{
    sf_audio_playback_status_t state;
    USHORT current_time;
    USHORT total_time;
    SHORT volume;
} app_player_status_t;

typedef struct st_app_wav_info
{
    struct
    {
        USHORT format;
        USHORT channels;
        USHORT sample_rate;
        USHORT resolution;
    } header;
    struct
    {
        SHORT  offset;
        UINT   size;
    } data;
} app_wav_info_t;

#define APP_EVENT_STATUS_CHANGED (GX_FIRST_APP_EVENT + 1)
#define APP_EVENT_VOLUME_CHANGED (GX_FIRST_APP_EVENT + 2)
#define APP_EVENT_USB_INSERTED   (GX_FIRST_APP_EVENT + 3)
#define APP_EVENT_USB_REMOVED    (GX_FIRST_APP_EVENT + 4)

#define APP_EVENT_ERROR          (GX_FIRST_APP_EVENT + 5)

#define APP_ERR_NOT_RIFF         (SF_MESSAGE_EVENT_APP_LAST + 1)
#define APP_ERR_NOT_WAVE         (SF_MESSAGE_EVENT_APP_LAST + 2)
#define APP_ERR_NOT_PCM          (SF_MESSAGE_EVENT_APP_LAST + 3)
#define APP_ERR_NOT_MONO         (SF_MESSAGE_EVENT_APP_LAST + 4)
#define APP_ERR_NOT_44KHZ        (SF_MESSAGE_EVENT_APP_LAST + 5)
#define APP_ERR_NOT_16BIT        (SF_MESSAGE_EVENT_APP_LAST + 6)
#define APP_ERR_NO_DATA_CHUNK    (SF_MESSAGE_EVENT_APP_LAST + 7)

#define APP_TIMER_SPLASH         (100)
#define APP_TIMER_PLAYBACK       (101)
#define APP_TIMER_STATUS         (102)

#define APP_ERROR_RETURN(a) if(a) return a;
#define APP_ERROR_BREAK(a) if(a) break;
#define APP_ERROR_TRAP(a) if(a) while(1);

#endif /* APPLICATION_DEFINE_H_ */
