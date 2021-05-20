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

#define REBOL_IMPLICIT_END
#define DEBUG_STDIO_OK
// Include the rebol core for the rebnative c-function
#include "sys-core.h"

#include "tmp-mod-viewgtk3.h"

#include <gtk/gtk.h>

// Helper functions

// End Helper Functions

// "warning: implicit declaration of function"

// General functions

//
//  export gtk-init: native [
//
//      {Call this function before using any other GTK+ functions in your GUI applications. 
// It will initialize everything needed to operate the toolkit and parses some standard command line options.}
//
//      return: [void!]
//  ]
//
REBNATIVE(gtk_init)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_INIT;

    int argc = 0;
    gtk_init(&argc, nullptr);
    
    return rebVoid();
}

//
//  export gtk-init-check: native [
//
//      {Call this function before using any other GTK+ functions in your GUI applications. 
// It will initialize everything needed to operate the toolkit and parses some standard command line options.
// With a check so will not crash the application if it does not succeed.}
//
//      return: [void!]
//  ]
//
REBNATIVE(gtk_init_check)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_INIT_CHECK;

    int argc = 0;
    if (not gtk_init_check(&argc, nullptr))
        fail ("gtk_init_check() failed");

    return rebVoid();
}

//
//  export gtk-main: native [
//
//      {Runs the main loop until gtk_main_quit() is called.
// You can nest calls to gtk_main(). In that case gtk_main_quit() will make the innermost invocation of the main loop return.}
//
//      return: [void!]
//  ]
//
REBNATIVE(gtk_main)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_MAIN;

    gtk_main();

    return rebVoid();
}

//
//  export gtk-main-level: native [
//
//      {Asks for the current nesting level of the main loop.}
//
//      return: [integer!]
//  ]
//
REBNATIVE(gtk_main_level)
// Convenience function for debugging purposes
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_MAIN_LEVEL;

    unsigned int result = gtk_main_level();

    return rebInteger(result);
}

//
//  export gtk-main-quit: native [
//
//      {Makes the innermost invocation of the main loop return when it regains control.}
//
//      return: [void!]
//  ]
//
REBNATIVE(gtk_main_quit)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_MAIN_QUIT;

    gtk_main_quit();

    return rebVoid();
}

// Signal function(s)

// g_signal_connect, g_signal_connect_after and g_signal_connect_swapped
// are convenience wrappers (macro/define) around the actual function 
// g_signal_connect_data.
// gulong g_signal_connect_data (gpointer instance,
//                               const gchar *detailed_signal,
//                               GCallback c_handler,
//                               gpointer data,
//                               GClosureNotify destroy_data,
//                               GConnectFlags connect_flags);
// #define g_signal_connect(instance, detailed_signal, c_handler, data) \
//    g_signal_connect_data ((instance), (detailed_signal), (c_handler), (data), NULL, (GConnectFlags) 0)
// #define g_signal_connect_after(instance, detailed_signal, c_handler, data) \
//    g_signal_connect_data ((instance), (detailed_signal), (c_handler), (data), NULL, G_CONNECT_AFTER)
// #define g_signal_connect_swapped(instance, detailed_signal, c_handler, data) \
//    g_signal_connect_data ((instance), (detailed_signal), (c_handler), (data), NULL, G_CONNECT_SWAPPED)
// Where the connectflags are defined to be
// typedef enum
// {
//   G_CONNECT_AFTER       = 1 << 0,
//   G_CONNECT_SWAPPED     = 1 << 1
// } GConnectFlags;

//
//  export g-signal-connect-data: native [
//
//      {Connects a GCallback function to a signal for a particular object. Similar to g_signal_connect(), 
// but allows to provide a GClosureNotify for the data which will be called when the signal handler is 
// disconnected and no longer used.
// Specify connect_flags if you need ..._after() or ..._swapped() variants of this function.
// Usage example: g-signal-connect-data  as-handle window*  "destroy"  as-integer :quit  null
// g-connect-signal-data instance detailed-signal handler data null 0
// g-connect-signal-data instance detailed-signal handler data null connect-swapped 
// (where connect-swapped = 2 and connect-after = 1, normal = 0)}
//
//      return: [integer!]
//      instance [handle!]
//      detailedsignal [text!]
//      chandler [handle! action!]
//      data [<opt> handle! block!]
//      destroydata [<opt> handle!]
//      flags [integer!]
//  ]
//
REBNATIVE(g_signal_connect_data)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_G_SIGNAL_CONNECT_DATA;

    // instance is a gpointer type, handle
    gpointer *instance = VAL_HANDLE_POINTER(gpointer, ARG(instance));

    // detailedsignal is a text to hold the description of the action. For example "quit", "clicked"
    // signal names are to make distinction between the action to perform.
    const char * detailedsignal = cast(char*, VAL_STRING_AT(ARG(detailedsignal)));

    // chandler is handle that must be cast to a gcallback
    REBVAL *value = ARG(chandler);
    GCallback chandler = G_CALLBACK(value);

    // data is a gpointer for data to pass to c_handler calls. 
    gpointer *data = VAL_HANDLE_POINTER(gpointer, ARG(data));

    // destroydata is handle for g-closure-notify, often a value of null is used in examples
    GClosureNotify *destroydata = VAL_HANDLE_POINTER(GClosureNotify, ARG(destroydata));

    // flags is an integer value, 0 = normal signal connect, 1 = after, 2 = swapped
    GConnectFlags flags = (GConnectFlags) rebUnboxInteger(ARG(flags));

    // result is > 0 for success adding a signal
    //unsigned int result = g_signal_connect_data(instance, detailedsignal, chandler, data, destroydata, flags);
    unsigned int result = g_signal_connect_data(instance, detailedsignal, chandler, NULL, NULL, flags);

    return rebInteger(result);
}

