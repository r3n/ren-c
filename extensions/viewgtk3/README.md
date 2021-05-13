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

For now only the most basic functions will be implemented. 
First want to experiment in how all works and how for example 
the callback functionality works like when a button is clicked and
because of that a block of Rebol code is to be evaluated/run.
  
    gtk3-window
    gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Window");
  gtk_window_set_default_size (GTK_WINDOW (window), 200, 200);
  gtk_widget_show_all (window);
  gtk_box_new
  gtk_box_pack_start
  gtk_grid_new 
  gtk_grid_attach
  gtk_grid_insert_row ()
  gtk_grid_insert_column (

gtk_label_new ()
gtk_label_set_text ()
gtk_label_get_text ()
gtk_label_get_label ()

gtk_image_new ()
gtk_image_new_from_file ()
gtk_image_clear ()

https://developer.gnome.org/gtk3/stable/ButtonWidgets.html
gtk_button_new ()
gtk_button_new_with_label ()
gtk_button_pressed ()
gtk_button_released ()
gtk_button_clicked ()
gtk_button_get_label ()
gtk_button_set_label ()

And do not forget 
gtk_widget_show_all ()
gtk_widget_show ()
gtk_widget_hide ()
gtk_widget_show_now ()


## Typical use of RENC in combination with GTK3

The use of the GTK3 interface is straightforward and best illustrated with some example code.

### Example code 

