/* A file-manager stand-in for gui_check.py: a small window ("drag me")
 * that offers the files named on its command line as a GdkFileList when
 * dragged, as Dolphin or Files would.
 *
 *   cc drag_source.c $(pkg-config --cflags --libs gtk4) -o drag_source
 *   drag_source FILE... */
#include <gtk/gtk.h>

static GdkContentProvider* prepare(GtkDragSource* source, double x, double y, gpointer files) {
    (void)source;
    (void)x;
    (void)y;
    return gdk_content_provider_new_typed(GDK_TYPE_FILE_LIST, files);
}

static void activate(GtkApplication* app, gpointer files) {
    GtkWidget* window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "tinta-drag-source");
    gtk_window_set_default_size(GTK_WINDOW(window), 300, 200);
    GtkWidget* label = gtk_label_new("drag me");
    GtkDragSource* source = gtk_drag_source_new();
    gtk_drag_source_set_actions(source, GDK_ACTION_COPY);
    g_signal_connect(source, "prepare", G_CALLBACK(prepare), files);
    gtk_widget_add_controller(label, GTK_EVENT_CONTROLLER(source));
    gtk_window_set_child(GTK_WINDOW(window), label);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char** argv) {
    GSList* list = NULL;
    for (int i = argc - 1; i >= 1; i--) list = g_slist_prepend(list, g_file_new_for_path(argv[i]));
    GdkFileList* files = gdk_file_list_new_from_list(list);
    GtkApplication* app = gtk_application_new(NULL, G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), files);
    char* args[] = {argv[0], NULL};
    return g_application_run(G_APPLICATION(app), 1, args);
}
