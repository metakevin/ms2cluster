/******************************************************************************
* File:              main.c
* Author:            Kevin Day
* Date:              February, 2005
* Description:       
*                    Main program for audi radio interface
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


#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/wdt.h>

#include "types.h"
#include "tasks.h"
#include "comms_generic.h"
#include "timers.h"
#include "adc.h"
#include "persist.h"
#include "hcms.h"
#include "gpsavr.h"
#include "hud.h"
#include "spimaster.h"
#include "avrcan.h"
#include "avrms2.h"
#include "swuart.h"
#include "hwi2c.h"
#include "adcgauges.h"
#include "miscgpio.h"
#include "sensors.h"

#define LED_PORT PORTG
#define LED_DIR  DDRG
#define LED_PIN  PING
#define LED_BIT  0

#define ANT_PORT PORTB
#define ANT_DIR  DDRB
#define ANT_PIN  PINB
#define ANT_BIT  2

void debug_led(u8 val);
static inline void start_blink_timer();

void bufferpool_init();
task_t* comms_task_create();
task_t *radio_input_task_create();


#define STACK_CANARY 0xC5
void StackPaint(void) __attribute__ ((naked)) __attribute__ ((section (".init1")));

void StackPaint(void)
{
#if 0
    uint8_t *p = &_end;

    while(p <= &__stack)
    {
        *p = STACK_CANARY;
        p++;
    }
#else
    __asm volatile ("    ldi r30,lo8(_end)\n"
                    "    ldi r31,hi8(_end)\n"
                    "    ldi r24,lo8(0xc5)\n" /* STACK_CANARY = 0xc5 */
                    "    ldi r25,hi8(__stack)\n"
                    "    rjmp .cmp\n"
                    ".loop:\n"
                    "    st Z+,r24\n"
                    ".cmp:\n"
                    "    cpi r30,lo8(__stack)\n"
                    "    cpc r31,r25\n"
                    "    brlo .loop\n"
                    "    breq .loop"::);
#endif
} 

extern uint8_t __heap_start; /* not _end because of .bufferpool */
extern uint8_t __stack; 

uint16_t StackCount(void)
{
    const uint8_t *p = &__heap_start;
    uint16_t       c = 0;

    while(*p == STACK_CANARY && p <= &__stack)
    {
        p++;
        c++;
    }

    return c;
} 




#define ADC_CONTEXTS 8
adc_context_t adc_context[ADC_CONTEXTS];
u8 num_adc;
u16 stack_high;

#define DISPAVR_RESET_PIN 4 /* on port B */
int main()
{    
    u8 mcusr_rst = MCUSR;
    MCUSR = 0;
    wdt_disable();
    wdt_enable(WDTO_2S);

    /* Disable JTAG so the ADC pins are available.
     * Don't do this if the JTAG reset flag is set -- 
     * presumably the jtag pod is connected in that
     * case. */
    if (! (mcusr_rst & (1<<JTRF)))
    {
        MCUCR |= (1<<JTD);
    }

    /* Assert remote AVR reset */
    PORTB &= ~(1<<DISPAVR_RESET_PIN);
    DDRB  |= (1<<DISPAVR_RESET_PIN);

    bufferpool_init();
    
    init_persist_data();
    
    systimer_init();

    //fuel_gauge_init(&adc_context[num_adc++]);

    num_adc = sensors_init(&adc_context[num_adc], num_adc);

    adc_init_adc(ADC_DIV128, num_adc, adc_context);

    /* Deassert remote AVR reset */
    PORTB |= (1<<DISPAVR_RESET_PIN);
    

    spimaster_init();

    init_avrms2(); 

//    swuart_init();

    /* Enable interrupts */
    sei();

    /* Populate the tasklist in priority order */    
    tasklist[num_tasks++] = comms_task_create();
    tasklist[num_tasks++] = gps_task_create();
    tasklist[num_tasks++] = can_task_create();
    tasklist[num_tasks++] = hud_task_create(); 
//    tasklist[num_tasks++] = i2c_task_create(); 
    tasklist[num_tasks++] = gpio_task_create();
    //
    start_blink_timer();

    /* non-preemptive static priority scheduler */    
    while(1)
    {
        wdt_reset();

        u8 taskidx;
	u8 r;
        for(taskidx=0; taskidx<num_tasks; taskidx++)
        {
            r = tasklist[taskidx]->taskfunc();
            if (r)
                break;
        }
        if (r == 0)
        {
            stack_high = StackCount(); /* only run this after all tasks run */
        }
    }
}

void swuart_rx_notify(u8 data)
{
    send_to_task(TASK_ID_HUD, HUD_EVT_USER_INPUT, 1, &data);
}

timerentry_t blink_timer;

//u8 dsflag;
void blink_callback(timerentry_t *t)
{
    u16 ms;
    if (t->key == 0)
    {
        debug_led(0);
        ms = 1000;

//        ow_2760_write_reg(8, dsflag?0xFF:0);
//        dsflag=!dsflag;
    }
    else
    {
        debug_led(1);
        ms = 1000;
    }

//    UDR1 = 0x5A;

    register_timer_callback(&blink_timer, MS_TO_TICK(ms), blink_callback,
            !t->key);
}

static inline void start_blink_timer()
{
    blink_callback(&blink_timer);
}

void debug_led(u8 val)
{
    if (!val)
    {
        LED_DIR &= ~(1<<LED_BIT);
    }
    else
    {
        LED_DIR |= (1<<LED_BIT);
        LED_PORT &= ~(1<<LED_BIT);
    }
}

