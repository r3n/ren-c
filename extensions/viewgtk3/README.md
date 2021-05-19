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

See the example code below for the most recent status of development.
## Implemented functions

Basic functions

For now only the most basic functions will be implemented. 
First want to experiment in how all works and how for example 
the callback functionality works like when a button is clicked and
because of that a block of Rebol code is to be evaluated/run.
  

## Typical use of RENC in combination with GTK3

The use of the GTK3 interface is straightforward and best illustrated with some example code.

### Example code 

gtk-init
window: gtk-window-new 0
gtk-window-set-title window "Ren-C"
g-signal-connect-data window "destroy" :gtk-main-quit null null 2
box1: gtk-box-new 0 6
gtk-container-add window box1
label1: gtk-label-new "Label!"
blok: [print "Hello"]
button1: gtk-button-new-with-label "Impressive!"
g-signal-connect-data button1 "clicked" :do blok null 0
gtk-container-add box1 label1
gtk-container-add box1 button1
gtk-widget-show-all window
gtk-main

At the moment both pressing the window close and pressing the button result in the window closing 
with a segfault.

Good ideas are very welcome, help appreciated.
