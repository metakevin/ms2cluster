/******************************************************************************
* File:              hcms.h
* Author:            Kevin Day
* Date:              December, 2004
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


#ifndef HCMS_H
#define HCMS_H

#include <avr/pgmspace.h>
#include "types.h"

void hcms_init();
void hcms_reset(u8 reset_low);
void hcms_set_control_reg(u8 val, u8 num_units);
void hcms_shift_out(u8 val);
void hcms_set_dot_test_pattern();
void hcms_setup();
void hcms_set_brightness(u8 pwm, u8 cur);
void hcms_push_brightness(u8 pwm, u8 cur);

void hcms_push_start();
void hcms_push_end();
void hcms_push_bmap_P(prog_char *pch);
void hcms_push_ascii(u8 ch);
void hcms_push_str_X(u8 *pstr, u8 is_pgm, prog_char *prefix);
#define hcms_push_str_P(x) hcms_push_str_X(x,1,NULL)
#define hcms_push_str(x)   hcms_push_str_X(x,0,NULL)
#define hcms_push_str_clear(x) hcms_push_str_X(x,0,P("        "))
void hcms_push_decimal(u32 v, u8 width);

/* delay is in units of 1 msec */
void hcms_ramp_brightness(u8 delay);

u8 get_font_line(char ch, u8 line);

#endif /* !HCMS_H */
