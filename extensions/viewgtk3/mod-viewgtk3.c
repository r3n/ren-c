//
//  File: %mod-viewgtk3.c
//  Summary: "ViewGTK3 interface extension"
//  Section: Extension
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012-2021 Ren-C Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information.
//
// Licensed under the Lesser GPL, Version 3.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.gnu.org/licenses/lgpl-3.0.html
//
//=////////////////////////////////////////////////////////////////////////=//
//
// See README.md for notes about this extension.
//

#include <gtk/gtk.h>

#define REBOL_IMPLICIT_END
// Include the rebol core for the rebnative c-function
#include "sys-core.h"

#include "tmp-mod-viewgtk3.h"

// Helper functions

// End Helper Functions

// "warning: implicit declaration of function"

// Window functions

//
//  export gtk-window-new: native [
//
//      {Creates a new label with the given text inside it.}
//
//      return: [handle! void!]
//      type [text!]
//  ]
//
REBNATIVE(gtk_window_new)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_WINDOW_NEW;

    GtkWindowType * type = cast(GtkWindowType*, VAL_STRING_AT(ARG(type)));
    
    if (type == NULL or type == "") {
        type = GTK_WINDOW_TOPLEVEL;
    }
    GtkWidget *window = gtk_window_new(type);

    return rebHandle(window, 0, nullptr);
}

// Other Widget functions

// Widget Label functions

//
//  export gtk-label-new: native [
//
//      {Creates a new label with the given text inside it.}
//
//      return: [handle! void!]
//      str [text!]
//  ]
//
REBNATIVE(gtk_label_new)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_LABEL_NEW;

    const char * str = cast(char*, VAL_STRING_AT(ARG(str)));
    
    GTKWidget *label = gtk_label_new(str);

    return rebHandle(label, 0, nullptr);
}

//
//  export gtk-button-get-text: native [
//
//      {Creates a GtkButton widget with a GtkLabel child containing the given text.}
//
//      return: [text!]
//      label [handle!]
//  ]
//
REBNATIVE(gtk_label_get_text)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_LABEL_GET_TEXT;

    GTKWidget *label = VAL_HANDLE_POINTER(GTKWidget, ARG(label));
     
    const char *result = gtk_label_get_text(label);

    return rebText(result);
}

//
//  export gtk-label-set-text: native [
//
//      {Sets the text within the GtkLabel widget. It overwrites any text that was there before.
// This function will clear any previously set mnemonic accelerators, 
// and set the “use-underline” property to FALSE as a side effect.
//
// This function will set the “use-markup” property to FALSE as a side effect.}
//
//      return: [void!]
//      label [handle!]
//      str [text!]
//  ]
//
REBNATIVE(gtk_label_set_text)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_LABEL_SET_TEXT;

    GTKWidget *label = VAL_HANDLE_POINTER(GTKWidget, ARG(label));

    const char * str = cast(char*, VAL_STRING_AT(ARG(str)));
     
    GTKWidget *label = gtk_label_set_text(str);

    return rebVoid();
}

// Widget Button functions

//
//  export gtk-button-new: native [
//
//      {Creates a new GtkButton widget. To add a child widget to the button, use gtk_container_add().}
//
//      return: [handle! void!]
//  ]
//
REBNATIVE(gtk_button_new)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_BUTTON_NEW;

    GTKWidget *button = gtk_button_new();

    return rebHandle(button, 0, nullptr);
}

//
//  export gtk-button-new-with-label: native [
//
//      {Creates a GtkButton widget with a GtkLabel child containing the given text.}
//
//      return: [handle! void!]
//      str [text!]
//  ]
//
REBNATIVE(gtk_button_new_with_label)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_BUTTON_NEW_WITH_LABEL;

    const char * str = cast(char*, VAL_STRING_AT(ARG(str)));
     
    GTKWidget *button = gtk_button_new_with_label(str);

    return rebHandle(button, 0, nullptr);
}

//
//  export gtk-button-get-label: native [
//
//      {Creates a GtkButton widget with a GtkLabel child containing the given text.}
//
//      return: [text!]
//      button [handle!]
//  ]
//
REBNATIVE(gtk_button_get_label)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_BUTTON_GET_LABEL;

    GTKWidget *button = VAL_HANDLE_POINTER(GTKWidget, ARG(button));
     
    const char *result = gtk_button_get_label(button);

    return rebText(result);
}

