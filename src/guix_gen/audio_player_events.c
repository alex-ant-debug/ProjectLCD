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

/***********************************************************************************************************************
 * Includes
 ***********************************************************************************************************************/
#include "application_define.h"

#include "audio_player_resources.h"
#include "audio_player_specifications.h"

/***********************************************************************************************************************
 * Private function prototypes
 ***********************************************************************************************************************/
static UINT app_gx_goto_window (char * name, GX_WIDGET * p_current, GX_WIDGET * p_new, bool destroy);

static UINT app_gx_refresh_file_names (UINT first_index);

static UINT app_gx_set_controls (void);

static UINT app_fx_get_file_list (void);

/***********************************************************************************************************************
 * Private global variables
 ***********************************************************************************************************************/
static GX_CHAR             g_file_list[ENTRIES_PER_DIR][FX_MAX_SHORT_NAME_LEN + 1];

extern app_player_status_t g_player_status;
extern FX_MEDIA          * gp_media;
extern GX_WINDOW_ROOT    * p_window_root;

/** Splash Screen Event Handler */
UINT w_splash_event (GX_WINDOW * widget, GX_EVENT * event_ptr)
{
    UINT        status;
    static bool usb_detected = false;

    switch (event_ptr->gx_event_type)
    {
        case GX_EVENT_SHOW:
        {
            status = gx_prompt_text_set(&w_bg_splash.w_bg_splash_txt_version, APP_VERSION);
            APP_ERROR_TRAP(status)

            status = gx_system_timer_start(widget, APP_TIMER_SPLASH, 150, 0);
            APP_ERROR_TRAP(status)

            return gx_window_event_process(widget, event_ptr);
        }

        case GX_EVENT_TIMER:
        {
            if (usb_detected)
            {
                status = app_gx_goto_window("w_bg_main", (GX_WIDGET *) widget, (GX_WIDGET *) &w_bg_main, true);
                APP_ERROR_TRAP(status)
            }
            else
            {
                status = app_gx_goto_window("w_bg_usb", (GX_WIDGET *) widget, (GX_WIDGET *) &w_bg_usb, true);
            }

            APP_ERROR_TRAP(status)
            break;
        }

        case APP_EVENT_USB_INSERTED:
        {
            usb_detected = true;
            return gx_window_event_process(widget, event_ptr);
        }

        case APP_EVENT_USB_REMOVED:
        {
            usb_detected = false;
            return gx_window_event_process(widget, event_ptr);
        }

        default:
            return gx_window_event_process(widget, event_ptr);
    }

    return 0;
}

/** USB Screen Event Handler */
UINT w_usb_event (GX_WINDOW * widget, GX_EVENT * event_ptr)
{
    UINT status;

    switch (event_ptr->gx_event_type)
    {
        case GX_EVENT_SHOW:
        {
            return gx_window_event_process(widget, event_ptr);
        }

        case APP_EVENT_USB_INSERTED:
        {
            status = app_gx_goto_window("w_bg_main", (GX_WIDGET *) widget, (GX_WIDGET *) &w_bg_main, true);
            APP_ERROR_TRAP(status)
            break;
        }

        default:
            return gx_window_event_process(widget, event_ptr);
    }

    return 0;
}

