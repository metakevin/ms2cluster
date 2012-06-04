/******************************************************************************
* File:              swuart.h
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

#ifndef SWUART_H
#define SWUART_H

#include "types.h"

/******************************************************************************
* swuart_rx_notify
*        External -- not defined in swuart.c
*        Called from interrupt context when a byte is received 
*******************************************************************************/
void swuart_rx_notify(u8 byte);

/******************************************************************************
* swuart_init
*        Initialize the software uart
*******************************************************************************/
void swuart_init();

#endif /* !SWUART_H */
