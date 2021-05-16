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
//  export gtk-init-plain: native [
//
//      {Call this function before using any other GTK+ functions in your GUI applications. 
// It will initialize everything needed to operate the toolkit and parses some standard command line options.}
//
//      return: [void!]
//  ]
//
REBNATIVE(gtk_init_plain)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_INIT_PLAIN;

    int argc = 0;
    gtk_init(&argc, nullptr);
    
    return rebVoid();
}

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
    VIEWGTK3_INCLUDE_PARAMS_OF_GTK_INIT;

    gtk_main();

    return rebVoid();
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
//#define g_signal_connect_swapped(instance, detailed_signal, c_handler, data) \
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
//      handler [handle!]
//      data [handle! integer!]
//      cleardata [integer!]
//      flags [integer!]
//  ]
//
REBNATIVE(g_signal_connect_data)
{
    VIEWGTK3_INCLUDE_PARAMS_OF_G_SIGNAL_CONNECT_DATA;

    // instance is a gpointer type, handle, so integer
    GtkWidget *instance = VAL_HANDLE_POINTER(GtkWidget, ARG(instance));

    // detailedsignal is a text to hold the description of the action. For example "quit", "clicked"
    // signal names are to make distinction between the action to perform.
    const char * detailedsignal = cast(char*, VAL_STRING_AT(ARG(detailedsignal)));

    // handler is an integer type for a g-callback
    GCallback handler = (GCallback) rebUnboxInteger(ARG(handler));

    // data is a gpointer for a handle, so an integer
    gpointer *data = VAL_HANDLE_POINTER(gpointer, ARG(data));

    // cleardata is an integer for g-closure-notify, often value null is used
    GClosureNotify cleardata = (GClosureNotify) rebUnboxInteger(ARG(cleardata));

    // flags is an integer value, 0 = normal signal connect, 1 = after, 2 = swapped
    GConnectFlags flags = (GConnectFlags) rebUnboxInteger(ARG(flags));

    // result is > 0 for success adding a signal
    unsigned int result = g_signal_connect_data(instance, detailedsignal, handler, data, cleardata, flags);

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
    // GtkWindowType type = cast(GtkWindowType, VAL_STRING_AT(ARG(type)));
    
    if (type == GTK_WINDOW_TOPLEVEL) {
        type = GTK_WINDOW_TOPLEVEL;
    }
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
    REBVAL *logic = rebValue("TO LOGIC!", result);

    return rebValue(logic);
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
    REBVAL *logic = rebValue("TO LOGIC!", result);

    return rebValue(logic);
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
    REBVAL *logic = rebValue("TO LOGIC!", result);

    return rebValue(logic);
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
//      return: [void!]
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

    return rebVoid();
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

    GtkWidget *box = gtk_box_new(*orientation, spacing);
    
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
//  export gtk-box-set_spacing: native [
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
