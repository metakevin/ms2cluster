MCU = -mmcu=at90can128
CC = avr-gcc
CFLAGS = $(MCU) -Wall -W -Wstrict-prototypes
CFLAGS += -O0
#CFLAGS += -O2
#CFLAGS += -Os
#CFLAGS += -mrtl
CFLAGS += -Wa,-ahlms=$(<:.c=.lst)
CFLAGS += -g
LD = avr-gcc

#Comment out all but the applicable target micro, NB largest possible boot block sizes are used.
#LDFLAGS = $(MCU) -Wl,-M,--section-start=.text=0x1800# mega8 2kB Boot Block
#LDFLAGS = $(MCU) -Wl,-M,--section-start=.text=0x1E00# mega8 512 byte boot block
#LDFLAGS = $(MCU) -Wl,-M,--section-start=.text=0x3800# mega16 2kB BB
#LDFLAGS = $(MCU) -Wl,-M,--section-start=.text=0x7000# mega32 4kB BB (DES code will fit)
#LDFLAGS = $(MCU) -Wl,-M,--section-start=.text=0xE000# mega64 8kB BB (DES code will fit)
#LDFLAGS = $(MCU) -Wl,-M,--section-start=.text=0x1E000# mega128/at90can128 8kB BB
#LDFLAGS = $(MCU) -Wl,-M,--section-start=.text=0x3800# mega162 2kB BB
#LDFLAGS = $(MCU) -Wl,-M,--section-start=.text=0x3800# mega163 2kB BB
#LDFLAGS = $(MCU) -Wl,-M,--section-start=.text=0x3800# mega169 2kB BB
#LDFLAGS = $(MCU) -Wl,-M,--section-start=.text=0x1800# mega8515 2kB BB
#LDFLAGS = $(MCU) -Wl,-M,--section-start=.text=0x1800# mega8535 2kB BB

LDFLAGS = $(MCU) -T stk500boot.x

UISP=../../../../avr/uisp/src/uisp.exe

#PRG = uisp -dprog=pavr -dserial=/dev/com2 -dpart=auto -v=3
PRG = $(UISP) -dprog=stk500 -dpart=ATmega128 -dserial=/dev/ttyS7  -v=3

OBJS = stk500boot.o

OUT = stk500boot.elf
SREC = stk500boot.srec
IHEX = stk500boot.hex
DIS  = stk500boot.dis

all: $(SREC) $(IHEX) $(DIS)

list: stk500boot.c
	$(CC) $(CFLAGS) -S stk500boot.c -o stk500boot.s
	avr-as -a stk500boot.s > stk500boot.lst
	rm stk500boot.s

pp: stk500boot.c
	$(CC) $(CFLAGS) -E stk500boot.c -o stk500boot.i


$(SREC): $(OUT)
	avr-objcopy --output-target=srec $(OUT) $(SREC)

$(IHEX): $(OUT)
	avr-objcopy --output-target=ihex $(OUT) $(IHEX)

$(DIS): $(OUT)
	avr-objdump -D $(OUT) > $(DIS)
	avr-objdump -D $(IHEX) -m avr > ihex$(DIS)
flash-erase:
	$(PRG) --erase

flash-write: $(SREC)
	$(PRG) --upload --verify if=$(SREC)

flash: $(SREC)
	$(PRG) --erase --upload --verify if=$(SREC)

$(OUT): $(OBJS)
	$(LD) $(OBJS) -o $(OUT) $(LDFLAGS) 

clean:
	rm -f $(OBJS) $(OUT) $(SREC) $(IHEX) 

# fuses for at90can128:
# BOOTSZ0 = 0
# BOOTSZ1 = 0  (4096 words - bl start at 0xf000 
# BOOTRST = 0 
# These are bits 2:0 in the high fuse byte.  
wrbootresetfuse:
	$(PRG) --wr_fuse_l=0xe0

readfuses:
	$(PRG) --rd_fuses

# BLB0: 111 (no restriction)
# BLB1: 111 (no restriction)

# bootsz 11 = 128 words
#        10 = 256 
#        01 = 512	<-- just a little too large.
#        00 = 1024
# bits 2:1 in high byte  -- d8 is 00, dc is 10
# bit 4 high: ckopt (program to 1 for max swing)
# DC = 11011000 - bootsz1 = 0 bootsz0 = 0 bootrst = 0
# FF = 10111111 - max startup time, crystal, BOD enabled
fuses:
	# ef = external crystal
	$(PRG) --rd_fuses
	$(PRG) --wr_fuse_h=0xd8
	$(PRG) --wr_fuse_l=0xbf			
	$(PRG) --rd_fuses


