/******************************************************************************
* File:              datalogger.c
* Author:            Kevin Day
* Date:              November, 2005
* Description:       
*                    
*                    
* Copyright (c) 2005 Kevin Day
* 
*     This program is free software: you can redistribute it and/or modify
*     it under the terms of the GNU General Public License as published by
*     the Free Software Foundation, either version 3 of the License, or
*     (at your option) any later version.
*
*     This program is distributed in the hope that it will be useful,
*     but WITHOUT ANY WARRANTY; without even the implied warranty of
*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*     GNU General Public License for more details.
*
*     You should have received a copy of the GNU General Public License
*     along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*******************************************************************************/

#include "dataflash.h"
#include "datalogger.h"
#include "timers.h"
#include "comms_generic.h"
#include "userinterface.h"
#include "audiradio.h"

task_t dl_taskinfo;
static u8 dl_mailbox_buf[20];

u8 sampling_flag;

u8 dl_task();
void datalogger_display_func(ui_mode_t mode, u8 mode_just_changed);
u8 datalogger_config_func(ui_mode_t mode, ui_config_event_t event);

task_t *data_logger_task_create()
{
    dataflash_init();
    datalogger_init();
    
    register_display_mode(MODE_DATALOGGER, datalogger_display_func, datalogger_config_func);

    return setup_task(&dl_taskinfo, TASK_ID_DATALOGGER, dl_task, dl_mailbox_buf, sizeof(dl_mailbox_buf));
}

u8 dl_task()
{
    u8 payload_len, code;

    if (mailbox_head(&dl_taskinfo.mailbox, &code, &payload_len))
    {
        flash_err_t err = 0;
        switch(code)
        {
            case FLASH_CMD_INITIALIZE:
                {
                    u8 v = 0;
                    send_msg(BROADCAST_NODE_ID, TASK_ID_DATALOGGER<<4|FLASH_CMD_INITIALIZE, 
                            1, &v, 0);
                    dataflash_erase_all();
                    datalogger_init();
                    v=1;
                    send_msg(BROADCAST_NODE_ID, TASK_ID_DATALOGGER<<4|FLASH_CMD_INITIALIZE, 
                            1, &v, 0);
                }
                break;
            case FLASH_CMD_READ_RANGE:
                {
                    flash_offset_t addr;                    
                    u8 len;
                    const u8 maxsz = 15;
                    const flash_offset_t flashsz = 512*1024L;
                    u8 msgbuf[maxsz];
                    u8 payload[4];

                    if (payload_len != 4)
                    {
                        err = FLASH_ERR_BAD_PARAMS;
                        break;
                    }
                    
                    mailbox_copy_payload(&dl_taskinfo.mailbox, payload, 4, 0);
                    
                    addr = ((flash_offset_t)payload[0]<<16) | ((flash_offset_t)payload[1]<<8) | ((flash_offset_t)payload[2]);
                    len = payload[3];

                    if (len > maxsz)
                    {
                        err = FLASH_ERR_TOO_LARGE;
                        break;
                    }
                    if (addr > (flashsz-len))
                    {
                        err = FLASH_ERR_BAD_ADDR;
                        break;
                    }

                    dataflash_read_range(addr, len, msgbuf);
                    
                    send_msg(BROADCAST_NODE_ID, TASK_ID_DATALOGGER<<4|FLASH_CMD_READ_RANGE, 
                            len, msgbuf, 1);
                    break;
                }
#if 0
            case FLASH_CMD_BEGIN_SAMPLING:
                {
                    sampling_flag = !sampling_flag;
                    if (sampling_flag)
                    {
                        begin_sampling();
                    }
                    else
                    {
                        end_sampling();
                    }
                    break;
                }
#endif
                
#if 1
            case FLASH_CMD_LOG_SAMPLE:
                {
                    u8 buf[4];
                    mailbox_copy_payload(&dl_taskinfo.mailbox, buf, 4, 0);
                    log_data_sample(0x77, payload_len, buf);
                }
                break;
#endif
            default:
                err = FLASH_ERR_BAD_CMD;
        }
        if (err != 0)
        {
            send_msg(BROADCAST_NODE_ID, TASK_ID_DATALOGGER<<4|FLASH_CMD_ERROR, 1, (u8*)&err, 0);
        }        
        
        mailbox_advance(&dl_taskinfo.mailbox);
    }

    return 0;
}
    


log_index_t         next_available_index;
flash_offset_t      next_available_data;
delta_time_t        last_sample_time;

logged_data_descriptor_t active_desc;

