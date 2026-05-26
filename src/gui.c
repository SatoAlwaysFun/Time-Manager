#include "gui.h"
#include "pomodoro.h"
#include "task_manager.h"
#include "file_manager.h"
#include "task.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ================================================================
   Deadline helpers – defined here so gui.c is self-contained
   ================================================================ */
static DeadlineStatus dl_status(time_t deadline) {
    if (deadline == 0) return DEADLINE_NONE;
    double diff = difftime(deadline, time(NULL));
    if (diff < 0)         return DEADLINE_OVERDUE;
    if (diff <= 86400)    return DEADLINE_URGENT;
    if (diff <= 3*86400)  return DEADLINE_WARNING;
    return DEADLINE_NORMAL;
}

static Priority dl_effective_priority(Priority base, DeadlineStatus ds) {
    if (ds == DEADLINE_OVERDUE || ds == DEADLINE_URGENT)
        return PRIORITY_HIGH;
    if (ds == DEADLINE_WARNING && base < PRIORITY_HIGH)
        return (Priority)(base + 1);
    return base;
}

/* ================================================================
   Globals – Task input widgets
   ================================================================ */
static GtkWidget *task_list_box   = NULL;
static GtkWidget *entry_title     = NULL;
static GtkWidget *entry_desc      = NULL;
static GtkWidget *combo_priority  = NULL;
static GtkWidget *spin_hour       = NULL;
static GtkWidget *spin_minute     = NULL;
static GtkWidget *cal_widget      = NULL;   /* GtkCalendar */
static GtkWidget *cal_deadline    = NULL;   /* GtkCalendar for deadline */
static GtkWidget *chk_deadline    = NULL;   /* toggle whether deadline is set */
static GtkWidget *deadline_box    = NULL;   /* container to show/hide */
static GtkWidget *spin_dl_hour    = NULL;   /* deadline hour */
static GtkWidget *spin_dl_minute  = NULL;   /* deadline minute */

/* Schedule tab */
static GtkWidget *schedule_grid      = NULL;
static GtkWidget *schedule_week_label = NULL;  /* "Tuần DD/MM – DD/MM/YYYY" */
static time_t     schedule_week_start = 0;      /* Monday 00:00:00 of displayed week */

/* Edit dialog globals */
static int        edit_task_id    = -1;
static GtkWidget *edit_entry_title    = NULL;
static GtkWidget *edit_entry_desc     = NULL;
static GtkWidget *edit_combo_priority = NULL;
static GtkWidget *edit_spin_hour      = NULL;
static GtkWidget *edit_spin_minute    = NULL;
static GtkWidget *edit_cal            = NULL;
static GtkWidget *edit_cal_deadline   = NULL;
static GtkWidget *edit_chk_deadline   = NULL;
static GtkWidget *edit_deadline_box   = NULL;
static GtkWidget *edit_dl_spin_hour   = NULL;   /* deadline hour in edit dialog */
static GtkWidget *edit_dl_spin_minute = NULL;   /* deadline minute in edit dialog */
static GtkWidget *edit_dialog         = NULL;

/* Forward declarations */
static void refresh_task_list(void);
static void refresh_schedule(void);

/* ================================================================
   Helpers
   ================================================================ */

/* Format a time_t into "DD/MM HH:MM" or empty string if 0 */
static void format_task_time(time_t t, char *buf, size_t len) {
    if (t == 0) { buf[0] = '\0'; return; }
    struct tm *tm = localtime(&t);
    strftime(buf, len, "%d/%m %H:%M", tm);
}

/* Format deadline with urgency text */
static void format_deadline_label(time_t dl, char *buf, size_t len) {
    if (dl == 0) { buf[0] = '\0'; return; }
    char date_buf[32];
    struct tm *tm = localtime(&dl);
    strftime(date_buf, sizeof(date_buf), "%d/%m/%Y %H:%M", tm);

    DeadlineStatus ds = dl_status(dl);
    switch (ds) {
        case DEADLINE_OVERDUE:
            snprintf(buf, len, "⚠ Quá hạn: %s", date_buf);
            break;
        case DEADLINE_URGENT:
            snprintf(buf, len, "🔥 Gấp! Còn < 1 ngày (%s)", date_buf);
            break;
        case DEADLINE_WARNING: {
            double diff = difftime(dl, time(NULL));
            int days = (int)(diff / 86400);
            snprintf(buf, len, "⏰ Còn %d ngày (%s)", days, date_buf);
            break;
        }
        default:
            snprintf(buf, len, "📅 HH: %s", date_buf);
            break;
    }
}

/* Compare tasks by effective priority (including deadline boost), then id */
static int compare_tasks_by_priority(const void *a, const void *b) {
    const Task *ta = (const Task *)a;
    const Task *tb = (const Task *)b;
    Priority eff_a = dl_effective_priority(ta->priority, dl_status(ta->deadline));
    Priority eff_b = dl_effective_priority(tb->priority, dl_status(tb->deadline));
    if (eff_b != eff_a) return (int)eff_b - (int)eff_a;
    return ta->id - tb->id;
}

/* ================================================================
   Edit Task Dialog
   ================================================================ */
static void on_edit_save(GtkWidget *btn, gpointer data) {
    (void)btn; (void)data;
    Task *t = tm_find(edit_task_id);
    if (!t) return;

    const char *new_title = gtk_entry_get_text(GTK_ENTRY(edit_entry_title));
    const char *new_desc  = gtk_entry_get_text(GTK_ENTRY(edit_entry_desc));
    int prio_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(edit_combo_priority));
    if (prio_idx < 0) prio_idx = 0;

    if (!new_title || strlen(new_title) == 0) return;

    strncpy(t->title,       new_title, MAX_TITLE_LEN - 1);
    t->title[MAX_TITLE_LEN - 1] = '\0';
    strncpy(t->description, new_desc ? new_desc : "", MAX_DESC_LEN - 1);
    t->description[MAX_DESC_LEN - 1] = '\0';
    t->priority = (Priority)prio_idx;

    guint year, month, day;
    gtk_calendar_get_date(GTK_CALENDAR(edit_cal), &year, &month, &day);
    struct tm tm_val = {0};
    tm_val.tm_year  = (int)year  - 1900;
    tm_val.tm_mon   = (int)month;
    tm_val.tm_mday  = (int)day;
    tm_val.tm_hour  = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(edit_spin_hour));
    tm_val.tm_min   = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(edit_spin_minute));
    tm_val.tm_sec   = 0;
    tm_val.tm_isdst = -1;
    t->start_time = mktime(&tm_val);

    /* Save deadline */
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(edit_chk_deadline))) {
        guint dy, dm, dd;
        gtk_calendar_get_date(GTK_CALENDAR(edit_cal_deadline), &dy, &dm, &dd);
        struct tm dl_val = {0};
        dl_val.tm_year  = (int)dy - 1900;
        dl_val.tm_mon   = (int)dm;
        dl_val.tm_mday  = (int)dd;
        dl_val.tm_hour  = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(edit_dl_spin_hour));
        dl_val.tm_min   = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(edit_dl_spin_minute));
        dl_val.tm_sec   = 0;
        dl_val.tm_isdst = -1;
        t->deadline = mktime(&dl_val);
    } else {
        t->deadline = 0;
    }

    fm_save();
    refresh_task_list();
    refresh_schedule();
    gtk_widget_destroy(edit_dialog);
    edit_dialog = NULL;
}

static void on_deadline_toggle(GtkToggleButton *btn, gpointer data) {
    GtkWidget *box = GTK_WIDGET(data);
    gtk_widget_set_sensitive(box, gtk_toggle_button_get_active(btn));
}

