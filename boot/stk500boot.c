/*
stk500boot.c  20030810

Copyright (c) 2003, Jason P. Kyle
All rights reserved.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

Licence can be viewed at http://www.fsf.org/licenses/gpl.txt


Target = Atmel AVR m128,m64,m32,m16,m8,m162,m163,m169,m8515,m8535
ATmega161 has a very small boot block so isn't supported.

Tested with m128,m8,m163 - feel free to let me know how/if it works for you.
*/

#include <inttypes.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

#define F_CPU			16000000L
#define BAUD_RATE		115200L

#define DECRYPT 0
#define ENCRYPT 1
//#define DES_ENCRYPTION

#define HW_VER	0x02
#define SW_MAJOR	0x01
#define SW_MINOR	0x0e



#define SIG1	0x1E	// Yep, Atmel is the only manufacturer of AVR micros.  Single source :(
#if defined __AVR_ATmega128__
	#define SIG2	0x97
	#define SIG3	0x02
	#define PAGE_SIZE	0x80U	//128 words
	#define UART0
//	#define UART1
#elif defined __AVR_ATmega64__
	#define SIG2	0x96
	#define SIG3	0x02
	#define PAGE_SIZE	0x80U	//128 words
	#define UART0
//	#define UART1
#elif defined __AVR_ATmega32__
	#define SIG2	0x95
	#define SIG3	0x02
	#define PAGE_SIZE	0x40U	//64 words
#elif defined __AVR_ATmega16__
	#define SIG2	0x94
	#define SIG3	0x03
	#define PAGE_SIZE	0x40U	//64 words
#elif defined __AVR_ATmega8__
	#define SIG2	0x93
	#define SIG3	0x07
	#define PAGE_SIZE	0x20U	//32 words
#elif defined __AVR_ATmega162__
	#define SIG2	0x94
	#define SIG3	0x04
	#define PAGE_SIZE	0x40U	//64 words
	#define UART0
//	#define UART1
#elif defined __AVR_ATmega163__
	#define SIG2	0x94
	#define SIG3	0x02
	#define PAGE_SIZE	0x40U	//64 words
#elif defined __AVR_ATmega169__
	#define SIG2	0x94
	#define SIG3	0x05
	#define PAGE_SIZE	0x40U	//64 words
#elif defined __AVR_ATmega8515__
	#define SIG2	0x93
	#define SIG3	0x06
	#define PAGE_SIZE	0x20U	//32 words
#elif defined __AVR_ATmega8535__
	#define SIG2	0x93
	#define SIG3	0x08
	#define PAGE_SIZE	0x20U	//32 words
#elif defined __AVR_AT90CAN128__
	#define SIG2	0x97
	#define SIG3	0x81
	#define PAGE_SIZE	0x80U	//128 words
	#define UART0
#endif


void serout(char);
void serstr(char *str);
uint16_t serhexwordin(void);
void serhexwordout(uint16_t v);
void serhexbyteout(uint8_t v);
char getch(void);
void getNch(uint8_t);
void byte_response(uint8_t);
void nothing_response(void);


union address_union {
	uint16_t word;
	uint8_t  byte[2];
} address;

union length_union {
	uint16_t word;
	uint8_t  byte[2];
} length;

#if 0
struct flags_struct {
	unsigned eeprom : 1;
	unsigned rampz  : 1;
    unsigned did_program : 1;
} flags;
#endif

uint8_t did_program;

uint8_t buff[256];
uint8_t address_high;

uint8_t pagesz=0x80;

void (*app_reset)(void) = 0x0000;

void app_start(void)
{
    uint8_t m = MCUCR;
    MCUCR = m | (1<<IVCE);  /* interrupt vector change enable */
    MCUCR = m | (1<<IVSEL); /* Interrupt vector select = application */
    app_reset();
}

#define BL_DDR DDRC
#define BL_PORT PORTC
#define BL_PIN PINC
#define BL PIND4

#define LED_DDR DDRG
#define LED_PORT PORTG
#define LED_PIN PING
#define LED PING1

