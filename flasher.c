#include <ch32v003fun.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "rv003usb/rv003usb/rv003usb.h"
#include "swio.h"
#include "v003_flash.h"

#define BACKUP_ADDR 0x08003800
#define BOOTLOAD_ADDR 0x1FFFF000

uint8_t scratch[80];
volatile uint8_t print_buf[8] = {0x8A, 'd', 'm', 'l', 'o', 'c', 'k'};
volatile uint8_t transmission_done;

uint32_t count;
bool dm_unlocked = false;
bool backup_present = false;

// "bootloader.bin" will be inserted into this structure at build time
typedef struct {
  uint8_t *binary_addr;
  uint32_t binary_size;
} bootloader_binary;

extern const bootloader_binary bootloader_image;

volatile int last = 0;
// Process incomming strings from the terminal
void handle_debug_input(int numbytes, uint8_t * data) {
	last = data[0];
	count += numbytes;
}

// DM unlock sequence taken from minichlik's code
void attempt_unlock(uint8_t t1coeff) {
  if (*DMDATA0 == *DMDATA1 && *DMDATA0) {
    AFIO->PCFR1 &= ~(AFIO_PCFR1_SWJ_CFG);
    funPinMode(PD1, GPIO_Speed_50MHz | GPIO_CNF_OUT_PP);
    MCFWriteReg32(DMSHDWCFGR, 0x5aa50000 | (1<<10), t1coeff); // Shadow Config Reg
    MCFWriteReg32(DMCFGR, 0x5aa50000 | (1<<10), t1coeff); // CFGR (1<<10 == Allow output from slave)
    MCFWriteReg32(DMCFGR, 0x5aa50000 | (1<<10), t1coeff); 
    MCFWriteReg32(DMABSTRACTAUTO, 0x00000000, t1coeff);
    MCFWriteReg32(DMCONTROL, 0x80000001 | (1<<10), t1coeff); 
    MCFWriteReg32(DMCONTROL, 0x40000001 | (1<<10), t1coeff);
    count = 0;
  }
}

void usb_handle_user_in_request(struct usb_endpoint * e, uint8_t * scratchpad, int endp, uint32_t sendtok, struct rv003usb_internal * ist) {
  // Make sure we only deal with control messages.  Like get/set feature reports.
  if(endp) {
    usb_send_empty(sendtok);
  }
}

void usb_handle_user_data(struct usb_endpoint * e, int current_endpoint, uint8_t * data, int len, struct rv003usb_internal * ist) {
	// Differentiate between ID for the terminal and rest of incomming data
  if (e->custom == 0xAB) {
		// We reading only one character at a time, the same as minichlink
    last = data[1];
  } else {
    int offset = e->count<<3;
    int torx = e->max_len - offset;
    if (torx > len) torx = len;
    if (torx > 0) {
      memcpy(scratch + offset, data, torx);
      e->count++;
      if ((e->count << 3) >= e->max_len) {
        transmission_done = e->max_len;
      }
    }
  }
}

void usb_handle_hid_get_report_start(struct usb_endpoint * e, int reqLen, uint32_t lValueLSBIndexMSB) {
  // You can check the lValueLSBIndexMSB word to decide what you want to do here
  // But, whatever you point this at will be returned back to the host PC where
  // it calls hid_get_feature_report. 
  //
  // Please note, that on some systems, for this to work, your return length must
  // match the length defined in HID_REPORT_COUNT, in your HID report, in usb_config.h

	// Check feature ID of a request. 0xAB is ID for the terminal
  if ((lValueLSBIndexMSB & 0xFF) == 0xAB) {
		// Checking if DM is unlocked, without it we can't use DMDATA0/1
    if ((*DMDATA0 != *DMDATA1) && *DMDATA0) dm_unlocked = true;
    if (dm_unlocked) {
			// Copy data from DMDATA0/1 and clear them to signal to printf that it has been read
      memcpy(print_buf, (uint8_t*)DMDATA0, 4);
      memcpy(print_buf+4, (uint8_t*)DMDATA1, 4);
      *DMDATA0 = 0x0;
      *DMDATA1 = 0x0;
    }
		// Set outgoing buffer to one we just copied to
    e->opaque = print_buf;
    e->max_len = 8;
  } else {
    if (reqLen > sizeof(scratch)) reqLen = sizeof(scratch);
    e->opaque = scratch;
    e->max_len = reqLen;
  }
  
}

