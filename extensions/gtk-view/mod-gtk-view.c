//
//  File: %mod-gtk-view.c
//  Summary: "Beginnings of GUI Interface based on GTK as an extension"
//  Section: Extension
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 Atronix Engineering
// Copyright 2012-2020 Rebol Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
//=////////////////////////////////////////////////////////////////////////=//
// 
// This GTK-View module is selected in the config file %gtkview.r

#define REBOL_IMPLICIT_END  // don't require rebEND in API calls (C99 or C++)

#include <gtk/gtk.h>

// Include the rebol core for the rebnative c-function
#include "sys-core.h"

#include "tmp-mod-gtk-view.h"

// Helper functions
// Find a child of a GUI element using the name, this is not standard in GTK
// probably because 'nobody needs this anyway, we always use the GTKBuilder file for our apps'
// named view-gtk-find-child to avoid possible collisions with other possible versions.
GtkWidget*
view-gtk-find-child(GtkWidget* parent, const gchar* name)
{
    if (g_utf8_collate(g_utf8_strdown (gtk_widget_get_name((GtkWidget*)parent), -1), g_utf8_strdown ((gchar*)name, -1)) == 0) {
        return parent;
    }

    if (GTK_IS_BIN(parent)) {
        GtkWidget *child = gtk_bin_get_child(GTK_BIN(parent));
        return view-gtk-find-child(child, name);
    }

    if (GTK_IS_CONTAINER(parent)) {
        GList *children = gtk_container_get_children(GTK_CONTAINER(parent));
        if (children != NULL){
            do {
                GtkWidget* widget = view-gtk-find-child(children->data, name);
                if (widget != NULL) {
                    return widget;
                }
           } while ((children = g_list_next(children)) != NULL);
       }
    }

    return NULL;
}

// End of Helper functions section

// activate
static void
activate (GtkApplication* app,
          gpointer        user_data)
{
    // Create the window
    GtkWidget *window;
    window = gtk_application_window_new (app);
    // view-gtk-add-window
    
    // In some way loop here over the widgets to put upon the window.
    // view-gtk-add-widgets

    /* Now that we are done packing our widgets, we show them all
     * in one go, by calling gtk_widget_show_all() on the window.
     * This call recursively calls gtk_widget_show() on all widgets
     * that are contained in the window, directly or indirectly.
     */
    gtk_widget_show_all (window);
}

// Open the Window
int open_window ()
{
    GtkApplication *app;
    int status;
    int argc = 0;
    char **argv;
    app = gtk_application_new ("info.rebol.rencview", G_APPLICATION_FLAGS_NONE);
    g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
    status = g_application_run (G_APPLICATION (app), argc, argv);

    g_object_unref (app);

    return status;
}


// "warning: implicit declaration of function"

//
//  export open-view: native [
//  ]
//
REBNATIVE(open_view)
{
  CHAT_INCLUDE_PARAMS_OF_OPEN_VIEW;
  REBINT n = 0;
  n = open_window();
  Init_Integer(D_OUT, n);
  return D_OUT;
}