static void on_edit_task(GtkWidget *btn, gpointer data) {
    (void)data;
    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "task-id"));
    Task *t = tm_find(id);
    if (!t) return;
    edit_task_id = id;

    /* Build dialog */
    GtkWidget *parent = gtk_widget_get_toplevel(btn);
    edit_dialog = gtk_dialog_new_with_buttons(
        "Chỉnh sửa công việc",
        GTK_WINDOW(parent),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        NULL);
    gtk_window_set_default_size(GTK_WINDOW(edit_dialog), 440, 700);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(edit_dialog));
    GtkWidget *grid    = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
    gtk_container_add(GTK_CONTAINER(content), grid);

    /* Title */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Tiêu đề:"), 0, 0, 1, 1);
    edit_entry_title = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(edit_entry_title), t->title);
    gtk_widget_set_hexpand(edit_entry_title, TRUE);
    gtk_grid_attach(GTK_GRID(grid), edit_entry_title, 1, 0, 3, 1);

    /* Description */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Mô tả:"), 0, 1, 1, 1);
    edit_entry_desc = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(edit_entry_desc), t->description);
    gtk_widget_set_hexpand(edit_entry_desc, TRUE);
    gtk_grid_attach(GTK_GRID(grid), edit_entry_desc, 1, 1, 3, 1);

    /* Priority */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Ưu tiên:"), 0, 2, 1, 1);
    edit_combo_priority = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(edit_combo_priority), "Thấp");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(edit_combo_priority), "Trung bình");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(edit_combo_priority), "Cao");
    gtk_combo_box_set_active(GTK_COMBO_BOX(edit_combo_priority), (int)t->priority);
    gtk_grid_attach(GTK_GRID(grid), edit_combo_priority, 1, 2, 1, 1);

    /* Calendar */
    GtkWidget *lbl_date = gtk_label_new("Ngày:");
    gtk_widget_set_valign(lbl_date, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), lbl_date, 0, 3, 1, 1);
    edit_cal = gtk_calendar_new();
    if (t->start_time != 0) {
        struct tm *stm = localtime(&t->start_time);
        gtk_calendar_select_month(GTK_CALENDAR(edit_cal), stm->tm_mon, 1900 + stm->tm_year);
        gtk_calendar_select_day(GTK_CALENDAR(edit_cal), stm->tm_mday);
    }
    gtk_grid_attach(GTK_GRID(grid), edit_cal, 1, 3, 2, 1);

    /* Time spinbuttons */
    GtkWidget *time_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Giờ:"), 0, 4, 1, 1);
    GtkWidget *hms = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    edit_spin_hour   = gtk_spin_button_new_with_range(0, 23, 1);
    edit_spin_minute = gtk_spin_button_new_with_range(0, 59, 1);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(edit_spin_hour),   TRUE);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(edit_spin_minute), TRUE);
    gtk_widget_set_size_request(edit_spin_hour,   56, -1);
    gtk_widget_set_size_request(edit_spin_minute, 56, -1);
    if (t->start_time != 0) {
        struct tm *stm = localtime(&t->start_time);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(edit_spin_hour),   stm->tm_hour);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(edit_spin_minute), stm->tm_min);
    }
    gtk_box_pack_start(GTK_BOX(hms), edit_spin_hour,          FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hms), gtk_label_new(":"),      FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hms), edit_spin_minute,        FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(time_box), hms,                FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), time_box, 1, 4, 2, 1);

    /* Deadline section */
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_grid_attach(GTK_GRID(grid), sep, 0, 5, 4, 1);

    edit_chk_deadline = gtk_check_button_new_with_label("⏳ Đặt deadline");
    gtk_grid_attach(GTK_GRID(grid), edit_chk_deadline, 0, 6, 4, 1);

    edit_deadline_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *dl_lbl = gtk_label_new("Ngày deadline:");
    gtk_widget_set_halign(dl_lbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(edit_deadline_box), dl_lbl, FALSE, FALSE, 0);
    edit_cal_deadline = gtk_calendar_new();
    gtk_box_pack_start(GTK_BOX(edit_deadline_box), edit_cal_deadline, FALSE, FALSE, 0);

    /* Deadline time spinbuttons */
    GtkWidget *dl_time_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *dl_time_lbl = gtk_label_new("Giờ hết hạn:");
    gtk_box_pack_start(GTK_BOX(dl_time_box), dl_time_lbl, FALSE, FALSE, 0);
    edit_dl_spin_hour   = gtk_spin_button_new_with_range(0, 23, 1);
    edit_dl_spin_minute = gtk_spin_button_new_with_range(0, 59, 1);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(edit_dl_spin_hour),   TRUE);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(edit_dl_spin_minute), TRUE);
    gtk_widget_set_size_request(edit_dl_spin_hour,   56, -1);
    gtk_widget_set_size_request(edit_dl_spin_minute, 56, -1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(edit_dl_spin_hour),   23);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(edit_dl_spin_minute), 59);
    gtk_box_pack_start(GTK_BOX(dl_time_box), edit_dl_spin_hour,          FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(dl_time_box), gtk_label_new(":"),         FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(dl_time_box), edit_dl_spin_minute,        FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(edit_deadline_box), dl_time_box, FALSE, FALSE, 0);

    gtk_widget_set_sensitive(edit_deadline_box, FALSE);
    gtk_grid_attach(GTK_GRID(grid), edit_deadline_box, 0, 7, 4, 1);

    /* Pre-fill deadline if exists */
    if (t->deadline != 0) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(edit_chk_deadline), TRUE);
        gtk_widget_set_sensitive(edit_deadline_box, TRUE);
        struct tm *dtm = localtime(&t->deadline);
        gtk_calendar_select_month(GTK_CALENDAR(edit_cal_deadline), dtm->tm_mon, 1900 + dtm->tm_year);
        gtk_calendar_select_day(GTK_CALENDAR(edit_cal_deadline), dtm->tm_mday);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(edit_dl_spin_hour),   dtm->tm_hour);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(edit_dl_spin_minute), dtm->tm_min);
    }
    g_signal_connect(edit_chk_deadline, "toggled", G_CALLBACK(on_deadline_toggle), edit_deadline_box);

    /* Save button (moved after deadline) */
    GtkWidget *save_btn2 = gtk_button_new_with_label("💾 Lưu thay đổi");
    GtkStyleContext *sc_save2 = gtk_widget_get_style_context(save_btn2);
    gtk_style_context_add_class(sc_save2, "btn-primary");
    g_signal_connect(save_btn2, "clicked", G_CALLBACK(on_edit_save), NULL);
    gtk_grid_attach(GTK_GRID(grid), save_btn2, 0, 8, 4, 1);

    gtk_widget_show_all(edit_dialog);
}

/* ================================================================
   Task tab callbacks
   ================================================================ */
static void on_toggle_done(GtkToggleButton *btn, gpointer data) {
    (void)data;
    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "task-id"));
    tm_toggle_done(id);
    fm_save();
    refresh_task_list();
    refresh_schedule();
}

static void on_delete_task(GtkWidget *btn, gpointer data) {
    (void)data;
    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "task-id"));
    tm_delete(id);
    fm_save();
    refresh_task_list();
    refresh_schedule();
}

/* Expand/collapse description on row click */
static void on_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    (void)box; (void)data;
    GtkWidget *revealer = GTK_WIDGET(g_object_get_data(G_OBJECT(row), "revealer"));
    if (revealer) {
        gboolean visible = gtk_revealer_get_reveal_child(GTK_REVEALER(revealer));
        gtk_revealer_set_reveal_child(GTK_REVEALER(revealer), !visible);
    }
}

/* ================================================================
   refresh_task_list  –  sorted by priority (HIGH first)
   ================================================================ */
