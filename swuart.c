/******************************************************************************
* File:              swuart.c
* Author:            Kevin Day
* Date:              January, 2009
* Description:       
*                    Interrupt-driven receive only software uart
*                    Note: uses timer 2 with at90can128 bit settings
*                    Also hard-coded for 2400 bps with a 16MHz core
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

#include <avr/io.h>
#include <avr/interrupt.h>
#include "swuart.h"

#define RX_BIT  5
#define RX_PORT PORTE
#define RX_DIR  DDRE
#define RX_PIN  PINE
#define RX_INT  SIG_INTERRUPT5
#define RX_INT_EN   INT5
#define RX_INT_SNS0 ISC50
#define RX_INT_SNS1 ISC51

/* These timings will center the bit samping but
 * don't account for interrupt latency. 
 * Since the output compare / clear on match 
 * timer mode is used, any slop in timer
 * interrupt processing does not affect the sampling. */
#define BIT_TIME_3_2 39
/* 1 bit time calculated at 26 timer ticks, but 25
 * puts the sampling point closer to the center of the 
 * incoming signal, at least against the (crystal-driven)
 * 8 port serial card tested against. */
#define BIT_TIME_2_2 25 

void swuart_init()
{
    /* 2400 bits/sec = 6666.6 core cycles/bit
     * We need half-bit-time resolution for the
     * timer.  
     * 1/2 bit time = 3333.3 core cycles = 13.02*256 cycles
     * 3/2 bit times = 9999.9 cycles = 39.06
     * So use /256 prescaler */

    /* Set up timer - CTC mode, prescaler to /256 */
    TCCR2A = ((1<<WGM21) | (1<<CS22) |  (1<<CS21));
    
    /* Make RX pin an input */
    RX_DIR &= ~(1<<RX_BIT);
    /* Turn on pullup */
    RX_PORT |= (1<<RX_BIT);

    /* Enable RX interrupt */
    EIMSK &= ~(1<<RX_INT_EN);   // mask 
    EICRB |= (1<<RX_INT_SNS1);  // falling edge
    EICRB &= ~(1<<RX_INT_SNS0); // falling edge
    EIFR |=  (1<<RX_INT_EN);    // clear
    EIMSK |= (1<<RX_INT_EN);    // unmask

#if SAMPLE_DEBUG
    /* temp debug */
    DDRD |= (1<<4);
    PORTD &= ~(1<<4);
#endif
}

static u8 bit;
static u8 byte;

SIGNAL(RX_INT)
{
    /* falling edge of start bit detected.
     * set timer for 1 bit time + 1/2 bit time */
    TIMSK2 = 0;
    TCNT2 = 0;
    OCR2A = BIT_TIME_3_2;
    TIFR2 |= (1<<OCF2A);  // clear any pending interrupt
    TIMSK2 = (1<<OCIE2A); // unmask

    /* Mask edge interrupt */
    EIMSK &= ~(1<<RX_INT_EN);

    bit  = 0;
#if SAMPLE_DEBUG
    PORTD |= (1<<4); 
#endif
}

SIGNAL(SIG_OUTPUT_COMPARE2)
{
#if SAMPLE_DEBUG
    PORTD ^= (1<<4);
#endif
    /* Sample input.  First bit received is LSB. */
    u8 v = (RX_PIN & (1<<RX_BIT)) ? 0x80 : 0x0;
    if (bit < 8)
    {
        byte >>= 1;
        byte |= v;
    }
    else if (bit == 8)
    {
        /* Stop bit.  End of byte.
         * Stop bit is a 'mark' (high at ttl) so
         * it should be safe to re-enable the
         * edge interrupt. */
        /* Disable timer */
        TIMSK2 = 0;
        /* Clear and re-enable start bit interrupt */
        EIFR |=  (1<<RX_INT_EN);
        EIMSK |= (1<<RX_INT_EN);
        swuart_rx_notify(byte);
#if SAMPLE_DEBUG
        PORTD &= ~(1<<4);
#endif
    }        
    else
    {
        while(1);
    }
    ++bit;
    /* Sample next bit in the middle, one bit time from now */
    OCR2A = BIT_TIME_2_2;
}


    



