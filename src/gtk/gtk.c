/* Synge-GTK: A GTK+ wrapper for Synge
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

#define _BSD_SOURCE

#include <synge.h>
#include <definitions.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <gtk/gtk.h>

#include <time.h>
#include <unistd.h>

#include "xmlui.h" /* generated header to bake the gtk_builder xml string */

#ifdef _WINDOWS
#	define __EXPORT_SYMBOL __declspec(dllexport)
#else
#	define __EXPORT_SYMBOL
#endif

#define GUI_MAX_PRECISION 10
#define lenprintf(format, ...) (snprintf(NULL, 0, format, __VA_ARGS__))

enum {
	FUNCL_COLUMN_NAME,
	FUNCL_COLUMN_DESCRIPTION,
	FUNCL_N_COLUMNS
};

static GObject *input, *output, *statusbar, *func_window, *func_tree;
static GtkBuilder *builder;
static GtkWidget *window;

static int isout = false;
static int iserr = false;

__EXPORT_SYMBOL gboolean kill_window(GtkWidget *widget, GdkEvent *event, gpointer data) {
	gtk_main_quit();
	return FALSE;
} /* kill_window() */

__EXPORT_SYMBOL void gui_compute_string(GtkWidget *widget, gpointer data) {
	synge_t result;
	mpfr_init2(result, SYNGE_PRECISION);

	error_code ecode;

	char *text = (char *) gtk_entry_get_text(GTK_ENTRY(input));

	isout = true;
	iserr = false;

	if((ecode = synge_compute_string(text, &result)).code != SUCCESS && !synge_is_ignore_code(ecode.code)) {
		gtk_label_set_selectable(GTK_LABEL(output), FALSE);

		char *markup = g_markup_printf_escaped("<b><span color=\"%s\">%s</span></b>", synge_is_success_code(ecode.code) ? "#008800" : "red", synge_error_msg(ecode));
		gtk_label_set_markup(GTK_LABEL(statusbar), markup);
		gtk_label_set_text(GTK_LABEL(output), "");

		g_free(markup);

		iserr = !synge_is_success_code(ecode.code);
	}
	else if(!synge_is_ignore_code(ecode.code)) {
		gtk_label_set_selectable(GTK_LABEL(output), TRUE);

		int precision = synge_get_precision(result);
		if(precision > GUI_MAX_PRECISION)
			precision = GUI_MAX_PRECISION;

		char *outputs = malloc((synge_snprintf(NULL, 0, "%.*" SYNGE_FORMAT, precision, result) + 1) * sizeof(char));
		synge_sprintf(outputs, "%.*" SYNGE_FORMAT, precision, result);

		char *markup = g_markup_printf_escaped("<b>%s</b>", outputs);
		gtk_label_set_markup(GTK_LABEL(output), markup);
		gtk_label_set_text(GTK_LABEL(statusbar), "");

		free(outputs);
		g_free(markup);
	}
	else {
		gtk_label_set_text(GTK_LABEL(output), "");
		gtk_label_set_text(GTK_LABEL(statusbar), "");
	}

	mpfr_clears(result, NULL);
} /* gui_compute_string() */

__EXPORT_SYMBOL void gui_append_key(GtkWidget *widget, gpointer data) {
	if(isout && !iserr) {
		gtk_entry_set_text(GTK_ENTRY(input), "");
		isout = iserr = false;
	}

	char *newstr = malloc((gtk_entry_get_text_length(GTK_ENTRY(input)) + strlen(gtk_button_get_label(GTK_BUTTON(widget))) + 1) * sizeof(char));
	sprintf(newstr, "%s%s", gtk_entry_get_text(GTK_ENTRY(input)), gtk_button_get_label(GTK_BUTTON(widget)));

	gtk_entry_set_text(GTK_ENTRY(input), newstr);
	free(newstr);
} /* gui_append_key() */

__EXPORT_SYMBOL void gui_backspace_string(GtkWidget *widget, gpointer data) {
	if(gtk_entry_get_text_length(GTK_ENTRY(input)) == 0) return;

	char *newstr = malloc((gtk_entry_get_text_length(GTK_ENTRY(input)) + 1) * sizeof(char));
	strcpy(newstr, gtk_entry_get_text(GTK_ENTRY(input)));
	newstr[strlen(newstr)-1] = '\0';
	gtk_entry_set_text(GTK_ENTRY(input), newstr);
	free(newstr);

	isout = false;
} /* gui_backspace_string() */

__EXPORT_SYMBOL void gui_clear_string(GtkWidget *widget, gpointer data) {
	gtk_entry_set_text(GTK_ENTRY(input), "");
	gtk_label_set_text(GTK_LABEL(output), "");
	gtk_label_set_text(GTK_LABEL(statusbar), "");

	isout = false;
} /* gui_clear_string() */

__EXPORT_SYMBOL void gui_about_popup(GtkWidget *widget, gpointer data) {
	gtk_dialog_run(GTK_DIALOG(gtk_builder_get_object(builder, "about_popup")));
	gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(builder, "about_popup")));
} /* gui_about_popup() */

__EXPORT_SYMBOL void gui_toggle_mode(GtkWidget *widget, gpointer data) {
	synge_settings new = synge_get_settings();

	if(!strcasecmp(gtk_button_get_label(GTK_BUTTON(widget)), "degrees"))
		new.mode = degrees;
	else if(!strcasecmp(gtk_button_get_label(GTK_BUTTON(widget)), "radians"))
		new.mode = radians;
	else if(!strcasecmp(gtk_button_get_label(GTK_BUTTON(widget)), "gradians"))
		new.mode = gradians;

	synge_set_settings(new);
} /* gui_toggle_mode() */