static void refresh_task_list(void) {
    if (!task_list_box) return;

    GList *children = gtk_container_get_children(GTK_CONTAINER(task_list_box));
    for (GList *l = children; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);

    /* Sort a local copy by priority */
    Task sorted[MAX_TASKS];
    int  count = task_count;
    memcpy(sorted, tasks, sizeof(Task) * count);
    qsort(sorted, count, sizeof(Task), compare_tasks_by_priority);

    for (int i = 0; i < count; i++) {
        Task *t = &sorted[i];

        GtkWidget *row  = gtk_list_box_row_new();
        GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_add(GTK_CONTAINER(row), vbox);

        /* ---- Top row: checkbox + title/time + edit + delete ---- */
        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

        GtkWidget *chk = gtk_check_button_new();
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk), t->done);
        g_object_set_data(G_OBJECT(chk), "task-id", GINT_TO_POINTER(t->id));
        g_signal_connect(chk, "toggled", G_CALLBACK(on_toggle_done), NULL);
        gtk_box_pack_start(GTK_BOX(hbox), chk, FALSE, FALSE, 4);

        /* Priority badge colour indicator — use EFFECTIVE priority for display */
        DeadlineStatus ds = dl_status(t->deadline);
        Priority eff_prio = dl_effective_priority(t->priority, ds);
        const char *prio_icon;
        const char *prio_tag_class;
        switch (eff_prio) {
            case PRIORITY_HIGH:   prio_icon = "🔴"; prio_tag_class = "tag-high";   break;
            case PRIORITY_MEDIUM: prio_icon = "🟡"; prio_tag_class = "tag-medium"; break;
            default:              prio_icon = "🟢"; prio_tag_class = "tag-low";    break;
        }

        /* Priority + title + optional time */
        char time_buf[32];
        format_task_time(t->start_time, time_buf, sizeof(time_buf));

        char label_text[300];
        if (time_buf[0])
            snprintf(label_text, sizeof(label_text), "%s [%s]  %s  🕐 %s",
                     prio_icon, priority_to_string(eff_prio), t->title, time_buf);
        else
            snprintf(label_text, sizeof(label_text), "%s [%s]  %s",
                     prio_icon, priority_to_string(eff_prio), t->title);

        GtkWidget *lbl = gtk_label_new(label_text);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);

        if (t->done) {
            PangoAttrList *attrs = pango_attr_list_new();
            pango_attr_list_insert(attrs, pango_attr_strikethrough_new(TRUE));
            gtk_label_set_attributes(GTK_LABEL(lbl), attrs);
            pango_attr_list_unref(attrs);
        }
        gtk_box_pack_start(GTK_BOX(hbox), lbl, TRUE, TRUE, 0);

        /* Priority tag pill */
        GtkWidget *tag_lbl = gtk_label_new(priority_to_string(eff_prio));
        GtkStyleContext *sc_tag = gtk_widget_get_style_context(tag_lbl);
        gtk_style_context_add_class(sc_tag, prio_tag_class);
        gtk_box_pack_start(GTK_BOX(hbox), tag_lbl, FALSE, FALSE, 0);

        /* Deadline badge (show only if not done) */
        if (!t->done && t->deadline != 0) {
            char dl_buf[80];
            format_deadline_label(t->deadline, dl_buf, sizeof(dl_buf));
            GtkWidget *dl_lbl = gtk_label_new(dl_buf);
            const char *dl_class;
            switch (ds) {
                case DEADLINE_OVERDUE: dl_class = "deadline-overdue"; break;
                case DEADLINE_URGENT:  dl_class = "deadline-urgent";  break;
                case DEADLINE_WARNING: dl_class = "deadline-warning"; break;
                default:               dl_class = "deadline-normal";  break;
            }
            GtkStyleContext *sc_dl = gtk_widget_get_style_context(dl_lbl);
            gtk_style_context_add_class(sc_dl, dl_class);
            gtk_box_pack_start(GTK_BOX(hbox), dl_lbl, FALSE, FALSE, 4);
        }

        /* Edit button */
        GtkWidget *edit_btn = gtk_button_new_with_label("✏");
        GtkStyleContext *sc_edit = gtk_widget_get_style_context(edit_btn);
        gtk_style_context_add_class(sc_edit, "btn-icon");
        gtk_style_context_add_class(sc_edit, "btn-edit");
        g_object_set_data(G_OBJECT(edit_btn), "task-id", GINT_TO_POINTER(t->id));
        g_signal_connect(edit_btn, "clicked", G_CALLBACK(on_edit_task), NULL);
        gtk_box_pack_end(GTK_BOX(hbox), edit_btn, FALSE, FALSE, 0);

        GtkWidget *del_btn = gtk_button_new_with_label("✕");
        GtkStyleContext *sc_del = gtk_widget_get_style_context(del_btn);
        gtk_style_context_add_class(sc_del, "btn-icon");
        gtk_style_context_add_class(sc_del, "btn-delete");
        g_object_set_data(G_OBJECT(del_btn), "task-id", GINT_TO_POINTER(t->id));
        g_signal_connect(del_btn, "clicked", G_CALLBACK(on_delete_task), NULL);
        gtk_box_pack_end(GTK_BOX(hbox), del_btn, FALSE, FALSE, 4);

        /* ---- Revealer for description ---- */
        GtkWidget *revealer = gtk_revealer_new();
        gtk_revealer_set_transition_type(GTK_REVEALER(revealer),
                                         GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
        gtk_revealer_set_reveal_child(GTK_REVEALER(revealer), FALSE);

        if (t->description[0] != '\0') {
            GtkWidget *desc_lbl = gtk_label_new(t->description);
            gtk_label_set_xalign(GTK_LABEL(desc_lbl), 0.0f);
            gtk_label_set_line_wrap(GTK_LABEL(desc_lbl), TRUE);
            gtk_widget_set_margin_start(desc_lbl, 32);
            gtk_widget_set_margin_bottom(desc_lbl, 4);
            GtkStyleContext *sc = gtk_widget_get_style_context(desc_lbl);
            gtk_style_context_add_class(sc, "dim-label");
            gtk_container_add(GTK_CONTAINER(revealer), desc_lbl);
        } else {
            GtkWidget *no_desc = gtk_label_new("(Không có mô tả)");
            gtk_label_set_xalign(GTK_LABEL(no_desc), 0.0f);
            gtk_widget_set_margin_start(no_desc, 32);
            gtk_widget_set_margin_bottom(no_desc, 4);
            GtkStyleContext *sc = gtk_widget_get_style_context(no_desc);
            gtk_style_context_add_class(sc, "dim-label");
            gtk_container_add(GTK_CONTAINER(revealer), no_desc);
        }
        gtk_box_pack_start(GTK_BOX(vbox), revealer, FALSE, FALSE, 0);

        g_object_set_data(G_OBJECT(row), "revealer", revealer);

        gtk_container_add(GTK_CONTAINER(task_list_box), row);
    }

    g_signal_connect(task_list_box, "row-activated", G_CALLBACK(on_row_activated), NULL);
    gtk_widget_show_all(task_list_box);
}

/* ================================================================
   Add task callback
   ================================================================ */
