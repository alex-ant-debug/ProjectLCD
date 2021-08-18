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

#ifndef APPLICATION_PAYLOADS_H_
#define APPLICATION_PAYLOADS_H_

#include "fx_api.h"
#include "sf_message.h"

typedef struct st_app_message_payload
{
    sf_message_header_t header;
    union
    {
        CHAR  file_name[FX_MAX_SHORT_NAME_LEN + 1];
        ULONG seek_msec;
        UCHAR vol_change;
        UINT  error_code;
    } data;
} app_message_payload_t;

#endif /* APPLICATION_PAYLOADS_H_ */