// Window functions

//
//  export gtk-window-new: native [
//
//      {Creates a new GtkWindow, which is a toplevel window that can contain other widgets.
// Nearly always, the type of the window should be GTK_WINDOW_TOPLEVEL. If you’re implementing
// something like a popup menu from scratch (which is a bad idea, just use GtkMenu), you might
// use GTK_WINDOW_POPUP. GTK_WINDOW_POPUP is not for dialogs, though in some other toolkits dialogs 
// are called “popups”. In GTK+, GTK_WINDOW_POPUP means a pop-up menu or pop-up tooltip.
// On X11, popup windows are not controlled by the window manager.
// 
// If you simply want an undecorated window (no window borders), use gtk_window_set_decorated(), don’t use GTK_WINDOW_POPUP.
// 
// All top-level windows created by gtk_window_new() are stored in an internal top-level window list. 
// This list can be obtained from gtk_window_list_toplevels(). Due to Gtk+ keeping a reference to the window internally, 
// gtk_window_new() does not return a reference to the caller.
// 
// To delete a GtkWindow, call gtk_widget_destroy().}
//
//      return: [handle! void!]
//      type [integer!]
//  ]
//
REBNATIVE(gtk_window_new)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_WINDOW_NEW;

    GtkWindowType type = (GtkWindowType) rebUnboxInteger(ARG(type));
    
    GtkWidget *window = gtk_window_new(type);

    return rebHandle(window, 0, nullptr);
}

//
//  export gtk-window-set-title: native [
//
//      {Sets the title of the GtkWindow. 
// The title of a window will be displayed in its title bar; on the X Window System, 
// the title bar is rendered by the window manager, so exactly how the title appears 
// to users may vary according to a user’s exact configuration. 
// The title should help a user distinguish this window from other windows they may have open. 
// A good title might include the application name and current document filename, for example.}
//
//      return: [void!]
//      window [handle!]
//      title [text!]
//  ]
//
REBNATIVE(gtk_window_set_title)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_WINDOW_SET_TITLE;

    GtkWindow *window = VAL_HANDLE_POINTER(GtkWindow, ARG(window));

    const char * title = cast(char*, VAL_STRING_AT(ARG(title)));
     
    gtk_window_set_title(window, title);

    return rebVoid();
}

// Window placement functions

//
//  export gtk-window-set-position: native [
//
//      {Sets a position constraint for this window. If the old or new constraint is GTK_WIN_POS_CENTER_ALWAYS, 
// this will also cause the window to be repositioned to satisfy the new constraint.
// enum GtkWindowPosition
// Window placement can be influenced using this enumeration. 
// Note that using GTK_WIN_POS_CENTER_ALWAYS is almost always a bad idea. 
// It won’t necessarily work well with all window managers or on all windowing systems.
// Members:
// GTK_WIN_POS_NONE             No influence is made on placement.
// GTK_WIN_POS_CENTER           Windows should be placed in the center of the screen.
// GTK_WIN_POS_MOUSE            Windows should be placed at the current mouse position.
// GTK_WIN_POS_CENTER_ALWAYS    Keep window centered as it changes size, etc.
// GTK_WIN_POS_CENTER_ON_PARENT Center the window on its transient parent (see gtk_window_set_transient_for()).}
//
//      return: [void!]
//      window [handle!]
//      position [integer!]
//  ]
//
REBNATIVE(gtk_window_set_position)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_WINDOW_SET_POSITION;

    GtkWindow *window = VAL_HANDLE_POINTER(GtkWindow, ARG(window));

    GtkWindowPosition position = (GtkWindowPosition) rebUnboxInteger(ARG(position));
    
    gtk_window_set_position(window, position);

    return rebVoid();
}

//
//  export gtk-window-get-position: native [
//
//      {This function returns the position you need to pass to gtk_window_move() 
// to keep window in its current position. This means that the meaning of the returned value 
// varies with window gravity. See gtk_window_move() for more details.)}
//
//      return: [pair!]
//      window [handle!]
//  ]
//
REBNATIVE(gtk_window_get_position)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_WINDOW_GET_POSITION;

    GtkWindow *window = VAL_HANDLE_POINTER(GtkWindow, ARG(window));

    gint x;
    gint y;

    gtk_window_get_position(window, &x, &y);

    REBVAL *pair = rebValue("make pair! [", rebI(x), rebI(y), "]");

    return pair;
}

