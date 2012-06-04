/******************************************************************************
* File:              hud.c
* Author:            Kevin Day
* Date:              December, 2008
* Description:       
*                    
*                    
* Copyright (c) 2008 Kevin Day
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

#include <stdio.h>
#include "platform.h"
#include "comms_generic.h"
#include "tasks.h"
#include "timers.h"
#include "avrsys.h"
#include "bufferpool.h"
#include "gpsavr.h"
#include "spimaster.h"
#include "hcms.h"
#include "hud.h"
#include "../3updisp/ledswpwm.h"
#include "avrms2.h"

#define EGT_PORT PORTA
#define EGT_DIR  DDRA
#define EGT_BIT  4
#define EGT_POLL_MS 250
timerentry_t max6675_timer;
u8 read_max6675;
void max6675_read_callback(timerentry_t *t)
{
    read_max6675 = 1;
    
    register_timer_callback(&max6675_timer, MS_TO_TICK(EGT_POLL_MS), 
                            max6675_read_callback, 0);
}    

#define NUM_2911 9
#define HCMS_NUM_UNITS  (2*NUM_2911)
#define DISPCHARS (8*NUM_2911)
#define MAX_DISPLAY_AREAS  (DISPCHARS/4)

/* Each character is 5 bytes */
#define DISPBUFSZ (DISPCHARS*5)
u8 dispbuf[DISPBUFSZ];

static task_t hud_taskinfo;
static u8 hud_mailbox_buf[20];

u8 hud_task();

#define HCMS_CTRL_REG_0 0x00
#define HCMS_CTRL_REG_1 0x80
#define HCMS_SLEEP_OFF  0x40
#define HCMS_NORM_BRT   0x0F

void spihcms_set_control_reg(u8 val, u8 num_disp)
{
    u8 i;
    for(i=0; i<num_disp; i++)
    {
        spi_send(SLAVE_HCMS1_CTRL, 1, &val);
    }
}

/* pwm is 0-15; cur is 0-3 */
void spihcms_set_brightness(u8 pwm, u8 cur)
{
    const u8 cur_map[] = {2<<4, 1<<4, 0, 3<<4};
    
    spihcms_set_control_reg(HCMS_SLEEP_OFF | cur_map[cur] | pwm, HCMS_NUM_UNITS);
}


static inline void enable_hcms()
{
    send_msg(NODE_ID_DISPAVR, 0xD<<4|ALL_LEDS_OFF, 0, NULL, 0);
    u8 p = 0;
    send_msg(NODE_ID_DISPAVR, 0xD<<4|HCMS_CTRL, 1, &p, 0);
}    

u8 initialized;
u8 nightmode;
static inline void reinit_hcms()
{
    spihcms_set_control_reg(HCMS_CTRL_REG_0 | HCMS_SLEEP_OFF, HCMS_NUM_UNITS);
    if (nightmode)
    {
        spihcms_set_brightness(10, 1);
    }
    else
    {
        spihcms_set_brightness(15, 2);
    }
}

timerentry_t display_init_timer;
void display_init_callback(timerentry_t *t)
{
    enable_hcms();
    spi_send(SLAVE_HCMS1_DATA, DISPBUFSZ, dispbuf);
    reinit_hcms();
    initialized = 1;
}

void dispbuf_init()
{
    u16 i;
    for(i=0; i<DISPBUFSZ; i++)
    {
        dispbuf[i] = 0;
    }
}    

task_t *hud_task_create()
{
    dispbuf_init();

    // A5 = high when tail lights on
    DDRA  &= ~(1<<5); // input
    PORTA &= ~(1<<5); // no pullup 

    // start polling EGT
    max6675_read_callback(NULL);

    return setup_task(&hud_taskinfo, TASK_ID_HUD, hud_task, 
                      hud_mailbox_buf, sizeof(hud_mailbox_buf));
}

static inline u8 tailfuse() 
{
    return (PINA & (1<<5))?1:0;
}

void render_string(u8 *dbuf, u8 start, u8 len, char *string)
{
    u8 i, j, p;
    p=0;
    for(i=start; i<start+len; i++)
    {
        for(j=0; j<5; j++)
        {
            dbuf[p++] = get_font_line(string[i], j);
        }
    }
}

    
typedef struct {
    u8            start; /* max 255 characters */
    u8            length;
    dispup_func_t drawfunc;
    u16           arg;
} disp_area_t;

static disp_area_t areas[MAX_DISPLAY_AREAS];
static u8 area_count;

void register_display_area(dispup_func_t drawfunc, u16 arg, u8 start, u8 length)
{
    areas[area_count].start    = start;
    areas[area_count].length   = length;
    areas[area_count].drawfunc = drawfunc;
    areas[area_count].arg      = arg;
    ++area_count;
}

void testdraw(u8 *dbuf, u8 update_only, u16 arg)
{
    char buf[9];
    snprintf(buf, 9, "0_%04u_7", arg);
    render_string(dbuf, 0, 8, buf);
}

static inline void redraw_area(disp_area_t *a, u8 update_only)
{
    if (a->start + a->length <= DISPCHARS)
    {
        u16 ofs = a->start * 5; /* 5 bytes/char */
        a->drawfunc(&dispbuf[ofs], update_only, a->arg);
#if 0
        if (ofs >= 160 && ofs <= 240)
        {
            dispbuf[ofs] = 0xaa;
        }
        testdraw(&dispbuf[ofs], update_only, ofs);
#endif
    }
}

