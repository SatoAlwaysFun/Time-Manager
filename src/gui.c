#include "gui.h"
#include "pomodoro.h"
#include "task_manager.h"
#include "file_manager.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

static GtkWidget *task_list_box  = NULL;
static GtkWidget *entry_title    = NULL;
static GtkWidget *entry_desc     = NULL;
static GtkWidget *combo_priority = NULL;

/* Forward declaration */
static void refresh_task_list(void);

/* ---- Callbacks for task rows ---- */
static void on_toggle_done(GtkToggleButton *btn, gpointer data) {
    (void)data;
    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "task-id"));
    tm_toggle_done(id);
    fm_save();
    refresh_task_list();
}

static void on_delete_task(GtkWidget *btn, gpointer data) {
    (void)data;
    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "task-id"));
    tm_delete(id);
    fm_save();
    refresh_task_list();
}

/* ------------------------------------------------------------------ */
static void refresh_task_list(void) {
    if (!task_list_box) return;

    GList *children = gtk_container_get_children(GTK_CONTAINER(task_list_box));
    for (GList *l = children; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);

    for (int i = 0; i < task_count; i++) {
        Task *t = &tasks[i];

        GtkWidget *row  = gtk_list_box_row_new();
        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_container_add(GTK_CONTAINER(row), hbox);

        /* Done checkbox */
        GtkWidget *chk = gtk_check_button_new();
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk), t->done);
        g_object_set_data(G_OBJECT(chk), "task-id", GINT_TO_POINTER(t->id));
        g_signal_connect(chk, "toggled", G_CALLBACK(on_toggle_done), NULL);
        gtk_box_pack_start(GTK_BOX(hbox), chk, FALSE, FALSE, 4);

        /* Title + priority label */
        char label_text[300];
        snprintf(label_text, sizeof(label_text), "[%s]  %s",
                 priority_to_string(t->priority), t->title);
        GtkWidget *lbl = gtk_label_new(label_text);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
        gtk_widget_set_tooltip_text(lbl, t->description);

        if (t->done) {
            PangoAttrList *attrs = pango_attr_list_new();
            pango_attr_list_insert(attrs, pango_attr_strikethrough_new(TRUE));
            gtk_label_set_attributes(GTK_LABEL(lbl), attrs);
            pango_attr_list_unref(attrs);
        }
        gtk_box_pack_start(GTK_BOX(hbox), lbl, TRUE, TRUE, 0);

        /* Delete button */
        GtkWidget *del_btn = gtk_button_new_with_label("X");
        g_object_set_data(G_OBJECT(del_btn), "task-id", GINT_TO_POINTER(t->id));
        g_signal_connect(del_btn, "clicked", G_CALLBACK(on_delete_task), NULL);
        gtk_box_pack_end(GTK_BOX(hbox), del_btn, FALSE, FALSE, 4);

        gtk_container_add(GTK_CONTAINER(task_list_box), row);
    }
    gtk_widget_show_all(task_list_box);
}

/* ---- Add task ---- */
static void on_add_task(GtkWidget *btn, gpointer data) {
    (void)btn; (void)data;
    const char *title = gtk_entry_get_text(GTK_ENTRY(entry_title));
    const char *desc  = gtk_entry_get_text(GTK_ENTRY(entry_desc));
    int prio_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(combo_priority));
    if (prio_idx < 0) prio_idx = 0;

    if (title && strlen(title) > 0) {
        tm_add(title, desc, (Priority)prio_idx);
        fm_save();
        gtk_entry_set_text(GTK_ENTRY(entry_title), "");
        gtk_entry_set_text(GTK_ENTRY(entry_desc),  "");
        refresh_task_list();
    }
}

/* ================================================================
   Build Task tab
   ================================================================ */
