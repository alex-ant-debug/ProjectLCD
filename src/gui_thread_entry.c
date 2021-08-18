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
#ifdef BSP_BOARD_S7G2_SK
#include "lcd_setup/lcd_setup.h"
#endif
#ifdef BSP_BOARD_S5D9_PK
#include "lcd_setup/lcd_setup.h"
#endif

#include "guix_gen/audio_player_resources.h"
#include "guix_gen/audio_player_specifications.h"

void           gui_thread_entry (void);

GX_WINDOW_ROOT * p_window_root = NULL;

/* GUI Thread entry function */
void gui_thread_entry (void)
{
    UINT                    status;
    app_message_payload_t * p_message = NULL;

    status = gx_system_initialize();
    APP_ERROR_TRAP(status)

    status = g_sf_el_gx.p_api->open(g_sf_el_gx.p_ctrl, g_sf_el_gx.p_cfg);
    APP_ERROR_TRAP(status)

    status = gx_studio_display_configure(0, g_sf_el_gx.p_api->setup, 0, 0, &p_window_root);
    APP_ERROR_TRAP(status)

    status = g_sf_el_gx.p_api->canvasInit(g_sf_el_gx.p_ctrl, p_window_root);
    APP_ERROR_TRAP(status)

    status = gx_studio_named_widget_create("w_bg_splash", (GX_WIDGET *) p_window_root, GX_NULL);
    APP_ERROR_TRAP(status)

    status = gx_widget_show(p_window_root);
    APP_ERROR_TRAP(status)

    status = gx_system_start();
    APP_ERROR_TRAP(status)

#ifdef BSP_BOARD_S7G2_SK
    /** Open the SPI driver to initialize the LCD and setup ILI9341V driver **/
    status = ILI9341V_Init(&g_spi0);
    APP_ERROR_TRAP(status)
#endif

#ifdef BSP_BOARD_S5D9_PK
    /** Open the SPI driver to initialize the LCD and setup ILI9341V driver **/
    status = ILI9341V_Init(&g_spi0);
    APP_ERROR_TRAP(status)
#endif

#ifdef BSP_BOARD_S7G2_DK
    /** Enable the display */
    status = g_ioport.p_api->pinWrite(IOPORT_PORT_07_PIN_10, IOPORT_LEVEL_HIGH);
    APP_ERROR_TRAP(status)

    /** Enable display backlight */
    status = g_ioport.p_api->pinWrite(IOPORT_PORT_07_PIN_12, IOPORT_LEVEL_HIGH);
    APP_ERROR_TRAP(status)
#endif

#ifdef BSP_BOARD_S7G2_PE_HMI1
    /** Enable the display */
    status = g_ioport.p_api->pinWrite(IOPORT_PORT_10_PIN_03, IOPORT_LEVEL_HIGH);
    APP_ERROR_TRAP(status)

    /** Enable display backlight */
    status = g_ioport.p_api->pinWrite(IOPORT_PORT_10_PIN_05, IOPORT_LEVEL_HIGH);
    APP_ERROR_TRAP(status)
#endif

    status = tx_thread_resume(&touch_thread);
    APP_ERROR_TRAP(status)

    while (1)
    {
        status = g_sf_message.p_api->pend(g_sf_message.p_ctrl,
                                          &gui_thread_message_queue,
                                          (sf_message_header_t **) &p_message,
                                          TX_WAIT_FOREVER);
        if (!status && (p_message->header.event_b.class_code == SF_MESSAGE_EVENT_CLASS_APP_CB))
        {
            GX_EVENT gxe =
            {
                .gx_event_type           = GX_NULL,
                .gx_event_sender         = GX_ID_NONE,
                .gx_event_target         = GX_ID_NONE,
                .gx_event_display_handle = GX_ID_NONE
            };

            switch (p_message->header.event_b.code)
            {
                case SF_MESSAGE_EVENT_APP_CB_STATUS:
                {
                    gxe.gx_event_type = APP_EVENT_STATUS_CHANGED;

                    break;
                }

                case SF_MESSAGE_EVENT_APP_CB_VOLUME:
                {
                    gxe.gx_event_type = APP_EVENT_VOLUME_CHANGED;

                    break;
                }

                case SF_MESSAGE_EVENT_APP_CB_USB_IN:
                {
                    gxe.gx_event_type = APP_EVENT_USB_INSERTED;

                    break;
                }

                case SF_MESSAGE_EVENT_APP_CB_USB_OUT:
                {
                    gxe.gx_event_type = APP_EVENT_USB_REMOVED;

                    break;
                }

                case SF_MESSAGE_EVENT_APP_ERR_OPEN:
                case SF_MESSAGE_EVENT_APP_ERR_PAUSE:
                case SF_MESSAGE_EVENT_APP_ERR_RESUME:
                case SF_MESSAGE_EVENT_APP_ERR_CLOSE:
                case SF_MESSAGE_EVENT_APP_ERR_VOLUME:
                case SF_MESSAGE_EVENT_APP_ERR_PLAYBACK:
                {
                    gxe.gx_event_type                       = APP_EVENT_ERROR;
                    gxe.gx_event_payload.gx_event_ulongdata = p_message->header.event_b.code;

                    break;
                }

                case SF_MESSAGE_EVENT_APP_ERR_HEADER:
                {
                    gxe.gx_event_type                       = APP_EVENT_ERROR;
                    gxe.gx_event_payload.gx_event_ulongdata = p_message->data.error_code;

                    break;
                }
            }

            if (gxe.gx_event_type != GX_NULL)
            {
                status = gx_system_event_send(&gxe);
                APP_ERROR_TRAP(status)
            }
        }

        status = g_sf_message.p_api->bufferRelease(g_sf_message.p_ctrl,
                                                   (sf_message_header_t *) p_message,
                                                   SF_MESSAGE_RELEASE_OPTION_ACK);
        APP_ERROR_TRAP(status);
    }
}
