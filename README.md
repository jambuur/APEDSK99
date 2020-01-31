# APEDSK99
### *Arduino DSKx emulator shield for the TI99/4a*

APEDSK99 is an Arduino shield that emulates 3 DS/SD floppy drives for the TI99/4a home computer. It is used together with a SD card shield for storing Disk-On-A-Disk (DOAD) floppy images.

![KiCAD 3D view](img/APEDSK-AU-GH.jpg)

To stay with those good old times APEDSK99 is based on trusty through-hole technology (aka I hate SMD soldering with a passion). 

The interface to the TI is the classic design, although sans CRU, with 2x 74LS541 for address/control lines and a 74LS245 for the databus. An 8Kx8 RAM contains the DSR code, uploaded from SD by the Arduino at powerup or reset.  

The DSR is a modified TI Disk Controller version and as you would expect most changes are made within the low level routines. CRU is emulated through 2 memory mapped addresses, simplifying shield design. By removing quite a bit of code dealing with floppy wonkiness ("let's try to read this sector 10 times and see how we go"), some 1KB of valuable DSR space for BASIC DOAD support and future enhancements has become available.

The Arduino UNO controls the TI interface, has R/W access to RAM, can halt the TI and tries to act as a FD1771. Due to the limited number of GPIO pins available, The Arduino RAM interface is a serial-to-parallel scheme through 74HC595 shift registers. 

### *How does it work?*

When the TI issues a disk controller command by writing to one of the FD1771 registers, the Arduino receives an interrupt through a 74LS138 3->8 decoder.  The Arduino then:

1. disables the TI interface buffers
2. halts the TI by making READY low; this also enables the shift registers
3. enables the Arduino RAM control bus
4. executes the command including updating the relevant FD1771 and CRU registers
5. executes the opposite of steps 3, 2 and 1

### *DOAD's*

The SD card can be filled with as many DOAD's as you see fit :-). DOAD filenames must follow the familiair MS-DOS 8.3 format, with a ".dsk" extension. At powerup / reset the Arduino looks for optional DSK1[1-3].dsk files and maps them accordingly. The DSR has support for DOAD mapping and write protection through BASIC CALL's. 

Once a DOAD is mapped to a particular DSK, it behaves very much like a normal (but quite fast) floppy. 

### *BASIC support*

The DSR contains 3 additional CALL's to manage DOAD's:

CALL PDSK([1-3]): This subprogram applies a virtual "adhesive tab" (remember those?) to the Volume Information Block preventing the Arduino writing to the DOAD. It is separate to the familiar Protected flag as used by disk managers and it can only be removed by ...

CALL UDSK([1-3]): This subprogram removes the virtual "adhesive tab"

CALL CDSK([1-3],"8 character DOAD name without the .dsk extension"): This subprogram maps a drive to a DOAD. The DOAD name must be 8 characters, padded with the appropriate amount of spaces if shorter.

CALL error reporting is limited to "* INCORRECT STATEMENT" only, so it's up to the user to determine WTH went wrong (wrong drive #, DOAD doesnt' exist etc).

### *BUG's*

If a particular program or module behaves nicely by accessing disks solely through the regular DSR routines there shouldn't be any new ones (are there any existing disk controller bugs?) In other words, any funky direct disk access and weird copy protection schemes will surely fail. 

Maybe more a peculiarity than a bug is a specific ignition sequence: switch the TI on first, apply power to APEDSK99, wait for a second for APEDSK99 to load the DSR and then soft-reset the TI. I find in my setup that applying power to APEDSK99 first sometimes corrupts the DSR (as it's in RAM not ROM). I have a slight suspicion it is related to FG99 initialisation but could be wrong. Anyway it's on the investigation list.

### *Future*

Quite some time after I came up with the name APEDSK99 I realised that DSK emulation is just a first application. The shield is actually a generic DSR interface to a substantial catalogue of available Arduino shields ... including very useful ones such as Ethernet / WiFi. Instead of trying to squeeze a tiny SLIP stack into the TI, it's just a matter of writing a suitable DSR and Bob's your uncle. 

Speaking of DSR's, initially I considered using a bigger RAM size for larger and/or concurrent DSR's. But after optimising  Arduino RAM R/W access I had DSR loading times reduced to ~450ms so switching DSR's based on CALL's would be entirely feasible. I will go with that approach for now.  

### *Acknowledgement*

This project owes a lot to Thierry Nouspikel's marvelous [TI Tech Pages website](http://www.unige.ch/medecine/nouspikel/ti99/disks.htm) which has a wealth of information on the TI Disk Controller, including a commented disassembly of its DSR ROM.

Another valuable source of information has been Monthy Schmidt's excellent book "Technical Drive".

The aforementioned Arduino's serial-to-parallel RAM addressing scheme is neither new or mine but I have gratefully used part of [this excellent project](https://github.com/mkeller0815/MEEPROMMER) by Mario Keller.

Last but not least is the expert input from former


