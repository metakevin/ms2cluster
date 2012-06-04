# These two lines may need to be adjusted for your build environment.
# This setting works with AVR Studio 5's included GCC toolchain on a windows 7 64 bit machine.
AVRBASE=$(shell cygpath -m "C:\Program Files (x86)\Atmel\AVR Studio 5.0\AVR Toolchain")
AVRGCCLIBDIR="$(AVRBASE)/lib/gcc/avr/4.5.1/$(ARCH)"

AVRLIB="$(AVRBASE)/avr/lib"
AVRCC = "$(AVRBASE)/bin/avr-gcc" -Wall
AVRLD = "$(AVRBASE)/bin/avr-ld"
AVROBJCOPY = "$(AVRBASE)/bin/avr-objcopy"
AVROBJDUMP = "$(AVRBASE)/bin/avr-objdump"

HOSTCC			= $(CC)
HOSTCFLAGS		= $(CFLAGS) -g   -DPACKET_RECEIVE_SUPPORT=1 -DHOST

HOSTLIBS		= -lreadline

AVRARCH=$(ARCH)
AVRCRT=$(AVRLIB)/$(ARCH)/$(CRT)
AVRCLIBDIR=$(AVRLIB)/$(ARCH)

AVROBJDIR=avrobj
AVRINTDIR=avrint
HOSTOBJDIR=hostobj

#JTAGDEV=/dev/ttyUSB1
#STKDEV=/dev/ttyUSB1
#STKDEV=/dev/ttyS0
STKDEV=/dev/ttyUSB0

AVRTYPE=AT90CAN128
AVR=at90can128
CRT=crtcan128.o
ARCH=avr5

AVRPROG   = squirt128
AVRCFILES = main.c \
			comms_avr.c \
			comms_generic.c \
			tasks.c	\
			timers.c \
			adc.c	\
			persist.c \
			eeprom.c \
			bufferpool.c \
			hcms.c \
			hud.c \
			spimaster.c \
			avrcan.c \
			avrms2.c \
			swuart.c \
			hwi2c.c \
			adcgauges.c \
			miscgpio.c \
			gpsavr.c \
			sensors.c


DEPFILES = $(addprefix $(AVRINTDIR)/, $(AVRCFILES:.c=.d))

ifdef RADIO_IN_SUPPORT
AVRCFILES += radioin.c 
OPTION_FLAGS += -DRADIO_IN_SUPPORT=1
endif


AVRSFILES = 
#udelay.s

AVROBJS       = $(addprefix $(AVROBJDIR)/, \
						$(AVRCFILES:.c=.o) $(AVRSFILES:.s=.o))
AVRARCHFLAGS  = -mmcu=$(AVR)
AVRSTACK      = 0x1FF
AVRDEBUG      = -gdwarf-2
#AVRDEBUG      = 
AVRLINKSCRIPT = $(AVRPROG).x
AVRFLAGS      = $(AVRARCHFLAGS) -DAVRTYPE=$(AVRTYPE) -DEMBEDDED=1 $(OPTION_FLAGS)

#
#	Note: PACKET_RECEIVE_SUPPORT requires on the order of 256 WORDS
#		  of flash on the AVR.
#
AVRCFLAGS     = $(AVRFLAGS) $(AVRDEBUG) -I/usr/avr/include  \
				-O2 \
				-DPACKET_RECEIVE_SUPPORT=1 

AVRSFLAGS     = $(AVRFLAGS) -x assembler-with-cpp 
#-Wa,-gstabs 
#AVRLDSOPT     = -T $(AVRLINKSCRIPT) -g
AVRLDSOPT     = -T $(AVRLINKSCRIPT) #--gc-sections
AVROTHERDEPS += $(AVRLINKSCRIPT)
#AVRLDSOPT	   = -T $(AVRLIB)/ldscripts/$(AVRARCH).x

all: mkdirs avr host

mkdirs:
#	@if test ! -d $(AVROBJDIR); then mkdir $(AVROBJDIR); fi
#	@if test ! -d $(AVRINTDIR); then mkdir $(AVRINTDIR); fi
#	@if test ! -d $(HOSTOBJDIR); then mkdir $(HOSTOBJDIR); fi

avr: $(AVRPROG).hex 

#$(AVROBJCOPY) -j .text -O ihex $(AVRPROG).elf $(AVRPROG).hex

$(AVRPROG).hex : $(AVRPROG).elf
	$(AVROBJCOPY) -O ihex $(AVRPROG).elf $(AVRPROG).hex

