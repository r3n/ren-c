//
//  File: %mod-viewgtk3.c
//  Summary: "GTK3 interface extension"
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
    
    if (type == NULL) {
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
    
    GTKWidget *label = gtk_label_new(str)

    return rebHandle(label, 0, nullptr);
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

    GTKWidget *button = gtk_button_new()

    return rebHandle(button, 0, nullptr);
}
