# Standalone bootloader flasher for CH32V003
For those who has CH32V003 board with installed rv003usb bootloader but doesn't have a programmer to update the bootloader with. (Because USB bootloader can't update itself obviously)

This tool provides safe way (hopefully) to update your bootloader, before flashing a new binary it will request you to make a backup of the old one into built-in flash, if something will go wrong you will be able to restore the backup.

It is an interactive tool that requires you to use USB terminal either with minichlink from my ch32v003fun branch that has support for USB or with [WebLink](https://subjectiverealitylabs.com/WeblinkUSB/). You can also use minichlink's terminal via SWIO but if you have a programmer on your hands it defeats the purpose of this tool (but it can be handy for testing and debugging).

# To use:

``git clone https://github.com/monte-monte/ch32_user_bootloader_flasher/ --recursive``

``ch32-user-bootloader-flasher``

Place your bootloader binary as a ``bootloader.bin`` file in this folder (make sure that it's not bigger than 1920 bytes).

``make``

``make flash``

``./ch32v003/minichlink/minichlink -kT -c 0x1209d003``

or open Google Chrome or any other Chromium-based browser and go to https://subjectiverealitylabs.com/WeblinkUSB/ press ``t`` button in the UI.

If you're on linux, you may need to add udev rule:

```
echo -ne "KERNEL==\"hidraw*\", SUBSYSTEM==\"hidraw\", MODE=\"0664\", GROUP=\"plugdev\"\n" | sudo tee /etc/udev/rules.d/20-hidraw.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```