//
//  export gtk-button-set-label: native [
//
//      {Sets the text of the label of the button to str.}
//
//      return: [void!]
//      str [text!]
//  ]
//
REBNATIVE(gtk_button_set_label)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_BUTTON_SET_LABEL;

    const char * str = cast(char*, VAL_STRING_AT(ARG(str)));
     
    gtk_button_set_label(str);

    return rebVoid();
}

// Widget Image functions

//
//  export gtk-image-new: native [
//
//      {Creates a new empty GtkImage widget.}
//
//      return: [handle! void!]
//  ]
//
REBNATIVE(gtk_image_new)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_IMAGE_NEW;

    GTKWidget *image = gtk_image_new();

    return rebHandle(image, 0, nullptr);
}

//
//  export gtk-image-new-from-file: native [
//
//      {Creates a new empty GtkImage widget.}
//
//      return: [handle! void!]
//      str [text!]
//  ]
//
REBNATIVE(gtk_image_new_from_file)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_IMAGE_NEW_FROM_FILE;

    const char * str = cast(char*, VAL_STRING_AT(ARG(str)));

    GTKWidget *image = gtk_image_new_from_file(str);

    return rebHandle(image, 0, nullptr);
}

//
//  export gtk-image-new-from-file: native [
//
//      {Creates a new empty GtkImage widget.}
//
//      return: [void!]
//      image [handle!]
//      str [text!]
//  ]
//
REBNATIVE(gtk_image_set_from_file)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_IMAGE_NEW_FROM_FILE;

    GTKWidget *image = VAL_HANDLE_POINTER(GTKWidget, ARG(image));

    const char * str = cast(char*, VAL_STRING_AT(ARG(str)));

    gtk_image_set_from_file(image, str);

    return rebVoid();
}

//
//  export gtk-image-clear: native [
//
//      {Sets the text of the label of the button to str.}
//
//      return: [void!]
//      image [handle!]
//  ]
//
REBNATIVE(gtk_image_clear)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_BUTTON_IMAGE_CLEAR;

    GTKWidget *image = VAL_HANDLE_POINTER(GTKWidget, ARG(image));
     
    gtk_image_clear(image);

    return rebVoid();
}

// Widget Entry (Field, single line) functions

//
//  export gtk-entry-new: native [
//
//      {Creates a new entry (field).}
//
//      return: [handle! void!]
//  ]
//
REBNATIVE(gtk_entry_new)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_ENTRY_NEW;

    GTKWidget *field = gtk_entry_new();

    return rebHandle(field, 0, nullptr);
}

//
//  export gtk-entry-new-with-buffer: native [
//
//      {Creates a new entry with the specified text buffer.}
//
//      return: [handle! void!]
//      buffer [handle!]
//  ]
//
REBNATIVE(gtk_entry_new_with_buffer)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_ENTRY_NEW_WITH_BUFFER;

    GTKWidget *buffer = VAL_HANDLE_POINTER(GTKWidget, ARG(buffer));

    GTKWidget *field = gtk_entry_new_with_buffer(buffer);

    return rebHandle(field, 0, nullptr);
}

//
//  export gtk-entry-new-with-buffer: native [
//
//      {Get the GtkEntryBuffer object which holds the text for this widget.}
//
//      return: [handle! void!]
//      field [handle!]
//  ]
//
REBNATIVE(gtk_entry_get_buffer)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_ENTRY_GET_BUFFER;

    GTKWidget *field = VAL_HANDLE_POINTER(GTKWidget, ARG(field));

    GTKWidget *buffer = gtk_entry_get_buffer(field);

    return rebHandle(buffer, 0, nullptr);
}

//
//  export gtk-entry-set-text: native [
//
//      {Sets the text in the widget to the given value, replacing the current contents.}
//
//      return: [void!]
//      field [handle!]
//      str [text!]
//  ]
//
REBNATIVE(gtk_entry_set_text)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_ENTRY_SET_TEXT;

    GTKWidget *field = VAL_HANDLE_POINTER(GTKWidget, ARG(field));

    const char * str = cast(char*, VAL_STRING_AT(ARG(str)));

    gtk_entry_set_text(field, str);

    return rebVoid();
}