int main(void)
{
    wdt_disable();


    uint8_t ch,ch2;
    uint16_t w;

        
#if 0    
	asm volatile("nop\n\t");
	if(__ELPM_enhanced__(0x0000) != 0xFF) {				// Don't start application if it isn't programmed yet
		if(bit_is_set(BL_PIN,BL)) app_start();	// Do we start the application or enter bootloader? RAMPZ=0
	}
#endif
    
#if 0
#ifdef __AVR_ATmega163__
	UBRR = (uint8_t)(F_CPU/(BAUD_RATE*16L)-1);
	UBRRHI = (F_CPU/(BAUD_RATE*16L)-1) >> 8;
	UCSRA = 0x00;
	UCSRB = _BV(TXEN)|_BV(RXEN);	
#elif defined UART0
	UBRR0L = (uint8_t)(F_CPU/(BAUD_RATE*16L)-1);
	UBRR0H = (F_CPU/(BAUD_RATE*16L)-1) >> 8;
	UCSR0A = 0x00;
	UCSR0C = 0x06;
	UCSR0B = _BV(TXEN0)|_BV(RXEN0);
#elif defined UART1
	UBRR1L = (uint8_t)(F_CPU/(BAUD_RATE*16L)-1);
	UBRR1H = (F_CPU/(BAUD_RATE*16L)-1) >> 8;
	UCSR1A = 0x00;
	UCSR1C = 0x06;
	UCSR1B = _BV(TXEN1)|_BV(RXEN1);
#else		// m8m,16,32,169,8515,8535
	UBRRL = (uint8_t)(F_CPU/(BAUD_RATE*16L)-1);
	UBRRH = (F_CPU/(BAUD_RATE*16L)-1) >> 8;
	UCSRA = 0x00;
	UCSRC = 0x06;
	UCSRB = _BV(TXEN)|_BV(RXEN);
#endif
#endif


#if 0
    /* Stk500boot has a bug for the mega8, in the way
     * it writes to UCSRC.  -kday */
    
    // u2x = 0
    // umsel = 0
    // upm0:1 = 0
    // usbs = 0
    // ucsz = 011   (bit 2 (0) is #2 in UCSRB )
    // ucpol = 0
    // UCSRC = 10000110
    UBRRH = 0x86;
//    UBBRH = ((baudrate>>8)&0xF);
    UBRRH = 0;
    UBRRL = 7; /* 38400 is 23 - 115.2k is 7 */
#endif

    UBRR0H = 0;
//    UBRR0L = 8; /* 115200 */
    UBRR0L = 25; /* 38400 */
    UCSR0A = 0;
    UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);
    UCSR0B = (1<<RXEN0)|(1<<TXEN0);

#if 1
    serout('O');
    serout('K');
    serout('\r');
    serout('\n');
#endif


	serout('\0');    
       
    
    /* Wait 5 seconds for intervention; otherwise if there is a program loaded,
     * run it. */
    
    /* Program timer 1 to divider = 1024 
     * CS1 2:0 = 101 */


    /* Turn off (active low) LED */
    LED_DDR |= (1<<LED);
    LED_PORT |= (1<<LED);


    
    TCCR1A = 0;
    TCCR1B = 5; /* 1024 divisor.  Each tick is 64 us. */
    TCNT1 = 0;

    while (1)
    {
#if 1
        if ((UCSR0A) & _BV(RXC))
        {            
            goto do_bootloader;
        }
#endif

#if 1
        if (TCNT1 > 0x4000 && __ELPM_enhanced__(0x0000) != 0xFF)
        {
            /* Timeout.  Program is loaded.  Run it. */
            LED_PORT |= (1<<LED);
            DDRG |= (1<<PING0);
            PORTG &= ~(1<<PING0);
            app_start();
        }
#endif
        
        if ((TCNT1 & 0x7FF) == 0)
        {
            LED_PORT &= ~(1<<LED);
        }
        else if ((TCNT1 & 0x3FF) == 0)
        {
            LED_PORT |= (1<<LED);
        }
    }

