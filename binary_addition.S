.section .rodata
bootload_binary_start:
  .incbin "./bootloader.bin"
bootload_binary_end:

.global bootloader_image
bootloader_image:
  .int bootload_binary_start
  .int bootload_binary_end - bootload_binary_start