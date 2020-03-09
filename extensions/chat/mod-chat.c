//
//  File: %mod-chat.c
//  Summary: "Beginnings of chat GUI Interface as an extension"
//  Section: Extension
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
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

#include <gtk/gtk.h>

// Include the rebol core for the rebnative c-function
#include "sys-core.h"

#include "tmp-mod-chat.h"

#define CHAT_LINUX_64_GTK

// Helper functions
// Find a child of a GUI element using the name, this is not standard in GTK
// probably because 'nobody needs this anyway, we always use the GTKBuilder file for our apps'
// named chat_find_child to avoid possible collisions with the version in view (later)
GtkWidget*
chat_find_child(GtkWidget* parent, const gchar* name)
{
    if (g_utf8_collate(g_utf8_strdown (gtk_widget_get_name((GtkWidget*)parent), -1), g_utf8_strdown ((gchar*)name, -1)) == 0) {
        return parent;
    }

    if (GTK_IS_BIN(parent)) {
        GtkWidget *child = gtk_bin_get_child(GTK_BIN(parent));
        return chat_find_child(child, name);
    }

    if (GTK_IS_CONTAINER(parent)) {
        GList *children = gtk_container_get_children(GTK_CONTAINER(parent));
        if (children != NULL){
            do {
                GtkWidget* widget = chat_find_child(children->data, name);
                if (widget != NULL) {
                    return widget;
                }
           } while ((children = g_list_next(children)) != NULL);
       }
    }

    return NULL;
}

static void
read_data_from_url (GtkWidget* widget)
{
    const gchar *entry_text;
    GtkWidget* entry = chat_find_child(gtk_widget_get_toplevel (widget), "entryreadurl");
    entry_text = gtk_entry_get_text (GTK_ENTRY (entry));

    const gchar *area_text;
    // do some Ren-C read magic with rebelide
    area_text = "Here some magic";
    char *rval_text = rebSpell("read ", entry_text, rebEND);
    area_text = rval_text;
    //g_print ("returned Rebval is : %s \n", (gchar*)rval_text);
    // feed the collected data into the text_view area
    GtkWidget* area = chat_find_child(gtk_widget_get_toplevel (widget), "areatextview");

    GtkTextBuffer *buffer;
    buffer = gtk_text_buffer_new(NULL);
    //gtk_text_buffer_set_text(GTK_TEXT_BUFFER (buffer), entry_text, -1);
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER (buffer), area_text, -1);
    gtk_text_view_set_buffer (GTK_TEXT_VIEW (area), buffer);
}

static void
activate (GtkApplication* app,
          gpointer        user_data)
{
    GtkWidget *window;
    GtkWidget *grid;
    GtkWidget *button;
    GtkWidget *entry;
    GtkWidget *area; // text_view

    window = gtk_application_window_new (app);
    gtk_window_set_title (GTK_WINDOW (window), "REN-C 'Chat' Window");
    gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER_ALWAYS);
    gtk_window_set_default_size (GTK_WINDOW (window), 700, 500);

    gtk_container_set_border_width (GTK_CONTAINER (window), 10);

    /* Here we construct the container that is going pack our items */
    grid = gtk_grid_new ();

    /* Pack the container in the window */
    gtk_container_add (GTK_CONTAINER (window), grid);

  // Entry field
    entry = gtk_entry_new();
    //gtk_buildable_set_name(GTK_ENTRY (entry), "entry_read_url"); // GTK_BUILDABLE ??
    gtk_widget_set_name((GtkWidget*) entry, "entryreadurl");
    gtk_entry_set_max_length(GTK_ENTRY (entry), 500);

    /* Place the Entry field in the grid cell (0, 0), and make it
     * span 2 columns.
     */
    gtk_grid_attach (GTK_GRID (grid), entry, 0, 0, 2, 1);

  // Action button for loading the entered data
    button = gtk_button_new_with_label ("Read data from url");
    g_signal_connect (button, "clicked", G_CALLBACK (read_data_from_url), button);

    /* Place the first button in the grid cell (0, 1), and make it fill
     * just 1 cell horizontally and vertically (ie no spanning)
     */
    gtk_grid_attach (GTK_GRID (grid), button, 0, 1, 1, 1);

  // Text area
    area = gtk_text_view_new();
    gtk_widget_set_name((GtkWidget*) area, "areatextview");
    gtk_widget_set_size_request((GtkWidget*) area, 300, 200);

    /* Place the area inside a scrollwindow */
    GtkWidget* scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request((GtkWidget*) scrolledwindow, 300, 200);
    gtk_container_add(GTK_CONTAINER(scrolledwindow), area);
    /* Place the area in the grid cell (0, 2), and make it
     * span 4 columns and 4 rows.
     */
    //gtk_grid_attach (GTK_GRID (grid), area, 0, 2, 4, 4);
    gtk_grid_attach (GTK_GRID (grid), scrolledwindow, 0, 2, 4, 4);

  // Quit button
    button = gtk_button_new_with_label ("Quit");
    g_signal_connect_swapped (button, "clicked", G_CALLBACK (gtk_widget_destroy), window);
    /* Place the Quit button in the grid cell (0, 6), and make it fill
     * just 1 cell horizontally and vertically (ie no spanning)
     */
    gtk_grid_attach (GTK_GRID (grid), button, 0, 6, 1, 1);

    /* Now that we are done packing our widgets, we show them all
     * in one go, by calling gtk_widget_show_all() on the window.
     * This call recursively calls gtk_widget_show() on all widgets
     * that are contained in the window, directly or indirectly.
     */
    gtk_widget_show_all (window);
}

int open_chat_window ()
{
    GtkApplication *app;
    int status;
    int argc = 0;
    char **argv;
    app = gtk_application_new ("info.rebol.rencexample", G_APPLICATION_FLAGS_NONE);
    g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
    status = g_application_run (G_APPLICATION (app), argc, argv);

    g_object_unref (app);

    return status;
}

// "warning: implicit declaration of function"

//
//  export open-chat: native [
//  ]
//
REBNATIVE(open_chat)
{
  CHAT_INCLUDE_PARAMS_OF_OPEN_CHAT;
  REBINT n = 0;
  n = open_chat_window();
  Init_Integer(D_OUT, n);
  return D_OUT;
}