//
//  export gtk-window-move: native [
//
//      {Asks the window manager to move window to the given position. 
// Window managers are free to ignore this; most window managers ignore requests for initial window positions
// (instead using a user-defined placement algorithm) and honor requests after the window has already been shown.}
//
//      return: [void!]
//      window [handle!]
//      x [integer!]
//      y [integer!]
//  ]
//
REBNATIVE(gtk_window_move)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_WINDOW_MOVE;

    GtkWindow *window = VAL_HANDLE_POINTER(GtkWindow, ARG(window));
   
    unsigned int x = rebUnboxInteger(ARG(x));

    unsigned int y = rebUnboxInteger(ARG(y));

    gtk_window_move(window, x, y);

    return rebVoid();
}

//
//  export gtk-window-set-resizable: native [
//
//      {Sets whether the user can resize a window. Windows are user resizable by default.}
//
//      return: [void!]
//      window [handle!]
//      resizable [logic!]
//  ]
//
REBNATIVE(gtk_window_set_resizable)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_WINDOW_SET_RESIZABLE;

    GtkWindow *window = VAL_HANDLE_POINTER(GtkWindow, ARG(window));

    bool resizable = rebDid(ARG(resizable));

    gtk_window_set_resizable(window, resizable);

    return rebVoid();
}

//
//  export gtk-window-set-default-size: native [
//
//      {Sets the default size of a window. If the window’s “natural” size (its size request) is larger than the default,
// the default will be ignored. More generally, if the default size does not obey the geometry hints for the window
// (gtk_window_set_geometry_hints() can be used to set these explicitly), the default size will be clamped to the nearest permitted size.}
//
//      return: [void!]
//      window [handle!]
//      width [integer!]
//      height [integer!]
//  ]
//
REBNATIVE(gtk_window_set_default_size)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_WINDOW_SET_DEFAULT_SIZE;

    GtkWindow *window = VAL_HANDLE_POINTER(GtkWindow, ARG(window));

    unsigned int width = rebUnboxInteger(ARG(width));

    unsigned int height = rebUnboxInteger(ARG(height));

    gtk_window_set_default_size(window, width, height);

    return rebVoid();
}

//
//  export gtk-window-resize: native [
//
//      {Resizes the window as if the user had done so, obeying geometry constraints.
// The default geometry constraint is that windows may not be smaller than their size request; 
// to override this constraint, call gtk_widget_set_size_request() to set the window s request to a smaller value.
// If gtk_window_resize() is called before showing a window for the first time, 
// it overrides any default size set with gtk_window_set_default_size().}
//
//      return: [void!]
//      window [handle!]
//      width [integer!]
//      height [integer!]
//  ]
//
REBNATIVE(gtk_window_resize)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_WINDOW_RESIZE;

    GtkWindow *window = VAL_HANDLE_POINTER(GtkWindow, ARG(window));

    unsigned int width = rebUnboxInteger(ARG(width));

    unsigned int height = rebUnboxInteger(ARG(height));

    gtk_window_resize(window, width, height);

    return rebVoid();
}

//
//  export gtk-window-get-size: native [
//
//      {Obtains the current size of window.}
//
//      return: [pair!]
//      window [handle!]
//  ]
//
REBNATIVE(gtk_window_get_size)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_WINDOW_GET_SIZE;

    GtkWindow *window = VAL_HANDLE_POINTER(GtkWindow, ARG(window));

    gint x, y;

    gtk_window_get_size(window, &x, &y);

    REBVAL *pair = rebValue("make pair! [", rebI(x), rebI(y), "]");

    return pair;
}

//
//  export gtk-window-maximize: native [
//
//      {Asks to maximize window , so that it becomes full-screen. Note that you shouldn’t assume the window is
// definitely maximized afterward, because other entities (e.g. the user or window manager) could unmaximize it again,
// and not all window managers support maximization. But normally the window will end up maximized.
// Just don’t write code that crashes if not.
// It’s permitted to call this function before showing a window, in which case the window will be maximized when it appears onscreen initially.
// You can track maximization via the “window-state-event” signal on GtkWidget, or by listening to notifications on the “is-maximized” property.}
//
//      return: [void!]
//      window [handle!]
//  ]
//
REBNATIVE(gtk_window_maximize)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_WINDOW_MAXIMIZE;

    GtkWindow *window = VAL_HANDLE_POINTER(GtkWindow, ARG(window));

    gtk_window_maximize(window);

    return rebVoid();
}

//
//  export gtk-window-unmaximize: native [
//
//      {Asks to unmaximize window. Note that you shouldn’t assume the window is definitely unmaximized afterward,
// because other entities (e.g. the user or window manager) could maximize it again, and not all window managers honor
// requests to unmaximize. But normally the window will end up unmaximized. Just don’t write code that crashes if not.}
//
//      return: [void!]
//      window [handle!]
//  ]
//
REBNATIVE(gtk_window_unmaximize)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_WINDOW_UNMAXIMIZE;

    GtkWindow *window = VAL_HANDLE_POINTER(GtkWindow, ARG(window));

    gtk_window_unmaximize(window);

    return rebVoid();
}

