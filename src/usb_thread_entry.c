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

void usb_thread_entry (void);

UINT host_change_function (ULONG event, UX_HOST_CLASS * class, VOID * instance);

/** Global variables */
       FX_MEDIA                    * gp_media = FX_NULL;
static UX_HOST_CLASS_STORAGE       * gp_storage       = UX_NULL;
static UX_HOST_CLASS_STORAGE_MEDIA * gp_storage_media = UX_NULL;


/* USB Thread entry function */
void usb_thread_entry (void)
{
    UINT status;

    /* Specify the callback function name for USB insertion and removal events */
    _ux_system_host->ux_system_host_change_function = host_change_function;

    status = tx_thread_resume(&gui_thread);
    APP_ERROR_TRAP(status)

    while (1)
    {
        tx_thread_sleep(TX_WAIT_FOREVER);
    }
}


UINT host_change_function (ULONG event, UX_HOST_CLASS * class, VOID * instance)
{
    UINT                     status;

    UX_HOST_CLASS          * p_storage_class = NULL;
    app_message_payload_t  * p_message       = NULL;
    sf_message_acquire_cfg_t cfg_acquire     =
    {
        .buffer_keep = false
    };
    sf_message_post_cfg_t    cfg_post        =
    {
        .priority = SF_MESSAGE_PRIORITY_NORMAL
    };

    /* Check if there is a device insertion.  */
    if (UX_DEVICE_INSERTION == event)
    {
        /* Get the storage class.  */
        status = ux_host_stack_class_get(_ux_system_host_class_storage_name, &p_storage_class);
        if (UX_SUCCESS != status)
        {
            return (status);
        }

        /* Check if we got a storage class device.  */
        if (p_storage_class == class)
        {
            /* We only get the first media attached to the class container. */
            if (FX_NULL == gp_media)
            {
                gp_storage       = instance;
                gp_storage_media = class->ux_host_class_media;
                gp_media         = &gp_storage_media->ux_host_class_storage_media;

                /** Trigger app event */
                status = g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl,
                                                           (sf_message_header_t **) &p_message,
                                                           &cfg_acquire,
                                                           TX_NO_WAIT);
                APP_ERROR_TRAP(status);

                p_message->header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_APP_CB;
                p_message->header.event_b.class_instance = 0;
                p_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_CB_USB_IN;

                status                                   = g_sf_message.p_api->post(g_sf_message.p_ctrl,
                                                                                    &p_message->header,
                                                                                    &cfg_post,
                                                                                    NULL,
                                                                                    TX_NO_WAIT);
                APP_ERROR_TRAP(status);
            }
        }
    }

    /* Check if some device is removed.  */
    else if (UX_DEVICE_REMOVAL == event)
    {
        /*  Check if the storage device is removed.  */
        if (instance == gp_storage)
        {
            /* Set pointers to null, so that the demo thread will not try to access the media any more.  */
            gp_media         = FX_NULL;
            gp_storage_media = UX_NULL;
            gp_storage       = UX_NULL;

            /** Trigger app event */
            status = g_sf_message.p_api->bufferAcquire(g_sf_message.p_ctrl,
                                                       (sf_message_header_t **) &p_message,
                                                       &cfg_acquire,
                                                       TX_NO_WAIT);
            APP_ERROR_TRAP(status);

            p_message->header.event_b.class_code          = SF_MESSAGE_EVENT_CLASS_APP_CB;
            p_message->header.event_b.class_instance = 0;
            p_message->header.event_b.code           = SF_MESSAGE_EVENT_APP_CB_USB_OUT;

            status                                   = g_sf_message.p_api->post(g_sf_message.p_ctrl,
                                                                                &p_message->header,
                                                                                &cfg_post,
                                                                                NULL,
                                                                                TX_NO_WAIT);
            APP_ERROR_TRAP(status);
        }
    }
    else
    {
        /* Default case, nothing to do */
    }

    return (UX_SUCCESS);
}
