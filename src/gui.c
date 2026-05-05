#include <gtk/gtk.h>
#include "gui.h"
#include "pomodoro.h"

static void on_button_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *label = (GtkWidget *)data;
    start_pomodoro_timer(label);
}

static void activate(GtkApplication* app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *button;
    GtkWidget *box;

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Time Manager");
    gtk_window_set_default_size(GTK_WINDOW(window), 300, 200);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), box);

    GtkWidget *label = gtk_label_new("Quản lý thời gian của bạn");
    gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);

    button = gtk_button_new_with_label("Bắt đầu Pomodoro");
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_clicked), label);
    gtk_box_pack_start(GTK_BOX(box), button, TRUE, TRUE, 0);

    gtk_widget_show_all(window);
}

void start_gui(int argc, char **argv) {
    GtkApplication *app;
    int status;
    app = gtk_application_new("com.sato.timemanager", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
}