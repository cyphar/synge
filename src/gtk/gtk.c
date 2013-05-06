/* Synge-Gui: A GTK+ gui wrapper for Synge
 * Copyright (C) 2013 Cyphar
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * 1. The above copyright notice and this permission notice shall be included in
 *    all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include <stack.h>
#include <synge.h>
#include <definitions.h>

#ifndef __SYNGE_GTK_VERSION__
#define __SYNGE_GTK_VERSION__ ""
#endif

static GObject *input, *output;
GtkBuilder *builder;
GtkWidget *window;

gboolean kill_window(GtkWidget *widget, GdkEvent *event, gpointer data) {
	gtk_main_quit();
	return FALSE;
} /* kill_window() */

void gui_compute_string(GtkWidget *widget, gpointer data) {
	double result;
	error_code ecode;

	char *text = (char *) gtk_entry_get_text(GTK_ENTRY(input));

	if((ecode = compute_infix_string(text, &result)) != SUCCESS) {
		gtk_label_set_selectable(GTK_LABEL(output), FALSE);

		char *markup = g_markup_printf_escaped("<b><span color=\"red\">%s</span></b>", get_error_msg(ecode));

		gtk_label_set_markup(GTK_LABEL(output), markup);
		g_free(markup);
	} else {
		gtk_label_set_selectable(GTK_LABEL(output), TRUE);

		char *outputs = malloc((snprintf(NULL, 0, "= %.*f", get_precision(result), result) + 1) * sizeof(char));
		sprintf(outputs, "= %.*f", get_precision(result), result);

		char *markup = g_markup_printf_escaped("<b>%s</b>", outputs);
		gtk_label_set_markup(GTK_LABEL(output), markup);

		free(outputs);
		g_free(markup);
	}
} /* gui_compute_string() */

void gui_append_key(GtkWidget *widget, gpointer data) {
	char *newstr = malloc((gtk_entry_get_text_length(GTK_ENTRY(input)) + strlen(gtk_button_get_label(GTK_BUTTON(widget))) + 1) * sizeof(char));
	sprintf(newstr, "%s%s", gtk_entry_get_text(GTK_ENTRY(input)), gtk_button_get_label(GTK_BUTTON(widget)));

	gtk_entry_set_text(GTK_ENTRY(input), newstr);
	free(newstr);
} /* gui_append_key() */

void gui_clear_string(GtkWidget *widget, gpointer data) {
	gtk_entry_set_text(GTK_ENTRY(input), "");
	gtk_label_set_text(GTK_LABEL(output), "");
} /* gui_clear_string() */

/* stop compile-time warnings for gui functions not being used */
void gui_fake_usage(int a, ...) {
	if(a || !a) return;
} /* gui_fake_usage() */

int main(int argc, char **argv) {
	gtk_init(&argc, &argv);

	builder = gtk_builder_new();
	if(!gtk_builder_add_from_file(builder, "gtk.glade", NULL)) {
		printf("Couldn't open gtk.glade");
		exit(0);
	}

	input = gtk_builder_get_object(builder, "input");
	output = gtk_builder_get_object(builder, "output");

	if(!gtk_builder_get_object(builder, "basewindow")) {
		printf("Couldn't load base window\n");
		exit(0);
	}

	window = GTK_WIDGET(gtk_builder_get_object(builder, "basewindow"));
	gtk_builder_connect_signals(builder, NULL);
	g_object_unref(G_OBJECT(builder));

	gtk_widget_show(GTK_WIDGET(window));
	gtk_main();
	gui_fake_usage(-1, gui_clear_string, gui_append_key, gui_compute_string, kill_window);

	return 0;
}