//
//  export gtk-window-fullscreen: native [
//
//      {Asks to place window in the fullscreen state. Note that you shouldn’t assume the window is definitely full screen afterward,
// because other entities (e.g. the user or window manager) could unfullscreen it again, and not all window managers honor requests 
// to fullscreen windows. But normally the window will end up fullscreen. Just don’t write code that crashes if not.
// You can track the fullscreen state via the “window-state-event” signal on GtkWidget.}
//
//      return: [void!]
//      window [handle!]
//  ]
//
REBNATIVE(gtk_window_fullscreen)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_WINDOW_FULLSCREEN;

    GtkWindow *window = VAL_HANDLE_POINTER(GtkWindow, ARG(window));

    gtk_window_fullscreen(window);

    return rebVoid();
}

//
//  export gtk-window-unfullscreen: native [
//
//      {Asks to toggle off the fullscreen state for window.
// Note that you shouldn’t assume the window is definitely not full screen afterward, because other entities 
// (e.g. the user or window manager) could fullscreen it again, and not all window managers honor requests to unfullscreen windows.
// But normally the window will end up restored to its normal state. Just don’t write code that crashes if not.}
//
//      return: [void!]
//      window [handle!]
//  ]
//
REBNATIVE(gtk_window_unfullscreen)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_WINDOW_UNFULLSCREEN;

    GtkWindow *window = VAL_HANDLE_POINTER(GtkWindow, ARG(window));

    gtk_window_unfullscreen(window);

    return rebVoid();
}

//
//  export gtk-window-get-screen: native [
//
//      {Returns the GdkScreen associated with window.}
//
//      return: [handle! void!]
//      window [handle!]
//  ]
//
REBNATIVE(gtk_window_get_screen)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_WINDOW_GET_SCREEN;

    GtkWindow *window = VAL_HANDLE_POINTER(GtkWindow, ARG(window));

    GdkScreen *screen = gtk_window_get_screen(window);

    return rebHandle(screen, 0, nullptr);
}

//
//  export gtk-window-set-screen: native [
//
//      {Sets the GdkScreen where the window is displayed; if the window is already mapped, it will be unmapped, and then remapped on the new screen.}
//
//      return: [void!]
//      window [handle!]
//      screen [handle!]
//  ]
//
REBNATIVE(gtk_window_set_screen)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_WINDOW_SET_SCREEN;

    GtkWindow *window = VAL_HANDLE_POINTER(GtkWindow, ARG(window));

    GdkScreen *screen = VAL_HANDLE_POINTER(GdkScreen, ARG(screen));

    gtk_window_set_screen(window, screen);

    return rebVoid();
}

// Other Widget functions

// Container functions

//
//  export gtk-container-add: native [
//
//      {Adds widget to container. Typically used for simple containers such as GtkWindow, GtkFrame, or GtkButton; 
// for more complicated layout containers such as GtkBox or GtkGrid, this function will pick 
// default packing parameters that may not be correct. So consider functions such as gtk_box_pack_start() 
// and gtk_grid_attach() as an alternative to gtk_container_add() in those cases. A widget may be added to 
// only one container at a time; you can’t place the same widget inside two different containers.}
//
//      return: [void!]
//      container [handle!]
//      widget [handle!]
//  ]
//
REBNATIVE(gtk_container_add)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_CONTAINER_ADD;

    GtkContainer *container = VAL_HANDLE_POINTER(GtkContainer, ARG(container));

    GtkWidget *widget = VAL_HANDLE_POINTER(GtkWidget, ARG(widget));
     
    gtk_container_add(container, widget);

    return rebVoid();
}

//
//  export gtk-container-remove: native [
//
//      {Removes widget from container. The widget must be inside container. 
// Note that container will own a reference to widget, and that this may be the last reference held; 
// so removing a widget from its container can destroy that widget. If you want to use widget again, 
// you need to add a reference to it before removing it from a container, using g_object_ref(). 
// If you don’t want to use widget again it’s usually more efficient to simply destroy it directly 
// using gtk_widget_destroy() since this will remove it from the container and help break any circular 
// reference count cycles.}
//
//      return: [void!]
//      container [handle!]
//      widget [handle!]
//  ]
//
REBNATIVE(gtk_container_remove)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_CONTAINER_REMOVE;

    GtkContainer *container = VAL_HANDLE_POINTER(GtkContainer, ARG(container));

    GtkWidget *widget = VAL_HANDLE_POINTER(GtkWidget, ARG(widget));
     
    gtk_container_remove(container, widget);

    return rebVoid();
}

//
//  export gtk-container-get-children: native [
//
//      {Returns the container’s non-internal children. See gtk_container_forall() for details on what constitutes an "internal" child.}
//
//      return: [block!]
//      container [handle!]
//  ]
//
REBNATIVE(gtk_container_get_children)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_CONTAINER_GET_CHILDREN;

    GtkContainer *container = VAL_HANDLE_POINTER(GtkContainer, ARG(container));

    GList *list = gtk_container_get_children(container);

    REBVAL *block = rebValue("[]");

    GList *l;

    for (l = list; l != NULL; l = l->next)
    {
        rebElide("print {list processing}");
        gpointer element_data = l->data;
        rebElide("append", block, rebValue(element_data));
    }

    return block;
}

