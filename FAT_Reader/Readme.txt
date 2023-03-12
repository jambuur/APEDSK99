
---------------------------------------------------------------------------
  FAT Reader v0.8 rev.1                             written by: A. Sanasi
---------------------------------------------------------------------------

Introduction
------------

Ever since MS-DOS based Windows98 was discontinued, I longed for a way
to physically sort directory entries to my liking (I still make extensive
use of the Command Prompt and hate it when things are not orderd as I
want).  Before that I had used the fabulous LFNSORT command-line tool by
D.J. Murdoch, which alas does not work on NT-based operating systems.

Even after many years, still no program seems to be able to do this job
right.  Some programs only check drives connected to the USB port, allow
only alphabetical sorting with no manual sort options, sort only music
files, or even require Windows Media Player to be present.  But what if
you want to sort other things too?  Like image files on a digital frame
or video files on your local harddisk?  Or if you simply prefer a
non-alphabetical order with directories and files in a special
arrangement?

Other programs just work with tricks by simply moving all files into a
temporary directory and then move them back in the desired order.  The
problem with this approach is, when you have for example 50 file entries
that you move out of a directory and then back, you don't get 50 sorted
file entries, you get 50 (invisible) deleted entries followed by 50 normal
file entries, because Windows will always try to append first when creating
new entries, not overwrite.  Even worse, when the cluster of a directory
can hold let's say 128 entries and you move 100 files out and back in,
Windows will first use up the remaining 28 free entries and then start
overwriting the entries marked as deleted (because that's more efficient
than allocating a new cluster to expand the directory), resulting in file
entries 29 - 100 being on top, followed by 28 remaining deleted entries,
followed by file entries 1 - 28.

The only program and my personal favorite that really gave you full sorting
control was YAFS (Yet Another FAT Sorter) by Luis Henrique Oliveira Rios.
Alas, this one has no GUI and works with .XML lists that you can only
handle with an appropriate editor, and you have to set the new order by
manually changing numbers individually for each file, which is VERY
tedious.  It also seems to not keep deleted entries, so you can't undelete
anything after the sorting anymore in case you need to.

Luckily, recently a friend explained the basics of VB .NET to me through
Skype (all I knew before was a little bit of x86 assembler), and with that
knowledge I was finally able to write my own program.  Aaand here it is! ;)
This one not only has a GUI, but can also do real sorting on the filesystem
level.  And since it communicates with the device drivers, it should be
able to handle all drives that Windows recognizes as FAT, be it MP3 player,
USB stick or harddisk, even mounted TrueCrypt volumes work.


Disclaimer
----------

This really is my VERY FIRST program in a high-level language (everything
not being assembler counts as high-level for me).  All the programming
magic I was able to learn on the internet went into it, and I tried my
best to make everything as fool-proof as possible.  But I don't know much
about Windows interna, so if you purposely try tricking the program to
write data to a device it shouldn't, there is a chance you might succeed.
JUST DON'T DO IT!

I'm not a programmer, only a hobbyist at best.  Why?  For the same
reason why I'm not a brain surgeon.  If you don't do it solely for fun
(for example if you liked the ending of "Silence Of The Lambs" part 2
so much), then you have to know the solution right away and can't search
hours online for every little problem.  And I had to do a LOT of
searching.