/** Main Screen Event Handler */
UINT w_main_event (GX_WINDOW * widget, GX_EVENT * event_ptr)
{
    UINT                     status;
    static USHORT            g_file_list_index;

    app_message_payload_t  * p_message   = NULL;
    sf_message_acquire_cfg_t cfg_acquire =
    {
        .buffer_keep = false
    };
    sf_message_post_cfg_t    cfg_post    =
    {
        .priority = SF_MESSAGE_PRIORITY_NORMAL
    };

    switch (event_ptr->gx_event_type)
    {
        case GX_EVENT_SHOW:
        {
            app_fx_get_file_list();
            g_file_list_index = 0;
            app_gx_refresh_file_names(g_file_list_index);

            status = gx_progress_bar_pixelmap_set(&w_bg_main.w_bg_main_bar_volume, GX_PIXELMAP_ID_BAR_VOL);
            APP_ERROR_TRAP(status);
            status = gx_progress_bar_value_set(&w_bg_main.w_bg_main_bar_volume, g_player_status.volume);
            APP_ERROR_TRAP(status);

            return gx_window_event_process(widget, event_ptr);
        }

        case APP_EVENT_USB_REMOVED:
        {
            app_gx_goto_window("w_bg_usb", (GX_WIDGET *) widget, (GX_WIDGET *) &w_bg_usb, true);

            break;
        }

        case GX_SIGNAL(ID_BTN_LEFT, GX_EVENT_CLICKED):
        {
            switch (g_player_status.state)
            {
                case SF_AUDIO_PLAYBACK_STATUS_STOPPED:
                {
                    if (!g_file_list[g_file_list_index][0])
                    {
                        break;
                    }

                    app_gx_refresh_file_names(++g_file_list_index);

                    break;
                }

                case SF_AUDIO_PLAYBACK_STATUS_PLAYING:
                {
                    /* Post REWIND command */
                    status = g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl,
                                                               (sf_message_header_t **) &p_message,
                                                               &cfg_acquire,
                                                               TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    p_message->header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_APP_CMD;
                    p_message->header.event_b.class_instance = 0;
                    p_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_REWIND;
                    p_message->data.seek_msec                = 10000;

                    status                                   = g_sf_message.p_api->post(g_sf_message.p_ctrl,
                                                                                        &p_message->header,
                                                                                        &cfg_post,
                                                                                        NULL,
                                                                                        TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    break;
                }

                case SF_AUDIO_PLAYBACK_STATUS_PAUSED:
                {
                    /* Post RESTART command */
                    status = g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl,
                                                               (sf_message_header_t **) &p_message,
                                                               &cfg_acquire,
                                                               TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    p_message->header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_APP_CMD;
                    p_message->header.event_b.class_instance = 0;
                    p_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_RESTART;

                    status                                   = g_sf_message.p_api->post(g_sf_message.p_ctrl,
                                                                                        &p_message->header,
                                                                                        &cfg_post,
                                                                                        NULL,
                                                                                        TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    break;
                }

                default:
                    break;
            }

            return gx_window_event_process(widget, event_ptr);
        }

        case GX_SIGNAL(ID_BTN_RIGHT, GX_EVENT_CLICKED):
        {
            switch (g_player_status.state)
            {
                case SF_AUDIO_PLAYBACK_STATUS_STOPPED:
                {
                    if (!g_file_list_index)
                    {
                        break;
                    }

                    app_gx_refresh_file_names(--g_file_list_index);

                    break;
                }

                case SF_AUDIO_PLAYBACK_STATUS_PLAYING:
                {
                    /* Post FORWARD command */
                    status = g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl,
                                                               (sf_message_header_t **) &p_message,
                                                               &cfg_acquire,
                                                               TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    p_message->header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_APP_CMD;
                    p_message->header.event_b.class_instance = 0;
                    p_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_FORWARD;
                    p_message->data.seek_msec                = 10000;

                    status                                   = g_sf_message.p_api->post(g_sf_message.p_ctrl,
                                                                                        &p_message->header,
                                                                                        &cfg_post,
                                                                                        NULL,
                                                                                        TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    break;
                }

                case SF_AUDIO_PLAYBACK_STATUS_PAUSED:
                {
                    /* Post CLOSE command */
                    status = g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl,
                                                               (sf_message_header_t **) &p_message,
                                                               &cfg_acquire,
                                                               TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    p_message->header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_APP_CMD;
                    p_message->header.event_b.class_instance = 0;
                    p_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_CLOSE;

                    status                                   = g_sf_message.p_api->post(g_sf_message.p_ctrl,
                                                                                        &p_message->header,
                                                                                        &cfg_post,
                                                                                        NULL,
                                                                                        TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    break;
                }

                default:
                    break;
            }

            return gx_window_event_process(widget, event_ptr);
        }

        case GX_SIGNAL(ID_BTN_MID, GX_EVENT_CLICKED):
        {
            switch (g_player_status.state)
            {
                case SF_AUDIO_PLAYBACK_STATUS_STOPPED:
                {
                    INT g_file_index;

                    status        = gx_vertical_list_selected_index_get(&w_bg_main.w_bg_main_list_files, &g_file_index);
                    APP_ERROR_TRAP(status)
                    g_file_index += g_file_list_index;

                    status        = fx_directory_name_test(gp_media, (CHAR *) g_file_list[g_file_index]);
                    if (!status)
                    {
                        status = fx_directory_default_set(gp_media, (CHAR *) g_file_list[g_file_index]);
                        APP_ERROR_TRAP(status)

                        app_fx_get_file_list();
                        g_file_list_index = 0;
                        app_gx_refresh_file_names(g_file_list_index);

                        break;
                    }
                    else if (status != FX_NOT_DIRECTORY)
                    {
                        /* Media error */

                        break;
                    }

                    /* Post OPEN command */
                    status = g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl,
                                                               (sf_message_header_t **) &p_message,
                                                               &cfg_acquire,
                                                               TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    p_message->header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_APP_CMD;
                    p_message->header.event_b.class_instance = 0;
                    p_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_OPEN;
                    status                                   = fx_directory_short_name_get(gp_media,
                                                                                           (CHAR *) g_file_list[
                                                                                               g_file_index],
                                                                                           p_message->data.file_name);
                    APP_ERROR_TRAP(status);

                    status = g_sf_message.p_api->post(g_sf_message.p_ctrl,
                                                      &p_message->header,
                                                      &cfg_post,
                                                      NULL,
                                                      TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    break;
                }

                case SF_AUDIO_PLAYBACK_STATUS_PLAYING:
                {
                    /* Post PAUSE command */
                    status = g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl,
                                                               (sf_message_header_t **) &p_message,
                                                               &cfg_acquire,
                                                               TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    p_message->header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_APP_CMD;
                    p_message->header.event_b.class_instance = 0;
                    p_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_PAUSE;

                    status                                   = g_sf_message.p_api->post(g_sf_message.p_ctrl,
                                                                                        &p_message->header,
                                                                                        &cfg_post,
                                                                                        NULL,
                                                                                        TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    break;
                }

                case SF_AUDIO_PLAYBACK_STATUS_PAUSED:
                {
                    /* Post RESUME command */
                    status = g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl,
                                                               (sf_message_header_t **) &p_message,
                                                               &cfg_acquire,
                                                               TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    p_message->header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_APP_CMD;
                    p_message->header.event_b.class_instance = 0;
                    p_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_RESUME;

                    status                                   = g_sf_message.p_api->post(g_sf_message.p_ctrl,
                                                                                        &p_message->header,
                                                                                        &cfg_post,
                                                                                        NULL,
                                                                                        TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    break;
                }

                default:
                    break;
            }

            return gx_window_event_process(widget, event_ptr);
        }

        case APP_EVENT_STATUS_CHANGED:
        {
            app_gx_set_controls();

            return gx_window_event_process(widget, event_ptr);
        }

        case APP_EVENT_VOLUME_CHANGED:
        {
            status = gx_progress_bar_value_set(&w_bg_main.w_bg_main_bar_volume, g_player_status.volume);
            APP_ERROR_TRAP(status);

            return gx_window_event_process(widget, event_ptr);
        }

        case APP_EVENT_ERROR:
        {
            switch (event_ptr->gx_event_payload.gx_event_ulongdata)
            {
                case SF_MESSAGE_EVENT_APP_ERR_OPEN:
                {
                    status = gx_prompt_text_id_set(&w_bg_main.w_bg_main_txt_status, GX_STRING_ID_ERR_OPEN);
                    APP_ERROR_TRAP(status)

                    break;
                }

                case SF_MESSAGE_EVENT_APP_ERR_PAUSE:
                {
                    status = gx_prompt_text_id_set(&w_bg_main.w_bg_main_txt_status, GX_STRING_ID_ERR_PAUSE);
                    APP_ERROR_TRAP(status)

                    break;
                }

                case SF_MESSAGE_EVENT_APP_ERR_RESUME:
                {
                    status = gx_prompt_text_id_set(&w_bg_main.w_bg_main_txt_status, GX_STRING_ID_ERR_RESUME);
                    APP_ERROR_TRAP(status)

                    break;
                }

                case SF_MESSAGE_EVENT_APP_ERR_CLOSE:
                {
                    status = gx_prompt_text_id_set(&w_bg_main.w_bg_main_txt_status, GX_STRING_ID_ERR_CLOSE);
                    APP_ERROR_TRAP(status)

                    break;
                }

                case SF_MESSAGE_EVENT_APP_ERR_VOLUME:
                {
                    status = gx_prompt_text_id_set(&w_bg_main.w_bg_main_txt_status, GX_STRING_ID_ERR_VOLUME);
                    APP_ERROR_TRAP(status)

                    break;
                }

                case SF_MESSAGE_EVENT_APP_ERR_PLAYBACK:
                {
                    status = gx_prompt_text_id_set(&w_bg_main.w_bg_main_txt_status, GX_STRING_ID_ERR_PLAYBACK);
                    APP_ERROR_TRAP(status)

                    break;
                }

                case APP_ERR_NOT_RIFF:
                {
                    status = gx_prompt_text_id_set(&w_bg_main.w_bg_main_txt_status, GX_STRING_ID_ERR_NOT_RIFF);
                    APP_ERROR_TRAP(status)

                    break;
                }

                case APP_ERR_NOT_WAVE:
                {
                    status = gx_prompt_text_id_set(&w_bg_main.w_bg_main_txt_status, GX_STRING_ID_ERR_NOT_WAVE);
                    APP_ERROR_TRAP(status)

                    break;
                }

                case APP_ERR_NOT_PCM:
                {
                    status = gx_prompt_text_id_set(&w_bg_main.w_bg_main_txt_status, GX_STRING_ID_ERR_NOT_PCM);
                    APP_ERROR_TRAP(status)

                    break;
                }

                case APP_ERR_NOT_MONO:
                {
                    status = gx_prompt_text_id_set(&w_bg_main.w_bg_main_txt_status, GX_STRING_ID_ERR_NOT_MONO);
                    APP_ERROR_TRAP(status)

                    break;
                }

                case APP_ERR_NOT_44KHZ:
                {
                    status = gx_prompt_text_id_set(&w_bg_main.w_bg_main_txt_status, GX_STRING_ID_ERR_NOT_44KHZ);
                    APP_ERROR_TRAP(status)

                    break;
                }

                case APP_ERR_NOT_16BIT:
                {
                    status = gx_prompt_text_id_set(&w_bg_main.w_bg_main_txt_status, GX_STRING_ID_ERR_NOT_16BIT);
                    APP_ERROR_TRAP(status)

                    break;
                }

                case APP_ERR_NO_DATA_CHUNK:
                {
                    status = gx_prompt_text_id_set(&w_bg_main.w_bg_main_txt_status, GX_STRING_ID_ERR_NO_DATA_CHUNK);
                    APP_ERROR_TRAP(status)

                    break;
                }

                default:
                    return gx_window_event_process(widget, event_ptr);
            }

            status = gx_system_timer_start((GX_WIDGET *) &w_bg_main, APP_TIMER_STATUS, 150, 0);
            APP_ERROR_TRAP(status)

            return gx_window_event_process(widget, event_ptr);
        }

        case GX_EVENT_TIMER:
        {
            switch (event_ptr->gx_event_payload.gx_event_timer_id)
            {
                case APP_TIMER_PLAYBACK:
                {
                    static GX_CHAR txt_buffer[14];

                    sprintf(txt_buffer, "%02u:%02u / %02u:%02u",
                            g_player_status.current_time / 60, g_player_status.current_time % 60,
                            g_player_status.total_time   / 60, g_player_status.total_time   % 60);
                    status = gx_prompt_text_set(&w_bg_main.w_bg_main_txt_time, txt_buffer);
                    APP_ERROR_TRAP(status)

                    return gx_window_event_process(widget, event_ptr);
                }

                case APP_TIMER_STATUS:
                {
                    status = gx_prompt_text_id_set(&w_bg_main.w_bg_main_txt_status, GX_STRING_ID_STOPPED);
                    APP_ERROR_TRAP(status)

                    gx_system_timer_stop((GX_WIDGET *) &w_bg_main, APP_TIMER_STATUS);

                    return gx_window_event_process(widget, event_ptr);
                }
            }

            return gx_window_event_process(widget, event_ptr);
        }

#ifdef BSP_BOARD_S7G2_PE_HMI1

        /* Implementing on-screen volume controls as PE-HMI1 board hasn't got physical pushbuttons */

        case GX_SIGNAL(ID_BTN_VOL_UP, GX_EVENT_CLICKED):
        {
            g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl, (sf_message_header_t **) &p_message, &cfg_acquire,
                                              TX_NO_WAIT);

            p_message->header.event_b.class          = SF_MESSAGE_EVENT_CLASS_APP_CMD;
            p_message->header.event_b.class_instance = 0;
            p_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_VOL_UP;
            p_message->data.vol_change               = 15;

            g_sf_message.p_api->post(g_sf_message.p_ctrl, &p_message->header, &cfg_post, NULL, TX_NO_WAIT);

            return gx_window_event_process(widget, event_ptr);
        }

        case GX_SIGNAL(ID_BTN_VOL_DOWN, GX_EVENT_CLICKED):
        {
            g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl, (sf_message_header_t **) &p_message, &cfg_acquire,
                                              TX_NO_WAIT);

            p_message->header.event_b.class          = SF_MESSAGE_EVENT_CLASS_APP_CMD;
            p_message->header.event_b.class_instance = 0;
            p_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_VOL_DOWN;
            p_message->data.vol_change               = 15;

            g_sf_message.p_api->post(g_sf_message.p_ctrl, &p_message->header, &cfg_post, NULL, TX_NO_WAIT);

            return gx_window_event_process(widget, event_ptr);
        }

#endif

        default:
            return gx_window_event_process(widget, event_ptr);
    }

    return 0;
}

/***********************************************************************************************************************
 * Functions
 ***********************************************************************************************************************/

static UINT app_gx_goto_window (char * name, GX_WIDGET * p_current, GX_WIDGET * p_new, bool destroy)
{
    UINT status;

    status = gx_studio_named_widget_create(name, GX_NULL, GX_NULL);
    if(GX_ALREADY_CREATED != status)
        APP_ERROR_RETURN(status)

    if (destroy)
    {
        status = gx_widget_detach(p_current);
        APP_ERROR_RETURN(status)
    }

    if (p_window_root->gx_widget_first_child == GX_NULL)
    {
        status = gx_widget_attach(p_window_root, p_new);
        APP_ERROR_RETURN(status)
    }

    status = gx_system_focus_claim(p_new);
    APP_ERROR_RETURN(status)

    return GX_SUCCESS;
}

static UINT app_gx_refresh_file_names (UINT first_index)
{
    UINT           status, i;
    static GX_CHAR file_name[5][FX_MAX_LONG_NAME_LEN];

    memset(file_name, 0, sizeof(file_name));

    for (i = 0; i < 5; i++)
    {
        status = fx_directory_long_name_get(gp_media, (CHAR *) g_file_list[first_index + i], (CHAR *) file_name[i]);
        if (status)
        {
            strcpy(file_name[i], "");
        }
    }

    if (!strcmp(g_file_list[first_index], ".."))
    {
        strcpy(file_name[0], "<- go back");
    }

    status = gx_prompt_text_set(&w_bg_main.w_bg_main_list_item_0, file_name[0]);
    APP_ERROR_RETURN(status)
    status = gx_prompt_text_set(&w_bg_main.w_bg_main_list_item_1, file_name[1]);
    APP_ERROR_RETURN(status)
    status = gx_prompt_text_set(&w_bg_main.w_bg_main_list_item_2, file_name[2]);
    APP_ERROR_RETURN(status)
    status = gx_prompt_text_set(&w_bg_main.w_bg_main_list_item_3, file_name[3]);
    APP_ERROR_RETURN(status)
    status = gx_prompt_text_set(&w_bg_main.w_bg_main_list_item_4, file_name[4]);
    APP_ERROR_RETURN(status)

    return GX_SUCCESS;
}

static UINT app_gx_set_controls (void)
{
    UINT status;

    switch (g_player_status.state)
    {
        case SF_AUDIO_PLAYBACK_STATUS_STOPPED:
        {
            status = gx_prompt_text_id_set(&w_bg_main.w_bg_main_txt_status, GX_STRING_ID_STOPPED);
            APP_ERROR_RETURN(status)
            status = gx_prompt_text_id_set(&w_bg_main.w_bg_main_txt_time, GX_STRING_ID_BLANK);
            APP_ERROR_RETURN(status)
            status = gx_pixelmap_button_pixelmap_set(&w_bg_main.w_bg_main_btn_left,
                                                     GX_PIXELMAP_ID_BTN_DOWN, 0, 0);
            APP_ERROR_RETURN(status)
            status = gx_pixelmap_button_pixelmap_set(&w_bg_main.w_bg_main_btn_mid,
                                                     GX_PIXELMAP_ID_BTN_CIRCLE, 0, 0);
            APP_ERROR_RETURN(status)
            status = gx_pixelmap_button_pixelmap_set(&w_bg_main.w_bg_main_btn_right,
                                                     GX_PIXELMAP_ID_BTN_UP, 0, 0);
            APP_ERROR_RETURN(status)

            gx_system_timer_stop((GX_WIDGET *) &w_bg_main, APP_TIMER_PLAYBACK);
            gx_system_timer_stop((GX_WIDGET *) &w_bg_main, APP_TIMER_STATUS);

            break;
        }

        case SF_AUDIO_PLAYBACK_STATUS_PAUSED:
        {
            status = gx_prompt_text_id_set(&w_bg_main.w_bg_main_txt_status, GX_STRING_ID_PAUSED);
            APP_ERROR_RETURN(status)
            status = gx_pixelmap_button_pixelmap_set(&w_bg_main.w_bg_main_btn_left,
                                                     GX_PIXELMAP_ID_BTN_RESTART, 0, 0);
            APP_ERROR_RETURN(status)
            status = gx_pixelmap_button_pixelmap_set(&w_bg_main.w_bg_main_btn_mid,
                                                     GX_PIXELMAP_ID_BTN_PLAY, 0, 0);
            APP_ERROR_RETURN(status)
            status = gx_pixelmap_button_pixelmap_set(&w_bg_main.w_bg_main_btn_right,
                                                     GX_PIXELMAP_ID_BTN_EJECT, 0, 0);
            APP_ERROR_RETURN(status)

            gx_system_timer_stop((GX_WIDGET *) &w_bg_main, APP_TIMER_PLAYBACK);
            gx_system_timer_stop((GX_WIDGET *) &w_bg_main, APP_TIMER_STATUS);

            break;
        }

        case SF_AUDIO_PLAYBACK_STATUS_PLAYING:
        {
            status = gx_prompt_text_id_set(&w_bg_main.w_bg_main_txt_status, GX_STRING_ID_PLAYING);
            APP_ERROR_RETURN(status)
            status = gx_pixelmap_button_pixelmap_set(&w_bg_main.w_bg_main_btn_left,
                                                     GX_PIXELMAP_ID_BTN_REW, 0, 0);
            APP_ERROR_RETURN(status)
            status = gx_pixelmap_button_pixelmap_set(&w_bg_main.w_bg_main_btn_mid,
                                                     GX_PIXELMAP_ID_BTN_PAUSE, 0, 0);
            APP_ERROR_RETURN(status)
            status = gx_pixelmap_button_pixelmap_set(&w_bg_main.w_bg_main_btn_right,
                                                     GX_PIXELMAP_ID_BTN_FWD, 0, 0);
            APP_ERROR_RETURN(status)

            status = gx_system_timer_start((GX_WIDGET *) &w_bg_main, APP_TIMER_PLAYBACK, 5, 5);
            APP_ERROR_TRAP(status)

            gx_system_timer_stop((GX_WIDGET *) &w_bg_main, APP_TIMER_STATUS);

            break;
        }

        default:
            break;
    }

    return GX_SUCCESS;
}

static UINT app_fx_get_file_list (void)
{
    UINT status, attributes, i = 0;
    CHAR dir_name[FX_MAX_LONG_NAME_LEN];

    memset(g_file_list, 0, sizeof(g_file_list));

    status = fx_directory_first_full_entry_find(gp_media, dir_name, &attributes,
                                                0, 0, 0, 0, 0, 0, 0);

    /* Filter out sub-directory dot entry */
    if (!strcmp((GX_CHAR *) dir_name, "."))
    {
        status = fx_directory_next_full_entry_find(gp_media, dir_name, &attributes,
                                                   0, 0, 0, 0, 0, 0, 0);
    }

    /* First pass: only directories are added to the list */
    while (!status)
    {
        if (attributes & FX_DIRECTORY)
        {
            fx_directory_short_name_get(gp_media, dir_name, (CHAR *) g_file_list[i++]);
        }

        status = fx_directory_next_full_entry_find(gp_media, dir_name, &attributes,
                                                   0, 0, 0, 0, 0, 0, 0);
    }

    status = fx_directory_first_full_entry_find(gp_media, dir_name, &attributes,
                                                0, 0, 0, 0, 0, 0, 0);

    /* Second pass: volume name and directory entries are filtered out */
    while (!status)
    {
        if (!((attributes & FX_VOLUME) || (attributes & FX_DIRECTORY)))
        {
            fx_directory_short_name_get(gp_media, dir_name, (CHAR *) g_file_list[i++]);
        }

        status = fx_directory_next_full_entry_find(gp_media, dir_name, &attributes,
                                                   0, 0, 0, 0, 0, 0, 0);
    }

    return FX_SUCCESS;
}

VOID list_files_callback (GX_VERTICAL_LIST * list, GX_WIDGET * widget, INT index)
{
    SSP_PARAMETER_NOT_USED(list);
    SSP_PARAMETER_NOT_USED(widget);
    SSP_PARAMETER_NOT_USED(index);
}
