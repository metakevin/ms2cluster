/******************************************************************************
* File:              pcf8583.c
* Author:            Kevin Day
* Date:              January, 2009
* Description:       
*                    
*                    
* Copyright (c) 2009 Kevin Day
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

#include "platform.h"
#include "comms_generic.h"
#include "tasks.h"
#include "timers.h"
#include "avrsys.h"
#include "bufferpool.h"
#include "hwi2c.h"


typedef struct {
    u8 csr;         // loc 0
    u8 csec_10s : 4; // loc 1 upper nibble
    u8 csec_1s  : 4; 
    u8 sec_10s  : 4; // loc 2
    u8 sec_1s   : 4;
    u8 min_10s  : 4; // loc 3
    u8 min_1s   : 4;
    u8 hrs_24h  : 1; // loc 4
    u8 hrs_pm   : 1;
    u8 hrs_10s  : 2;
    u8 hrs_1s   : 4;
    u8 year     : 2; // loc 5
    u8 day_10s  : 2;
    u8 day_1s   : 4;
    u8 weekday  : 3; // loc 6
    u8 mon_10s  : 1; 
    u8 mon_1s   : 4;
} pcf_regs_t;

pcf_regs_t rtc_regs;


void rtcdraw(u8 *dbuf, u8 update_only, u16 arg);
{
    char buf[9];
    sprintf(buf, "%u%u:%u%u:%u%u", 
            rtc_regs.hrs_10s, rtc_regs.hrs_1s,
            rtc_regs.min_10s, rtc_regs.min_1s,
            rtc_regs.sec_10s, rtc_regs.sec_1s);
    render_string(dbuf, 0, 8, buf);
}


#define PCF8583_ADDR 0xA0
static inline void read_rtc()
{
    /* assuming read pointer is 0 */
    u8 req[] = {PCF8583_ADDR|1, sizeof(pcf_regs_t), 0, 
                TASK_ID_RTC<<4|RTC_UPDATE};
    send_to_task(TASK_ID_I2C, I2C_READ, sizeof(req), req);
}

static inline void reset_ptr()
{
    u8 req[] = {PCF8583_ADDR, 1, 0, 0, 0};
    send_to_task(TASK_ID_I2C, I2C_WRITE, sizeof(req), req);
}

timerentry_t rtc_timer;
void rtc_callback(timerentry_t *t)
{
    read_rtc();
    register_timer_callback(&rtc_timer, MS_TO_TICK(1000), rtc_callback, 0);
}


static task_t rtc_taskinfo;
static u8     rtc_mailbox_buf[2+sizeof(pcf_regs_t)];

u8 rtc_task();

task_t *rtc_task_create()
{
    register_display_area(rtcdraw, 0, start, length);



    return setup_task(&rtc_taskinfo, TASK_ID_RTC, rtc_task,
                      rtc_mailbox_buf, sizeof(rtc_mailbox_buf));
}

u8 rtc_task()
{
    u8 code, payload_len;
    if (mailbox_head(&rtc_taskinfo.mailbox, &code, &payload_len))
    {
        switch(code)
        {
            case RTC_UPDATE:
                if (payload_len == sizeof(pcf_regs_t))
                {
                    mailbox_copy_payload(&rtc_taskinfo.mailbox, 
                                         &rtc_regs, sizeof(pcf_regs_t),
                                         0);
                    reset_ptr();
                }
                else
                {
                    while(1);
                }
                break;
            default:
                while(1);
                break;
        }
    }
}