Real programmers (those who don't eat quiche) would probably scream in
agony if they saw my source code, and some of my solutions might not be
very professional.  The program wasn't planned this way from the start,
I just kept adding and changing stuff while it progressed.  But hey, it
does seem to work... sort of.

Usage is solely at your own risk, I guarantee for nothing and won't be
liable for anything.

Now let's get started.


Get Drives
----------

This program can only handle drives that use the FAT filesystem format
(FAT12, FAT16 and FAT32 are supported).  Click on the button "Get Drives"
to identify all FAT devices on your system, then select the device you
want to modify from the listbox below.


Open / Close
------------

To physically sort directory entries, exclusive read and write access
is needed.  Click on the button "Open" to open the selected drive for
exclusive access.  Make sure that all files on that drive are closed and
that you have the appropriate access rights (right-click on the program
and choose "Run As Administrator" if you get a warning).  Also, if you
are using a command-line window, make sure that you are not currently
located anywhere on that particular drive.

After all changes have been made and written, you can click on "Close"
to allow other applications access again or if you want to select a
different drive.


Directory Sorting
-----------------

The contents of the selected directory are shown in the list on the right,
with the name of each entry in the left half and the type of the entry in
the right half, easily distinguishable by different colors (hidden entries
are shown slightly darker than normal entries).

As of v0.8 rev.1, the program will remember the new column widths if you
modify them.  To resize, hover with the mouse above the right border of the
header field of the column that you wish to resize (so that it displays a
double-sided arrow), and then drag the column border.  You can now also
resize the program window, and the columns will automatically adapt in a
matching ratio.

To automatically sort a directory, simply click on the header field of the
list, and it will cycle between ASCending sort (0 - 9, A - Z), DESCending
sort (Z - A, 9 - 0) and no sort (the original directory order).  You can
also influence sort behavior under "Main Menu" - "Settings" - "Sorting
Options" (see below).

To manually sort a directory, left-click on any entry and drag it to the
desired new position while keeping the mouse button pressed.  If you reach
the top or bottom of the list it will start scrolling, so you can easily
reach positions further up and down without having to use the scrollbar.


Changing Directories
--------------------

The path of the current directory is shown in the white textbox right
below the buttons.  To switch to a directory further down in the tree,
double-click on its entry in the directory list.  To switch to a
higher-level directory in the tree, double-click on the ".." entry
in the directory list.  The "." entry represents the current directory
and will just re-load that when clicked.


Manual Directory Sorting By Keyboard
------------------------------------

You can also sort directories manually by using the keyboard.  While
the directory list has focus, press <Enter> once to show the selection
bar.  Press <Enter> a second time to 'grab' a selected entry and another
time to release it again.  A white focus rectangle will appear around
the entry while it is moveable.

Use the <Up> and <Down> keys to move the selection, <PageUp> and <PageDown>
keys to move it page-wise, and <Home> and <End> keys to jump to the begin
and end of the list.

To switch directories, select the appropriate entry and then press
<Enter> two times in quick succession (like a mouse double-click).

Pressing the <Left> or <Right> key will sort the directory automatically
(same as clicking on the list header field).

Please note that the keyboard control was a last-minute quick-and-dirty
addition (kept separate from mouse control and in a way so I could
completely remove it from the source code again within one minute) and
thus follows more my personal liking than Windows standards.  Originally
I wanted to simply block all keyboard input for the directory list
altogether, until I noticed that with larger directories, manual sorting
by keyboard could be much faster than with using the mouse.


Add To Queue / Update Queue / Remove From Queue
-----------------------------------------------

This program works with a Queue list, which means that no data whatsoever
will be written until you explicitely request processing of this list
(this is an intended security measure).  When you have sorted a directory
to your liking, click on "Add To Queue" and it will be added to the list.
From now on, this directory will be shown to you as it is stored in the
Queue and not as it is stored on the disk, giving you a preview of how
things will look after applying the changes.

The Status field on the left shows you the state of the current directory.
"Not Stored" (red) means the directory isn't in the Queue at all,
"Stored / Up-To-Date" (green) means the directory is stored in the Queue
and matches the displayed directory list, "Stored / Changed" (yellow)
means the directory is stored in the Queue, but the displayed directory
list does not match anymore (because you made further changes).  In the
latter case, the "Add To Queue" button changes to "Update Queue", and
when you click on it, those new changes will be added too.

If you decide that an added directory should not be processed at all, you
can click "Remove From Queue" to remove it from the list.


Write Data To Drive
-------------------

If you are satisfied with how everything looks, click on "Write Data To
Drive" to do the actual writing.  The program will then process all
directories that are stored in the Queue and write them to the disk in
their new order and with whatever modifications you specified.  This is
the ONLY time where actual changes are made to the disk.

Disk writing must be explicitely activated first (see "Main Menu" -
"Settings" - "Activate Disk Write").  If the program acted weird in
some way during your modifications or if things don't look as intended,
DO NOT WRITE ANYTHING.


The Main Menu
-------------

The main menu offers additional options and features for sorting and
error checking.


Main Menu - Settings
--------------------

If you 'just want to sort', then you can (and should) stay with the
"Simple Mode", in which case deleted entries and long filenames will
not be touched.  If you want to have more functionality and/or sort
more than one directory at a time, then turn on "Professional Mode".


Checkboxes
----------

In "Professional Mode", some checkbox options are shown on the left that
offer extra modifications beside simple sorting.  When a directory is added
to the Queue, those options are stored with it, and are shown accordingly.

"Deleted Entries" refers to directory entries that are marked as deleted
(first byte = E5), which can be both entries of files/folders or long
filename parts.  If for example a file has a long filename that takes up
three entries in the directory, after deletion this would count as four
deleted entries (one for the file as such and three for the long filename
parts).

If a directory contains deleted entries, the program shows you its number
and if they are all located at the end of the directory (after all the
visible entries), or if the deleted entries and the visible entries are
mixed (at least one deleted entry is located before a visible entry).  You
can choose to either remove them completely (overwrite them with zeroes)
by checking the option "Remove Entries" or to keep them by checking
"Move Entries To End", in which case they will all be placed after the
sorted visible entries.  The way my program works, you cannot keep the
deleted entries mixed with the visible entries, so if you try un-checking
both checkboxes, the selection will always default to "Move Entries To End".
Also, if all deleted entries already are located after all the visible
entries, that selection is grayed out.

"8.3 LFN" refers to names where their ANSI shortname matches their Unicode
Longname counterpart (apart from upper / lowercase).  Technically, those
LFN are not really needed, but they do consume directory space, so checking
the option "Remove 8.3 LFN" will remove those special longnames.  The list
on the right will display all name fields accordingly.

"Real LFN" refers to names where their Unicode Longname is different from
their ANSI shortname.  Removing those LFN can result in programs being
unable to find their paths and files anymore (because with the LFNs gone,
paths and names will be different).  Removing those LFN is therefore NOT
ADVISED, but you can check the option "Remove Real LFN" if for whatever
reason you want to do it.  The list on the right will display all name
fields accordingly.


Main Menu - Settings - Sorting Options
--------------------------------------

Here you can refine the behavior of the automatic directory sort (when
you click on the header field of the list).  If for example you want
folder entries to always be on top or want to sort only file entries,
then change the options appropriately and the automatic sort will take it
into account (changes take effect after you do the next automatic sort).


Main Menu - Settings - Activate Disk Write
------------------------------------------

Disk writing must be explicitely activated (this is an intended security
measure, originally added for risk-free testing), otherwise writing will
only be simulated.  A simulated write does everything EXACTLY as a real
write, it sets the file pointer, creates the new sector data, writes info
to the logfile etc., with the sole exception that the function which does
the actual writing is never called.  Activate this option only if you
really mean it (and say GOODBYE to your data, WUA HA HAA... ahem).


Main Menu - File
----------------

The options under "File" incorporate a cool feature that I always liked
in D.J. Murdoch's LFNSORT: it lets you create a directory sorting list
in plain text that you can then modify in a text editor.  "Load Queue
List" reads an existing list, while "Save Queue List" saves whatever is
currently stored in the Queue to a text file.

In "Professional Mode", you have two additional options named "Include
Checkbox Selections" and "Process Checkbox Commands".  If "Professional
Mode" is active and you activate "Include Checkbox Selections" before
saving, then those selections will also be saved to the list as additional
commands, individually for each directory.  Likewise, if "Professional Mode"
is active and you activate "Process Checkbox Commands" before loading,
addidional commands in the list will be processed.  Otherwise they will be
ignored for security reasons, which means that deleted entries will always
be kept and no long filenames will be removed.

If a directory is already stored in the Queue, any reference to it in a
list that you load will be ignored (manually-made additions to the Queue
always have priority).  If you want to make sure that every directory is
processed, choose "Main Menu" - "Clear" - "Empty Queue" first.

When you load or save a list, remember to choose the right format.  The
UTF-8 option will create a directory sorting list with longnames where
they are available and shortnames where longnames are not available, based
on the UTF-8 standard (chars 32 - 127 as single-bytes based on the
ASCII-standard, other chars based on Unicode).  ANSI will create a
directory sorting list with shortnames only, based on the currently
active ANSI codepage.

Do not load a list that was saved in UTF-8 format as an ANSI list or
vice versa, because then many characters might be read incorrectly and
matching / sorting will be incorrect.  Of course, since all entries
that are not found are only put at the end of the directory and thus
won't get lost, the worst that can happen is that the entries in the
sorted directories are not in the desired order afterward or that a
directory isn't sorted at all because its path wasn't found.

If you want to be on the safe side, I suggest always choosing UTF-8 as
your preferred format.

Also, please note that when sorting directories based on a text file,
the program has to do a string compare for every given entry with all the
entries in the directory that is sorted until a match is found, which can
be a bit time consuming depending on the total number of directories.


The Queue List File Format
--------------------------

If you are unsure about how a working Queue list should look like, let it
create by the program and only make modifications.  The basics are as
follows:

A valid directory always starts with a full pathline to the directory that
will be sorted, followed by the directory entries in the desired order.
Entries that are not listed or not found in the specified directory will
be added to the end after all the sorted entries.  So for example, if you
only want one specific entry to be on top of a directory, it's enough to
specify the path followed by the name of that one entry.

Checkbox Commands must start with an asterisk (*) and can be placed
anywhere within a valid directory.  Valid commands are

To remove deleted entries in that directory:

    * Rem Deleted

To remove 8.3 shortnames in that directory:

    * Rem 8.3 LFN

To remove Real LFN in that directory:

    * Rem Real LFN

Checkbox Commands must be present for each directory individually,
otherwise the program defaults to keeping all deleted entries and not
removing any long filenames in that directory.  Also, if the specified
directory does not contain any deleted entries or matching LFN entries,
the commands are ignored.


Main Menu - Recurse
-------------------

In "Professional Mode", you can also add or remove subdirectories
recursively instead of sorting and adding each one manually.

"Add Recursive" lets you add all subdirectories of the current directory.
In the new window that appears, select the options that you want to be
applied (default always is the current sort and checkbox selections) and
then click on "Start".  All subdirectories will be added to the Queue,
modified based on the options you chose.

"Remove Recursive" removes all subdirectories of the current directory
from the Queue.  This is for example useful if you added subdirectories
and then want one subdirectory and everything below it excluded.  First
go to that directory and select "Remove Recursive" to remove all
subdirectories, then click on "Remove From Queue" to remove the
current directory.


Main Menu - Clear
-----------------

"Empty Queue" removes all entries from the Queue.  Choose this option if
you want to get rid of all the modifications made so far.


Main Menu - Debug
-----------------

This program offers extensive debugging information.  Originally all this
was sent to a textbox, so I could see what the program does and better
check for errors (which was REALLY needed).  When everything ran halfway
stable, I decided to not remove the functions but instead re-direct the
output to a logfile.  So if you think something is not working as it
should, or if you just want to get a deeper insight about how things
are done, this is where you should start.

"Begin Logging" and "End Logging" starts and stops logging.  The output is
saved to a file named "LogFile.txt" located in the same directory as the
program.  While logging is active, a small label will flash at the top
right of the program window to indicate this.

By default, only basic info like positioning of the file pointer and sector
read / write messages are logged.

"Extra Logging Options" lets you select additional stuff that you want to
be logged.

"Log Drive Info" will write detailled info about the available drives to
the logfile (whenever you click on the button "Get Drives").  Activate
this if you want to see if your drives are recognized correctly.

"Log Sectors Read" will write the data of every sector that the program
reads to the logfile, in both hexadecimal and ASCII representation
(similar to a Hex Editor output).  Activate this if you want to check
if the program interprets all data correctly.

"Log Sectors To Write" will write the data of every sector that the
program is going to write to the disk to the logfile, in both hexadecimal
and ASCII representation (similar to a Hex Editor output).  Activate this
if you want to validate the data that will be written.

"Log Queue List Loading" will write detailled processing info to the
logfile when you load a Queue list text file.  Activate this if you want
to check if a Queue list is processed as intended.

Be careful which logging options you activate and for how long, or the
logfile will soon get pretty big (for example, you really shouldn't
select "Add Subdirectories" from the root of your full 1 TB harddisk
when logging is active).


For The Professionals
---------------------

If you feel safe enough, you can use some command-line options to
bypass certain security measures.

To start with Professional Mode active

   -Professional         or      -Pro

To start with checkbox default "Remove Deleted Entries"
(change only has effect when Professional Mode is active)

   -Remove_Deleted       or      -Rem_Del

To start with checkbox default "Remove 8.3 LFN"
(change only has effect when Professional Mode is active)

   -Remove_8.3           or      -Rem_8.3

To start with deactivated warning message for Real LFN removal

   -No_LFN_Warning       or      -No_Warn

To start with checkbox default "Remove Real LFN"
(change only has effect when Professional Mode is active - this
will also deactivate the warning message for Real LFN removal)

   -Remove_Real          or      -Rem_Real

To start with activated disk writing

   -Activate_Write       or      -Write_On

To start with activated "Process Checkbox Commands" and "Include
Checkbox Selections" for Queue List Loading

   -Process_Checkboxes   or      -Use_CB


Example:

   FAT_Reader.exe -Pro -Activate_Write

will start the program in Professional Mode and with disk writing enabled.


Freqently Asked Questions
-------------------------

Q:  How can I sort a whole drive?
A:  Go to the root directory of the drive.  Then first select "Main Menu" -
    "Settings" - "Professional Mode" and then "Main Menu" - "Recurse" -
    "Add Subdirectories".  Click 'Start' to add all subdirectories to the
    Queue based on the selected sorting options.  Then sort the root
    directory accordingly and click "Add To Queue" to add that one too.

Q:  Checkbox selections seem to change without reason.
A:  When a directory is displayed that is already stored in the Queue, the
    checkboxes will reflect the selections stored with that directory.  If
    a directory is not in the Queue, the checkboxes will display their
    current default settings.  Whenever the user manually changes a
    checkbox, this selection becomes the new default.

Q:  Can't uncheck "Move Entries To End" checkbox.
A:  When the program reads the entries of a directory, it creates two
    separate lists.  One contains all the normal entries, while the other
    contains all the deleted entries.  That's why "Move Entries To End"
    must be used if you don't want the entries to be deleted, and the
    checkbox reflects that.  But it wouldn't make much sense anyway to
    keep deleted entries mixed with sorted entries, now would it?

Q:  Can't move / sort the "." and ".." directory entries.
A:  Those entries must always be first in any subdirectory, that's why
    they are treated as not movable / not sortable.

Q:  How can I just separate all directory and file entries without
    changing anything else?
A:  Go to "Main Menu" - "Settings" - "Sorting Options" and there select
    "Directory Entries On Top" and also "Don't Sort Directory Entries"
    and "Don't Sort File Entries".  Then the next time you do an automatic
    sort, the program will only split directory and file entries up without
    doing any actual sorting.  It doesn't matter if you choose ASCending or
    DESCending sort in that case, because with this setting, both methods
    will give the same result.

Q:  Options "Don't Sort Directory Entries" / "Don't Sort File Entries"
    are grayed out.
A:  If "Directory & File Entries Mixed" option is selected, you can't
    exclude entries from sorting (such a sort wouldn't make sense).

Q:  Selection disappears when mouse button is released.
A:  That's intended.  Originally I only used mouse control, where it
    doesn't make much sense to keep the last selection visible (in
    contrary to keyboard control, which I added much later).

Q:  Program doesn't write sector data to the logfile when reading from
    File Allocation Table.
A:  The File Allocation Table contains only cluster references (in
    contrary to directory entries, which are much easier to interpret
    with the bare eye).  That's why I purposely left that out.

Q:  Button "Write Data To Drive" is grayed out.
A:  It's not enough to just make changes to a directory, you must also add
    it to the Queue list for processing.  Otherwise there will be nothing
    to write.

Q:  Writing to disk does not seem to have any effect.
A:  You must explicitely activate disk writing first (see "Main Menu" -
    "Settings" - "Activate Disk Write").

Q:  Some error messages are partially not in English / use the system
    language instead.
A:  For certain errors I used the Win32Exception Class to get a detailled
    error message (instead of just showing the returned error code).
    Since those messages are generated by the system, of course they
    are localized.

Q:  Who is General Failure and why is he reading my C:\ drive?
A:  Please see "Disclaimer" further up in this file.  Also check
    out the "Bugfixes" in the "History" section further down.

Q:  You are German, why is this program in English?
A:  I wanted my program to be accessible by as many people as possible,
    so it had to be in English.  Also, it turned out that German text
    doesn't fit as well inside the labels, menus and button fields as
    English text does.


Some Technical Details
----------------------

To get exclusive disk access and to communicate with the drive, Kernel32
API calls are used (CreateFile, LockFile, SetFilePointer, ReadFile,
WriteFile, UnlockFile and CloseFile).  There already were a few examples
for this on the Internet, but only for VB6, so it took me quite a while
to make this work right.  If you want to try too, http://www.pinvoke.net/
is a good place to get info.

If you remove and re-insert a locked USB device, the program will usually
no longer be able to access it, because the assigned device handle will
become invalid.  If you use a fixed card-reader and only remove and
re-insert the card, then access might still work, but the safest option
always is to simply leave the device connected all the time.

The FAT format is determined by using the methods described here:

  http://homepage.ntlworld.com/jonathan.deboynepollard/FGA/determining-fat-widths.html

Info about the File Allocation Table / directory structure etc. mostly
came from here:

  https://www.pjrc.com/tech/8051/ide/fat32.html

Data is read sector by sector, because it started as a 'proof of concept'
project.  Reading whole clusters might be more efficient, but on today's
hardware, I guess speedy code isn't such an issue anymore.

This program does not mess with the File Allocation Table.  If after
removing deleted entries a directory cluster is not needed anymore, the
cluster will remain allocated, just with all zeroes in it.

The displayed directory list is simply a DataGridView (set to adopt the
appearance of a ListView) to which I attach my list of directory entries
as BindingSource and have the fields colored accordingly.  Sounds easy?
It was NOT! ;)