//
//  export gtk-container-set-border-width: native [
//
//      {Sets the border width of the container.
// 
// The border width of a container is the amount of space to leave around the outside of the container. 
// The only exception to this is GtkWindow; because toplevel windows can’t leave space outside,
// they leave the space inside. The border is added on all sides of the container.
// To add space to only one side, use a specific “margin” property on the child widget, for example “margin-top”.}
//
//      return: [void!]
//      container [handle!]
//      border_width [integer!]
//  ]
//
REBNATIVE(gtk_container_set_border_width)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_CONTAINER_SET_BORDER_WIDTH;

    GtkContainer *container = VAL_HANDLE_POINTER(GtkContainer, ARG(container));

    unsigned int border_width = rebUnboxInteger(ARG(border_width));
     
    gtk_container_set_border_width(container, border_width);

    return rebVoid();
}

// Editable functions

//
//  export gtk-editable-set-editable: native [
//
//      {Determines if the user can edit the text in the editable widget or not.}
//
//      return: [void!]
//      editable [handle!]
//      is_editable [logic!]
//  ]
//
REBNATIVE(gtk_editable_set_editable)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_EDITABLE_SET_EDITABLE;

    GtkEditable *editable = VAL_HANDLE_POINTER(GtkEditable, ARG(editable));
 
    bool is_editable = rebDid(ARG(is_editable));

    gtk_editable_set_editable(editable, is_editable);

    return rebVoid();
}

//
//  export gtk-editable-get-editable: native [
//
//      {Retrieves whether editable is editable. See gtk_editable_set_editable().}
//
//      return: [logic!]
//      editable [handle!]
//  ]
//
REBNATIVE(gtk_editable_get_editable)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_EDITABLE_GET_EDITABLE;

    GtkEditable *editable = VAL_HANDLE_POINTER(GtkEditable, ARG(editable));
     
    bool result = gtk_editable_get_editable(editable);

    return rebLogic(result);
}


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
    
    GtkWidget *label = gtk_label_new(str);

    return rebHandle(label, 0, nullptr);
}

//
//  export gtk-label-get-text: native [
//
//      {Fetches the text from a label widget, as displayed on the screen. 
// This does not include any embedded underlines indicating mnemonics or Pango markup. 
// (See gtk_label_get_label())}
//
//      return: [text!]
//      label [handle!]
//  ]
//
REBNATIVE(gtk_label_get_text)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_LABEL_GET_TEXT;

    GtkLabel *label = VAL_HANDLE_POINTER(GtkLabel, ARG(label));
     
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

    GtkLabel *label = VAL_HANDLE_POINTER(GtkLabel, ARG(label));

    const char * str = cast(char*, VAL_STRING_AT(ARG(str)));
     
    gtk_label_set_text(label, str);

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

    GtkWidget *button = gtk_button_new();

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
     
    GtkWidget *button = gtk_button_new_with_label(str);

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

    GtkButton *button = VAL_HANDLE_POINTER(GtkButton, ARG(button));
     
    const char *result = gtk_button_get_label(button);

    return rebText(result);
}

//
//  export gtk-button-set-label: native [
//
//      {Sets the text of the label of the button to str.}
//
//      return: [void!]
//      button [handle!]
//      str [text!]
//  ]
//
REBNATIVE(gtk_button_set_label)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_BUTTON_SET_LABEL;

    GtkButton *button = VAL_HANDLE_POINTER(GtkButton, ARG(button));

    const char * str = cast(char*, VAL_STRING_AT(ARG(str)));
     
    gtk_button_set_label(button, str);

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

    GtkWidget *image = gtk_image_new();

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

    GtkWidget *image = gtk_image_new_from_file(str);

    return rebHandle(image, 0, nullptr);
}

//
//  export gtk-image-set-from-file: native [
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
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_IMAGE_SET_FROM_FILE;

    GtkImage *image = VAL_HANDLE_POINTER(GtkImage, ARG(image));

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
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_IMAGE_CLEAR;

    GtkImage *image = VAL_HANDLE_POINTER(GtkImage, ARG(image));
     
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

    GtkWidget *field = gtk_entry_new();

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

    GtkEntryBuffer *buffer = VAL_HANDLE_POINTER(GtkEntryBuffer, ARG(buffer));

    GtkWidget *field = gtk_entry_new_with_buffer(buffer);

    return rebHandle(field, 0, nullptr);
}

//
//  export gtk-entry-get-buffer: native [
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

    GtkEntry *field = VAL_HANDLE_POINTER(GtkEntry, ARG(field));

    GtkEntryBuffer *buffer = gtk_entry_get_buffer(field);

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

    GtkEntry *field = VAL_HANDLE_POINTER(GtkEntry, ARG(field));

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

    GtkEntry *field = VAL_HANDLE_POINTER(GtkEntry, ARG(field));
     
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

    GtkEntry *field = VAL_HANDLE_POINTER(GtkEntry, ARG(field));
     
    unsigned int result = gtk_entry_get_text_length(field);

    return rebInteger(result);
}

