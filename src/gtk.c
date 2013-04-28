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

#include "stack.h"
#include "synge.h"

#ifndef __SYNGE_GTK_VERSION__
#define __SYNGE_GTK_VERSION__ ""
#endif

static GtkWidget *input_entry, *output_label;

static gboolean kill_window(GtkWidget *widget, GdkEvent *event, gpointer data) {
	gtk_main_quit();
	return FALSE;
} /* kill_window() */

static void gui_compute_string (GtkWidget *widget, gpointer data) {
	double result;
	error_code ecode;
	char *text = (char *) gtk_entry_get_text(GTK_ENTRY(input_entry));

	if((ecode = compute_infix_string(text, &result)) != SUCCESS) {
		gtk_label_set_selectable(GTK_LABEL(output_label), FALSE);
		char *markup = g_markup_printf_escaped("<b><span color=\"red\">%s</span></b>", get_error_msg(ecode));
		gtk_label_set_markup(GTK_LABEL(output_label), markup);
		g_free(markup);
	} else {
		gtk_label_set_selectable(GTK_LABEL(output_label), TRUE);
		char *output = malloc((snprintf(NULL, 0, "= %.*f", get_precision(result), result) + 1) * sizeof(char));
		sprintf(output, "= %.*f", get_precision(result), result);
		char *markup = g_markup_printf_escaped("<b>%s</b>", output);
		gtk_label_set_markup(GTK_LABEL(output_label), markup);
		free(output);
		g_free(markup);
	}
} /* gui_compute_string() */

static void gui_append_key(GtkWidget *widget, gpointer data) {
	char *newstr = malloc((gtk_entry_get_text_length(GTK_ENTRY(input_entry)) + strlen(gtk_button_get_label(GTK_BUTTON(widget))) + 1) * sizeof(char));
	sprintf(newstr, "%s%s", gtk_entry_get_text(GTK_ENTRY(input_entry)), gtk_button_get_label(GTK_BUTTON(widget)));
	gtk_entry_set_text(GTK_ENTRY(input_entry), newstr);
	free(newstr);
} /* gui_append_key() */

static void gui_clear_string(GtkWidget *widget, gpointer data) {
	gtk_entry_set_text(GTK_ENTRY(input_entry), "");
	gtk_label_set_text(GTK_LABEL(output_label), "");
} /* gui_clear_string() */