do_bootloader:
 
 did_program = 0;
 
 for (;;) {     
   ch = getch();
	if(ch=='0') {		// Hello is anyone home?
		nothing_response();
	}
		// Request programmer ID
	else if(ch=='1') {		//Yes i've heard of the switch statement, a bunch of else if's -> smaller code
		if (getch() == ' ') {
			serout(0x14);
			serout('A');	//Not using PROGMEM string due to boot block in m128 being beyond 64kB boundry
			serout('V');	//Would need to selectively manipulate RAMPZ, and it's only 9 characters anyway so who cares.
			serout('R');
			serout(' ');
			serout('I');
			serout('S');
			serout('P');
			serout(0x10);
		}
	}
	else if(ch=='@') {		// AVR ISP/STK500 board commands  DON'T CARE so default nothing_response
		ch2 = getch();
		if (ch2>0x85) getch();
		nothing_response();
	}
	else if(ch=='A') {		// AVR ISP/STK500 board requests
		ch2 = getch();
		if(ch2==0x80) byte_response(HW_VER);		// Hardware version
		else if(ch2==0x81) byte_response(SW_MAJOR);	// Software major version
		else if(ch2==0x82) byte_response(SW_MINOR);	// Software minor version
		else if(ch2==0x98) byte_response(0x03);		// Unknown but seems to be required by avr studio 3.56
		else byte_response(0x00);					// Covers various unnecessary responses we don't care about
	}
	else if(ch=='B') {		// Device Parameters  DON'T CARE, DEVICE IS FIXED
		getNch(20);
		nothing_response();
	}
	else if(ch=='E') {		// Parallel programming stuff  DON'T CARE
		getNch(5);
		nothing_response();
	}
	else if(ch=='P') {		// Enter programming mode
		nothing_response();
	}
	else if(ch=='Q') {		// Leave programming mode
		nothing_response();

        if (did_program)
        {
            //serstr("Reset...\r\n");
            /* Force a reset */
            wdt_enable(WDTO_15MS);
            while(1)
                ;
        }
	}
	else if(ch=='R') {		// Erase device, don't care as we will erase one page at a time anyway.
		nothing_response();
        did_program = 1;
	}
	else if(ch=='U') {		//Set address, little endian. EEPROM in bytes, FLASH in words
							//Perhaps extra address bytes may be added in future to support > 128kB FLASH.
							//This might explain why little endian was used here, big endian used everywhere else.
		address.byte[0] = getch();
		address.byte[1] = getch();
		nothing_response();
	}
	else if(ch=='V') {		// Universal SPI programming command, disabled.  Would be used for fuses and lock bits.
		getNch(4);
		byte_response(0x00);
	}
	else if(ch=='d') {		// Write memory, length is big endian and is in bytes
		length.byte[1] = getch();
		length.byte[0] = getch();
#if 0
		flags.eeprom = 0;
		if (getch() == 'E') flags.eeprom = 1;
#else
        getch();
#endif
		for (w=0;w<length.word;w++) {
		  buff[w] = getch();	// Store data in buffer, can't keep up with serial data stream whilst programming pages
		}
		if (getch() == ' ') {
#if 0
			if (flags.eeprom) {		//Write to EEPROM one byte at a time
				for(w=0;w<length.word;w++) {
					eeprom_write_byte((uint8_t *)address.word,buff[w]);
					address.word++;
				}			
			}
			else 
#endif
            {					//Write to FLASH one page at a time
				if (address.byte[1]>127) address_high = 0x01;	//Only possible with m128, m256 will need 3rd address byte. FIXME
				else address_high = 0x00;
#if defined(__AVR_ATmega128__) || defined(__AVR_AT90CAN128__)
				RAMPZ = address_high;
#endif
				address.word = address.word << 1;	//address * 2 -> byte location
//				if ((length.byte[0] & 0x01) == 0x01) length.word++;	//Even up an odd number of bytes
				if ((length.byte[0] & 0x01)) length.word++;	//Even up an odd number of bytes
				cli();									//Disable interrupts, just to be sure
				while(bit_is_set(EECR,EEWE));			//Wait for previous EEPROM writes to complete
				asm volatile("clr	r17				\n\t"	//page_word_count
							 "lds	r30,address		\n\t"	//Address of FLASH location (in bytes)
							 "lds	r31,address+1	\n\t"
							 "ldi	r28,lo8(buff)		\n\t"	//Start of buffer array in RAM
							 "ldi	r29,hi8(buff)		\n\t"
							 "lds	r24,length		\n\t"	//Length of data to be written (in bytes)
							 "lds	r25,length+1	\n\t"
							 "length_loop:			\n\t"	//Main loop, repeat for number of words in block							 							 
							 "cpi	r17,0x00	\n\t"	//If page_word_count=0 then erase page
							 "brne	no_page_erase	\n\t"						 
							 "wait_spm1:			\n\t"
							 "lds	r16,0x57		\n\t"	//Wait for previous spm to complete
							 "andi	r16,1\n\t"
							 "cpi	r16,1\n\t"
							 "breq	wait_spm1\n\t"
							 "ldi	r16,0x03		\n\t"	//Erase page pointed to by Z
							 "sts	0x57,r16		\n\t"
							 "spm					\n\t"							 
#ifdef __AVR_ATmega163__
	 						 ".word 0xFFFF					\n\t"
							 "nop							\n\t"
#endif
							 "wait_spm2:			\n\t"
							 "lds	r16,0x57		\n\t"	//Wait for previous spm to complete
							 "andi	r16,1\n\t"
							 "cpi	r16,1\n\t"
							 "breq	wait_spm2\n\t"									 

							 "ldi	r16,0x11				\n\t"	//Re-enable RWW section
					 		 "sts	0x57,r16				\n\t"						 			 
					 		 "spm							\n\t"
#ifdef __AVR_ATmega163__
	 						 ".word 0xFFFF					\n\t"
							 "nop							\n\t"
#endif
							 "no_page_erase:		\n\t"							 
							 "ld	r0,Y+			\n\t"		//Write 2 bytes into page buffer
							 "ld	r1,Y+			\n\t"							 
							 
							 "wait_spm3:			\n\t"
							 "lds	r16,0x57		\n\t"	//Wait for previous spm to complete
							 "andi	r16,1\n\t"
							 "cpi	r16,1\n\t"
							 "breq	wait_spm3\n\t"
							 "ldi	r16,0x01		\n\t"	//Load r0,r1 into FLASH page buffer
							 "sts	0x57,r16		\n\t"
							 "spm					\n\t"
							 
							 "inc	r17				\n\t"	//page_word_count++
							 "cpi r17,%0	\n\t"
							 "brlo	same_page		\n\t"	//Still same page in FLASH
							 "write_page:			\n\t"
							 "clr	r17				\n\t"	//New page, write current one first
							 "wait_spm4:			\n\t"
							 "lds	r16,0x57		\n\t"	//Wait for previous spm to complete
							 "andi	r16,1\n\t"
							 "cpi	r16,1\n\t"
							 "breq	wait_spm4\n\t"
#ifdef __AVR_ATmega163__
							 "andi	r30,0x80		\n\t"	// m163 requires Z6:Z1 to be zero during page write
#endif							 							 
							 "ldi	r16,0x05		\n\t"	//Write page pointed to by Z
							 "sts	0x57,r16		\n\t"
							 "spm					\n\t"
#ifdef __AVR_ATmega163__
	 						 ".word 0xFFFF					\n\t"
							 "nop							\n\t"
							 "ori	r30,0x7E		\n\t"		// recover Z6:Z1 state after page write (had to be zero during write)
#endif
							 "wait_spm5:			\n\t"
							 "lds	r16,0x57		\n\t"	//Wait for previous spm to complete
							 "andi	r16,1\n\t"
							 "cpi	r16,1\n\t"
							 "breq	wait_spm5\n\t"									 
							 "ldi	r16,0x11				\n\t"	//Re-enable RWW section
					 		 "sts	0x57,r16				\n\t"						 			 
					 		 "spm							\n\t"					 		 
#ifdef __AVR_ATmega163__
	 						 ".word 0xFFFF					\n\t"
							 "nop							\n\t"
#endif
							 "same_page:			\n\t"							 
							 "adiw	r30,2			\n\t"	//Next word in FLASH
							 "sbiw	r24,2			\n\t"	//length-2
							 "breq	final_write		\n\t"	//Finished
							 "rjmp	length_loop		\n\t"
							 "final_write:			\n\t"
							 "cpi	r17,0			\n\t"
							 "breq	block_done		\n\t"
							 "adiw	r24,2			\n\t"	//length+2, fool above check on length after short page write
							 "rjmp	write_page		\n\t"
							 "block_done:			\n\t"
							 "clr	__zero_reg__	\n\t"	//restore zero register
							 : : "M" (PAGE_SIZE) : "r0","r16","r17","r24","r25","r30","r31");

/* Should really add a wait for RWW section to be enabled, don't actually need it since we never */
/* exit the bootloader without a power cycle anyhow */
			}
			serout(0x14);
			serout(0x10);
		}
        did_program = 1;
	}
	else if(ch=='t') {		//Read memory block mode, length is big endian.
		length.byte[1] = getch();
		length.byte[0] = getch();
#if 0
		if (getch() == 'E') {
            flags.eeprom = 1;
        }
		else {
			flags.eeprom = 0;
		}
#else
        getch();
#endif
		if (getch() == ' ') {		// Command terminator
			serout(0x14);
			for (w=0;w < length.word;w++) {		// Can handle odd and even lengths okay
//				if (flags.eeprom) {	// Byte access EEPROM read
//					serout(eeprom_read_byte((uint8_t *)address.word));
//					address.word++;
//				}
//				else {
                    /* kday: address.word is the word address in flash. 
                     * elpm is required for >64kB flash (lpm doesn't use RAMPSZ).
                     */
                    uint8_t flv;
                    uint16_t fla;
                    fla = address.word;
                    fla <<= 1;
                    fla += w;
                    if (address.word > 0x7FFF) {
                        RAMPZ = 1;
                    }
                    else {
                        RAMPZ = 0;
                    }
                    asm("elpm %0, Z" : "=r"(flv) : "z"(fla));
                    serout(flv);
//				}
			}
			serout(0x10);
		}
        did_program = 1; /* want to reset after verify also */
        /* Note!  For some reason if did_program is set before the flash reading/programming, and
         * not after, it gets cleared.  Not sure why, but there is a bug lurking... */
	}
	else if(ch=='u') {		// Get device signature bytes
		if (getch() == ' ') {
			serout(0x14);
			serout(SIG1);
			serout(SIG2);
			serout(SIG3);
			serout(0x10);
		}
	}
	else if(ch=='v') {		// Read oscillator calibration byte
		byte_response(0x00);
	}
    else if (ch == 'K') {
        serstr("\r\nWord address (4 hex characters, no prefix): ");
        uint16_t waddr = serhexwordin();
        serstr("Address ");
        serhexwordout(waddr);
        serstr(": ");
        uint16_t i;
        for(i=0; i<4; i++)
        {
            uint32_t baddr = waddr;
            baddr <<= 1;
            baddr += i;
            RAMPZ = (baddr > 0xFFFFUL) ? 1 : 0;
            if (RAMPZ) {
                serstr("RAMPZ ");
            }
            //uint16_t fv = __LPM_enhanced__((baddr & 0xFFFF));
            uint16_t fv;
            uint16_t b = baddr;
            asm ("elpm %0, Z" : "=r"(fv) : "z" (b));
            serhexbyteout(fv);
            serstr(" ");
        }
        serstr("\r\n");
    }
  }
}