//
//  export gtk-entry-get-text: native [
//
//      {Retrieves the contents of the entry widget. See also gtk_editable_get_chars().
// This is equivalent to getting entry 's GtkEntryBuffer 
// and calling gtk_entry_buffer_get_text() on it.}
//
//      return: [text!]
//      field [handle!]
//  ]
//
REBNATIVE(gtk_entry_get_text)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_ENTRY_GET_TEXT;

    GTKWidget *field = VAL_HANDLE_POINTER(GTKWidget, ARG(field));
     
    const char *result = gtk_entry_get_text(field);

    return rebText(result);
}

//
//  export gtk-entry-get-text-length: native [
//
//      {Retrieves the current length of the text in entry .
// This is equivalent to getting entry's GtkEntryBuffer and 
// calling gtk_entry_buffer_get_length() on it.}
//
//      return: [integer!]
//      field [handle!]
//  ]
//
REBNATIVE(gtk_entry_get_text_length)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_ENTRY_GET_TEXT_LENGTH;

    GTKWidget *field = VAL_HANDLE_POINTER(GTKWidget, ARG(field));
     
    unsigned int result = gtk_entry_get_text_length(field);

    return rebInteger(result);
}

// todo
// Don't know yet how to read or return a character (issue! type)
// void gtk_entry_set_invisible_char (GtkEntry *entry,
//                                    gunichar ch);
// Sets the character to use in place of the actual text when gtk_entry_set_visibility() 
// has been called to set text visibility to FALSE. i.e. this is the character used in “password mode” 
// to show the user how many characters have been typed. By default, 
// GTK+ picks the best invisible char available in the current font. If you set the invisible char to 0, 
//then the user will get no feedback at all; there will be no text on the screen as they type.

//
//  export gtk-entry-get-invisible-char: native [
//
//      {Retrieves the character displayed in place of the real characters for entries with visibility set to false.}
//
//      return: [issue! text!]
//      field [handle!]
//  ]
//
REBNATIVE(gtk_entry_get_invisible_char)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_ENTRY_GET_INVISIBLE_CHAR;

    GTKEntry *field = VAL_HANDLE_POINTER(GTKEntry, ARG(field));

    const char* result = gtk_entry_get_invisible_char(field);

    return rebText(result);
}

//
//  export gtk-entry-set-max-length: native [
//
//      {Sets the maximum allowed length of the contents of the widget. 
// If the current contents are longer than the given length, then they will be truncated to fit.}
//
//      return: [void!]
//      field [handle!]
//      maxlen [integer!]
//  ]
//
REBNATIVE(gtk_entry_set_max_length)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_ENTRY_SET_TEXT;

    GTKWidget *field = VAL_HANDLE_POINTER(GTKWidget, ARG(field));

    unsigned int maxlen = rebUnboxInteger(ARG(maxlen));

    gtk_entry_set_max_length(field, maxlen);

    return rebVoid();
}

//
//  export gtk-entry-get-maxfield
//      return: [integer!]
//      field [handle!]
//  ]
//
REBNATIVE(gtk_entry_get_max_length)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_ENTRY_GET_MAX_LENGTH;

    GTKWidget *field = VAL_HANDLE_POINTER(GTKWidget, ARG(field));
     
    unsigned int result = gtk_entry_get_max_length(field);

    return rebInteger(result);
}

//
//  export gtk-entry-set-visibility: native [
//
//      {Sets whether the contents of the entry are visible or not.}
//
//      return: [void!]
//      field [handle!]
//      visible [logic!]
//  ]
//
REBNATIVE(gtk_entry_set_visibility)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_ENTRY_SET_VISIBILITY;

    GTKWidget *field = VAL_HANDLE_POINTER(GTKWidget, ARG(field));

    bool visible = rebDid(ARG(visible));
 
    gtk_entry_set_visibility(field, visible);

    return rebVoid();
}

//
//  export gtk-entry-get-visibility: native [
//
//      {Retrieves whether the text in entry is visible.}
//
//      return: [logic!]
//      field [handle!]
//  ]
//
REBNATIVE(gtk_entry_get_visibility)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_ENTRY_GET_VISIBILITY;

    GTKWidget *field = VAL_HANDLE_POINTER(GTKWidget, ARG(field));
     
    bool result = gtk_entry_get_visibility(connection);
    REBVAL *logic = rebValue("TO LOGIC!", result);

    return rebValue(logic);
}

// Widget Canvas (drawing area) functions