__EXPORT_SYMBOL void gui_open_function_list(GtkWidget *widget, gpointer data) {
	gtk_widget_show_all(GTK_WIDGET(func_window));
} /* gui_open_function_list() */

__EXPORT_SYMBOL void gui_close_function_list(GtkWidget *widget, gpointer data) {
	gtk_widget_hide(GTK_WIDGET(func_window));
} /* gui_close_function_list() */

__EXPORT_SYMBOL void gui_populate_function_list(void) {
	GtkTreeIter iter;
	GtkListStore *func_store;
	GtkCellRenderer *cell;
	function *tmp_function_list = synge_get_function_list();

	func_store = gtk_list_store_new(FUNCL_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_view_set_model(GTK_TREE_VIEW(func_tree), GTK_TREE_MODEL(func_store));

	cell = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(func_tree), -1, "Prototype",	 cell, "text", FUNCL_COLUMN_NAME, NULL);
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(func_tree), -1, "Description", cell, "text", FUNCL_COLUMN_DESCRIPTION, NULL);

	int i;
	for(i = 0; tmp_function_list[i].name != NULL; i++) {
		gtk_list_store_append(func_store, &iter);
#ifdef DEBUG
		printf("Adding to function list: \"%s\" -> \"%s\"\n", tmp_function_list[i].prototype, tmp_function_list[i].description);
#endif
		gtk_list_store_set(func_store, &iter,
				FUNCL_COLUMN_NAME, tmp_function_list[i].prototype,
				FUNCL_COLUMN_DESCRIPTION, tmp_function_list[i].description,
				-1);
	}
} /* gui_populate_function_list() */

__EXPORT_SYMBOL void gui_add_function_to_expression(GtkWidget *widget, gpointer data) {
	GtkTreeIter tmpiter;
	GtkTreeSelection *func_select = gtk_tree_view_get_selection(GTK_TREE_VIEW(func_tree));

	if(gtk_tree_selection_get_selected(GTK_TREE_SELECTION(func_select), NULL, &tmpiter)) {
		GtkTreeModel *func_model = gtk_tree_view_get_model(GTK_TREE_VIEW(func_tree));

		gchar *value = NULL;
		gtk_tree_model_get(func_model, &tmpiter, FUNCL_COLUMN_NAME, &value, -1);

		if(strchr(value, '('))
			*(strchr(value, '(') + 1) = '\0';

#ifdef DEBUG
		g_print("selected expression is: %s\n", value);
		printf("'%s' => %d\n", value, (int) strlen(value));
#endif

		char *newstr = malloc((gtk_entry_get_text_length(GTK_ENTRY(input)) + strlen(value) + 1) * sizeof(char));
		sprintf(newstr, "%s%s", gtk_entry_get_text(GTK_ENTRY(input)), value);
		gtk_entry_set_text(GTK_ENTRY(input), newstr);

		free(newstr);
		g_free(value);
	}
} /* gui_populate_function_list() */

void gtk_default_settings(void) {
	synge_settings new = synge_get_settings();

	new.error = simple;
	new.strict = flexible;

	synge_set_settings(new);
} /* gtk_default_settings() */

int main(int argc, char **argv) {
	synge_start();
	synge_seed(time(NULL) ^ getpid());
	gtk_default_settings();

	gtk_init(&argc, &argv);

	builder = gtk_builder_new();

#ifdef SYNGE_GTK_XML_UI
	gtk_builder_add_from_string(builder, SYNGE_GTK_XML_UI, -1, NULL);
#else
	if(!gtk_builder_add_from_file(builder, "synge-gtk.glade", NULL)) {
		printf("Couldn't open synge-gtk.glade\n");
		exit(0);
	}
#endif

	input = gtk_builder_get_object(builder, "input");
	output = gtk_builder_get_object(builder, "output");
	statusbar = gtk_builder_get_object(builder, "statusbar");
	func_window = gtk_builder_get_object(builder, "function_select");
	func_tree = gtk_builder_get_object(builder, "function_tree");

	gui_populate_function_list();

	if(!gtk_builder_get_object(builder, "basewindow")) {
		printf("Couldn't load base window\n");
		exit(0);
	}

	window = GTK_WIDGET(gtk_builder_get_object(builder, "basewindow"));
	gtk_builder_connect_signals(builder, NULL);

#ifdef _UNIX
	/* hint that the gui should be floating -- windows does stupid things with this hint */
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_type_hint(GTK_WINDOW(func_window), GDK_WINDOW_TYPE_HINT_DIALOG);
#endif

	synge_v core = synge_get_version();
	char *comments = NULL;
	int len = lenprintf("Core: %s\nGUI: %s", core.version, SYNGE_GTK_VERSION);

	if(strlen(SYNGE_GIT_VERSION) == 40) {
		/* add git revision */
		len += lenprintf("\nRevision: %s", SYNGE_GIT_VERSION);

		/* format the version information */
		comments = malloc(len + 1);
		sprintf(comments, "Core: %s\nGUI: %s\nRevision: %s", core.version, SYNGE_GTK_VERSION, SYNGE_GIT_VERSION);
	} else {
		/* format the version information */
		comments = malloc(len + 1);
		sprintf(comments, "Core: %s\nGUI: %s", core.version, SYNGE_GTK_VERSION);
	}

	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(gtk_builder_get_object(builder, "about_popup")), comments);
	gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(gtk_builder_get_object(builder, "about_popup")), SYNGE_GTK_LICENSE "\n" SYNGE_WARRANTY);
	free(comments);

	gtk_widget_show(GTK_WIDGET(window));
	gtk_main();
	g_object_unref(G_OBJECT(builder));

	synge_end();
	return 0;
}
