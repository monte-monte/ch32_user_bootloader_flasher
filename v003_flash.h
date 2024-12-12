#ifndef SRL_FLASH_CH32
#define SRL_FLASH_CH32

#include <stdint.h>
#include <stdbool.h>

// #define DEBUG_FLASH
#define PRINT_PROGRESS
#define I_WILL_RESPECT_64_BYTES_ALLIGNMENT

int flash_unlock(bool option_bytes, bool bootloader_bytes);

void flash_lock();

int flash_erase(uint32_t* addr);

int flash_erase_bulk(uint32_t* addr, uint32_t len);

int flash_write(uint8_t* addr, uint8_t* data, uint32_t len, bool erase);

int flash_write_bulk(uint8_t* addr, uint8_t* data, uint32_t len, bool erase);

#endif