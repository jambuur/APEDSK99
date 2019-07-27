# APEDSK99
Arduino DSKx emulator shield for the TI99/4a

APEDSK99 is an Arduino shield that, in combination with a compatible SD card shield, emulates 3 DS/SD floppy drives 
for the TI99/4a home computer. 

Interface to the TI is through the classic signaling/buffering with 74LS244 (74LS541) and 74LS251. An 8Kx8 RAM contains the DSR code,
uploaded by the Arduino when powered up (battery backup is on the todo-list)

CRU is not used but where necessary emulated through 2 memory mapped addresses; this both simplifies the shield design and streamlines the DSR code. 

The DSR is a modified version of the original TI Disk Controller DSR but with CRU comms removed and optimised for reliable 
SD card access instead of wonky floppies. At present there is about 0.5K of free DSR space available; some things planned for future versions are realtime clock access (the SD card has one) and mapping DOAD's from TI BASIC.

The Arduino UNO controls the TI interface, has R/W access to RAM and can halt the TI (READY line). Due to the limited number of GPIO pins, R/W to RAM is serial-to-parallel scheme through a couple of 74HC595 shift registers. This idea is neither new or mine but I have gratefully used part the this excellent project by Mario Keller: https://github.com/mkeller0815/MEEPROMMER

When the TI issues a disk command by writing to the relevant registers or emulated CRU addresses an interrupt is generated through a 74LS138. The Arduino:

1 disables the TI interface
2 halts the TI
3 enables RAM access
4 executes the command including updating the relevant registers/CRU addresses
5 executes the opposite of steps 3,2 and 1 in that order

BTW I owe a lot to Thierry Nouspikel's marvelous TI Tech Pages which has a wealth of information on the TI Disk Controller, including a commented disassembly of it's ROM: http://www.unige.ch/medecine/nouspikel/ti99/disks.htm

Another valuable source of information is Monthy Schmidt's excellent book "Technical Drive". 