static void on_add_task(GtkWidget *btn, gpointer data) {
    (void)btn; (void)data;
    const char *title = gtk_entry_get_text(GTK_ENTRY(entry_title));
    const char *desc  = gtk_entry_get_text(GTK_ENTRY(entry_desc));
    int prio_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(combo_priority));
    if (prio_idx < 0) prio_idx = 0;

    if (!title || strlen(title) == 0) return;

    /* Build time_t from calendar + spinbuttons */
    guint year, month, day;
    gtk_calendar_get_date(GTK_CALENDAR(cal_widget), &year, &month, &day);

    struct tm tm_val = {0};
    tm_val.tm_year = (int)year  - 1900;
    tm_val.tm_mon  = (int)month;          /* GtkCalendar month is 0-based */
    tm_val.tm_mday = (int)day;
    tm_val.tm_hour = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin_hour));
    tm_val.tm_min  = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin_minute));
    tm_val.tm_sec  = 0;
    tm_val.tm_isdst = -1;
    time_t start_time = mktime(&tm_val);

    tm_add(title, desc, (Priority)prio_idx, start_time);

    /* Set deadline on the newly-added task */
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chk_deadline))) {
        guint dy, dm, dd;
        gtk_calendar_get_date(GTK_CALENDAR(cal_deadline), &dy, &dm, &dd);
        struct tm dl_val = {0};
        dl_val.tm_year  = (int)dy - 1900;
        dl_val.tm_mon   = (int)dm;
        dl_val.tm_mday  = (int)dd;
        dl_val.tm_hour  = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin_dl_hour));
        dl_val.tm_min   = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin_dl_minute));
        dl_val.tm_sec   = 0;
        dl_val.tm_isdst = -1;
        tasks[task_count - 1].deadline = mktime(&dl_val);
    }

    fm_save();
    gtk_entry_set_text(GTK_ENTRY(entry_title), "");
    gtk_entry_set_text(GTK_ENTRY(entry_desc),  "");
    refresh_task_list();
    refresh_schedule();
}

/* ================================================================
   Build Task tab
   ================================================================ */
static GtkWidget *build_task_tab(void) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);

    /* ---------- Input frame ---------- */
    GtkWidget *input_frame = gtk_frame_new("Thêm công việc mới");
    GtkWidget *input_grid  = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(input_grid), 8);
    gtk_grid_set_row_spacing(GTK_GRID(input_grid), 6);
    gtk_container_set_border_width(GTK_CONTAINER(input_grid), 8);
    gtk_container_add(GTK_CONTAINER(input_frame), input_grid);

    /* Row 0: Title */
    GtkWidget *lbl_title = gtk_label_new("Tiêu đề:");
    gtk_widget_set_halign(lbl_title, GTK_ALIGN_END);
    entry_title = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_title), "Nhập tiêu đề công việc...");
    gtk_widget_set_hexpand(entry_title, TRUE);
    gtk_grid_attach(GTK_GRID(input_grid), lbl_title,   0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(input_grid), entry_title, 1, 0, 3, 1);

    /* Row 1: Description */
    GtkWidget *lbl_desc = gtk_label_new("Mô tả:");
    gtk_widget_set_halign(lbl_desc, GTK_ALIGN_END);
    entry_desc = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_desc), "Mô tả (tùy chọn)...");
    gtk_widget_set_hexpand(entry_desc, TRUE);
    gtk_grid_attach(GTK_GRID(input_grid), lbl_desc,   0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(input_grid), entry_desc, 1, 1, 3, 1);

    /* Row 2: Priority */
    GtkWidget *lbl_prio = gtk_label_new("Ưu tiên:");
    gtk_widget_set_halign(lbl_prio, GTK_ALIGN_END);
    combo_priority = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_priority), "Thấp");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_priority), "Trung bình");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_priority), "Cao");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_priority), 1);
    gtk_grid_attach(GTK_GRID(input_grid), lbl_prio,       0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(input_grid), combo_priority, 1, 2, 1, 1);

    /* Row 3: Date picker (GtkCalendar) */
    GtkWidget *lbl_date = gtk_label_new("Ngày:");
    gtk_widget_set_halign(lbl_date, GTK_ALIGN_END);
    gtk_widget_set_valign(lbl_date, GTK_ALIGN_START);
    cal_widget = gtk_calendar_new();
    gtk_grid_attach(GTK_GRID(input_grid), lbl_date,    0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(input_grid), cal_widget,  1, 3, 2, 1);

    /* Row 3 (right): Time spinbuttons */
    GtkWidget *time_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_time = gtk_label_new("Giờ bắt đầu:");
    gtk_widget_set_halign(lbl_time, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(time_box), lbl_time, FALSE, FALSE, 0);

    GtkWidget *hms = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    spin_hour   = gtk_spin_button_new_with_range(0, 23, 1);
    spin_minute = gtk_spin_button_new_with_range(0, 59, 1);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin_hour),   TRUE);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin_minute), TRUE);
    gtk_widget_set_size_request(spin_hour,   56, -1);
    gtk_widget_set_size_request(spin_minute, 56, -1);
    gtk_box_pack_start(GTK_BOX(hms), spin_hour,             FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hms), gtk_label_new(":"),    FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hms), spin_minute,           FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(time_box), hms, FALSE, FALSE, 0);

    gtk_widget_set_valign(time_box, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(input_grid), time_box, 3, 3, 1, 1);

    /* Row 4: Deadline toggle + calendar */
    chk_deadline = gtk_check_button_new_with_label("⏳ Đặt deadline");
    gtk_grid_attach(GTK_GRID(input_grid), chk_deadline, 0, 4, 2, 1);

    deadline_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_dl = gtk_label_new("Ngày deadline:");
    gtk_widget_set_halign(lbl_dl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(deadline_box), lbl_dl, FALSE, FALSE, 0);
    cal_deadline = gtk_calendar_new();
    gtk_box_pack_start(GTK_BOX(deadline_box), cal_deadline, FALSE, FALSE, 0);

    /* Deadline time spinbuttons */
    GtkWidget *dl_time_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *lbl_dl_time = gtk_label_new("Giờ hết hạn:");
    gtk_box_pack_start(GTK_BOX(dl_time_row), lbl_dl_time, FALSE, FALSE, 0);
    spin_dl_hour   = gtk_spin_button_new_with_range(0, 23, 1);
    spin_dl_minute = gtk_spin_button_new_with_range(0, 59, 1);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin_dl_hour),   TRUE);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin_dl_minute), TRUE);
    gtk_widget_set_size_request(spin_dl_hour,   56, -1);
    gtk_widget_set_size_request(spin_dl_minute, 56, -1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_dl_hour),   23);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_dl_minute), 59);
    gtk_box_pack_start(GTK_BOX(dl_time_row), spin_dl_hour,          FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(dl_time_row), gtk_label_new(":"),    FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(dl_time_row), spin_dl_minute,        FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(deadline_box), dl_time_row, FALSE, FALSE, 0);

    gtk_widget_set_sensitive(deadline_box, FALSE);
    gtk_grid_attach(GTK_GRID(input_grid), deadline_box, 2, 4, 2, 1);
    g_signal_connect(chk_deadline, "toggled", G_CALLBACK(on_deadline_toggle), deadline_box);

    /* Row 5: Add button */
    GtkWidget *add_btn = gtk_button_new_with_label("＋ Thêm công việc");
    GtkStyleContext *sc_add = gtk_widget_get_style_context(add_btn);
    gtk_style_context_add_class(sc_add, "btn-primary");
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_task), NULL);
    gtk_grid_attach(GTK_GRID(input_grid), add_btn, 0, 5, 4, 1);

    gtk_box_pack_start(GTK_BOX(vbox), input_frame, FALSE, FALSE, 0);

    /* ---------- Sort label hint ---------- */
    GtkWidget *sort_hint = gtk_label_new("🔴 Cao  🟡 Trung bình  🟢 Thấp  —  Deadline sắp tới sẽ tự động tăng ưu tiên");
    gtk_label_set_xalign(GTK_LABEL(sort_hint), 0.0f);
    GtkStyleContext *sc_hint = gtk_widget_get_style_context(sort_hint);
    gtk_style_context_add_class(sc_hint, "dim-label");
    gtk_widget_set_margin_start(sort_hint, 4);
    gtk_box_pack_start(GTK_BOX(vbox), sort_hint, FALSE, FALSE, 0);

    /* ---------- Task list ---------- */
    GtkWidget *list_frame = gtk_frame_new("Danh sách công việc  (nhấn để xem mô tả)");
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroll, -1, 260);

    task_list_box = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(task_list_box), GTK_SELECTION_NONE);
    gtk_container_add(GTK_CONTAINER(scroll), task_list_box);
    gtk_container_add(GTK_CONTAINER(list_frame), scroll);
    gtk_box_pack_start(GTK_BOX(vbox), list_frame, TRUE, TRUE, 0);

    refresh_task_list();
    return vbox;
}

