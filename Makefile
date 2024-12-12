TARGET:=flasher
CH32V003FUN:=./ch32v003fun/ch32v003fun
RV003USB:=./rv003usb
MINICHLINK?=./ch32v003fun/minichlink

ADDITIONAL_C_FILES+=./rv003usb/rv003usb/rv003usb.S ./rv003usb/rv003usb/rv003usb.c ./v003_flash.c ./binary_addition.S
EXTRA_CFLAGS:=-I./rv003usb/lib -I./rv003usb/rv003usb

LDFLAGS+=-Wl,--print-memory-usage

FLASH_USB_COMMAND=$(MINICHLINK)/minichlink -C b003boot -w $< $(WRITE_SECTION) -b

all : $(TARGET).bin

include $(CH32V003FUN)/ch32v003fun.mk

flash_usb: $(TARGET).bin
	$(FLASH_USB_COMMAND)
flash : cv_flash
clean : cv_clean
