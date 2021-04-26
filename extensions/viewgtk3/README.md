## viewGTK3 extension

This extension provides functions to connect to and use a GTK3 widget set.

To compile the extension it needs the *gtk3.h* file and the development library to be present on the system.

On Ubuntu (20.4) Linux the gtk3.h file is installed by default. Check your version:

    dpkg -l libgtk2.0-0 libgtk-3-0

For distributions using KDE / Qt or GTK4: Use the extensions for those.

And the development library by:

    sudo apt-get install libgtk-3-dev

When compiling the extension the flag `pkg-config --cflags gtk+-3.0` and `pkg-config --libs gtk+-3.0` will have to be present, 
like added in the rebmake.r file.
The example compile string from the developer.gnome.org site is:
gcc `pkg-config --cflags gtk+-3.0` -o example example.c `pkg-config --libs gtk+-3.0`

## Implemented functions

Basic functions

    gtk3-window


## Typical use of RENC in combination with GTK3

The use of the GTK3 interface is straightforward and best illustrated with some example code.

### Example code 