void serstr(char *str)
{
    while(*str) {
        serout(*str++);
    }
}

char hex[] = "0123456789ABCDEF";
void serhexwordout(uint16_t v)
{
    char i;
    for(i=3; i>=0; i--)
    {
        serout(hex[(v>>(4*i)&0xF)]);
    }
}
void serhexbyteout(uint8_t v)
{
    char i;
    for(i=1; i>=0; i--)
    {
        serout(hex[(v>>(4*i)&0xF)]);
    }
}
uint16_t serhexwordin(void)
{
    char i;
    uint16_t r = 0;
    for(i=3; i>=0; i--)
    {
        char c = getch();
        if (c >= '0' && c <= '9') {
            r |= (c-'0')<<(4*i);
            serout(c);
        }
        else if (c >= 'A' && c <= 'F') {
            r |= (c-'A'+0xA)<<(4*i);
            serout(c);
        }
        else {
            i++;
        }
    }
    serout('\r');
    serout('\n');
    return r;
}
    

void serout(char ch)
{
#ifdef UART0
	while (!((UCSR0A) & _BV(UDRE0)));
	UDR0 = ch;
#elif defined UART1
	while (!((UCSR1A) & _BV(UDRE1)));
	UDR1 = ch;
#else		// m8,16,32,169,8515,8535,163
	while (!((UCSRA) & _BV(UDRE)));
	UDR = ch;
#endif
}

char getch(void)
{
#ifdef UART0
	while(!((UCSR0A) & _BV(RXC0)));
	return ((UDR0));
#elif defined UART1
	while(!((UCSR1A) & _BV(RXC1)));
	return ((UDR1));
#else		// m8,16,32,169,8515,8535,163
	while(!((UCSRA) & _BV(RXC)));
	return ((UDR));
#endif
}

void getNch(uint8_t count)
{
uint8_t i;
	for(i=0;i<count;i++) {
#ifdef UART0
		while(!((UCSR0A) & _BV(RXC0)));
		(UDR0);
#elif defined UART1
		while(!((UCSR1A) & _BV(RXC1)));
		(UDR1);
#else		// m8,16,32,169,8515,8535,163
		while(!((UCSRA) & _BV(RXC)));
		(UDR);
#endif		
	}
}

void byte_response(uint8_t val)
{
	if (getch() == ' ') {
		serout(0x14);
		serout(val);
		serout(0x10);
	}
}

void nothing_response(void)
{
	if (getch() == ' ') {
		serout(0x14);
		serout(0x10);
	}
}

