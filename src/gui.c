#include <gtk/gtk.h>
#include "../include/gui.h"

static void on_add_clicked(GtkWidget *widget, gpointer data) {
    g_print("Add Task clicked!\n");
}

static void on_start_clicked(GtkWidget *widget, gpointer data) {
    g_print("Pomodoro Started!\n");
}

void start_gui() {
    GtkWidget *window;
    GtkWidget *box;
    GtkWidget *title;
    GtkWidget *add_btn;
    GtkWidget *start_btn;

    gtk_init(NULL, NULL);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Time Manager");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), box);

    title = gtk_label_new("TIME MANAGER");
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 10);

    add_btn = gtk_button_new_with_label("Add Task");
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(box), add_btn, FALSE, FALSE, 5);

    start_btn = gtk_button_new_with_label("Start Pomodoro");
    g_signal_connect(start_btn, "clicked", G_CALLBACK(on_start_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(box), start_btn, FALSE, FALSE, 5);

    gtk_widget_show_all(window);
    gtk_main();
}