//
//  export gtk-drawing-area-new: native [
//
//      {Creates a new drawing area.}
//
//      return: [handle! void!]
//  ]
//
REBNATIVE(gtk_drawing_area_new)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_DRAWING_AREA_NEW;

    GTKWidget *canvas = gtk_drawing_area_new();

    return rebHandle(canvas, 0, nullptr);
}

// Widget Layout functions

// Box Layout

//
//  export gtk-box-new: native [
//
//      {Creates a new GtkBox with orientation and spacing.}
//
//      return: [handle! void!]
//      orientation [handle!]
//      spacing [integer!]
//  ]
//
REBNATIVE(gtk_box_new)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_BOX_NEW;

    GtkOrientation *orientation = VAL_HANDLE_POINTER(GtkOrientation, ARG(orientation));

    unsigned int spacing = rebUnboxInteger(ARG(spacing));

    GTKWidget *box = gtk_box_new(orientation, spacing);
    
    return rebHandle(box, 0, nullptr);
}

//
//  export gtk-box-pack-start: native [
//
//      {Adds child to box, packed with reference to the start of box. 
// The child is packed after any other child packed with reference to the start of box.}
//
//      return: [void!]
//      box [handle!]
//      child [handle!]
//      expand [logic!]
//      fill [logic!]
//      padding [integer!]
//  ]
//
REBNATIVE(gtk_box_pack_start)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_BOX_PACK_START;

    GTKBox *box = VAL_HANDLE_POINTER(GTKBox, ARG(box));
    GTKWidget *child = VAL_HANDLE_POINTER(GTKWidget, ARG(child));

    bool expand = rebDid(ARG(expand));
    bool fill = rebDid(ARG(fill));
 
    unsigned int padding = rebUnboxInteger(ARG(padding));

    gtk_box_pack_start(box, child, expand, fill, padding);

    return rebVoid();
}

//
//  export gtk-box-pack-end: native [
//
//      {Adds child to box, packed with reference to the end of box. 
// The child is packed after (away from end of) any other child packed with reference to the end of box.}
//
//      return: [void!]
//      box [handle!]
//      child [handle!]
//      expand [logic!]
//      fill [logic!]
//      padding [integer!]
//  ]
//
REBNATIVE(gtk_box_pack_end)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_BOX_PACK_END;

    GTKBox *box = VAL_HANDLE_POINTER(GTKBox, ARG(box));
    GTKWidget *child = VAL_HANDLE_POINTER(GTKWidget, ARG(child));

    bool expand = rebDid(ARG(expand));
    bool fill = rebDid(ARG(fill));
 
    unsigned int padding = rebUnboxInteger(ARG(padding));

    gtk_box_pack_end(box, child, expand, fill, padding);

    return rebVoid();
}

//
//  export gtk-box-get-spacing: native [
//
//      {Sets the “spacing” property of box, which is the number of pixels to place between children of box.}
//
//      return: [integer!]
//      box [handle!]
//  ]
//
REBNATIVE(gtk_box_get_spacing)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_BOX_GET_SPACING;

    GTKBox *box = VAL_HANDLE_POINTER(GTKWidget, ARG(box));
     
    unsigned int result = gtk_box_get_spacing(box);

    return rebInteger(result);
}

//
//  export gtk-box-set_spacing: native [
//
//      {Sets the “spacing” property of box, which is the number of pixels to place between children of box.}
//
//      return: [void!]
//      box [handle!]
//      spacing [integer!]
//  ]
//
REBNATIVE(gtk_box_set_spacing)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_BOX_SET_SPACING;

    GTKBox *box = VAL_HANDLE_POINTER(GTKBox, ARG(box));
 
    unsigned int spacing = rebUnboxInteger(ARG(spacing));

    gtk_box_set_spacing(box, spacing);

    return rebVoid();
}

// Grid Layout

//
//  export gtk-grid-new: native [
//
//      {Creates a new grid widget.}
//
//      return: [handle! void!]
//  ]
//
REBNATIVE(gtk_grid_new)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_GRID_NEW;

    GTKWidget *grid = gtk_grid_new();
    
    return rebHandle(grid, 0, nullptr);
}