/* ================================================================
   Build Pomodoro tab  (with editable durations)
   ================================================================ */
static GtkWidget *build_pomodoro_tab(void) {
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(outer), 16);

    /* ---- Timer section ---- */
    GtkWidget *timer_frame = gtk_frame_new("Bộ đếm thời gian");
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 16);
    gtk_widget_set_halign(vbox, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(timer_frame), vbox);

    GtkWidget *mode_label = gtk_label_new("LÀM VIỆC");
    GtkStyleContext *sc_mode = gtk_widget_get_style_context(mode_label);
    gtk_style_context_add_class(sc_mode, "mode-display");
    gtk_box_pack_start(GTK_BOX(vbox), mode_label, FALSE, FALSE, 0);

    GtkWidget *timer_label = gtk_label_new("25:00");
    GtkStyleContext *sc_timer = gtk_widget_get_style_context(timer_label);
    gtk_style_context_add_class(sc_timer, "timer-display");
    gtk_box_pack_start(GTK_BOX(vbox), timer_label, FALSE, FALSE, 0);

    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_CENTER);
    GtkWidget *btn_start = gtk_button_new_with_label("▶  Bắt đầu");
    GtkWidget *btn_pause = gtk_button_new_with_label("⏸  Tạm dừng");
    GtkWidget *btn_reset = gtk_button_new_with_label("↺  Đặt lại");
    GtkStyleContext *sc_bs = gtk_widget_get_style_context(btn_start);
    gtk_style_context_add_class(sc_bs, "btn-primary");
    gtk_box_pack_start(GTK_BOX(btn_box), btn_start, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(btn_box), btn_pause, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(btn_box), btn_reset, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn_box, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(outer), timer_frame, FALSE, FALSE, 0);

    /* ---- Settings section ---- */
    GtkWidget *settings_frame = gtk_frame_new("Cài đặt thời gian (phút)");
    GtkWidget *sgrid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(sgrid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(sgrid), 8);
    gtk_container_set_border_width(GTK_CONTAINER(sgrid), 12);
    gtk_container_add(GTK_CONTAINER(settings_frame), sgrid);

    /* Work */
    GtkWidget *lbl_w = gtk_label_new("🍅 Làm việc:");
    gtk_widget_set_halign(lbl_w, GTK_ALIGN_END);
    GtkWidget *spin_work = gtk_spin_button_new_with_range(1, 90, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_work), 25);
    gtk_grid_attach(GTK_GRID(sgrid), lbl_w,     0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(sgrid), spin_work, 1, 0, 1, 1);

    /* Short break */
    GtkWidget *lbl_s = gtk_label_new("☕ Nghỉ ngắn:");
    gtk_widget_set_halign(lbl_s, GTK_ALIGN_END);
    GtkWidget *spin_short = gtk_spin_button_new_with_range(1, 30, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_short), 5);
    gtk_grid_attach(GTK_GRID(sgrid), lbl_s,      0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(sgrid), spin_short, 1, 1, 1, 1);

    /* Long break */
    GtkWidget *lbl_l = gtk_label_new("🌟 Nghỉ dài:");
    gtk_widget_set_halign(lbl_l, GTK_ALIGN_END);
    GtkWidget *spin_long = gtk_spin_button_new_with_range(1, 60, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_long), 15);
    gtk_grid_attach(GTK_GRID(sgrid), lbl_l,     0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(sgrid), spin_long, 1, 2, 1, 1);

    GtkWidget *note = gtk_label_new("Thay đổi có hiệu lực sau chu kỳ hiện tại (hoặc khi nhấn Đặt lại).");
    gtk_label_set_xalign(GTK_LABEL(note), 0.0f);
    gtk_widget_set_margin_top(note, 4);
    GtkStyleContext *sc = gtk_widget_get_style_context(note);
    gtk_style_context_add_class(sc, "dim-label");
    gtk_grid_attach(GTK_GRID(sgrid), note, 0, 3, 2, 1);

    gtk_box_pack_start(GTK_BOX(outer), settings_frame, FALSE, FALSE, 0);

    /* Info */
    GtkWidget *info = gtk_label_new("Sau 4 chu kỳ làm việc → nghỉ dài.");
    gtk_label_set_justify(GTK_LABEL(info), GTK_JUSTIFY_CENTER);
    GtkStyleContext *sc2 = gtk_widget_get_style_context(info);
    gtk_style_context_add_class(sc2, "dim-label");
    gtk_box_pack_start(GTK_BOX(outer), info, FALSE, FALSE, 0);

    pomodoro_attach(timer_label, btn_start, btn_pause, btn_reset, mode_label,
                    spin_work, spin_short, spin_long);
    return outer;
}

/* ================================================================
   Build Schedule tab  – weekly timetable with Sáng/Chiều/Tối rows
   ================================================================ */

/* Returns ISO weekday 1=Mon … 7=Sun for a time_t, or 0 if t==0 */
static int weekday_of(time_t t) {
    if (t == 0) return 0;
    struct tm *tm = localtime(&t);
    return (tm->tm_wday == 0) ? 7 : tm->tm_wday;
}

/* Returns hour of day for a time_t, or -1 if t==0 */
static int hour_of(time_t t) {
    if (t == 0) return -1;
    struct tm *tm = localtime(&t);
    return tm->tm_hour;
}

/* Map hour to slot: 0=Sáng(0–11), 1=Chiều(12–17), 2=Tối(18–23) */
static int time_slot_of(int hour) {
    if (hour < 0)  return -1;
    if (hour < 12) return 0;
    if (hour < 18) return 1;
    return 2;
}

/* Returns Monday 00:00:00 of the week containing t (or current week if t==0) */
static time_t week_monday(time_t t) {
    if (t == 0) t = time(NULL);
    struct tm tm_copy = *localtime(&t);
    int wd = tm_copy.tm_wday; /* 0=Sun */
    int days_since_mon = (wd == 0) ? 6 : wd - 1;
    tm_copy.tm_mday  -= days_since_mon;
    tm_copy.tm_hour   = 0;
    tm_copy.tm_min    = 0;
    tm_copy.tm_sec    = 0;
    tm_copy.tm_isdst  = -1;
    return mktime(&tm_copy);
}

