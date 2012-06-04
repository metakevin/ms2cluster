/******************************************************************************
* File:              avrms2.h
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

#ifndef AVRMS2_H
#define AVRMS2_H

void ms2_can_rx(u8 *msg, u8 length);

void init_avrms2();

void send_user0(u16 u);
void set_user0_bit(u8 bit, u8 val);

void send_egt(u16 val);
void send_oilp(u16 val);
void send_oilt(u16 val);
void send_clp(u16 val);
void send_knots(u16 val);
void send_stack(u16 val);



extern u16 g_rpm;
extern u16 g_mapx10;

#endif /* !AVRMS2_H */
