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

#include "application_define.h"

void touch_thread_entry (void);

/* Touch Thread entry function */
void touch_thread_entry (void)
{
    UINT                       status;
    sf_touch_panel_payload_t * p_message = NULL;

#ifndef BSP_BOARD_S7G2_PE_HMI1 /* Pushbuttons are only present on the SK/DK-S7G2 boards */
    g_btn_down_irq.p_api->open(g_btn_down_irq.p_ctrl, g_btn_down_irq.p_cfg);
    g_btn_up_irq.p_api->open(g_btn_up_irq.p_ctrl, g_btn_up_irq.p_cfg);
#endif

    while (1)
    {
        status = g_sf_message.p_api->pend(g_sf_message.p_ctrl,
                                          &touch_thread_message_queue,
                                          (sf_message_header_t **) &p_message,
                                          TX_WAIT_FOREVER);
        APP_ERROR_TRAP(status)
        if ((p_message->header.event_b.class_code == SF_MESSAGE_EVENT_CLASS_TOUCH) &&
            (p_message->header.event_b.code == SF_MESSAGE_EVENT_NEW_DATA))
        {

#if !defined(__ICCARM__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
            GX_EVENT gxe =
            {
                .gx_event_type                                  = GX_NULL,
                .gx_event_sender                                = GX_ID_NONE,
                .gx_event_target                                = GX_ID_NONE,
                .gx_event_display_handle                        = GX_ID_NONE,
                .gx_event_payload.gx_event_pointdata.gx_point_x = (GX_VALUE) (p_message->x),
                .gx_event_payload.gx_event_pointdata.gx_point_y = (GX_VALUE) (p_message->y)
            };
#if !defined(__ICCARM__)
#pragma GCC diagnostic pop
#endif

#ifdef BSP_BOARD_S7G2_SK
            gxe.gx_event_payload.gx_event_pointdata.gx_point_y = (GX_VALUE) (320 - p_message->y);
#endif

#ifdef BSP_BOARD_S5D9_PK
            gxe.gx_event_payload.gx_event_pointdata.gx_point_y = (GX_VALUE) (320 - p_message->y);
#endif

            switch (p_message->event_type)
            {
                case SF_TOUCH_PANEL_EVENT_DOWN:
                {
                    gxe.gx_event_type = GX_EVENT_PEN_DOWN;
                    break;
                }

                case SF_TOUCH_PANEL_EVENT_UP:
                {
                    gxe.gx_event_type = GX_EVENT_PEN_UP;
                    break;
                }

                case SF_TOUCH_PANEL_EVENT_HOLD:
                case SF_TOUCH_PANEL_EVENT_MOVE:
                {
                    gxe.gx_event_type = GX_EVENT_PEN_DRAG;
                    break;
                }

                default:
                    break;
            }

            if (gxe.gx_event_type != GX_NULL)
            {
                status = gx_system_event_send(&gxe);
                APP_ERROR_TRAP(status)
            }
        }

        status = g_sf_message.p_api->bufferRelease(g_sf_message.p_ctrl,
                                                   (sf_message_header_t *) p_message,
                                                   SF_MESSAGE_RELEASE_OPTION_NONE);
        APP_ERROR_TRAP(status)
    }
}

#ifndef BSP_BOARD_S7G2_PE_HMI1 /* Pushbuttons are only present on the SK/DK-S7G2 boards */

void g_btn_down_callback (external_irq_callback_args_t * p_args)
{
    SSP_PARAMETER_NOT_USED(p_args);

    app_message_payload_t  * p_message = NULL;
    sf_message_acquire_cfg_t cfg_acquire =
    {
        .buffer_keep = false
    };
    sf_message_post_cfg_t    cfg_post    =
    {
        .priority = SF_MESSAGE_PRIORITY_NORMAL
    };

    g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl, (sf_message_header_t **) &p_message, &cfg_acquire,
                                      TX_NO_WAIT);

    p_message->header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_APP_CMD;
    p_message->header.event_b.class_instance = 0;
    p_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_VOL_DOWN;
    p_message->data.vol_change               = 15;

    g_sf_message.p_api->post(g_sf_message.p_ctrl, &p_message->header, &cfg_post, NULL, TX_NO_WAIT);
}

void g_btn_up_callback (external_irq_callback_args_t * p_args)
{
    SSP_PARAMETER_NOT_USED(p_args);

    app_message_payload_t  * p_message = NULL;
    sf_message_acquire_cfg_t cfg_acquire =
    {
        .buffer_keep = false
    };
    sf_message_post_cfg_t    cfg_post    =
    {
        .priority = SF_MESSAGE_PRIORITY_NORMAL
    };

    g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl, (sf_message_header_t **) &p_message, &cfg_acquire,
                                      TX_NO_WAIT);

    p_message->header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_APP_CMD;
    p_message->header.event_b.class_instance = 0;
    p_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_VOL_UP;
    p_message->data.vol_change               = 15;

    g_sf_message.p_api->post(g_sf_message.p_ctrl, &p_message->header, &cfg_post, NULL, TX_NO_WAIT);
}

#endif