// todo
// Don't know yet how to read or return a character (issue! type)
// void gtk_entry_set_invisible_char (GtkEntry *entry,
//                                    gunichar ch); This appears to be an unsigned int
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
//      return: [integer!]
//      field [handle!]
//  ]
//
REBNATIVE(gtk_entry_get_invisible_char)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_ENTRY_GET_INVISIBLE_CHAR;

    GtkEntry *field = VAL_HANDLE_POINTER(GtkEntry, ARG(field));

    unsigned int result = gtk_entry_get_invisible_char(field);

    return rebInteger(result);
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
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_ENTRY_SET_MAX_LENGTH;

    GtkEntry *field = VAL_HANDLE_POINTER(GtkEntry, ARG(field));

    unsigned int maxlen = rebUnboxInteger(ARG(maxlen));

    gtk_entry_set_max_length(field, maxlen);

    return rebVoid();
}

//
//  export gtk-entry-get-max-length: native [
//      return: [integer!]
//      field [handle!]
//  ]
//
REBNATIVE(gtk_entry_get_max_length)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_ENTRY_GET_MAX_LENGTH;

    GtkEntry *field = VAL_HANDLE_POINTER(GtkEntry, ARG(field));
     
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

    GtkEntry *field = VAL_HANDLE_POINTER(GtkEntry, ARG(field));

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

    GtkEntry *field = VAL_HANDLE_POINTER(GtkEntry, ARG(field));
     
    bool result = gtk_entry_get_visibility(field);

    return rebLogic(result);
}

// Widget Text View (Multi line text field) functions

//
//  export gtk-text-view-new: native [
//
//      {Creates a new GtkTextView. If you don’t call gtk_text_view_set_buffer() before using the text view, 
// an empty default buffer will be created for you. Get the buffer with gtk_text_view_get_buffer(). 
// If you want to specify your own buffer, consider gtk_text_view_new_with_buffer().}
//
//      return: [handle! void!]
//  ]
//
REBNATIVE(gtk_text_view_new)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_TEXT_VIEW_NEW;

    GtkWidget *textview = gtk_text_view_new();

    return rebHandle(textview, 0, nullptr);
}

//
//  export gtk-text-view-set-buffer: native [
//
//      {Sets buffer as the buffer being displayed by text_view. The previous buffer displayed by the text view 
// is unreferenced, and a reference is added to buffer. If you owned a reference to buffer before passing it to 
// this function, you must remove that reference yourself; GtkTextView will not “adopt” it.}
//
//      return: [void!]
//      textview [handle!]
//      buffer [handle!]
//  ]
//
REBNATIVE(gtk_text_view_set_buffer)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_TEXT_VIEW_SET_BUFFER;

    GtkTextView *textview = VAL_HANDLE_POINTER(GtkTextView, ARG(textview));

    GtkTextBuffer *buffer = VAL_HANDLE_POINTER(GtkTextBuffer, ARG(buffer));
 
    gtk_text_view_set_buffer(textview, buffer);

    return rebVoid();
}

//
//  export gtk-text-view-get-buffer: native [
//
//      {Returns the GtkTextBuffer being displayed by this text view. 
// The reference count on the buffer is not incremented; the caller of this function won’t own a new reference.}
//
//      return: [handle! void!]
//      textview [handle!]
//  ]
//
REBNATIVE(gtk_text_view_get_buffer)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_TEXT_VIEW_GET_BUFFER;

    GtkTextView *textview = VAL_HANDLE_POINTER(GtkTextView, ARG(textview));

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(textview);

    return rebHandle(buffer, 0, nullptr);
}

//
//  export gtk-text-view-set-editable: native [
//
//      {Sets the default editability of the GtkTextView. 
// You can override this default setting with tags in the buffer, using the “editable” attribute of tags..}
//
//      return: [void!]
//      textview [handle!]
//      setting [logic!]
//  ]
//
REBNATIVE(gtk_text_view_set_editable)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_TEXT_VIEW_SET_EDITABLE;

    GtkTextView *textview = VAL_HANDLE_POINTER(GtkTextView, ARG(textview));

    bool setting = rebDid(ARG(setting));
 
    gtk_text_view_set_editable(textview, setting);

    return rebVoid();
}

//
//  export gtk-text-view-get-editable: native [
//
//      {Returns the default editability of the GtkTextView. Tags in the buffer may override this setting for some ranges of text.}
//
//      return: [logic!]
//      textview [handle!]
//  ]
//
REBNATIVE(gtk_text_view_get_editable)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_TEXT_VIEW_GET_EDITABLE;

    GtkTextView *textview = VAL_HANDLE_POINTER(GtkTextView, ARG(textview));
     
    bool result = gtk_text_view_get_editable(textview);

    return rebLogic(result);
}

//
//  export gtk-text-view-set-cursor-visible: native [
//
//      {Toggles whether the insertion point should be displayed. 
// A buffer with no editable text probably shouldn’t have a visible cursor, so you may want to turn the cursor off.}
//
//      return: [void!]
//      textview [handle!]
//      setting [logic!]
//  ]
//
REBNATIVE(gtk_text_view_set_cursor_visible)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_TEXT_VIEW_SET_CURSOR_VISIBLE;

    GtkTextView *textview = VAL_HANDLE_POINTER(GtkTextView, ARG(textview));

    bool setting = rebDid(ARG(setting));

    gtk_text_view_set_cursor_visible(textview, setting);

    return rebVoid();
}

