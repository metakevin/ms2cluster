/******************************************************************************
* File:              miscgpio.c
* Author:            Kevin Day
* Date:              March, 2009
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
#include "avrcan.h"
#include "avrms2.h"
#include "hud.h"
#include "sensors.h"


static task_t gpio_taskinfo;
static u8 gpio_mailbox_buf[8];
static u8 gpio_task();

#define CLUTCHIN_PORT PORTA
#define CLUTCHIN_DIR  DDRA
#define CLUTCHIN_PIN  PINA
#define CLUTCHIN_BIT  0
#define PURGE_PORT    PORTC
#define PURGE_DIR     DDRC
#define PURGE_BIT     7
#define RFAN_PORT     PORTC
#define RFAN_DIR      DDRC
#define RFAN_BIT      6
#define ACREQ_PORT    PORTE
#define ACREQ_DIR     DDRE
#define ACREQ_PIN     PINE
#define ACREQ_BIT     3
#define ACDRIVE_PORT  PORTE
#define ACDRIVE_DIR   DDRE
#define ACDRIVE_BIT   6
#define HFANIN_PORT   PORTE
#define HFANIN_DIR    DDRE
#define HFANIN_PIN    PINE
#define HFANIN_BIT    2

#define AC_MODE_LED LED_BOTTOM_MIDDLE
#define PURGE_MODE_LED LED_BOTTOM_RIGHT /* N.B. red currently broken */

u8 clutchin;
u8 hfan;
u8 acreq;

#define MAKE_INPUT_WITH_PU(port, dir, bit) do { \
    dir &= ~(1<<bit); \
    port |= (1<<bit); \
} while(0)

#define MAKE_ZERO_OUTPUT(port, dir, bit) do { \
    port &= ~(1<<bit); \
    dir |= (1<<bit); \
} while(0);

#define MAKE_ONE_OUTPUT(port, dir, bit) do { \
    port |= (1<<bit); \
    dir |= (1<<bit); \
} while(0);


#define IS_GND(pin, bit) (!(pin & (1<<bit)))

task_t *gpio_task_create()
{
    MAKE_INPUT_WITH_PU(CLUTCHIN_PORT, CLUTCHIN_DIR, CLUTCHIN_BIT);
    MAKE_INPUT_WITH_PU(ACREQ_PORT,    ACREQ_DIR,    ACREQ_BIT);
    MAKE_INPUT_WITH_PU(HFANIN_PORT,   HFANIN_DIR,   HFANIN_BIT);
    MAKE_ZERO_OUTPUT(  PURGE_PORT,    PURGE_DIR,    PURGE_BIT);
    MAKE_ZERO_OUTPUT(  ACDRIVE_PORT,  ACDRIVE_DIR,  ACDRIVE_BIT);
    MAKE_ZERO_OUTPUT(  RFAN_PORT,     RFAN_DIR,     RFAN_BIT);

    return setup_task(&gpio_taskinfo, TASK_ID_GPIO, gpio_task,  
                      gpio_mailbox_buf, sizeof(gpio_mailbox_buf));
}

void change_ac_enable(u8 turn_on)
{
    /* AC compressor is engaged with external transistor, so
     * zero = no drive even though it is a low side drive */
    if (turn_on)
    {
        MAKE_ONE_OUTPUT(ACDRIVE_PORT, ACDRIVE_DIR, ACDRIVE_BIT);
    }
    else
    {
        MAKE_ZERO_OUTPUT(ACDRIVE_PORT, ACDRIVE_DIR, ACDRIVE_BIT);
    }
}

enum {AC_OFF, AC_WAIT, AC_ENABLED, AC_ON, AC_HOLDOFF, AC_HOLDOFF_DONE} ac_arm;
timerentry_t ac_timer;
void ac_timer_cb(timerentry_t *t)
{
    if (ac_arm == AC_WAIT)
    {
        ac_arm = AC_ENABLED;
    }
    if (ac_arm == AC_HOLDOFF)
    {
        ac_arm = AC_HOLDOFF_DONE;
    }
}

void ac_override(u8 turn_off)
{
    static u8 in_override;
    if (acreq)
    {
        if (turn_off)
        {
            hud_set_led(AC_MODE_LED, LED_RED, LED_BLINK_NONE);
            change_ac_enable(0);
            in_override = 1;
        }
        else if (in_override)
        {
            in_override = 0;
            hud_set_led(AC_MODE_LED, LED_GREEN, LED_BLINK_NONE);
            change_ac_enable(1);
        }
    }
}    

void change_purge(u8 turn_on)
{
    if (turn_on)
    {
        hud_set_led(PURGE_MODE_LED, LED_GREEN, LED_BLINK_FAST);
        PURGE_PORT |= (1<<PURGE_BIT);
    }
    else
    {
        hud_set_led(PURGE_MODE_LED, LED_OFF, LED_BLINK_NONE);
        PURGE_PORT &= ~(1<<PURGE_BIT);
    }
}

extern u16 g_cltx10;
extern u16 g_knotsx100;
u16 lastknots;

