/******************************************************************************
* File:              gpshost.c
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
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <termios.h> /* POSIX terminal control definitions */

char *serial_device = "/dev/ttyS3";
int port_speed = B115200;


int serial_fd = -1;
int open_serial()
{
    serial_fd = open(serial_device, O_RDWR|O_NOCTTY|O_NDELAY);
    if (serial_fd == -1)
    {
        perror("open_serial(): failed to open -");
    }
    else
    {
//        fcntl(serial_fd, F_SETFL, FNDELAY);
    }
    return serial_fd;
}
    
void config_port()
{
    struct termios options;
    tcgetattr(serial_fd, &options);
    cfsetispeed(&options, port_speed);
    cfsetospeed(&options, port_speed);
    
    options.c_cflag &= ~(CSIZE | PARENB | CSTOPB | CRTSCTS);
    options.c_cflag |= (CLOCAL|CREAD | CS8);

    options.c_lflag &= ~(ICANON|ECHO|ECHOE|ISIG);

    options.c_iflag &= ~(IXON|IXOFF|IXANY);

    options.c_oflag &= ~(OPOST);
    
    /* block for up to 1/10 sec on read */
    options.c_cc[VMIN] = 1;
    options.c_cc[VTIME] = 1;
    
    tcsetattr(serial_fd, TCSANOW, &options);
    
}

int main(int argc, char *argv[])
{
    char ch;

    while ((ch=getopt(argc, argv, "p:r:t:b:v")) != -1)
    {
        switch(ch)
        {
            case 'p':
                serial_device = strdup(optarg);
                break;
            case 'b':
                if (!strcmp(optarg, "115200"))
                {
                    port_speed = B115200;
                }
                else if (!strcmp(optarg, "38400"))
                {
                    port_speed = B38400;
                }
                fprintf(stderr, "Using %s bps\n", optarg);
                break;
            default:
                fprintf(stderr, "Unknown option %c\n", ch);
                return -1;
        }
    }


    open_serial();
    config_port();
    
    while (1)
    {
        FD_ZERO(&rdfds);
        FD_SET(serial_fd, &rdfds);
        FD_SET(ui_fd, &rdfds);
        ret = select(serial_fd+1, &rdfds, NULL, NULL, NULL);
//        fprintf(stderr, "select returned %d\n", ret);
        if (ret == -1)
        {
            perror("select");
            return;
        }
        else if (ret == 0)
        {
            continue;
        }

        if (FD_ISSET(serial_fd, &rdfds))
        {
            nb=read(serial_fd, &in, 1);
            
            if (nb <= 0)
            {
                fprintf(stderr, "read(serial_fd,...): %d - %s (%d)\n",
                    nb, strerror(errno), errno);
            }            
            else
            {
                //fprintf(stderr, "%02X\n", in);
                gps_rx_notify(in);
            }
        }
#if 0
        if (FD_ISSET(ui_fd, &rdfds))
        {
            rl_callback_read_char();
        }
#endif
    }
    
    return 0;
}

typedef struct {
    char bytes[83];
} nmea_sentance_t;

typedef enum {
    GP
} nmea_talker_t;

typedef enum {
    GGA, GLL, GSA, GSV, MSS, RMC, RMC, VTG, ZDA, ONEFIVEO
} nmea_sid_t;

struct _nmea_cb_table;
typedef void (*nmea_msgcallback_t)(struct _nmea_cb_table *t,
                                   u8 data);
typedef struct _nmea_cb_table {
    char msgid[6];  /* NULL-terminated identifier, e.g. GPGGA.  
                       Prefixed with a $ in the line output */
    nmea_msgcallback_t cb;
} nmea_cb_table_t;

typedef union {
    char b[10];
    struct {
        char hour[2];
        char minute[2];
        char second[2];
        char dot;
        char fracsec[3];
    } s;
} utctime_t;

typedef union {
    char b[1];
    struct {
        enum {VALID='A', INVALID='V'} status;  // sizeof?
    } s;
} status_t;

typedef union {
    char b[9];
    struct {
        char degrees[2];
        char minutes[2];
        char dot;
        char fracmin[4];
    } s;
} latitude_t;

typedef union {
    char b[1];
    struct {
        enum {NORTH='N', SOUTH='S'} nshemi;
    } s;
} nshemi_t;

typedef union {
    char b[10];
    struct {
        char degrees[3];
        char minutes[2];
        char dot;
        char fracmin[4];
    } s;
} longitude_t;

typedef union {
    char b[1];
    struct {
        enum {EAST='E', WEST='W'} ewhemi;
    } s;
} ewhemi_t;

typedef union {
    char b[4];
    struct {

void gps_rx_notify(u8 data)
{
    
