/******************************************************************************
* File:              hcms.c
* Author:            Kevin Day
* Date:              January, 2005
* Description:       
*                    
*                    
* Copyright (c) 2004 Kevin Day
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
#include "platform.h"
#include "hcms.h"
#include "5x7-hcms.h"
#include "types.h"

#define HCMS_PORT PORTB
#define HCMS_PORT_DIR DDRB
//#define HCMS_PIN_RESET  7  // not present PORTD
//#define HCMS_PIN_BLANK  4
#define HCMS_PIN_CE     5
#define HCMS_PIN_CLK    1
#define HCMS_PIN_RS     6
#define HCMS_PIN_DIN    2

/* 1 unit = 4 characters */
#define HCMS_NUM_UNITS  (2*3)

#define HCMS_CTRL_REG_0 0x00
#define HCMS_CTRL_REG_1 0x80
#define HCMS_SLEEP_OFF  0x40
#define HCMS_NORM_BRT   0x0F

//#define PIN_LOW(x)  {cbi(HCMS_PORT, x); }
//#define PIN_HIGH(x) {sbi(HCMS_PORT, x); }

#define PIN_LOW(x)  PORTB &= ~(1<<x)
#define PIN_HIGH(x) PORTB |= (1<<x)

void hcms_init()
{
    /* all pins output */
    DDRB |= ((1<<HCMS_PIN_CE)|(1<<HCMS_PIN_CLK)|(1<<HCMS_PIN_RS)|(1<<HCMS_PIN_DIN));
//    DDRD |= (1<<HCMS_PIN_RESET);
}

void hcms_reset(u8 reset_low)
{
#if 0
    if (reset_low)
    {
        /* Assert /RESET */
        PORTD &= ~(1<<HCMS_PIN_RESET);
    }
    else
    {
        /* Deassert /RESET */
        PORTD |= (1<<HCMS_PIN_RESET);
    }
#endif
}

void hcms_set_blank(u8 blank_high)
{
#if 0
    if (blank_high)
    {
        /* Assert BLANK */
        PIN_HIGH(HCMS_PIN_BLANK);    
    }
    else
    {
        /* Deassert BLANK */
        PIN_LOW(HCMS_PIN_BLANK);
    }
#endif
}


#define AVRCE 7
void avr_init()
{
    PIN_HIGH(HCMS_PIN_CE);
    PIN_LOW(AVRCE);
    hcms_shift_out(0);
    hcms_shift_out(255);
    hcms_shift_out(50);
    hcms_shift_out(50);
    PIN_HIGH(AVRCE);
}

void hcms_set_control_reg(u8 val, u8 num_disp)
{
    /* RS high */
    PIN_HIGH(HCMS_PIN_RS);
    
    /* /CE low */
    PIN_LOW(HCMS_PIN_CE);

    while(num_disp--) 
    {
        hcms_shift_out(val);
    }

    /* CLK low */
    PIN_LOW(HCMS_PIN_CLK);
    
    /* /CE high */
    PIN_HIGH(HCMS_PIN_CE);

    /* RS low */
    PIN_LOW(HCMS_PIN_RS);
}


void hcms_shift_out(u8 val)
{
    u8 i;
    /* Shift val into DIN MSB first on rising edge of CLK */
    
    for(i=0; i<8; i++)
    {
        /* CLK low */
        PIN_LOW(HCMS_PIN_CLK);
        if (val & 0x80)
        {
            /* DIN high */
            PIN_HIGH(HCMS_PIN_DIN);            
        }
        else
        {
            /* DIN low */
            PIN_LOW(HCMS_PIN_DIN);
        }
        delay_us(1);
        /* CLK high */
        PIN_HIGH(HCMS_PIN_CLK);
        
        /* shift val */
        val = val << 1;
    }
}


void hcms_push_ascii(u8 ch)
{
    if (ch >= sizeof(ascii57)/5)
    {
        ch = 1;
    }
    hcms_push_bmap_P((prog_char *)&ascii57[(u8)ch]);
}

void hcms_begin_shift()
{
    /* RS low */
    PIN_LOW(HCMS_PIN_RS);

    PIN_HIGH(HCMS_PIN_CE);
    
    PIN_HIGH(HCMS_PIN_CLK);
    
    /* /CE low */
    PIN_LOW(HCMS_PIN_CE);
}

void hcms_end_shift()
{
    /* CLK low */
    PIN_LOW(HCMS_PIN_CLK);
    
    /* /CE high */
    PIN_HIGH(HCMS_PIN_CE);
}    