$(AVRPROG).elf : $(AVROBJS) $(AVROTHERDEPS)
	$(AVRLD) -nostdlib -m $(AVRARCH) -o $(AVRPROG).elf $(AVRCRT) \
	-L$(AVRGCCLIBDIR) -L$(AVRCLIBDIR) -Map $(AVRPROG).map --cref \
	$(AVROBJS) -lgcc -lc -lgcc -v $(AVRLDSOPT) 
	$(AVROBJDUMP) -dS $(AVRPROG).elf > $(AVRPROG).disa
	$(AVROBJDUMP) -x $(AVRPROG).elf > $(AVRPROG).sect

$(AVROBJDIR)/%.o:%.s
	$(AVRCC) -c $(AVRSFLAGS) -Wa,-andhlms=$(addsuffix .lst,$(basename $<)) $< -o $@

$(AVROBJDIR)/%.o : $(AVRINTDIR)/%.s
	$(AVRCC) $(AVRCFLAGS) -Wa,-andhlms=$(addsuffix .lst,$(basename $<)) -o $@ -c $<

$(AVRINTDIR)/%.s : $(AVRINTDIR)/%.i
	$(AVRCC) $(AVRCFLAGS) -o $@ -S $<

$(AVRINTDIR)/%.i : %.c
	$(AVRCC) $(AVRCFLAGS) -o $@ -E $<

$(AVRINTDIR)/%.d : %.c
	$(AVRCC) $(AVRCFLAGS) -MM \
		-MF $(AVRINTDIR)/$(addsuffix .d,$(basename $<)) \
		-MT $(AVROBJDIR)/$(addsuffix .o,$(basename $<)) \
		$<

#$(AVROBJDIR)/%.o : %(AVRINTDIR)/%.s

.SECONDARY: $(AVRCFILES:%.c=$(AVRINTDIR)/%.s) $(AVRCFILES:%.c=$(AVRINTDIR)/%.i) $(AVRSSFILES:%.S=$(AVRINTDIR)/%.s)

#$(AVRINTDIR)/%.i : %.c

#$(AVRINTDIR)/%.s : $(AVRINTDIR)/%.i

$(AVROBJDIR)/%.o : %.c

clean:
	rm -f $(AVROBJS) $(HOSTOBJS) $(AVRINTDIR)/* \
		$(AVRPROG).elf $(AVRPROG).hex $(AVRPROG).map \
		$(AVRPROG).disa $(AVRPROG).sect

erasestk:
	uisp -dprog=stk500 -dpart=AT$(AVRTYPE) -dserial=$(STKDEV) --erase -v=3
# NOTE: -dpart=AT90S8535 does not work for 8535, but 8515 does!!
burnstk: $(AVRPROG).hex 
	uisp -dprog=stk500 -dpart=AT$(AVRTYPE) -dserial=$(STKDEV) --upload if=$(AVRPROG).hex -v=3

boot: $(AVRPROG).hex
	uisp -dprog=stk500 -dpart=ATmega8 -dserial=$(STKDEV) --upload if=$(AVRPROG).hex -v=3


HOSTCFILES = comms_generic.c \
			 comms_linux.c

HOSTPROG		= avrtalk
HOSTOBJS        = $(addprefix $(HOSTOBJDIR)/, $(HOSTCFILES:.c=.o))


$(HOSTOBJDIR)/%.o:%.c
	$(HOSTCC) -c $(HOSTCFLAGS) $< -o $@

host: avrtalk
	
	
avrtalk: $(HOSTOBJS)
	$(HOSTCC) $(HOSTCFLAGS) $(HOSTOBJS) -o $(HOSTPROG) $(HOSTLIBS)
	
	

jtag:
	avarice -j $(JTAGDEV) :4242 &

burnjtag: $(AVRPROG).hex
	avarice -j $(JTAGDEV) -p --file=$(AVRPROG).hex --erase

burn: burnjtag

erasejtag: $(AVRPROG).hex
	avarice -j $(JTAGDEV) -e

-include $(AVRINTDIR)/*.d

deps: mkdirs $(DEPFILES)

UISP=../../../avr/uisp/src/uisp.exe

SERIALPORT ?= /dev/ttyS0
PRG = $(UISP) -dprog=stk500 -dpart=ATmega128 -dserial=$(SERIALPORT) -dspeed=38400

$(SREC): $(AVRPROG).elf
	avr-objcopy --output-target=srec $(OUT) $(SREC)

LOADBL=cmd /c $(HOSTPROG) -p $(SERIALPORT) -R
FLASH=$(PRG) --upload if=$(AVRPROG).hex
VERIFY=$(PRG) --verify if=$(AVRPROG).hex
READ=$(PRG) --download of=read_$(AVRPROG).hex

flash: $(AVRPROG).hex
	$(FLASH)
	
reload: $(AVRPROG).hex 
	$(LOADBL)
	$(FLASH)
	sleep 5
	$(LOADBL)
	$(VERIFY)

read: $(AVRPROG).hex 
	$(LOADBL)
	$(VERIFY)


reloadq: $(AVRPROG).hex 
	$(LOADBL)
	$(FLASH)

