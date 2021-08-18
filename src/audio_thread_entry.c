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

void audio_thread_entry (void);

/** Local function declarations */
static UINT         app_read_wav_header ();

/** Global variables */
static FX_FILE          g_file;
static app_wav_info_t   g_file_info;
extern FX_MEDIA       * gp_media;

app_player_status_t g_player_status =
{
    .state  = SF_AUDIO_PLAYBACK_STATUS_STOPPED,
    .volume = 255
};

/** Local variables */
static ULONG   g_actual_size[2];
static CHAR    g_file_buffer[2][AUDIO_BUFFER_SIZE];
static uint8_t g_current_buffer = 0;

#if !defined(__ICCARM__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
const sf_audio_playback_data_t ref_audio_playback_data =
{
    .header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_AUDIO,
    .header.event_b.class_instance = 0,
    .header.event_b.code           = SF_MESSAGE_EVENT_AUDIO_START,
    .type.is_signed                = 1,
    .type.scale_bits_max           = 16,
    .loop_timeout                  = TX_NO_WAIT,
    .stream_end                    = false
};
#if !defined(__ICCARM__)
#pragma GCC diagnostic pop
#endif

void sf_audio_playback_callback (sf_message_callback_args_t * p_args)
{
    SSP_PARAMETER_NOT_USED(p_args);
    tx_semaphore_put(&g_sf_audio_playback_semaphore);
}