void usb_handle_hid_set_report_start(struct usb_endpoint * e, int reqLen, uint32_t lValueLSBIndexMSB) {
  // Here is where you get an alert when the host PC calls hid_send_feature_report.
  //
  // You can handle the appropriate message here.  Please note that in this
  // example, the data is chunked into groups-of-8-bytes.
  //
  // Note that you may need to make this match HID_REPORT_COUNT, in your HID
  // report, in usb_config.h
  if ((lValueLSBIndexMSB & 0xFF) == 0xAB) {
    reqLen = 8;  
  } else {
    if (reqLen > sizeof(scratch)) reqLen = sizeof(scratch);
  }

	// Using e->caustom as a container to store feature ID of incomming data
  e->custom = (lValueLSBIndexMSB & 0xFF);
  e->max_len = reqLen;
}

void usb_handle_other_control_message(struct usb_endpoint * e, struct usb_urb * s, struct rv003usb_internal * ist) {
  LogUEvent(SysTick->CNT, s->wRequestTypeLSBRequestMSB, s->lValueLSBIndexMSB, s->wLength);
}

void reboot_to_bootloader() {
  __disable_irq();
  // Unlock the ability to write to the MODE flag of STATR register.
  FLASH->BOOT_MODEKEYR = 0x45670123; // KEY1
  FLASH->BOOT_MODEKEYR = 0xCDEF89AB; // KEY2
  // Set to run BOOT flash at next reset (MODE = 1).
  FLASH->STATR = 0x4000;
  // Clear all reset status flags (RMVF = 1).
  RCC->RSTSCKR |= 0x1000000;
  // Perform software reset (KEYCODE = 0xBEEF, RESETSYS = 1).
  PFIC->CFGR = 0xBEEF0080;
}

int check_backup() {
	uint32_t* addr = (uint32_t*)BACKUP_ADDR;
	uint32_t* b_addr = (uint32_t*)BOOTLOAD_ADDR;
	uint32_t len = bootloader_image.binary_size / 4;
	int r = 0;
	for (int i = 0; i < 16; i++) {
		if (addr[i] != 0xFFFFFFFF) {
			backup_present = true;
			r = 1;
			break;
		}
	}
	if (backup_present) {
		for (int i = 0; i < len; i++) {
			if (addr[i] != b_addr[i]) {
				r = -1;
				break;
			}
		}
	}
	return r;
}

bool check_bootloader() {
	uint32_t* addr = (uint32_t*)bootloader_image.binary_addr;
	uint32_t* b_addr = (uint32_t*)BOOTLOAD_ADDR;
	uint32_t len = bootloader_image.binary_size / 4;

	for (int i = 0; i < len; i++) {
		if (addr[i] != b_addr[i]) return false;
	}

	return true;
}

int do_backup() {
	int ret = 0;

	Delay_Ms(2);
	flash_unlock(false, false);
	printf("\nFlash unlocked\n");
	Delay_Ms(10);
	
	ret = flash_write_bulk((uint8_t*)BACKUP_ADDR, (uint8_t*)BOOTLOAD_ADDR, 1920, true);
	printf("\nBackup is done\n");

	return ret;
}