static void refresh_schedule(void) {
    if (!schedule_grid) return;

    /* Initialise week_start to current week on first call */
    if (schedule_week_start == 0)
        schedule_week_start = week_monday(0);

    /* Update week label */
    if (schedule_week_label) {
        time_t sun = schedule_week_start + 6 * 86400;
        struct tm *ms = localtime(&schedule_week_start);
        char buf_mon[16], buf_sun[16];
        strftime(buf_mon, sizeof(buf_mon), "%d/%m", ms);
        struct tm *ss = localtime(&sun);
        strftime(buf_sun, sizeof(buf_sun), "%d/%m/%Y", ss);
        char week_str[64];
        snprintf(week_str, sizeof(week_str), "Tuần %s – %s", buf_mon, buf_sun);
        gtk_label_set_text(GTK_LABEL(schedule_week_label), week_str);
    }

    /* Remove all children except header row (row 0) and slot label col (col 0) */
    GList *children = gtk_container_get_children(GTK_CONTAINER(schedule_grid));
    for (GList *l = children; l; l = l->next) {
        GtkWidget *w = GTK_WIDGET(l->data);
        gint row_idx, col_idx;
        gtk_container_child_get(GTK_CONTAINER(schedule_grid), w,
                                "top-attach",  &row_idx,
                                "left-attach", &col_idx,
                                NULL);
        if (row_idx > 0 && col_idx > 0)
            gtk_widget_destroy(w);
        /* Also rebuild day headers (row 0, col 1..7) with dates */
        if (row_idx == 0 && col_idx > 0)
            gtk_widget_destroy(w);
    }
    g_list_free(children);

    /* Re-build day headers with actual dates */
    const char *day_names[] = { "Thứ Hai", "Thứ Ba", "Thứ Tư",
                                 "Thứ Năm", "Thứ Sáu", "Thứ Bảy", "Chủ Nhật" };
    for (int d = 0; d < 7; d++) {
        time_t day_t = schedule_week_start + d * 86400;
        struct tm *dtm = localtime(&day_t);
        char hdr_text[32];
        snprintf(hdr_text, sizeof(hdr_text), "%s\n%02d/%02d",
                 day_names[d], dtm->tm_mday, dtm->tm_mon + 1);

        GtkWidget *hdr = gtk_label_new(hdr_text);
        gtk_label_set_justify(GTK_LABEL(hdr), GTK_JUSTIFY_CENTER);
        gtk_widget_set_margin_top(hdr, 6);
        gtk_widget_set_margin_bottom(hdr, 6);
        PangoAttrList *al = pango_attr_list_new();
        pango_attr_list_insert(al, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
        gtk_label_set_attributes(GTK_LABEL(hdr), al);
        pango_attr_list_unref(al);

        /* Highlight today */
        time_t now = time(NULL);
        struct tm *ntm = localtime(&now);
        if (dtm->tm_mday == ntm->tm_mday &&
            dtm->tm_mon  == ntm->tm_mon  &&
            dtm->tm_year == ntm->tm_year) {
            GtkWidget *hdr_box = gtk_event_box_new();
            gtk_container_add(GTK_CONTAINER(hdr_box), hdr);
            GtkStyleContext *sc = gtk_widget_get_style_context(hdr_box);
            gtk_style_context_add_class(sc, "schedule-header-today");
            gtk_grid_attach(GTK_GRID(schedule_grid), hdr_box, d + 1, 0, 1, 1);
        } else {
            GtkWidget *hdr_box = gtk_event_box_new();
            gtk_container_add(GTK_CONTAINER(hdr_box), hdr);
            GtkStyleContext *sc = gtk_widget_get_style_context(hdr_box);
            gtk_style_context_add_class(sc, "schedule-header");
            gtk_grid_attach(GTK_GRID(schedule_grid), hdr_box, d + 1, 0, 1, 1);
        }
    }

    /*
     * Grid layout:
     *   col 0          : row labels (Sáng/Chiều/Tối) – rows 1,2,3
     *   col 1..7       : Mon..Sun
     *   row 0          : day headers
     *   rows 1,2,3     : Sáng, Chiều, Tối
     */

    /* Build 7×3 vboxes with background event boxes for alternating columns */
    GtkWidget *cell_boxes[8][3]; /* [day 1..7][slot 0..2] */
    const char *slot_labels[] = { "🌅 Sáng\n(0–11h)", "☀ Chiều\n(12–17h)", "🌙 Tối\n(18–23h)" };
    /* Alternating cell background classes */
    const char *col_bg_class[] = { "sched-cell-odd", "sched-cell-even" };

    for (int s = 0; s < 3; s++) {
        /* Slot row label (col 0) */
        GtkWidget *slot_lbl = gtk_label_new(slot_labels[s]);
        gtk_label_set_justify(GTK_LABEL(slot_lbl), GTK_JUSTIFY_CENTER);
        gtk_widget_set_margin_top(slot_lbl, 8);
        gtk_widget_set_margin_bottom(slot_lbl, 8);
        gtk_widget_set_margin_start(slot_lbl, 6);
        gtk_widget_set_margin_end(slot_lbl, 6);
        PangoAttrList *al = pango_attr_list_new();
        pango_attr_list_insert(al, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
        gtk_label_set_attributes(GTK_LABEL(slot_lbl), al);
        pango_attr_list_unref(al);
        GtkWidget *slot_eb = gtk_event_box_new();
        gtk_container_add(GTK_CONTAINER(slot_eb), slot_lbl);
        GtkStyleContext *sc = gtk_widget_get_style_context(slot_eb);
        gtk_style_context_add_class(sc, "schedule-slot-header");
        gtk_grid_attach(GTK_GRID(schedule_grid), slot_eb, 0, s + 1, 1, 1);

        for (int d = 1; d <= 7; d++) {
            /* Outer event box for alternating column background */
            GtkWidget *cell_eb = gtk_event_box_new();
            GtkStyleContext *sc_cell = gtk_widget_get_style_context(cell_eb);
            gtk_style_context_add_class(sc_cell, "sched-cell");
            gtk_style_context_add_class(sc_cell, col_bg_class[(d - 1) % 2]);

            GtkWidget *vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
            gtk_widget_set_margin_start(vb, 4);
            gtk_widget_set_margin_end(vb, 4);
            gtk_widget_set_margin_top(vb, 6);
            gtk_widget_set_margin_bottom(vb, 6);
            gtk_container_add(GTK_CONTAINER(cell_eb), vb);
            cell_boxes[d][s] = vb;
            gtk_grid_attach(GTK_GRID(schedule_grid), cell_eb, d, s + 1, 1, 1);
        }
    }

    /* Place tasks that fall in the displayed week */
    for (int i = 0; i < task_count; i++) {
        Task *t = &tasks[i];
        if (t->start_time == 0) continue;

        /* Check if task falls in displayed week */
        time_t week_end = schedule_week_start + 7 * 86400;
        if (t->start_time < schedule_week_start || t->start_time >= week_end) continue;

        /* ISO weekday 1=Mon..7=Sun */
        struct tm *ttm = localtime(&t->start_time);
        int wd = (ttm->tm_wday == 0) ? 7 : ttm->tm_wday;

        int slot = time_slot_of(ttm->tm_hour);
        if (slot < 0) continue;

        char time_buf[16];
        format_task_time(t->start_time, time_buf, sizeof(time_buf));

        /* Build card text with optional deadline */
        char card_text[220];
        if (t->deadline != 0) {
            char dl_buf[48];
            format_deadline_label(t->deadline, dl_buf, sizeof(dl_buf));
            snprintf(card_text, sizeof(card_text), "%s\n%s\n%s",
                     time_buf, t->title, dl_buf);
        } else {
            snprintf(card_text, sizeof(card_text), "%s\n%s", time_buf, t->title);
        }

        GtkWidget *card  = gtk_frame_new(NULL);
        GtkWidget *inner = gtk_label_new(card_text);
        gtk_label_set_xalign(GTK_LABEL(inner), 0.0f);
        gtk_label_set_line_wrap(GTK_LABEL(inner), TRUE);
        gtk_widget_set_margin_start(inner, 5);
        gtk_widget_set_margin_end(inner, 4);
        gtk_widget_set_margin_top(inner, 3);
        gtk_widget_set_margin_bottom(inner, 3);

        if (t->done) {
            PangoAttrList *attrs = pango_attr_list_new();
            pango_attr_list_insert(attrs, pango_attr_strikethrough_new(TRUE));
            gtk_label_set_attributes(GTK_LABEL(inner), attrs);
            pango_attr_list_unref(attrs);
        }

        /* Use effective priority for card color */
        DeadlineStatus ds = dl_status(t->deadline);
        Priority eff = dl_effective_priority(t->priority, ds);
        const char *css_class;
        switch (eff) {
            case PRIORITY_HIGH:   css_class = "task-high";   break;
            case PRIORITY_MEDIUM: css_class = "task-medium"; break;
            default:              css_class = "task-low";    break;
        }
        GtkStyleContext *sc = gtk_widget_get_style_context(card);
        gtk_style_context_add_class(sc, css_class);

        gtk_container_add(GTK_CONTAINER(card), inner);
        gtk_box_pack_start(GTK_BOX(cell_boxes[wd][slot]), card, FALSE, FALSE, 0);
    }

    gtk_widget_show_all(schedule_grid);
}

static void on_week_prev(GtkWidget *btn, gpointer data) {
    (void)btn; (void)data;
    schedule_week_start -= 7 * 86400;
    refresh_schedule();
}

static void on_week_next(GtkWidget *btn, gpointer data) {
    (void)btn; (void)data;
    schedule_week_start += 7 * 86400;
    refresh_schedule();
}

static void on_week_today(GtkWidget *btn, gpointer data) {
    (void)btn; (void)data;
    schedule_week_start = week_monday(0);
    refresh_schedule();
}

static GtkWidget *build_schedule_tab(void) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    /* ---- Week navigation bar ---- */
    GtkWidget *nav_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(nav_bar), 8);

    GtkWidget *btn_prev = gtk_button_new_with_label("◀  Tuần trước");
    GtkWidget *btn_today = gtk_button_new_with_label("📅 Hôm nay");
    GtkWidget *btn_next = gtk_button_new_with_label("Tuần sau  ▶");

    GtkStyleContext *sc_today = gtk_widget_get_style_context(btn_today);
    gtk_style_context_add_class(sc_today, "btn-primary");

    g_signal_connect(btn_prev,  "clicked", G_CALLBACK(on_week_prev),  NULL);
    g_signal_connect(btn_today, "clicked", G_CALLBACK(on_week_today), NULL);
    g_signal_connect(btn_next,  "clicked", G_CALLBACK(on_week_next),  NULL);

    schedule_week_label = gtk_label_new("Tuần ...");
    gtk_widget_set_hexpand(schedule_week_label, TRUE);
    gtk_label_set_xalign(GTK_LABEL(schedule_week_label), 0.5f);
    PangoAttrList *wal = pango_attr_list_new();
    pango_attr_list_insert(wal, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    pango_attr_list_insert(wal, pango_attr_scale_new(1.1));
    gtk_label_set_attributes(GTK_LABEL(schedule_week_label), wal);
    pango_attr_list_unref(wal);

    gtk_box_pack_start(GTK_BOX(nav_bar), btn_prev,            FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(nav_bar), btn_today,           FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(nav_bar), schedule_week_label, TRUE,  TRUE,  0);
    gtk_box_pack_end  (GTK_BOX(nav_bar), btn_next,            FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), nav_bar, FALSE, FALSE, 0);

    /* Thin separator below nav bar */
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);

    /* ---- Scrollable grid ---- */
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    schedule_grid = gtk_grid_new();
    gtk_grid_set_column_homogeneous(GTK_GRID(schedule_grid), TRUE);
    gtk_grid_set_row_spacing(GTK_GRID(schedule_grid), 2);
    gtk_grid_set_column_spacing(GTK_GRID(schedule_grid), 2);
    gtk_container_set_border_width(GTK_CONTAINER(schedule_grid), 8);

    /* Column 0 header: empty corner */
    GtkWidget *corner = gtk_label_new("");
    gtk_grid_attach(GTK_GRID(schedule_grid), corner, 0, 0, 1, 1);

    /* Day headers are rebuilt in refresh_schedule; add placeholders */
    for (int d = 0; d < 7; d++) {
        GtkWidget *ph = gtk_label_new("");
        gtk_grid_attach(GTK_GRID(schedule_grid), ph, d + 1, 0, 1, 1);
    }

    gtk_container_add(GTK_CONTAINER(scroll), schedule_grid);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    GtkWidget *hint = gtk_label_new("🌅 Sáng 0–11h  ☀ Chiều 12–17h  🌙 Tối 18–23h  |  ⚠ Quá hạn  🔥 Gấp  ⏰ Sắp tới  📅 Bình thường  — Chỉ hiện task có ngày bắt đầu trong tuần này");
    GtkStyleContext *sc_hint = gtk_widget_get_style_context(hint);
    gtk_style_context_add_class(sc_hint, "dim-label");
    gtk_widget_set_margin_top(hint, 4);
    gtk_widget_set_margin_bottom(hint, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hint, FALSE, FALSE, 0);

    refresh_schedule();
    return vbox;
}

