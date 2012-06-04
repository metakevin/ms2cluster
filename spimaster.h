/******************************************************************************
* File:              spimaster.h
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

#ifndef SPIMASTER_H
#define SPIMASTER_H

typedef enum {
    SLAVE_NONE,
    SLAVE_HCMS1_CTRL,
    SLAVE_HCMS1_DATA,
    SLAVE_DISP_AVR,
} spi_slave_id_t;

void spimaster_init();

void spi_send(spi_slave_id_t sid, u16 len, u8 *data);       
void spi_send_async(spi_slave_id_t sid, u16 len, u8 *data, u8 *status);


#endif /* !SPIMASTER_H */
