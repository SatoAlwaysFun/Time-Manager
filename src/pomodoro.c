#include "pomodoro.h"
#include <gtk/gtk.h>

static int seconds_left = 1500; // 25 phút
static gboolean is_running = FALSE;

// Hàm này sẽ được gọi mỗi 1000ms (1 giây)
gboolean update_timer_label(gpointer data) {
    GtkLabel *label = GTK_LABEL(data);
    
    if (seconds_left > 0 && is_running) {
        seconds_left--;
        int mins = seconds_left / 60;
        int secs = seconds_left % 60;
        char buffer[10];
        sprintf(buffer, "%02d:%02d", mins, secs);
        gtk_label_set_text(label, buffer);
        return TRUE; // Tiếp tục gọi hàm này sau 1 giây nữa
    }
    
    is_running = FALSE;
    return FALSE; // Dừng timer
}

void start_pomodoro_timer(GtkWidget *label) {
    if (!is_running) {
        is_running = TRUE;
        g_timeout_add(1000, update_timer_label, label);
    }
}