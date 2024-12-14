#include "v003_flash.h"
#include "ch32v003fun.h"
#include <stdio.h>

int flash_unlock(bool option_bytes, bool bootloader_bytes) {
	Delay_Ms(100);
  FLASH->KEYR = 0x45670123;
	FLASH->KEYR = 0xCDEF89AB;

	// Unlocking option bytes.
	if (option_bytes) {
		FLASH->OBKEYR = 0x45670123;
		FLASH->OBKEYR = 0xCDEF89AB;
	}

	// Unlocking BOOT area
	if (bootloader_bytes) {
		FLASH->BOOT_MODEKEYR = 0x45670123;
		FLASH->BOOT_MODEKEYR = 0xCDEF89AB;
	}

	// Unlocking 64B page fast erase/write
	FLASH->MODEKEYR = 0x45670123;
	FLASH->MODEKEYR = 0xCDEF89AB;

  #ifdef DEBUG_FLASH
	printf( "FLASH->CTLR = %08lx\n", FLASH->CTLR );
  #endif

	if (FLASH->CTLR & 0x8080) {
    #ifdef DEBUG_FLASH
		printf( "Flash still locked\n" );
    #endif
		return -1;
	}
	return 0;
}

void flash_lock() {
  FLASH->CTLR = CR_LOCK_Set;
}

int flash_erase(uint32_t* addr) {
  int ret = 0;
  if (FLASH->CTLR & 0x8080) {
    ret = -1;
    #ifdef DEBUG_FLASH
    printf("Flash is locked\n");
    #endif
  } else {
    FLASH->CTLR = CR_PAGE_ER;
    FLASH->ADDR = (intptr_t)addr;
    FLASH->CTLR = CR_STRT_Set | CR_PAGE_ER;
    while(FLASH->STATR & FLASH_STATR_BSY);  // Takes about 3ms.
    #ifdef DEBUG_FLASH
    printf("Erase complete\n");
    #endif
    if(addr[0] != 0xffffffff) {
      #ifdef DBUG_FLASH
      printf("Error: Flash general erasure failed\n");
      #endif
      ret = -2;
    }
  }
  
  return ret;
}

int flash_erase_bulk(uint32_t* addr, uint32_t len) {
  int ret = 0;

  for (int i = 0; i < len; i++) {
    ret = flash_erase(addr + (16*i));
    if (ret) break;
  }

  return ret;
}

int flash_write(uint8_t* addr, uint8_t* data, uint32_t len, bool erase) {
  if (!len) return -3;
	if (len > 64) len = 64;
  int ret = 0;

  if (FLASH->CTLR & 0x8080) {
    ret = -4;
    #ifdef DEBUG_FLASH
    printf("Flash is locked\n");
    #endif
  } else {
		uint8_t buffer[64];
		uint32_t* address = (uint32_t*)addr;
		// uint32_t* data_ptr = (uint32_t*)data;
		if (!(((intptr_t)addr & 63) == 0 && (len & 63) == 0)) {
			// printf("doing stuff... %08x-%d %d-%d\n", (intptr_t)addr, ((intptr_t)addr & 63), len, (len & 63));
			address = (uint32_t*)(((uintptr_t)addr / 64) * 64);
			uint8_t diff = (intptr_t)addr - (intptr_t)address;

			for (int n = 0; n < 64; n++) {
				if (diff > n) buffer[n] = address[n];
				else if ((len + diff) > n) buffer[n] = data[n];
				else buffer[n] = addr[n - diff];
			}
			// data_ptr = (uint32_t*)buffer;
			data = (uint32_t*)buffer;
		}

		if (erase) ret = flash_erase(address);
		if (ret) return ret;

    // Clear buffer and prep for flashing.
    FLASH->CTLR = CR_PAGE_PG;  // synonym of FTPG.
    FLASH->CTLR = CR_BUF_RST | CR_PAGE_PG;
    FLASH->ADDR = (intptr_t)address;  // This can actually happen about anywhere toward the end here.
		
    // Note: It takes about 6 clock cycles for this to finish.
    while( FLASH->STATR & FLASH_STATR_BSY );  // No real need for this.
    int i;
    for (i = 0; i < 16; i++) {
			address[i] = *((uint32_t*) data+i); //Write to the memory
			// address[i] = data_ptr[i]; //Write to the memory
      FLASH->CTLR = CR_PAGE_PG | FLASH_CTLR_BUF_LOAD; // Load the buffer.
      while(FLASH->STATR & FLASH_STATR_BSY);  // Only needed if running from RAM.
    }

    // Actually write the flash out. (Takes about 3ms)
    FLASH->CTLR = CR_PAGE_PG|CR_STRT_Set;
		// Wait until it finished
    while(FLASH->STATR & FLASH_STATR_BSY);
    #ifdef DEBUG_FLASH
    printf("FLASH->STATR = %08lx\n", FLASH->STATR);
    printf("Memory at: %08lx: %08lx %08lx\n", (uint32_t)addr, addr[0], addr[1]);
		Delay_Ms(10);
    #endif
		#ifdef PRINT_PROGRESS
		printf(".");
		Delay_Ms(10);
		#endif

    if (addr[0] != data[0]) {
      #ifdef DEBUG_FLASH
			printf("Write error at addr: 0x%08;x\n", (uint32_t)addr);
			for (int i = 0; i < 16; i++) {
				printf("Data: %08lx Flash: %08lx\n", data[i], addr[i]);
			}
			Delay_Ms(10);
			#endif
      ret = -5;
    }
  }
  
  return ret;
}

int flash_write_bulk(uint8_t* addr, uint8_t* data, uint32_t len, bool erase) {
  int ret = 0;

	int left = len;

	if (len <= 64) {
		ret = flash_write(addr, data, len, erase);
	} else {
		while (left > 0) {
			ret = flash_write(addr + (len - left), data + (len - left), left, erase);
			if (ret) break;
			left -= 64;
		}
	}

	if (ret && ret > -3) {
		printf("\nError erasing block: %lu\n", len - left);
	} else if (ret) {
		printf("\nError writing block: %lu\n", len - left);
	}

  return ret;
}