enum {PURGE_OFF, PURGE_WAIT, PURGE_ENABLE, PURGE_ON, PURGE_HOLDOFF, PURGE_CANCEL} purge_arm;
timerentry_t purge_timer;
void purge_timer_cb(timerentry_t *t)
{
    if (purge_arm == PURGE_WAIT)
    {
        purge_arm = PURGE_ENABLE;
    }
    else if (purge_arm == PURGE_HOLDOFF || purge_arm == PURGE_CANCEL)
    {
        purge_arm = PURGE_OFF;
    }
}    

extern u8 engine;
extern u16 stack_high;

u8 gpio_task()
{
    clutchin = IS_GND(CLUTCHIN_PIN, CLUTCHIN_BIT);
    hfan =     IS_GND(HFANIN_PIN,   HFANIN_BIT);
    acreq =    IS_GND(ACREQ_PIN,    ACREQ_BIT);

#if 0
    static u8 last_ac;
    if (last_ac != acreq)
    {
        last_ac = acreq;
        if (acreq && ac_arm == AC_OFF)
        {
            /* tell the ECU to up the idle */
            set_user0_bit(1, 1);
            /* wait a second for the idle up to take effect */
            hud_set_led(AC_MODE_LED, LED_ORANGE, LED_BLINK_FAST);
            register_timer_callback(&ac_timer, MS_TO_TICK(2000), 
                                    ac_timer_cb, 0);
            ac_arm = AC_WAIT;
        }
            
        if (!acreq && ac_arm == AC_ON)
        {
            hud_set_led(AC_MODE_LED, LED_RED, LED_BLINK_FAST);
            register_timer_callback(&ac_timer, MS_TO_TICK(2000), 
                                    ac_timer_cb, 0);
            ac_arm = AC_HOLDOFF;
        }

        /* Glitch */
        if (acreq && ac_arm == AC_HOLDOFF)
        {
            hud_set_led(AC_MODE_LED, LED_GREEN, LED_BLINK_NONE);
            ac_arm = AC_ON;
        }
    }
    if (acreq && ac_arm == AC_ENABLED)
    {
        hud_set_led(AC_MODE_LED, LED_GREEN, LED_BLINK_NONE);
        ac_arm = AC_ON;
        change_ac_enable(1);
    }
    if (!acreq && ac_arm == AC_HOLDOFF_DONE)
    {
        hud_set_led(AC_MODE_LED, LED_OFF, LED_BLINK_NONE);
        change_ac_enable(0);
        ac_arm = AC_OFF;
        /* tell the ECU to drop the idle */
        set_user0_bit(1, 0);
    }
#endif        

#if 0
    if (g_mapx10 > 750 || g_rpm < 1000)
    {
        ac_override(1);
    }
    else
    {
        ac_override(0);
    }
#endif

    /* Radiator fan -- turn on over 190F (a bit over the thermostat value) or when
     * the AC request button is engaged */
    if (g_cltx10 > 1900 || acreq)
    {
        RFAN_PORT |= (1<<RFAN_BIT);
    }
    else if (g_cltx10 < 1850)  /* 5 degrees hysterisis */
    {
        RFAN_PORT &= ~(1<<RFAN_BIT);
    }


    /* Canister Purge */
    /* engine flags tested are crank, startw, wu, tps accel, tps decel */
    if (0 && g_mapx10 > 350 && g_mapx10 < 750 && !(engine&(2|4|8|16|32)))
    {
        if (purge_arm == PURGE_OFF)
        {
            purge_arm = PURGE_WAIT;
            register_timer_callback(&purge_timer, MS_TO_TICK(3000), 
                                    purge_timer_cb, 0);
        }
        if (purge_arm == PURGE_ENABLE)
        {
            change_purge(1);
            /* send to ms2 to gate any VE tuning */
            /* TODO: need to disable this with a switch or some 
             * other mechanism for tuning */
            set_user0_bit(4, 1);
            purge_arm = PURGE_ON;
        }
    }
    else
    {
        if (purge_arm == PURGE_ON)
        {
            purge_arm = PURGE_HOLDOFF;
            register_timer_callback(&purge_timer, MS_TO_TICK(10000), 
                                    purge_timer_cb, 0);
        }
        else if (purge_arm == PURGE_WAIT)
        {
            purge_arm = PURGE_CANCEL;
        }

        if (purge_arm != PURGE_OFF)
        {
            change_purge(0);
            set_user0_bit(4, 0);
        }
    }

    static u8 last_clutchin = 0xFF;
    if (clutchin != last_clutchin)
    {
        if (clutchin)
        {
            set_user0_bit(0, 1);
        }
        else
        {
            set_user0_bit(0, 0);
        }
        last_clutchin = clutchin;
    }

    static u16 last_stack = 0xFFFF;
    if (stack_high != last_stack)
    {
        send_stack(stack_high);
        last_stack = stack_high;
    }


    if (sensor_update)
    {
        if (sensor_update&(1<<OILP))
        {
            send_oilp(oil_pressure);
        }
        if (sensor_update&(1<<OILT))
        {
            send_oilt(oil_temperature);
        }
        if (sensor_update&(1<<CLTP))
        {
            send_clp(coolant_pressure);
        }
        u8 flags = disable_interrupts();
        sensor_update = 0;
        restore_flags(flags);
    }

    /* NOTE: g_knotsx100 is actually mphx100.  fix this... */
    if (g_knotsx100 != lastknots)
    {
        send_knots(g_knotsx100);
    }

    return 0;
}


