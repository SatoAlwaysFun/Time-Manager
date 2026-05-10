#include "pomodoro.h"
#include <gtk/gtk.h>
#include <stdio.h>

/* Default durations (minutes) – overridden by spinbuttons */
static int work_mins       = 25;
static int short_break_mins = 5;
static int long_break_mins  = 15;

#define CYCLES_BEFORE_LONG 4

typedef enum { MODE_WORK, MODE_SHORT_BREAK, MODE_LONG_BREAK } PomodoroMode;

static int          seconds_left  = 25 * 60;
static gboolean     is_running    = FALSE;
static int          cycle_count   = 0;
static PomodoroMode current_mode  = MODE_WORK;
static guint        timer_id      = 0;

static GtkWidget *g_timer_label  = NULL;
static GtkWidget *g_btn_start    = NULL;
static GtkWidget *g_btn_pause    = NULL;
static GtkWidget *g_btn_reset    = NULL;
static GtkWidget *g_mode_label   = NULL;
static GtkWidget *g_spin_work    = NULL;
static GtkWidget *g_spin_short   = NULL;
static GtkWidget *g_spin_long    = NULL;

/* ------------------------------------------------------------------ */
static void update_display(void) {
    if (!g_timer_label) return;
    int mins = seconds_left / 60;
    int secs = seconds_left % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
    gtk_label_set_text(GTK_LABEL(g_timer_label), buf);
}

static void update_mode_label(void) {
    if (!g_mode_label) return;
    switch (current_mode) {
        case MODE_WORK:        gtk_label_set_text(GTK_LABEL(g_mode_label), "LÀM VIỆC");   break;
        case MODE_SHORT_BREAK: gtk_label_set_text(GTK_LABEL(g_mode_label), "NGHỈ NGẮN");  break;
        case MODE_LONG_BREAK:  gtk_label_set_text(GTK_LABEL(g_mode_label), "NGHỈ DÀI");   break;
    }
}

static void advance_mode(void) {
    if (g_spin_work)  work_mins        = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(g_spin_work));
    if (g_spin_short) short_break_mins = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(g_spin_short));
    if (g_spin_long)  long_break_mins  = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(g_spin_long));

    if (current_mode == MODE_WORK) {
        cycle_count++;
        if (cycle_count % CYCLES_BEFORE_LONG == 0) {
            current_mode = MODE_LONG_BREAK;
            seconds_left = long_break_mins * 60;
        } else {
            current_mode = MODE_SHORT_BREAK;
            seconds_left = short_break_mins * 60;
        }
    } else {
        current_mode = MODE_WORK;
        seconds_left = work_mins * 60;
    }
    update_mode_label();
    update_display();
}

static gboolean tick(gpointer data) {
    (void)data;
    if (!is_running) return G_SOURCE_REMOVE;

    if (seconds_left > 0) {
        seconds_left--;
        update_display();
        return G_SOURCE_CONTINUE;
    }

    is_running = FALSE;
    timer_id   = 0;
    advance_mode();

    is_running = TRUE;
    timer_id   = g_timeout_add(1000, tick, NULL);
    return G_SOURCE_REMOVE;
}

/* ------------------------------------------------------------------ */
static void on_start_clicked(GtkWidget *w, gpointer d) {
    (void)w; (void)d;
    if (!is_running) {
        is_running = TRUE;
        timer_id   = g_timeout_add(1000, tick, NULL);
    }
}

static void on_pause_clicked(GtkWidget *w, gpointer d) {
    (void)w; (void)d;
    if (is_running) {
        is_running = FALSE;
        if (timer_id) { g_source_remove(timer_id); timer_id = 0; }
    }
}

static void on_reset_clicked(GtkWidget *w, gpointer d) {
    (void)w; (void)d;
    if (g_spin_work) work_mins = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(g_spin_work));
    is_running   = FALSE;
    if (timer_id) { g_source_remove(timer_id); timer_id = 0; }
    current_mode = MODE_WORK;
    cycle_count  = 0;
    seconds_left = work_mins * 60;
    update_mode_label();
    update_display();
}

static void on_spin_work_changed(GtkSpinButton *sb, gpointer d) {
    (void)d;
    work_mins = (int)gtk_spin_button_get_value(sb);
    if (!is_running && current_mode == MODE_WORK) {
        seconds_left = work_mins * 60;
        update_display();
    }
}

static void on_spin_short_changed(GtkSpinButton *sb, gpointer d) {
    (void)d;
    short_break_mins = (int)gtk_spin_button_get_value(sb);
    if (!is_running && current_mode == MODE_SHORT_BREAK) {
        seconds_left = short_break_mins * 60;
        update_display();
    }
}

static void on_spin_long_changed(GtkSpinButton *sb, gpointer d) {
    (void)d;
    long_break_mins = (int)gtk_spin_button_get_value(sb);
    if (!is_running && current_mode == MODE_LONG_BREAK) {
        seconds_left = long_break_mins * 60;
        update_display();
    }
}

/* ------------------------------------------------------------------ */
void pomodoro_attach(GtkWidget *timer_label,
                     GtkWidget *btn_start,
                     GtkWidget *btn_pause,
                     GtkWidget *btn_reset,
                     GtkWidget *mode_label,
                     GtkWidget *spin_work,
                     GtkWidget *spin_short,
                     GtkWidget *spin_long)
{
    g_timer_label = timer_label;
    g_btn_start   = btn_start;
    g_btn_pause   = btn_pause;
    g_btn_reset   = btn_reset;
    g_mode_label  = mode_label;
    g_spin_work   = spin_work;
    g_spin_short  = spin_short;
    g_spin_long   = spin_long;

    g_signal_connect(btn_start,  "clicked",       G_CALLBACK(on_start_clicked),    NULL);
    g_signal_connect(btn_pause,  "clicked",       G_CALLBACK(on_pause_clicked),    NULL);
    g_signal_connect(btn_reset,  "clicked",       G_CALLBACK(on_reset_clicked),    NULL);
    g_signal_connect(spin_work,  "value-changed", G_CALLBACK(on_spin_work_changed),  NULL);
    g_signal_connect(spin_short, "value-changed", G_CALLBACK(on_spin_short_changed), NULL);
    g_signal_connect(spin_long,  "value-changed", G_CALLBACK(on_spin_long_changed),  NULL);

    update_mode_label();
    update_display();
}