The other FAT Sorter programs that I mentioned at the beginning of this
file can be found here:

   http://www.murraymoffatt.com/software-problem-0010.html


History
-------

v0.1   Proof of concept.  Get drive info, open / close drive, read first
       sector of root directory and follow-up sectors / clusters of a
       pre-set drive to textbox in HEX and ASCII.

v0.2   Display directory entries of a pre-set drive in double-buffered
       ListView with colored fields, change directories by double-clicking
       an entry, basic sorting of entries with mouse.  Functions split
       over several TabControl pages.

v0.3   Selectable drives from ComboBox, display of directory path, more
       sophisticated sort with mouse, automatic sort by header click,
       CheckBox options for deleted entries & LFN, usage of Queue list.
       Dropped ListView altogether in favor of GridView due to badly
       controllable scrolling in certain situations and other
       disadvantages.

v0.4   Menustrip, options to sort directories recursively and to clear
       Queue, basic code for loading / saving UTF-8 / ANSI with dialog.
       Debug data now redirected to logfile, 'About' window added.

v0.5   Queue list loading / saving, automatic scrolling of directory
       list, logging indicator, extra logging options.

v0.6   TabControl removed and all important stuff put on a single form,
       code and design improvements, simple and professional mode,
       bugfixes, command line options, secret functions.

