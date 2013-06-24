/*
 * Copyright (C) 2006-2013 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wshift-sign-overflow"
#include <gtk/gtk.h>
#include "dispatch.h"

#define HAYSTACK_SIZE 128 * 1024 * 1024
#define HAYSTACK_MIN   64 * 1024 * 1024
#define HAYSTACK_MAX HAYSTACK_SIZE

#define INTERVAL_MIN 500
#define INTERVAL_MAX 1500


GtkWidget *window;
GtkWidget *button;
static char *haystack;
static dispatch_queue_t queue;
GRand *size_generator;
GRand *interval_generator;


static void do_work(GtkWidget *widget, gpointer data)
{
	(void)widget;
	(void)data;
	
	gint32 size = g_rand_int_range(size_generator, HAYSTACK_MIN, HAYSTACK_MAX);
	double metrics[] = { size };
	atlas_job_t job = {
		.deadline = atlas_now() + 0.1,
		.metrics_count = 1,
		.metrics = metrics
	};
	dispatch_async_atlas(queue, job, ^{
		char remember = haystack[size];
		haystack[size] = '\0';
		if (strcasestr(haystack, "test"))
			puts("found");
		haystack[size] = remember;
	});
}

static gboolean clicker(gpointer data)
{
	(void)data;
	gtk_button_clicked(GTK_BUTTON(button));
	
	guint interval = (guint)g_rand_int_range(interval_generator, INTERVAL_MIN, INTERVAL_MAX);
	g_timeout_add(interval, clicker, NULL);
	return FALSE;
}

static void destroy(GtkWidget *widget, gpointer data)
{
	(void)widget;
	(void)data;
	gtk_main_quit();
	dispatch_sync(queue, ^{});
	dispatch_release(queue);
	g_rand_free(size_generator);
	g_rand_free(interval_generator);
	free(haystack);
}

static void handler(int signal)
{
	(void)signal;
	gtk_widget_destroy(window);
}


int main (int argc, char *argv[])
{
	gtk_init(&argc, &argv);
	size_generator = g_rand_new_with_seed(0);
	interval_generator = g_rand_new_with_seed(1);
	queue = dispatch_queue_create("worker", DISPATCH_QUEUE_SERIAL);
	haystack = malloc(HAYSTACK_SIZE);
	memset(haystack, 'A', HAYSTACK_SIZE);
	haystack[HAYSTACK_SIZE - 1] = '\0';
	
	button = gtk_button_new_with_label("Do Work");
	g_signal_connect(button, "clicked", G_CALLBACK(do_work), NULL);
	
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "ATLAS Worker");
	gtk_container_set_border_width(GTK_CONTAINER(window), 10);
	gtk_container_add(GTK_CONTAINER(window), button);
	g_signal_connect(window, "destroy", G_CALLBACK(destroy), NULL);
	gtk_widget_show(button);
	gtk_widget_show(window);
	
	gint width, height;
	gtk_window_get_size(GTK_WINDOW(window), &width, &height);
	gtk_window_resize(GTK_WINDOW(window), width + 150, height + 20);
	
	guint interval = (guint)g_rand_int_range(interval_generator, INTERVAL_MIN, INTERVAL_MAX);
	g_timeout_add(interval, clicker, NULL);
        struct sigaction action = {
                .sa_handler = handler
        };
        sigaction(SIGTERM, &action, NULL);
	
	gtk_main();
	return 0;
}