/* ================================================================
   CSS
   ================================================================ */
static void load_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,

    /* ── Global reset ── */
    "* { outline: none; color: #111111; }\n"

    /* ── Window background ── */
    "window, .background { background-color: #f5f5f3; }\n"

    /* ── Notebook (tab bar) ── */
    "notebook > header { background-color: #ffffff; border-bottom: 1px solid #e4e4e0; padding: 0; }\n"
    "notebook > header tab {"
    "   background-color: transparent; border: none; border-bottom: 2px solid transparent;"
    "   color: #444; padding: 10px 20px; font-size: 13px; margin-bottom: -1px; }\n"
    "notebook > header tab:checked {"
    "   color: #1a56db; border-bottom-color: #1a56db; font-weight: 500;"
    "   background-color: transparent; }\n"
    "notebook > header tab:hover { color: #111; background-color: transparent; }\n"
    "notebook > stack { background-color: #f5f5f3; }\n"

    /* ── Frames / cards ── */
    "frame { border: 1px solid #e4e4e0; border-radius: 8px; background: #ffffff; }\n"
    "frame > border { border: none; }\n"
    "frame > label { color: #333; font-size: 11px; font-weight: 600; padding: 0 6px; }\n"

    /* ── Entries ── */
    "entry {"
    "   border: 1px solid #e0e0da; border-radius: 6px;"
    "   background-color: #ffffff; color: #111; padding: 6px 10px; font-size: 13px; }\n"
    "entry:focus { border-color: #1a56db; box-shadow: none; }\n"

    /* ── Buttons ── */
    "button {"
    "   border: 1px solid #d0d0c8; border-radius: 6px;"
    "   background-color: #ffffff; color: #111; padding: 6px 14px;"
    "   font-size: 13px; box-shadow: none; text-shadow: none; }\n"
    "button:hover { background-color: #f0f0ec; border-color: #b0b0a8; }\n"
    "button:active { background-color: #e8e8e4; }\n"
    "button label { color: #111; }\n"

    /* ── Primary action button (Add task) ── */
    ".btn-primary {"
    "   background-color: #1a56db; color: #ffffff;"
    "   border-color: #1a56db; font-weight: 500; }\n"
    ".btn-primary label { color: #ffffff; }\n"
    ".btn-primary:hover { background-color: #1546b8; border-color: #1546b8; }\n"
    ".btn-primary:active { background-color: #1240a8; }\n"

    /* ── Icon-only small buttons (edit, delete) ── */
    ".btn-icon { padding: 4px 8px; min-width: 0; border-radius: 5px; color: #555; }\n"
    ".btn-icon label { color: #555; }\n"
    ".btn-icon:hover { color: #111; }\n"
    ".btn-delete:hover { color: #c0392b; border-color: #f5b7b1; background-color: #fdf2f1; }\n"
    ".btn-edit:hover   { color: #1a56db; border-color: #b0c8f8; background-color: #f0f4fd; }\n"

    /* ── Combo box ── */
    "combobox button { border: 1px solid #d0d0c8; border-radius: 6px; background-color: #fff; color: #111; }\n"

    /* ── Spin buttons ── */
    "spinbutton { border: 1px solid #d0d0c8; border-radius: 6px; background: #fff; font-size: 13px; color: #111; }\n"
    "spinbutton:focus { border-color: #1a56db; }\n"

    /* ── ListBox (task list) ── */
    "list { background-color: transparent; border: none; }\n"
    "list row {"
    "   background-color: #ffffff;"
    "   border: 1px solid #e8e8e4;"
    "   border-radius: 7px;"
    "   margin: 3px 0;"
    "   padding: 2px; }\n"
    "list row:hover { border-color: #c0c0b8; background-color: #fafafa; }\n"
    "list row:selected { background-color: #f0f4fd; border-color: #b0c8f8; }\n"
    "list row:selected label { color: #111; }\n"
    "list row label { color: #111; }\n"

    /* ── Scrolled window ── */
    "scrolledwindow { background-color: transparent; }\n"
    "scrolledwindow overshoot { background: none; }\n"

    /* ── Labels ── */
    "label { color: #111; }\n"
    ".dim-label { color: #555; font-size: 12px; }\n"
    ".section-label { color: #444; font-size: 11px; font-weight: 600; }\n"

    /* ── Priority tags in task list ── */
    ".tag-high   { color: #7a1a1a; background-color: #fcebeb; border-radius: 100px; padding: 1px 8px; font-size: 11px; font-weight: 600; }\n"
    ".tag-medium { color: #5a3200; background-color: #faeeda; border-radius: 100px; padding: 1px 8px; font-size: 11px; font-weight: 600; }\n"
    ".tag-low    { color: #1e4a00; background-color: #eaf3de; border-radius: 100px; padding: 1px 8px; font-size: 11px; font-weight: 600; }\n"

    /* ── Calendar ── */
    "calendar { border: 1px solid #d0d0c8; border-radius: 8px; background: #fff; font-size: 13px; color: #111; }\n"
    "calendar:selected { background-color: #1a56db; color: white; border-radius: 4px; }\n"

    /* ── Schedule navigation bar ── */
    ".schedule-nav { background-color: #ffffff; border-bottom: 1px solid #e4e4e0; }\n"
    ".schedule-nav label { color: #111; font-size: 14px; font-weight: 700; }\n"

    /* ── Schedule grid headers ── */
    ".schedule-header {"
    "   background-color: #e8e8e2; color: #111;"
    "   font-size: 12px; font-weight: 700; }\n"
    ".schedule-header label { color: #111; font-weight: 700; }\n"
    ".schedule-header-today {"
    "   background-color: #d4e3fc; color: #0a3a8f;"
    "   font-size: 12px; font-weight: 700; }\n"
    ".schedule-header-today label { color: #0a3a8f; font-weight: 700; }\n"
    ".schedule-slot-header {"
    "   background-color: #f0f0e8; color: #222;"
    "   font-size: 11px; font-weight: 700; }\n"
    ".schedule-slot-header label { color: #222; font-weight: 700; }\n"

    /* ── Schedule grid cells ── */
    ".sched-cell { border: 1px solid #dcdcd8; min-width: 110px; min-height: 80px; }\n"
    ".sched-cell-odd  { background-color: #ffffff; }\n"
    ".sched-cell-even { background-color: #f8f8f4; }\n"

    /* ── Deadline badges ── */
    ".deadline-overdue { color: #ffffff; background-color: #c0392b; border-radius: 100px; padding: 1px 7px; font-size: 11px; font-weight: 600; }\n"
    ".deadline-urgent  { color: #ffffff; background-color: #d35400; border-radius: 100px; padding: 1px 7px; font-size: 11px; font-weight: 600; }\n"
    ".deadline-warning { color: #4a2800; background-color: #faeeda; border-radius: 100px; padding: 1px 7px; font-size: 11px; font-weight: 600; }\n"
    ".deadline-normal  { color: #1a3d00; background-color: #eaf3de; border-radius: 100px; padding: 1px 7px; font-size: 11px; }\n"

    /* ── Schedule task cards ── */
    ".task-high   { border-left: 3px solid #c0392b; background-color: #fff5f5; border-radius: 4px; }\n"
    ".task-high   label { color: #111; }\n"
    ".task-medium { border-left: 3px solid #d35400; background-color: #fffbf2; border-radius: 4px; }\n"
    ".task-medium label { color: #111; }\n"
    ".task-low    { border-left: 3px solid #27ae60; background-color: #f5fbf5; border-radius: 4px; }\n"
    ".task-low    label { color: #111; }\n"

    /* ── Pomodoro timer label (large) ── */
    ".timer-display { font-size: 52px; font-weight: 500; color: #111111; letter-spacing: -2px; }\n"
    ".mode-display  { font-size: 13px; font-weight: 600; color: #444; letter-spacing: 0.06em; }\n"

    , -1, NULL);

    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