v0.7   Read/Write error warnings and own icons, simple protection against
       noobs with a Hex Editor, extended sorting options, sorting of
       directory entries with keyboard, speed improvements, final testing
       and bugfixing, code cleanup.  Beta Release Candidate #1 ...

       0.7.458.1

       Bugfix: when "Main Menu" - "Debug" - "Extra Logging Options" -
       "Log Queue List Loading" was activated, it was always writing
       this info to the logfile, even when logging was not active.

       0.8.458.2

       Bugfix: program caused an Overflow Exception on 64bit Windows
       systems when calculating a File Pointer Position where the
       low DWORD was bigger than 0x7fffffff (I used a signed 32-bit
       integer instead of an unsigned 32-bit integer for the result).

       From now on also writes Operating System name / version to the
       logfile, to make things more transparent.

       0.7.458.3

       Bugfix: program still caused an Overflow Exception on certain
       systems.  Now I use BitConverter to create the two DWORD values
       for the File Pointer, which finally seems to work as it should...

       0.7.458.4

       Bugfix: program failed to read follow-up clusters of root directory
       on FAT32 formatted devices.  If root directory occupied more than
       one cluster, contained at least one deleted entry in the first
       cluster and was sorted with option "Remove Deleted Entries",
       entries in follow-up clusters and associated subdirectories were
       not accessible anymore.  Because seeing only the first directory
       cluster, with that setting the deleted entries were put at its end
       and overwritten with zeroes, indicating an 'end of directory' at
       that position.

       To fix such damage, the easiest way would be go to that root
       directory and start creating new folders until all the now
       zeroed-out directory entries have been filled with valid entries
       again.  Since the original cluster chain has not been changed, as
       soon as all entries in the first cluster are filled, the entries
       in the follow-up clusters should become accessible again.

       I suggest using the Command Prompt for this & creating new directory
       entries consisting only of numbers (to avoid the creation of long
       filenames that span multiple entries).

       If sorting was done without "Remove Deleted Entries" or no
       deleted entries were present in the first cluster, worst that
       happened was that entries in follow-up clusters remained
       unsorted.

       Beta Release Candidate #2 ...