void datalogger_init()
{
    flash_offset_t offset = 0;
    logged_data_descriptor_t desc;
    u8 *buf = (u8*)&desc;
    u8 i, didx;
    
    /* Read in all existing headers.  Stop when 0xFF is read. */
    for(didx=0; didx<MAX_DESCRIPTORS; didx++)
    {
        dataflash_read_range(offset, sizeof(logged_data_descriptor_t), buf);
        if (desc.flags != 0xDD)
        {
            break;
        }

        next_available_index = desc.sequence_number+1;
        next_available_data = desc.data_start_offset + desc.data_length;
        offset += sizeof(desc);
    }

    if (next_available_index == 0)
    {
        /* empty flash */
        next_available_data = DATA_START_OFFSET;
    }
}
    

log_index_t begin_sampling()
{
    if (next_available_index >= MAX_DESCRIPTORS)
    {
        return MAX_DESCRIPTORS;
    }
    
    active_desc.flags = 0xDD;
    active_desc.sequence_number = next_available_index++;
    active_desc.data_start_offset = next_available_data;
    active_desc.data_length = 0;

    dataflash_write(active_desc.sequence_number * sizeof(logged_data_descriptor_t), 
            sizeof(logged_data_descriptor_t), (u8 *)&active_desc);    

    sampling_flag = 1;
    
    return active_desc.sequence_number;
}
        
void end_sampling()
{
    sampling_flag = 0;
    
    // interrupts off here
    dataflash_write(active_desc.sequence_number * sizeof(logged_data_descriptor_t), 
            sizeof(logged_data_descriptor_t), (u8 *)&active_desc);    

    next_available_data = active_desc.data_start_offset + active_desc.data_length;
}

static inline delta_time_t get_usec_time()
{
    delta_time_t t;

    t = TICK_TO_MSEC(readtime()) * 1000;
    t += SUBTICK_TO_USEC(readsubtick());

    return t;
}

void log_data_sample(logged_data_type_t type, u8 length, u8 *data)
{
    const unsigned max_log_len = 4;
    int i;
    logged_data_sample_t *hdr;
    u8 buf[sizeof(logged_data_sample_t) + max_log_len];
    
    if (!sampling_flag)
    {
        return;
    }
    
    hdr = (logged_data_sample_t *)buf;
    for(i=0; i<length; i++)
    {
        buf[sizeof(*hdr) + i] = data[i];
    }    
    
    if (length > max_log_len)
    {
        length = max_log_len;
    }
    
    flash_offset_t offset = active_desc.data_start_offset + active_desc.data_length;
    
    if (offset + sizeof(*hdr) + length > DATA_END_OFFSET)
    {
        /* end of flash */
        end_sampling();
        return;
    }
    
    hdr->flags = 0xA;
    hdr->data_length = length;
    hdr->data_type = type;
    
#if 0
    if (active_desc.data_length)
    {
        /* not first sample */
        hdr->time_delta_usec = last_sample_time - get_usec_time();
    }
    else
    {
        hdr->time_delta_usec = 0;
    }
    last_sample_time = hdr->time_delta_usec;
#else
    hdr->time_delta_usec = get_usec_time();
#endif

    dataflash_write(offset, sizeof(*hdr)+length, buf);

    active_desc.data_length += sizeof(*hdr) + length;
}


/* Change this to use output_number, etc.
 * Mode = M2
 * Flash log number when logging, otherwise hold solid.
 */

void datalogger_display_func(ui_mode_t mode, u8 mode_just_changed)
{
    u8 radio_msg[7];
    static blink;

    radio_msg[0] = 0x04;
    radio_msg[1] = 0x00;
    radio_msg[2] = 0x3F;
    radio_msg[3] = 0x3F;
    radio_msg[6] = 0x0D; /* MF */


    if (sampling_flag)
    {
        radio_msg[4] = active_desc.sequence_number/10;
        radio_msg[5] = active_desc.sequence_number%10;
        if (blink<4)
        {
            radio_msg[3] = 0x3B;
        }
    }
    else
    {
        radio_msg[4] = next_available_index/10;
        radio_msg[5] = next_available_index%10;        
        radio_msg[3] = 0x3C;
    }
    send_to_task(TASK_ID_RADIO_OUTPUT, RADIO_MSG_SEND, 7, radio_msg);

    blink = ((blink+1)&0x7);
}

u8 datalogger_config_func(ui_mode_t mode, ui_config_event_t event)
{
    sampling_flag = !sampling_flag;
    if (sampling_flag)
    {
        begin_sampling();
    }
    else
    {
        end_sampling();
    }

    return 0; /* done */
}