static GtkWidget *build_task_tab(void) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);

    GtkWidget *input_frame = gtk_frame_new("Them cong viec moi");
    GtkWidget *input_grid  = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(input_grid), 8);
    gtk_grid_set_row_spacing(GTK_GRID(input_grid), 6);
    gtk_container_set_border_width(GTK_CONTAINER(input_grid), 8);
    gtk_container_add(GTK_CONTAINER(input_frame), input_grid);

    GtkWidget *lbl_title = gtk_label_new("Tieu de:");
    gtk_widget_set_halign(lbl_title, GTK_ALIGN_END);
    entry_title = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_title), "Nhap tieu de cong viec...");
    gtk_widget_set_hexpand(entry_title, TRUE);
    gtk_grid_attach(GTK_GRID(input_grid), lbl_title,  0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(input_grid), entry_title, 1, 0, 2, 1);

    GtkWidget *lbl_desc = gtk_label_new("Mo ta:");
    gtk_widget_set_halign(lbl_desc, GTK_ALIGN_END);
    entry_desc = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_desc), "Mo ta (tuy chon)...");
    gtk_widget_set_hexpand(entry_desc, TRUE);
    gtk_grid_attach(GTK_GRID(input_grid), lbl_desc,  0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(input_grid), entry_desc, 1, 1, 2, 1);

    GtkWidget *lbl_prio = gtk_label_new("Uu tien:");
    gtk_widget_set_halign(lbl_prio, GTK_ALIGN_END);
    combo_priority = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_priority), "Thap");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_priority), "Trung binh");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_priority), "Cao");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_priority), 1);
    gtk_grid_attach(GTK_GRID(input_grid), lbl_prio,       0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(input_grid), combo_priority, 1, 2, 1, 1);

    GtkWidget *add_btn = gtk_button_new_with_label("+ Them");
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_task), NULL);
    gtk_grid_attach(GTK_GRID(input_grid), add_btn, 2, 2, 1, 1);

    gtk_box_pack_start(GTK_BOX(vbox), input_frame, FALSE, FALSE, 0);

    GtkWidget *list_frame = gtk_frame_new("Danh sach cong viec");
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroll, -1, 300);

    task_list_box = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(task_list_box), GTK_SELECTION_NONE);
    gtk_container_add(GTK_CONTAINER(scroll), task_list_box);
    gtk_container_add(GTK_CONTAINER(list_frame), scroll);
    gtk_box_pack_start(GTK_BOX(vbox), list_frame, TRUE, TRUE, 0);

    refresh_task_list();
    return vbox;
}

/* ================================================================
   Build Pomodoro tab
   ================================================================ */
static GtkWidget *build_pomodoro_tab(void) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 24);
    gtk_widget_set_halign(vbox, GTK_ALIGN_CENTER);

    GtkWidget *mode_label = gtk_label_new("Lam viec");
    PangoAttrList *al = pango_attr_list_new();
    pango_attr_list_insert(al, pango_attr_scale_new(1.4));
    gtk_label_set_attributes(GTK_LABEL(mode_label), al);
    pango_attr_list_unref(al);
    gtk_box_pack_start(GTK_BOX(vbox), mode_label, FALSE, FALSE, 0);

    GtkWidget *timer_label = gtk_label_new("25:00");
    PangoAttrList *al2 = pango_attr_list_new();
    pango_attr_list_insert(al2, pango_attr_scale_new(3.5));
    pango_attr_list_insert(al2, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes(GTK_LABEL(timer_label), al2);
    pango_attr_list_unref(al2);
    gtk_box_pack_start(GTK_BOX(vbox), timer_label, FALSE, FALSE, 0);

    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_CENTER);

    GtkWidget *btn_start = gtk_button_new_with_label("Bat dau");
    GtkWidget *btn_pause = gtk_button_new_with_label("Tam dung");
    GtkWidget *btn_reset = gtk_button_new_with_label("Dat lai");

    gtk_box_pack_start(GTK_BOX(btn_box), btn_start, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(btn_box), btn_pause, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(btn_box), btn_reset, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn_box, FALSE, FALSE, 0);

    GtkWidget *info = gtk_label_new(
        "25 phut lam viec -> 5 phut nghi\n"
        "Sau 4 chu ky -> nghi dai 15 phut");
    gtk_label_set_justify(GTK_LABEL(info), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), info, FALSE, FALSE, 8);

    pomodoro_attach(timer_label, btn_start, btn_pause, btn_reset, mode_label);
    return vbox;
}

/* ================================================================
   App activate
   ================================================================ */
static void activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    fm_load();

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Time Manager");
    gtk_window_set_default_size(GTK_WINDOW(window), 620, 540);

    GtkWidget *notebook = gtk_notebook_new();
    gtk_container_add(GTK_CONTAINER(window), notebook);

    GtkWidget *task_tab = build_task_tab();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), task_tab,
                             gtk_label_new("Cong viec"));

    GtkWidget *pomo_tab = build_pomodoro_tab();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), pomo_tab,
                             gtk_label_new("Pomodoro"));

    gtk_widget_show_all(window);
}

void start_gui(int argc, char **argv) {
    tm_init();
    GtkApplication *app = gtk_application_new("com.sato.timemanager",
                                               G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
}