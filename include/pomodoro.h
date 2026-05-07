#ifndef POMODORO_H
#define POMODORO_H

#include <gtk/gtk.h>

void pomodoro_attach(GtkWidget *timer_label,
                     GtkWidget *btn_start,
                     GtkWidget *btn_pause,
                     GtkWidget *btn_reset,
                     GtkWidget *mode_label);

#endif /* POMODORO_H */