int write_bootloader(bool from_backup) {
	int ret = 0;
	
	Delay_Ms(2);
	ret = flash_unlock(false, true);

	if (!ret) {
		printf("\nFlash unlocked\n");
	} else {
		printf("\nFailed to unlock flash: %d\n", ret);
		return ret;
	}
	Delay_Ms(10);

	if (from_backup) {
		ret = flash_write_bulk((uint8_t*)BOOTLOAD_ADDR, (uint8_t*)BACKUP_ADDR, 1920, true);
		if (!ret) printf("\nBackup restored\n");
		else printf("\nRestoring backup failed: %d\n", ret);
	} else {
		ret = flash_write_bulk((uint8_t*)BOOTLOAD_ADDR, bootloader_image.binary_addr, bootloader_image.binary_size, true);
		if (!ret) printf("\nBootloader written\n");
		else printf("\nWriting bootloader failed: %d\n", ret);
	}
	
	return ret;
}

int main() {
  SystemInit();
  
  SysTick->CNT = 0;
  Delay_Ms(1); // Ensures USB re-enumeration after bootloader or reset; Spec demand >2.5µs ( TDDIS )
  
  usb_setup();

  int ui_state = 0;
	
	//	Check if SWIO terminal was connected
	if ((*DMDATA0 != *DMDATA1) && *DMDATA0) dm_unlocked = true;
  
	while(1) {

		for(int i = 0; i < 10000; i++) {
			poll_input();
		}

		//	Process incomming HID command
    if(transmission_done) {
      switch (scratch[1]) {
        case 0xA1:
          // reboot into bootloader
          reboot_to_bootloader();
          break;
        
        case 0xA5:
          attempt_unlock(scratch[2]);
          break;
      }

      transmission_done = 0;
    }

		switch (ui_state) {
		case 0:
			if (dm_unlocked) {	// Only print UI if there is someone to read it
				printf("\n\n");
				int backup = check_backup();
				// printf("foo 0x%x + 0x%x\n", bootloader_image.binary_addr, bootloader_image.binary_size);
				if (check_bootloader()) {
					printf("\nCurrent bootloader is the same as we have here.\n");
					if (backup == -1) {
						printf("But there is a backup.\nType 'r' to restore the backup");
						ui_state = 4;
					} else {
						printf("Nothing to update\n");
						ui_state = 10;
					}
					break;
				}
				if (!backup) {
					printf("\nFirst we need to make a backup.\nType 'b' to proceed\n");	
					ui_state = 1;	
				} else if (backup == 1) {
					printf("\nBootloader already backed up.\nType 'u' to update the bootloader\n");
					ui_state = 2;	
				} else if (backup == -1) {
					printf("\nBackup found.\nType 'r' to restore the backup\nOr type 'u' to update the bootloader\n");
					ui_state = 3;
				}
			}
			last = 0;	// Reset incomming terminal string
			break;

		case 1:
			if (last == 'b') {
				printf("Backing up");
				if (!do_backup()) {
					printf("Type 'u' to update the bootloader\n");
					ui_state = 2;
				} else {
					printf("Backup failed.\n Type 'b' to retry\n");
				}
				last = 0;
			}
			break;

		case 2:
			if (last == 'u') {
				ui_state = 5;
				printf("Writing bootloader");
				write_bootloader(false);
				ui_state = 10;
				last = 0;	
			}
			break;

		case 3:
			if (last == 'r') {
				ui_state = 5;
				printf("Restoring from backup");
				write_bootloader(true);
				ui_state = 10;
				last = 0;	
			} else if (last == 'u') {
				ui_state = 5;
				printf("Writing bootloader");
				write_bootloader(false);
				ui_state = 10;
				last = 0;	
			}

		case 4:
			if (last == 'r') {
				ui_state = 5;
				printf("Restoring from backup");
				write_bootloader(true);
				ui_state = 10;
				last = 0;	
			}		
		
		case 10:
			printf("You can now disconnect the device\n\n");
			ui_state++;
		
		default:
			
			break;
		}
		// Ack the terminal (minichlink needs this)
		printf("%c", last);
  }
	
}