void hcms_ascii_middle(u8 ch)
{
    u8 i;
    if (ch >= sizeof(ascii57)/5)
    {
        ch = 1;
    }
    
    for(i=0; i<5; i++)
    {
        hcms_shift_out(pgm_read_byte((int)ascii57[ch] + i));
    }    
}
void hcms_push_bmap_P(prog_char *pch)
{
    u8 i;
    
    /* RS low */
    PIN_LOW(HCMS_PIN_RS);

    PIN_HIGH(HCMS_PIN_CE);
    
    PIN_HIGH(HCMS_PIN_CLK);
    
    /* /CE low */
    PIN_LOW(HCMS_PIN_CE);
    
    for(i=0; i<5; i++)
    {
        hcms_shift_out(pgm_read_byte((int)(pch + i)));
    }    
    
    /* CLK low */
    PIN_LOW(HCMS_PIN_CLK);
    
    /* /CE high */
    PIN_HIGH(HCMS_PIN_CE);
}

void hcms_push_start()
{
    /* RS low */
    PIN_LOW(HCMS_PIN_RS);

    PIN_HIGH(HCMS_PIN_CE);
    
    PIN_HIGH(HCMS_PIN_CLK);
    
    /* /CE low */
    PIN_LOW(HCMS_PIN_CE);
}

void hcms_push_end()
{
    
    /* CLK low */
    PIN_LOW(HCMS_PIN_CLK);
    
    /* /CE high */
    PIN_HIGH(HCMS_PIN_CE);
}
    

void hcms_push_str_X(u8 *str, u8 is_pgm, prog_char *prefix)
{
    u8 ch;
    int i;
    
    if(prefix)
    {
        while( (ch=(1?pgm_read_byte((int)prefix++):*prefix++)) )
        {
            for(i=0; i<5; i++)
            {
                hcms_shift_out(pgm_read_byte((int)&(ascii57[ch][i])));
            }
        }
    }
            
    
    while( (ch=(is_pgm?pgm_read_byte((int)str++):*str++)) )
    {
        for(i=0; i<5; i++)
        {
            hcms_shift_out(pgm_read_byte((int)&(ascii57[ch][i])));
        }                    
    }    
}

void hcms_push_decimal(u32 v, u8 width)
{
    char numstack[9];
    s8 i = sizeof(numstack) - 1;

    do {
        numstack[--i] = (v % 10) + '0';
        v /= 10;
    } while(v > 0 && i > 0);

    while (i > 0)
    {
        numstack[--i] = '_';
    }
    
    numstack[8] = 0;

    hcms_push_str_X(numstack + (sizeof(numstack) - 1 - width), 0, NULL);
    //hcms_push_str_X(&numstack[4], 0, NULL);

}    

void hcms_setup()
{
    hcms_init();
    
    avr_init();

    hcms_reset(1);
    PIN_HIGH(HCMS_PIN_CE);
    //PIN_HIGH(HCMS_PIN_BLANK);
    PIN_HIGH(HCMS_PIN_CLK);

    delay_us(100);
    hcms_reset(0);

    hcms_set_control_reg(HCMS_CTRL_REG_0 | HCMS_SLEEP_OFF | 0, HCMS_NUM_UNITS);

    //PIN_LOW(HCMS_PIN_BLANK);

}

/* pwm is 0-15; cur is 0-3 */
void hcms_set_brightness(u8 pwm, u8 cur)
{
    const u8 cur_map[] = {2<<4, 1<<4, 0, 3<<4};
    
    hcms_set_control_reg(HCMS_SLEEP_OFF | cur_map[cur] | pwm, HCMS_NUM_UNITS);
}

void hcms_push_brightness(u8 pwm, u8 cur)
{
    const u8 cur_map[] = {2<<4, 1<<4, 0, 3<<4};
    
    hcms_set_control_reg(HCMS_SLEEP_OFF | cur_map[cur] | pwm, 1);
}

void hcms_ramp_brightness(u8 delay)
{
    u8 i;
    for(i=0; i<16; i++)
    {
        hcms_set_control_reg(HCMS_CTRL_REG_0 | HCMS_SLEEP_OFF | i, HCMS_NUM_UNITS);
        delay_us(delay<<4);
        delay_us(delay<<4);
        delay_us(delay<<4);
        delay_us(delay<<4);
        delay_us(delay<<4);
        delay_us(delay<<4);
        delay_us(delay<<4);
        delay_us(delay<<4);
        delay_us(delay<<4);
        delay_us(delay<<4);
        delay_us(delay<<4);
        delay_us(delay<<4);
        delay_us(delay<<4);
        delay_us(delay<<4);
        delay_us(delay<<4);
    }
}


#if 0
void spihcms_set_control_reg(u8 val, u8 num_disp)
{
    u8 i;
    for(i=0; i<num_disp; i++)
    {
        spi_send(SLAVE_HCMS1_CTRL, 1, &val);
    }
}
#endif


u8 get_font_line(char ch, u8 line)
{
    return pgm_read_byte((int)&(ascii57[(u8)ch][line]));
}

