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

gtk_button_get_label ()

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
     
    GTKWidget *button = gtk_button_set_label(str);

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