/* Audio Thread entry function */
void audio_thread_entry (void)
{
    UINT                       status;
    sf_audio_playback_data_t * p_audio_playback_data = NULL;
    app_message_payload_t    * p_cmd_message         = NULL;
    app_message_payload_t    * p_cb_message          = NULL;
    sf_message_acquire_cfg_t   cfg_acquire           =
    {
        .buffer_keep = false
    };
    sf_message_post_cfg_t      cfg_post              =
    {
        .priority = SF_MESSAGE_PRIORITY_NORMAL
    };

#ifdef BSP_BOARD_S7G2_DK
    /* Enable audio amplifier on DK-S7G2 */
    status = g_ioport.p_api->pinWrite(IOPORT_PORT_09_PIN_02, IOPORT_LEVEL_HIGH);
    APP_ERROR_TRAP(status)
#endif

#ifdef BSP_BOARD_S7G2_PE_HMI1
    /* Enable audio amplifier on PE-HMI1 */
    status = g_ioport.p_api->pinWrite(IOPORT_PORT_10_PIN_04, IOPORT_LEVEL_HIGH);
    APP_ERROR_TRAP(status)
#endif

    while (1)
    {
        status = g_sf_message.p_api->pend(g_sf_message.p_ctrl,
                                          &audio_thread_message_queue,
                                          (sf_message_header_t **) &p_cmd_message,
                                          1);
        if (!status && (p_cmd_message->header.event_b.class_code == SF_MESSAGE_EVENT_CLASS_APP_CMD))
        {
            switch (p_cmd_message->header.event_b.code)
            {
                case SF_MESSAGE_EVENT_APP_OPEN:
                {
                    if (g_player_status.state != SF_AUDIO_PLAYBACK_STATUS_STOPPED)
                    {
                        /* API error: file is already open */
                        break;
                    }

                    // the extension checking can be skipped as the program recognizes wav riff headers

                    status = fx_file_open(gp_media, &g_file, p_cmd_message->data.file_name, FX_OPEN_FOR_READ);
                    if (status)
                    {
                        // post error callback: unable to open file
                        fx_file_close(&g_file);

                        break;
                    }

                    // initialize the stream
                    status = app_read_wav_header();
                    if (status)
                    {
                        /* App error: incompatible file */
                        UINT error_code = status;
                        fx_file_close(&g_file);

                        status = g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl,
                                                                   (sf_message_header_t **) &p_cb_message,
                                                                   &cfg_acquire,
                                                                   TX_NO_WAIT);
                        APP_ERROR_TRAP(status);

                        p_cb_message->header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_APP_CB;
                        p_cb_message->header.event_b.class_instance = 0;
                        p_cb_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_ERR_HEADER;
                        p_cb_message->data.error_code               = error_code;

                        status                                      = g_sf_message.p_api->post(g_sf_message.p_ctrl,
                                                                                               &p_cb_message->header,
                                                                                               &cfg_post,
                                                                                               NULL,
                                                                                               TX_NO_WAIT);
                        APP_ERROR_TRAP(status);

                        break;
                    }

                    g_current_buffer = 0;
                    memset(&g_file_buffer, 0, AUDIO_BUFFER_SIZE * 2);
                    g_actual_size[0] = AUDIO_BUFFER_SIZE;
                    g_actual_size[1] = AUDIO_BUFFER_SIZE;

                    // playback timer should be reset to zero already (upon file closure)

                    tx_semaphore_ceiling_put(&g_sf_audio_playback_semaphore, 1);

                    status = g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl,
                                                               (sf_message_header_t **) &p_audio_playback_data,
                                                               &cfg_acquire,
                                                               10);
                    APP_ERROR_TRAP(status)

                    * p_audio_playback_data           = ref_audio_playback_data;
                    p_audio_playback_data->size_bytes = g_actual_size[g_current_buffer];
                    p_audio_playback_data->p_data     = g_file_buffer[g_current_buffer];

                    status                            = g_sf_audio_playback.p_api->start(g_sf_audio_playback.p_ctrl,
                                                                                         p_audio_playback_data,
                                                                                         TX_NO_WAIT);
                    APP_ERROR_TRAP(status)

                    g_player_status.total_time =
                        (USHORT) ((g_file_info.data.size - (ULONG) g_file_info.data.offset) /
                                   (ULONG) (g_file_info.header.sample_rate * 2));

                    g_player_status.state = SF_AUDIO_PLAYBACK_STATUS_PLAYING;

                    /* App callback: status changed */
                    status = g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl,
                                                               (sf_message_header_t **) &p_cb_message,
                                                               &cfg_acquire,
                                                               TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    p_cb_message->header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_APP_CB;
                    p_cb_message->header.event_b.class_instance = 0;
                    p_cb_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_CB_STATUS;

                    status                                      = g_sf_message.p_api->post(g_sf_message.p_ctrl,
                                                                                           &p_cb_message->header,
                                                                                           &cfg_post,
                                                                                           NULL,
                                                                                           TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    break;
                }

                case SF_MESSAGE_EVENT_APP_PAUSE:
                {
                    if (g_player_status.state != SF_AUDIO_PLAYBACK_STATUS_PLAYING)
                    {
                        /* API error: file must be playing */
                        break;
                    }

                    status = g_sf_audio_playback.p_api->pause(g_sf_audio_playback.p_ctrl);
                    if (status)
                    {
                        // post error callback: unable to pause playback

                        break;
                    }

                    g_player_status.state = SF_AUDIO_PLAYBACK_STATUS_PAUSED;

                    /* App callback: status changed */
                    status = g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl,
                                                               (sf_message_header_t **) &p_cb_message,
                                                               &cfg_acquire,
                                                               TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    p_cb_message->header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_APP_CB;
                    p_cb_message->header.event_b.class_instance = 0;
                    p_cb_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_CB_STATUS;

                    status                                      = g_sf_message.p_api->post(g_sf_message.p_ctrl,
                                                                                           &p_cb_message->header,
                                                                                           &cfg_post,
                                                                                           NULL,
                                                                                           TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    break;
                }

                case SF_MESSAGE_EVENT_APP_RESUME:
                {
                    if (g_player_status.state != SF_AUDIO_PLAYBACK_STATUS_PAUSED)
                    {
                        /* API error: file must be paused */
                        break;
                    }

                    status = g_sf_audio_playback.p_api->resume(g_sf_audio_playback.p_ctrl);
                    if (status)
                    {
                        // post error callback: unable to resume playback

                        break;
                    }

                    g_player_status.state = SF_AUDIO_PLAYBACK_STATUS_PLAYING;

                    /* App callback: status changed */
                    status = g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl,
                                                               (sf_message_header_t **) &p_cb_message,
                                                               &cfg_acquire,
                                                               TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    p_cb_message->header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_APP_CB;
                    p_cb_message->header.event_b.class_instance = 0;
                    p_cb_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_CB_STATUS;

                    status                                      = g_sf_message.p_api->post(g_sf_message.p_ctrl,
                                                                                           &p_cb_message->header,
                                                                                           &cfg_post,
                                                                                           NULL,
                                                                                           TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    break;
                }

                case SF_MESSAGE_EVENT_APP_CLOSE:
                {
                    if (g_player_status.state == SF_AUDIO_PLAYBACK_STATUS_STOPPED)
                    {
                        /* API error: file is already closed */
                        break;
                    }

                    status = g_sf_audio_playback.p_api->stop(g_sf_audio_playback.p_ctrl);
                    if (status)
                    {
                        // post error callback: unable to stop playback

                        break;
                    }

                    g_player_status.state = SF_AUDIO_PLAYBACK_STATUS_STOPPED;

                    // clear buffers
                    memset(&g_file_buffer, 0, sizeof(g_file_buffer));
                    memset(&g_actual_size, 0, sizeof(g_actual_size));

                    status = fx_file_close(&g_file);
                    if (status)
                    {
                        // post error callback: unable to close file
                    }

                    /* App callback: status changed */
                    status = g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl,
                                                               (sf_message_header_t **) &p_cb_message,
                                                               &cfg_acquire,
                                                               TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    p_cb_message->header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_APP_CB;
                    p_cb_message->header.event_b.class_instance = 0;
                    p_cb_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_CB_STATUS;

                    status                                      = g_sf_message.p_api->post(g_sf_message.p_ctrl,
                                                                                           &p_cb_message->header,
                                                                                           &cfg_post,
                                                                                           NULL,
                                                                                           TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    break;
                }

                case SF_MESSAGE_EVENT_APP_REWIND:
                {
                    if (g_player_status.state == SF_AUDIO_PLAYBACK_STATUS_STOPPED)
                    {
                        /* API error: file must be open */
                        break;
                    }

                    status =
                        fx_file_relative_seek(&g_file,
                                              ((p_cmd_message->data.seek_msec * g_file_info.header.sample_rate *
                                                2) / 1000), FX_SEEK_BACK);
                    APP_ERROR_TRAP(status)

                    if (g_file.fx_file_current_file_offset < (USHORT) g_file_info.data.offset)
                    {
                        fx_file_seek(&g_file, (ULONG) g_file_info.data.offset);
                    }

                    /* App callback: status changed */
                    status = g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl,
                                                               (sf_message_header_t **) &p_cb_message,
                                                               &cfg_acquire,
                                                               TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    p_cb_message->header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_APP_CB;
                    p_cb_message->header.event_b.class_instance = 0;
                    p_cb_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_CB_STATUS;

                    status                                      = g_sf_message.p_api->post(g_sf_message.p_ctrl,
                                                                                           &p_cb_message->header,
                                                                                           &cfg_post,
                                                                                           NULL,
                                                                                           TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    break;
                }

                case SF_MESSAGE_EVENT_APP_FORWARD:
                {
                    if (g_player_status.state == SF_AUDIO_PLAYBACK_STATUS_STOPPED)
                    {
                        /* API error: file must be open */
                        break;
                    }

                    status =
                        fx_file_relative_seek(&g_file,
                                              ((p_cmd_message->data.seek_msec * g_file_info.header.sample_rate *
                                                2) / 1000), FX_SEEK_FORWARD);
                    APP_ERROR_TRAP(status)

                    /* App callback: status changed */
                    status = g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl,
                                                               (sf_message_header_t **) &p_cb_message,
                                                               &cfg_acquire,
                                                               TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    p_cb_message->header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_APP_CB;
                    p_cb_message->header.event_b.class_instance = 0;
                    p_cb_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_CB_STATUS;

                    status                                      = g_sf_message.p_api->post(g_sf_message.p_ctrl,
                                                                                           &p_cb_message->header,
                                                                                           &cfg_post,
                                                                                           NULL,
                                                                                           TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    break;
                }

                case SF_MESSAGE_EVENT_APP_GOTO:
                {
                    if (g_player_status.state == SF_AUDIO_PLAYBACK_STATUS_STOPPED)
                    {
                        /* API error: file must be open */
                        break;
                    }

                    status =
                        fx_file_seek(&g_file,
                                     (ULONG) (((p_cmd_message->data.seek_msec * g_file_info.header.sample_rate *
                                                2) / 1000) + (ULONG) g_file_info.data.offset));
                    APP_ERROR_TRAP(status)

                    /* App callback: status changed */
                    status = g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl,
                                                               (sf_message_header_t **) &p_cb_message,
                                                               &cfg_acquire,
                                                               TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    p_cb_message->header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_APP_CB;
                    p_cb_message->header.event_b.class_instance = 0;
                    p_cb_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_CB_STATUS;

                    status                                      = g_sf_message.p_api->post(g_sf_message.p_ctrl,
                                                                                           &p_cb_message->header,
                                                                                           &cfg_post,
                                                                                           NULL,
                                                                                           TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    break;
                }

                case SF_MESSAGE_EVENT_APP_RESTART:
                {
                    if (g_player_status.state != SF_AUDIO_PLAYBACK_STATUS_PAUSED)
                    {
                        /* API error: playback must be paused */
                        break;
                    }

                    status = fx_file_seek(&g_file, (ULONG) g_file_info.data.offset);
                    APP_ERROR_TRAP(status)

                    g_current_buffer = 0;
                    memset(&g_file_buffer, 0, AUDIO_BUFFER_SIZE * 2);
                    g_actual_size[0] = AUDIO_BUFFER_SIZE;
                    g_actual_size[1] = AUDIO_BUFFER_SIZE;

                    status           = g_sf_audio_playback.p_api->resume(g_sf_audio_playback.p_ctrl);
                    APP_ERROR_TRAP(status);

                    g_player_status.state = SF_AUDIO_PLAYBACK_STATUS_PLAYING;

                    /* App callback: status changed */
                    status = g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl,
                                                               (sf_message_header_t **) &p_cb_message,
                                                               &cfg_acquire,
                                                               TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    p_cb_message->header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_APP_CB;
                    p_cb_message->header.event_b.class_instance = 0;
                    p_cb_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_CB_STATUS;

                    status                                      = g_sf_message.p_api->post(g_sf_message.p_ctrl,
                                                                                           &p_cb_message->header,
                                                                                           &cfg_post,
                                                                                           NULL,
                                                                                           TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    break;
                }

                case SF_MESSAGE_EVENT_APP_VOL_UP:
                {
                    g_player_status.volume = (SHORT) (g_player_status.volume +
                                                      p_cmd_message->data.vol_change);

                    if (g_player_status.volume > 255)
                    {
                        g_player_status.volume = 255;
                    }

                    status = g_sf_audio_playback.p_api->volumeSet(g_sf_audio_playback.p_ctrl,
                                                                  (uint8_t) g_player_status.volume);
                    APP_ERROR_TRAP(status);

                    /* App callback: volume changed */
                    status = g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl,
                                                               (sf_message_header_t **) &p_cb_message,
                                                               &cfg_acquire,
                                                               TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    p_cb_message->header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_APP_CB;
                    p_cb_message->header.event_b.class_instance = 0;
                    p_cb_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_CB_VOLUME;

                    status                                      = g_sf_message.p_api->post(g_sf_message.p_ctrl,
                                                                                           &p_cb_message->header,
                                                                                           &cfg_post,
                                                                                           NULL,
                                                                                           TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    break;
                }

                case SF_MESSAGE_EVENT_APP_VOL_DOWN:
                {
                    g_player_status.volume = (SHORT) (g_player_status.volume -
                                                      p_cmd_message->data.vol_change);

                    if (g_player_status.volume < 1)
                    {
                        g_player_status.volume = 1;
                    }

                    status = g_sf_audio_playback.p_api->volumeSet(g_sf_audio_playback.p_ctrl,
                                                                  (uint8_t) g_player_status.volume);
                    APP_ERROR_TRAP(status);

                    /* App callback: volume changed */
                    status = g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl,
                                                               (sf_message_header_t **) &p_cb_message,
                                                               &cfg_acquire,
                                                               TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    p_cb_message->header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_APP_CB;
                    p_cb_message->header.event_b.class_instance = 0;
                    p_cb_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_CB_VOLUME;

                    status                                      = g_sf_message.p_api->post(g_sf_message.p_ctrl,
                                                                                           &p_cb_message->header,
                                                                                           &cfg_post,
                                                                                           NULL,
                                                                                           TX_NO_WAIT);
                    APP_ERROR_TRAP(status);

                    break;
                }
            }

            status = g_sf_message.p_api->bufferRelease(g_sf_message.p_ctrl,
                                                       (sf_message_header_t *) p_cmd_message,
                                                       SF_MESSAGE_RELEASE_OPTION_ACK);
            APP_ERROR_TRAP(status);
        }
        else
        {
            switch (g_player_status.state)
            {
                case SF_AUDIO_PLAYBACK_STATUS_PLAYING:
                {
                    status = tx_semaphore_get(&g_sf_audio_playback_semaphore, TX_NO_WAIT);
                    if (!status)
                    {
                        status = fx_file_read(&g_file,
                                              &g_file_buffer[!g_current_buffer],
                                              AUDIO_BUFFER_SIZE,
                                              &g_actual_size[!g_current_buffer]);
                        if (status)
                        {
                            g_player_status.state = SF_AUDIO_PLAYBACK_STATUS_STOPPED;

                            /* App callback: status changed */
                            status = g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl,
                                                                       (sf_message_header_t **) &p_cb_message,
                                                                       &cfg_acquire,
                                                                       TX_NO_WAIT);
                            APP_ERROR_TRAP(status);

                            p_cb_message->header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_APP_CB;
                            p_cb_message->header.event_b.class_instance = 0;
                            p_cb_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_CB_STATUS;

                            status                                      = g_sf_message.p_api->post(g_sf_message.p_ctrl,
                                                                                                   &p_cb_message->header,
                                                                                                   &cfg_post,
                                                                                                   NULL,
                                                                                                   TX_NO_WAIT);
                            APP_ERROR_TRAP(status);

                            // clear buffers
                            memset(&g_file_buffer, 0, sizeof(g_file_buffer));
                            memset(&g_actual_size, 0, sizeof(g_actual_size));

                            status = fx_file_close(&g_file);
                            if (FX_SUCCESS || FX_NOT_OPEN)
                            {
                                break;
                            }
                            else
                            {
                                // error callback file not accessible

                                break;
                            }
                        }

                        g_current_buffer = !g_current_buffer;

                        status           =
                            g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl,
                                                              (sf_message_header_t **) &p_audio_playback_data,
                                                              &cfg_acquire,
                                                              10);
                        APP_ERROR_TRAP(status)

                        * p_audio_playback_data           = ref_audio_playback_data;
                        p_audio_playback_data->size_bytes = g_actual_size[g_current_buffer];
                        p_audio_playback_data->p_data     = g_file_buffer[g_current_buffer];

                        status                            = g_sf_audio_playback.p_api->start(g_sf_audio_playback.p_ctrl,
                                                                                             p_audio_playback_data,
                                                                                             TX_NO_WAIT);
                        APP_ERROR_TRAP(status)

                        g_player_status.current_time = (USHORT) ((g_file.fx_file_current_file_offset -
                                                                  (ULONG64) g_file_info.data.offset) / (ULONG)
                                                                 (g_file_info.header.sample_rate * 2));
                    }
                    else
                    {
                        tx_thread_sleep(2);
                    }

                    // do nothing

                    break;
                }

                case SF_AUDIO_PLAYBACK_STATUS_PAUSED:
                case SF_AUDIO_PLAYBACK_STATUS_STOPPED:
                default:
                {
                    tx_thread_sleep(2);

                    break;
                }
            }
        }
    }
}

static UINT app_read_wav_header (void)
{
    UINT     status;
    USHORT * wav_ptr;

    status = fx_file_seek(&g_file, 0);
    APP_ERROR_RETURN(status);

    status = fx_file_read(&g_file, &g_file_buffer[0], 1024, &g_actual_size[0]);
    APP_ERROR_RETURN(status);

    wav_ptr = (USHORT *) &g_file_buffer[0];

    if (memcmp(wav_ptr, "RIFF", 4)) // check for RIFF label (offset = 0)
    {
        return APP_ERR_NOT_RIFF;    // no RIFF header found
    }

    wav_ptr += 4;
    if (memcmp(wav_ptr, "WAVE", 4)) // check for WAVE label (offset = 8 bytes)
    {
        return APP_ERR_NOT_WAVE;    // no WAVE header found
    }

    wav_ptr                  += 6;          // skipping fmt subchunk header
    g_file_info.header.format = *wav_ptr++; // 1: PCM; other: compressed
    if (g_file_info.header.format != 1)
    {
        return APP_ERR_NOT_PCM; // audio isn't PCM stream
    }

    g_file_info.header.channels = *wav_ptr++; // 1: mono; 2: stereo; & etc.
    if (g_file_info.header.channels != 1)
    {
        return APP_ERR_NOT_MONO; // audio isn't mono
    }

    g_file_info.header.sample_rate = *wav_ptr++; // in Hz (values up to 64kHz are recognized)
    if (g_file_info.header.sample_rate != 44100)
    {
        return APP_ERR_NOT_44KHZ; // sampling rate isn't 44.1kHz
    }

    wav_ptr                      += 4;          // skipping MSB of the sample rate (usually 0), entire byte rate and
                                                // block alignment
    g_file_info.header.resolution = *wav_ptr++; // in bits/sample
    if (g_file_info.header.resolution != 16)
    {
        return APP_ERR_NOT_16BIT; // audio isn't 16bits/sample
    }

    if (!memcmp(wav_ptr, "LIST", 4))
    {
        wav_ptr += 2;
        wav_ptr += ((*wav_ptr / sizeof(USHORT)) + 2);
    }

    if (memcmp(wav_ptr, "data", 4))
    {
        return APP_ERR_NO_DATA_CHUNK; // no data chunk found
    }

    wav_ptr               += 2;
    g_file_info.data.size  = *wav_ptr++;
    g_file_info.data.size  = (g_file_info.data.size + (UINT) (*wav_ptr++ << 16));

    g_file_info.data.offset = (SHORT) ((UINT) wav_ptr - (UINT) g_file_buffer);
    if (g_file_info.data.offset < 0)
    {
        return APP_ERR_NO_DATA_CHUNK; // invalid offset
    }

    status = fx_file_seek(&g_file, (ULONG) g_file_info.data.offset);
    APP_ERROR_RETURN(status);

    return FX_SUCCESS;
}