void draw_all_areas()
{
    u8 i;
    for(i=0; i<area_count; i++)
    {
        redraw_area(&areas[i], 0);
    }
}
u16 egt;
static volatile u8 async_busy;
u8 hud_reinit;
u8 hud_task()
{
    static u8 next_to_redraw;

    u8 code, payload_len;
    if (mailbox_head(&hud_taskinfo.mailbox, &code, &payload_len))
    {
        switch(code)
        {
            case HUD_EVT_USER_INPUT:
            {
                /* Received byte from user input processor.
                 *
                 * Coding:
                 *
                 * 7:6 : event source.  encoder = 00
                 *    00   5:4  encoder - 00 no change, 01 right, 10 left, 11 invalid
                 *    00   3:2  encoder button - 00 not pressed, 01 short press, 10 long press, 11 invalid
                 *
                 */
                u8 p[1];
                mailbox_copy_payload(&hud_taskinfo.mailbox, p, 1, 0);
                send_msg(1, 0xD<<4|HUD_EVT_USER_INPUT, sizeof(p), p, 0);
                mailbox_advance(&hud_taskinfo.mailbox);

                break;
            }            
        }
    }

    if (initialized != 1)
    {
	if (!initialized)
	{
	    register_timer_callback(&display_init_timer, MS_TO_TICK(200), 
        	                    display_init_callback, 0);
	    initialized = 2;
	}
        return 0;
    }

    if (async_busy)
        return 0;

    if (read_max6675)
    {
        u16 max6675_data;

        /* force MAX6675 /CS low */
        EGT_DIR  |= (1<<EGT_BIT);
        EGT_PORT &= ~(1<<EGT_BIT);

        /* read 16 bits */
        SPDR = 0;
        while (!(SPSR & (1<<SPIF)))
            ;
        max6675_data = SPDR << 8;
        SPDR = 0;
        while (!(SPSR & (1<<SPIF)))
            ;
        max6675_data |= SPDR;

        /* restart conversions */
        EGT_PORT |= (1<<EGT_BIT);        

	egt = max6675_data>>3;
        send_egt(egt);

        read_max6675 = 0;
        return 0;
    }

    if (tailfuse() != nightmode)
    {
        nightmode=tailfuse();
//        hud_reinit = 1;
        initialized = 0;
        /* assert reset */
        u8 p = 1;
        send_msg(NODE_ID_DISPAVR, 0xD<<4|HCMS_CTRL, 1, &p, 0);        
        /* deassert reset and set initialized flag after timeout */

	// Trying to do this here crashed.  Maybe stack space is 
	// right on the edge?  put a breakpoint here and check it.
        //register_timer_callback(&display_init_timer, MS_TO_TICK(250), 
        //                        display_init_callback, 0);
        return 0;
    }
    if (hud_reinit) 
    {
	/* this hangs if called right after startup */
        reinit_hcms();
        hud_reinit = 0;
    }

    if (next_to_redraw < area_count)
    {
        redraw_area(&areas[next_to_redraw], 0);
        ++next_to_redraw;
    }
    else
    {
#if 1
        async_busy = 1;
        spi_send_async(SLAVE_HCMS1_DATA, DISPBUFSZ, dispbuf, (u8 *)&async_busy);
#else
        async_busy = 0;
        spi_send(SLAVE_HCMS1_DATA, DISPBUFSZ, dispbuf);
#endif        
        
        next_to_redraw = 0;
    }
    /* always return no reschedule */
    return 0;
}

/* Note: not safe to call from interrupt context. */
void hud_set_led(hud_led_t led, hud_ledcolor_t color, hud_ledblink_t blink)
{
    u8 on, off;
    if (blink == LED_BLINK_NONE)
    {
        on = 100;
        off = 0;
    }
    else if (blink == LED_BLINK_FAST)
    {
        on = 20;
        off = 20;
    }
    else
    {
        on = 100;
        off = 100;
    }
    if (led <= LED_TOP_RIGHT || led >= LED_LEFT_SIDE)
    {
        u8 led_idx = (u8)led;
        u8 led_brt = color==LED_OFF?0:15;
        u8 p[4] = {led_idx, led_brt, on, off};
        send_msg(NODE_ID_DISPAVR, 0xD<<4|LED_SET, sizeof(p), p, 0);
    }
    else
    {
        u8 led_brt, led_2brt;
        if (color == LED_GREEN)
        {
            led_brt = 15;
            led_2brt = 0;
        }
        else if (color == LED_RED)
        {
            led_brt = 0;
            led_2brt = 15;
        }
        else if (color == LED_OFF)
        {
            led_brt = 0;
            led_2brt = 0;
        }
        else
        {
            /* orange */
            led_brt = 15;
            led_2brt = 12;
        }
        u8 p[4] = {(u8)led,   led_brt, on, off};
        u8 q[4] = {(u8)led+5, led_2brt, on, off};
        send_msg(NODE_ID_DISPAVR, 0xD<<4|LED_SET, sizeof(p), p, 0);
        send_msg(NODE_ID_DISPAVR, 0xD<<4|LED_SET, sizeof(q), q, 0);
    }
}