v0.8   0.8.465.0

       Bugfix: on partitions larger than 2147483647 (7FFF FFFF hex) sectors,
       WriteFile reports success, but 0 byte are actually written.  After
       a REALLY long time of searching and experimenting, I came to the
       conclusion that this may actually NOT be a bug in my program but in
       the Windows File System Driver that handles FAT partitions.  My guess
       is that somewhere the driver may treat the total number of sectors
       as a signed integer instead of unsigned, thus performing negative
       boundary checks, which let all writes fail because out-of-partition
       writes are prohibited...

       My solution: sending FSCTL_ALLOW_EXTENDED_DASD_IO to DeviceIoControl
       signals the File System Driver not to perform any I/O boundary checks,
       and let the Device Driver do it instead.  But then again, who would
       want to sort stuff on a > 1 TB FAT drive anyway, except meeee? ;)

       Also changed Queue List loading to check for a possible UTF-8 BOM
       (Byte Order Mark - EF BB BF) at beginning of file, and a few other
       minor internal modifications.

       Finally willing to not call it a 'beta release' anymore, Woo-Hoo!!

v0.8   0.8.510.1 (rev.1)

       Whelp, as it seems I somewhat broke the scrolling in my previous
       release by setting a wrong GridView height (for automatic downward
       scrolling with mouse to work correctly, the GridView must show
       'complete' rows only).  Now the height is adapted automatically,
       so it should never happen again.

       While at it, finally I also added the option to resize the window
       (previously blocked for the same reason as described above) and
       turned off GridView double-buffering (wasn't really needed anyway)
       to make the resizing bar for the columns show up correctly.  And I
       activated automatic form scaling based on screen DPI setting (for
       those weirdos who get a boner whenever they see a high-resolution
       monitor - it should now scale correctly at 125%, 150% and even 200%).
       Due to my bad eyes, I still use a resolution of only 800x600 and
       simply can't understand all this 'HD craziness'.

Like Misty from "Pokemon", I don't really like bugs.  So if you find
any in my program, please do let me know.

