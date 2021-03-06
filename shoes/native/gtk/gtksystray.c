#include "shoes/app.h"
#include "shoes/ruby.h"
#include "shoes/config.h"
#include "shoes/world.h"
#include "shoes/native/native.h"
#include "shoes/types/native.h"
#include "shoes/internal.h"
#include "shoes/native/gtk/gtksystray.h"

/* 
 * Many problems here - Gtk3 deprecates what works on Windows (and linux)
 * In order to use the more forward /useful method for linux we need to 
 * use g_application which requires a unique dbus address for the Process
 * like "shoes-#{pid}" perhaps. Gapplication/GtkApplication requires
 * much more work in Shoes. 
 * 
 * Meantime we need to turn off the whining about deprecations.
 * Then see if we can pick at runtime which one we want
 * 
 * ref:
 * https://stackoverflow.com/questions/3378560/how-to-disable-gcc-warnings-for-a-few-lines-of-code
*/

//extern GApplication *shoes_GApp;
#if !defined(SHOES_GTK_WIN32) // i.e. not Windows
// TODO: Doesn't work on Linux either but it did once.
static void shoes_native_systray_gapp(char *title, char *message, char *path) {
  GApplication *gapp = g_application_get_default();
  GNotification *note;
  note = g_notification_new (title);
  g_notification_set_body (note, message);
  GFile *iconf = g_file_new_for_path (path);
  GIcon *icon = g_file_icon_new (iconf);
  g_notification_set_icon(note, icon);
  g_application_send_notification (gapp, "Shoes", note);
}
#endif
// Always compile the old version (gtk_status_icon)
// use gtk_status_icon for Windows, deprecated but GNotification doesn't work
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
static GtkStatusIcon *stsicon = NULL;
static char *stspath = NULL;
static void shoes_native_systray_old(char *title, char *message, char *path) {
    if (stsicon == NULL) {
        stsicon = gtk_status_icon_new_from_file(path);
        stspath = path;
    }
    // detect change of icon
    if (strcmp(path, stspath)) {
        stspath = path;
        gtk_status_icon_set_from_file (stsicon, stspath);
    }
    gtk_status_icon_set_title(stsicon, title);
    gtk_status_icon_set_tooltip_text(stsicon, message);
}
#pragma GCC diagnostic pop

void shoes_native_systray(char *title, char *message, char *path) {
#ifdef SHOES_GTK_WIN32
  // always call the older stuff for Windows
  shoes_native_systray_old(title, message, path);
#else
  // TODO: Linux: make a runtime determination of which to call
  //shoes_native_systray_gapp(title, message, path);
  shoes_native_systray_old(title, message, path);
#endif
}