//
//  export gtk-text-view-get-cursor-visible: native [
//
//      {Find out whether the cursor should be displayed.}
//
//      return: [logic!]
//      textview [handle!]
//  ]
//
REBNATIVE(gtk_text_view_get_cursor_visible)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_TEXT_VIEW_GET_CURSOR_VISIBLE;

    GtkTextView *textview = VAL_HANDLE_POINTER(GtkTextView, ARG(textview));
     
    bool result = gtk_text_view_get_cursor_visible(textview);

    return rebLogic(result);
}

//
//  export gtk-text-buffer-set-text: native [
//
//      {Deletes current contents of buffer, and inserts text instead. 
// If len is -1, text must be nul-terminated. text must be valid UTF-8.}
//
//      return: [void!]
//      buffer [handle!]
//      str [text!]
//      length [integer!]
//  ]
//
REBNATIVE(gtk_text_buffer_set_text)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_TEXT_BUFFER_SET_TEXT;

    GtkTextBuffer *buffer = VAL_HANDLE_POINTER(GtkTextBuffer, ARG(buffer));

    const char * str = cast(char*, VAL_STRING_AT(ARG(str)));

    unsigned int length = rebUnboxInteger(ARG(length));

    gtk_text_buffer_set_text(buffer, str, length);

    return rebVoid();
}

//
//  export gtk-text-buffer-get-text: native [
//
//      {Returns the text in the range [start ,end ). Excludes undisplayed text (text marked with tags 
// that set the invisibility attribute) if include_hidden_chars is FALSE. 
// Does not include characters representing embedded images, so byte and character indexes into 
// the returned string do not correspond to byte and character indexes into the buffer. 
// Contrast with gtk_text_buffer_get_slice(). Not implemented (yet).}
//
//      return: [text!]
//      buffer [handle!]
//      start [handle!]
//      end [handle!]
//      hidden [logic!] "include hidden characters"
//  ]
//
REBNATIVE(gtk_text_buffer_get_text)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_TEXT_BUFFER_GET_TEXT;

    GtkTextBuffer *buffer = VAL_HANDLE_POINTER(GtkTextBuffer, ARG(buffer));

    GtkTextIter *start = VAL_HANDLE_POINTER(GtkTextIter, ARG(start));

    GtkTextIter *end = VAL_HANDLE_POINTER(GtkTextIter, ARG(end));

    bool hidden = rebDid(ARG(hidden));

    const char *result = gtk_text_buffer_get_text(buffer, start, end, hidden);

    return rebText(result);
}

//
//  export gtk-text-buffer-get-bounds: native [
//
//      {Retrieves the first and last iterators in the buffer, i.e. the entire buffer lies within the range [start ,end ).}
//
//      return: [block!]
//      buffer [handle!]
//      start [handle!]
//      end [handle!]
//  ]
//
REBNATIVE(gtk_text_buffer_get_bounds)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_TEXT_BUFFER_GET_BOUNDS;

    GtkTextBuffer *buffer = VAL_HANDLE_POINTER(GtkTextBuffer, ARG(buffer));

    GtkTextIter *start = VAL_HANDLE_POINTER(GtkTextIter, ARG(start));

    GtkTextIter *end = VAL_HANDLE_POINTER(GtkTextIter, ARG(end));

    gtk_text_buffer_get_bounds(buffer, start, end);

    REBVAL *block = rebValue("[]");

    rebElide("append", block, rebI((long)start));
    rebElide("append", block, rebI((long)end));

    return block;
}

// Widget Layout functions

// Box Layout

//
//  export gtk-box-new: native [
//
//      {Creates a new GtkBox with orientation and spacing.}
//
//      return: [handle! void!]
//      orientation [integer!]
//      spacing [integer!]
//  ]
//
REBNATIVE(gtk_box_new)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_BOX_NEW;

    GtkOrientation orientation = (GtkOrientation) rebUnboxInteger(ARG(orientation));

    unsigned int spacing = rebUnboxInteger(ARG(spacing));

    GtkWidget *box = gtk_box_new(orientation, spacing);
    
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

    GtkBox *box = VAL_HANDLE_POINTER(GtkBox, ARG(box));
    GtkWidget *child = VAL_HANDLE_POINTER(GtkWidget, ARG(child));

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

    GtkBox *box = VAL_HANDLE_POINTER(GtkBox, ARG(box));
    GtkWidget *child = VAL_HANDLE_POINTER(GtkWidget, ARG(child));

    bool expand = rebDid(ARG(expand));
    bool fill = rebDid(ARG(fill));
 
    unsigned int padding = rebUnboxInteger(ARG(padding));

    gtk_box_pack_end(box, child, expand, fill, padding);

    return rebVoid();
}