/* ================================================================
   Build app icon (SVG clock) programmatically
   ================================================================ */
static GdkPixbuf *build_app_icon(void) {
    /* Inline 48×48 SVG clock icon */
    static const char svg[] =
        "<svg xmlns='http://www.w3.org/2000/svg' width='48' height='48' viewBox='0 0 48 48'>"
        "<circle cx='24' cy='24' r='22' fill='#3d6b9e' stroke='white' stroke-width='2'/>"
        "<circle cx='24' cy='24' r='19' fill='#5a8fc8'/>"
        /* Hour hand */
        "<line x1='24' y1='24' x2='24' y2='10' stroke='white' stroke-width='3' stroke-linecap='round'/>"
        /* Minute hand */
        "<line x1='24' y1='24' x2='34' y2='24' stroke='white' stroke-width='2' stroke-linecap='round'/>"
        /* Center dot */
        "<circle cx='24' cy='24' r='2.5' fill='white'/>"
        /* Tick marks */
        "<line x1='24' y1='4'  x2='24' y2='8'  stroke='white' stroke-width='2'/>"
        "<line x1='24' y1='40' x2='24' y2='44' stroke='white' stroke-width='2'/>"
        "<line x1='4'  y1='24' x2='8'  y2='24' stroke='white' stroke-width='2'/>"
        "<line x1='40' y1='24' x2='44' y2='24' stroke='white' stroke-width='2'/>"
        "</svg>";

    GInputStream *stream = g_memory_input_stream_new_from_data(svg, sizeof(svg) - 1, NULL);
    GError *err = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_stream(stream, NULL, &err);
    g_object_unref(stream);
    if (err) {
        g_warning("Could not load app icon: %s", err->message);
        g_error_free(err);
    }
    return pb;
}

/* ================================================================
   App activate
   ================================================================ */
static void activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    load_css();
    fm_load();

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Time Manager");
    gtk_window_set_default_size(GTK_WINDOW(window), 960, 700);

    /* Set window icon */
    GdkPixbuf *icon = build_app_icon();
    if (icon) {
        gtk_window_set_icon(GTK_WINDOW(window), icon);
        g_object_unref(icon);
    }

    GtkWidget *notebook = gtk_notebook_new();
    gtk_container_add(GTK_CONTAINER(window), notebook);

    GtkWidget *task_tab = build_task_tab();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), task_tab,
                             gtk_label_new("📋 Công việc"));

    GtkWidget *pomo_tab = build_pomodoro_tab();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), pomo_tab,
                             gtk_label_new("🍅 Pomodoro"));

    GtkWidget *sched_tab = build_schedule_tab();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), sched_tab,
                             gtk_label_new("📅 Thời gian biểu"));

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