TARGET	= vita-baremetal-linux-loader
SRCDIR	= src
INCDIR	= include
OBJS	= start.o main.o \
	  FatFs/diskio.o FatFs/ff.o FatFs/ffsystem.o FatFs/ffunicode.o

PREFIX	= arm-vita-eabi
CC	= $(PREFIX)-gcc
AS	= $(PREFIX)-as
OBJCOPY	= $(PREFIX)-objcopy
CFLAGS	= -I$(INCDIR) -IFatFs -mcpu=cortex-a9 -mthumb-interwork \
	  -O0 -Wall -Wno-unused-const-variable -Wno-main
LDFLAGS	= -T linker.ld -nostartfiles -nostdlib -lgcc -lbaremetal
ASFLAGS	=
DEPS	= $(OBJS:.o=.d)

all: $(TARGET).bin

%.bin: %.elf
	$(OBJCOPY) -S -O binary $^ $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

%.o: $(SRCDIR)/%.s
	$(CC) $(ASFLAGS) -MMD -MP -c $< -o $@

.PHONY: clean send

clean:
	@rm -f $(TARGET).bin $(TARGET).elf $(OBJS) $(DEPS)

send: $(TARGET).bin
	curl -T $(TARGET).bin ftp://$(PSVITAIP):1337/ux0:/baremetal/payload.bin
	@echo "Sent."

-include $(DEPS)