//
//  export gtk-grid-attach: native [
//
//      {Adds a widget to the grid.
// The position of child is determined by left and top. 
// The number of “cells” that child will occupy is determined by width and height.}
//
//      return: [void!]
//      grid [handle!]
//      child [handle!]
//      left [integer!]
//      top [integer!]
//      width [integer!]
//      height [integer!]
//  ]
//
REBNATIVE(gtk_grid_attach)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_GRID_ATTACH;

    GTKGrid *grid = VAL_HANDLE_POINTER(GTKGrid, ARG(grid));
    GTKWidget *child = VAL_HANDLE_POINTER(GTKWidget, ARG(child));
 
    unsigned int left = rebUnboxInteger(ARG(left));
    unsigned int top = rebUnboxInteger(ARG(top));
    unsigned int width = rebUnboxInteger(ARG(width));
    unsigned int height = rebUnboxInteger(ARG(height));

    gtk_grid_attach(grid, child, left, top, width, height);

    return rebVoid();
}

//
//  export gtk-grid-insert-row : native [
//
//      {Inserts a row at the specified position.
// Children which are attached at or below this position are moved one row down. 
// Children which span across this position are grown to span the new row.}
//
//      return: [void!]
//      grid [handle!]
//      position [integer!]
//  ]
//
REBNATIVE(gtk_grid_insert_row)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_GRID_INSERT_ROW;

    GTKGrid *grid = VAL_HANDLE_POINTER(GTKGrid, ARG(grid));
 
    unsigned int position = rebUnboxInteger(ARG(position));

    gtk_grid_insert_row(grid, position);

    return rebVoid();
}

//
//  export gtk-grid-insert-column : native [
//
//      {Inserts a column at the specified position.
// Children which are attached at or to the right of this position are moved one column to the right. 
// Children which span across this position are grown to span the new column.}
//
//      return: [void!]
//      grid [handle!]
//      position [integer!]
//  ]
//
REBNATIVE(gtk_grid_insert_column)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_GRID_INSERT_COLUMN;

    GTKGrid *grid = VAL_HANDLE_POINTER(GTKGrid, ARG(grid));
 
    unsigned int position = rebUnboxInteger(ARG(position));

    gtk_grid_insert_column(grid, position);

    return rebVoid();
}

// Widget Show (and Hide) functions

//
//  export gtk-widget-show: native [
//
//      {Flags a widget to be displayed. 
// Any widget that isn’t shown will not appear on the screen. 
// If you want to show all the widgets in a container, 
// it’s easier to call gtk_widget_show_all() on the container, 
// instead of individually showing the widgets.
// 
// Remember that you have to show the containers containing a widget, 
// in addition to the widget itself, before it will appear onscreen.
// 
// When a toplevel container is shown, it is immediately realized and mapped; 
// other shown widgets are realized and mapped when 
// their toplevel container is realized and mapped.}
//
//      return: [void!]
//      widget [handle!]
//  ]
//
REBNATIVE(gtk_widget_show)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_WIDGET_SHOW;

    GTKWidget *widget = VAL_HANDLE_POINTER(GTKWidget, ARG(widget));
     
    gtk_widget_show(widget);

    return rebVoid();
}

//
//  export gtk-widget-show-now: native [
//
//      {Shows a widget. If the widget is an unmapped toplevel widget 
// (i.e. a GtkWindow that has not yet been shown), 
// enter the main loop and wait for the window to actually be mapped. 
// Be careful; because the main loop is running, 
// anything can happen during this function.}
//
//      return: [void!]
//      widget [handle!]
//  ]
//
REBNATIVE(gtk_widget_show_now)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_WIDGET_SHOW_NOW;

    GTKWidget *widget = VAL_HANDLE_POINTER(GTKWidget, ARG(widget));
     
    gtk_widget_show_now(widget);

    return rebVoid();
}

//
//  export gtk-widget-hide: native [
//
//      {Reverses the effects of gtk_widget_show(), 
// causing the widget to be hidden (invisible to the user).}
//
//      return: [void!]
//      widget [handle!]
//  ]
//
REBNATIVE(gtk_widget_hide)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_WIDGET_HIDE;

    GTKWidget *widget = VAL_HANDLE_POINTER(GTKWidget, ARG(widget));
     
    gtk_widget_hide(widget);

    return rebVoid();
}

//
//  export gtk-widget-show-all: native [
//
//      {Recursively shows a widget, and any child widgets (if the widget is a container).}
//
//      return: [void!]
//      widget [handle!]
//  ]
//
REBNATIVE(gtk_widget_show_all)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_WIDGET_SHOW;

    GTKWidget *widget = VAL_HANDLE_POINTER(GTKWidget, ARG(widget));
     
    gtk_widget_show_all(widget);

    return rebVoid();
}