int main(int argc, char **argv) {
	gtk_init(&argc, &argv);

	GtkWidget *window, *window_vbox;
	GtkWidget *io_vbox;
	GtkWidget *keypad_grid,
		  *plus, *minus, *multiply, *divide,/* *func,*/
		  *exp, *keypad7, *keypad8, *keypad9, *modulo,
		  *lparen, *keypad4, *keypad5, *keypad6, *clear,
		  *rparen, *keypad1, *keypad2, *keypad3, *equals,
		  *keypad0, *decimal;

	window		= gtk_window_new(GTK_WINDOW_TOPLEVEL);
	window_vbox	= gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_window_set_title(GTK_WINDOW(window), "Synge Calculator GTK+");
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	gtk_widget_set_size_request(window, 380, 400);

	io_vbox		= gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	input_entry	= gtk_entry_new();
	output_label	= gtk_label_new("");
	gtk_entry_set_placeholder_text(GTK_ENTRY(input_entry), "1+1");
	gtk_entry_set_alignment(GTK_ENTRY(input_entry), 1);
	gtk_entry_set_has_frame(GTK_ENTRY(input_entry), TRUE);
	gtk_label_set_line_wrap(GTK_LABEL(output_label), FALSE);
	gtk_misc_set_alignment(GTK_MISC(output_label), 1.0f, 0.5f);

	gtk_box_pack_start(GTK_BOX(io_vbox), input_entry, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(io_vbox), output_label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(window_vbox), io_vbox, FALSE, TRUE, 2);

	/* KEYPAD
	 * 1 2 3 4 5
	 * | | | | |
	 * ( ) ^ % f -- 1
	 * 7 8 9 / C -- 2
	 * 4 5 6 * = -- 3
	 * 1 2 3 - = -- 4
	 * 000 . + = -- 5
	 */

	keypad_grid	= gtk_table_new(5, 5, TRUE);
	equals		= gtk_button_new_with_label("=");
	plus		= gtk_button_new_with_label("+");
	minus		= gtk_button_new_with_label("-");
	multiply	= gtk_button_new_with_label("*");
	divide		= gtk_button_new_with_label("/");
	modulo		= gtk_button_new_with_label("%");
	exp		= gtk_button_new_with_label("^");
	lparen		= gtk_button_new_with_label("(");
	rparen		= gtk_button_new_with_label(")");
	clear		= gtk_button_new_with_label("C");
	/*func		= gtk_button_new_with_label("f(x)");*/
	decimal		= gtk_button_new_with_label(".");
	keypad0		= gtk_button_new_with_label("0");
	keypad1		= gtk_button_new_with_label("1");
	keypad2		= gtk_button_new_with_label("2");
	keypad3		= gtk_button_new_with_label("3");
	keypad4		= gtk_button_new_with_label("4");
	keypad5		= gtk_button_new_with_label("5");
	keypad6		= gtk_button_new_with_label("6");
	keypad7		= gtk_button_new_with_label("7");
	keypad8		= gtk_button_new_with_label("8");
	keypad9		= gtk_button_new_with_label("9");

	gtk_table_attach_defaults(GTK_TABLE(keypad_grid), lparen,	0, 1, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(keypad_grid), rparen,	1, 2, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(keypad_grid), exp,		2, 3, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(keypad_grid), modulo,	3, 4, 0, 1);
	/*gtk_table_attach_defaults(GTK_TABLE(keypad_grid), func,		4, 5, 0, 1);*/

	gtk_table_attach_defaults(GTK_TABLE(keypad_grid), keypad7,	0, 1, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(keypad_grid), keypad8,	1, 2, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(keypad_grid), keypad9,	2, 3, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(keypad_grid), divide,	3, 4, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(keypad_grid), clear,	4, 5, 1, 2);

	gtk_table_attach_defaults(GTK_TABLE(keypad_grid), keypad4,	0, 1, 2, 3);
	gtk_table_attach_defaults(GTK_TABLE(keypad_grid), keypad5,	1, 2, 2, 3);
	gtk_table_attach_defaults(GTK_TABLE(keypad_grid), keypad6,	2, 3, 2, 3);
	gtk_table_attach_defaults(GTK_TABLE(keypad_grid), multiply,	3, 4, 2, 3);
	gtk_table_attach_defaults(GTK_TABLE(keypad_grid), equals,	4, 5, 2, 5);

	gtk_table_attach_defaults(GTK_TABLE(keypad_grid), keypad1,	0, 1, 3, 4);
	gtk_table_attach_defaults(GTK_TABLE(keypad_grid), keypad2,	1, 2, 3, 4);
	gtk_table_attach_defaults(GTK_TABLE(keypad_grid), keypad3,	2, 3, 3, 4);
	gtk_table_attach_defaults(GTK_TABLE(keypad_grid), minus,	3, 4, 3, 4);

	gtk_table_attach_defaults(GTK_TABLE(keypad_grid), keypad0,	0, 2, 4, 5);
	gtk_table_attach_defaults(GTK_TABLE(keypad_grid), decimal,	2, 3, 4, 5);
	gtk_table_attach_defaults(GTK_TABLE(keypad_grid), plus,		3, 4, 4, 5);

	gtk_box_pack_start(GTK_BOX(window_vbox), keypad_grid, TRUE, TRUE, 2);

	gtk_container_add(GTK_CONTAINER(window), window_vbox);
	gtk_container_set_border_width(GTK_CONTAINER(window), 10);

	g_signal_connect(window, "delete_event", G_CALLBACK(kill_window), NULL);

	g_signal_connect(window, "delete_event", G_CALLBACK(kill_window), NULL);

	g_signal_connect(keypad0,  "clicked", G_CALLBACK(gui_append_key), NULL);
	g_signal_connect(keypad1,  "clicked", G_CALLBACK(gui_append_key), NULL);
	g_signal_connect(keypad2,  "clicked", G_CALLBACK(gui_append_key), NULL);
	g_signal_connect(keypad3,  "clicked", G_CALLBACK(gui_append_key), NULL);
	g_signal_connect(keypad4,  "clicked", G_CALLBACK(gui_append_key), NULL);
	g_signal_connect(keypad5,  "clicked", G_CALLBACK(gui_append_key), NULL);
	g_signal_connect(keypad6,  "clicked", G_CALLBACK(gui_append_key), NULL);
	g_signal_connect(keypad7,  "clicked", G_CALLBACK(gui_append_key), NULL);
	g_signal_connect(keypad8,  "clicked", G_CALLBACK(gui_append_key), NULL);
	g_signal_connect(keypad9,  "clicked", G_CALLBACK(gui_append_key), NULL);
	g_signal_connect(plus,	   "clicked", G_CALLBACK(gui_append_key), NULL);
	g_signal_connect(minus,	   "clicked", G_CALLBACK(gui_append_key), NULL);
	g_signal_connect(multiply, "clicked", G_CALLBACK(gui_append_key), NULL);
	g_signal_connect(divide,   "clicked", G_CALLBACK(gui_append_key), NULL);
	g_signal_connect(modulo,   "clicked", G_CALLBACK(gui_append_key), NULL);
	g_signal_connect(decimal,  "clicked", G_CALLBACK(gui_append_key), NULL);
	g_signal_connect(exp,	   "clicked", G_CALLBACK(gui_append_key), NULL);
	g_signal_connect(lparen,   "clicked", G_CALLBACK(gui_append_key), NULL);
	g_signal_connect(rparen,   "clicked", G_CALLBACK(gui_append_key), NULL);

	g_signal_connect(clear,    "clicked", G_CALLBACK(gui_clear_string), NULL);
	g_signal_connect(equals,   "clicked", G_CALLBACK(gui_compute_string), NULL);
	g_signal_connect(input_entry, "activate", G_CALLBACK(gui_compute_string), NULL);

	gtk_widget_show_all(window);
	gtk_main();
	return 0;
}