//
//  export gtk-box-get-spacing: native [
//
//      {Sets the spacing property of box, which is the number of pixels to place between children of box.}
//
//      return: [integer!]
//      box [handle!]
//  ]
//
REBNATIVE(gtk_box_get_spacing)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_BOX_GET_SPACING;

    GtkBox *box = VAL_HANDLE_POINTER(GtkBox, ARG(box));
     
    unsigned int result = gtk_box_get_spacing(box);

    return rebInteger(result);
}

//
//  export gtk-box-set-spacing: native [
//
//      {Sets the spacing property of box, which is the number of pixels to place between children of box.}
//
//      return: [void!]
//      box [handle!]
//      spacing [integer!]
//  ]
//
REBNATIVE(gtk_box_set_spacing)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_BOX_SET_SPACING;

    GtkBox *box = VAL_HANDLE_POINTER(GtkBox, ARG(box));
 
    unsigned int spacing = rebUnboxInteger(ARG(spacing));

    gtk_box_set_spacing(box, spacing);

    return rebVoid();
}

//
//  export gtk-box-get-homogeneous: native [
//
//      {Returns whether the box is homogeneous (all children are the same size). See gtk_box_set_homogeneous().}
//
//      return: [logic!]
//      box [handle!]
//  ]
//
REBNATIVE(gtk_box_get_homogeneous)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_BOX_GET_HOMOGENEOUS;

    GtkBox *box = VAL_HANDLE_POINTER(GtkBox, ARG(box));

    bool result = gtk_box_get_homogeneous(box);

    return rebLogic(result);
}

//
//  export gtk-box-set-homogeneous: native [
//
//      {Sets the “homogeneous” property of box, controlling whether or not all children of box are given equal space in the box.}
//
//      return: [void!]
//      box [handle!]
//      homogeneous [logic!]
//  ]
//
REBNATIVE(gtk_box_set_homogeneous)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_BOX_SET_HOMOGENEOUS;

    GtkBox *box = VAL_HANDLE_POINTER(GtkBox, ARG(box));
 
    bool homogeneous = rebDid(ARG(homogeneous));

    gtk_box_set_homogeneous(box, homogeneous);

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

    GtkWidget *grid = gtk_grid_new();
    
    return rebHandle(grid, 0, nullptr);
}

//
//  export gtk-grid-attach: native [
//
//      {Adds a widget to the grid.
// The position of child is determined by left and top. 
// The number of "cells" that child will occupy is determined by width and height.}
//
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

    GtkGrid *grid = VAL_HANDLE_POINTER(GtkGrid, ARG(grid));
    GtkWidget *child = VAL_HANDLE_POINTER(GtkWidget, ARG(child));
 
    unsigned int left = rebUnboxInteger(ARG(left));
    unsigned int top = rebUnboxInteger(ARG(top));
    unsigned int width = rebUnboxInteger(ARG(width));
    unsigned int height = rebUnboxInteger(ARG(height));

    gtk_grid_attach(grid, child, left, top, width, height);

    return rebVoid();
}

//
//  export gtk-grid-insert-row: native [
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

    GtkGrid *grid = VAL_HANDLE_POINTER(GtkGrid, ARG(grid));
 
    unsigned int position = rebUnboxInteger(ARG(position));

    gtk_grid_insert_row(grid, position);

    return rebVoid();
}

//
//  export gtk-grid-insert-column: native [
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

    GtkGrid *grid = VAL_HANDLE_POINTER(GtkGrid, ARG(grid));
 
    unsigned int position = rebUnboxInteger(ARG(position));

    gtk_grid_insert_column(grid, position);

    return rebVoid();
}

// Widget Show (and Hide) functions

//
//  export gtk-widget-show: native [
//
//      {Flags a widget to be displayed. 
// Any widget that is not shown will not appear on the screen. 
// If you want to show all the widgets in a container, 
// it is easier to call gtk_widget_show_all() on the container, 
// instead of individually showing the widgets.
// Remember that you have to show the containers containing a widget, 
// in addition to the widget itself, before it will appear onscreen.
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

    GtkWidget *widget = VAL_HANDLE_POINTER(GtkWidget, ARG(widget));
     
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

    GtkWidget *widget = VAL_HANDLE_POINTER(GtkWidget, ARG(widget));
     
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

    GtkWidget *widget = VAL_HANDLE_POINTER(GtkWidget, ARG(widget));
     
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
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_WIDGET_SHOW_ALL;

    GtkWidget *widget = VAL_HANDLE_POINTER(GtkWidget, ARG(widget));
     
    gtk_widget_show_all(widget);

    return rebVoid();
}

// Window and Widget destroy function

//
//  export gtk-widget-destroy: native [
//
//      {Destroys a widget.
// When a widget is destroyed all references it holds on other objects will be released:
//    if the widget is inside a container, it will be removed from its parent
//    if the widget is a container, all its children will be destroyed, recursively
//    if the widget is a top level, it will be removed from the list of top level widgets that GTK+ maintains internally}
//
//      return: [void!]
//      widget [handle!]
//  ]
//
REBNATIVE(gtk_widget_destroy)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_WIDGET_DESTROY;

    GtkWidget *widget = VAL_HANDLE_POINTER(GtkWidget, ARG(widget));

    gtk_widget_destroy(widget);

    return rebVoid();
}
