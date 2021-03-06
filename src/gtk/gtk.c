/* Synge-GTK: A graphical interface for Synge
 * Copyright (C) 2013, 2016 Aleksa Sarai
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define _DEFAULT_SOURCE

#include <synge.h>
#include <definitions.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <gtk/gtk.h>

#include <time.h>
#include <unistd.h>
#include <ctype.h>

#include "xmlui.h" /* generated header to bake the gtk_builder xml string */

#if defined(_WINDOWS)
#	define __EXPORT_SYMBOL __declspec(dllexport)
#else
#	define __EXPORT_SYMBOL
#endif /* _WINDOWS */

#define SYNGE_GTK_LICENSE	"Synge-GTK: A graphical interface for Synge\n" SYNGE_LICENSE

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

	struct synge_err ecode;

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
	char ch = *gtk_button_get_label(GTK_BUTTON(widget));

	if(isout && !iserr && isdigit(ch))
		gtk_entry_set_text(GTK_ENTRY(input), "");

	isout = iserr = false;

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
	struct synge_settings new = synge_get_settings();

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
	struct synge_func *tmp_function_list = synge_get_function_list();

	func_store = gtk_list_store_new(FUNCL_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_view_set_model(GTK_TREE_VIEW(func_tree), GTK_TREE_MODEL(func_store));

	cell = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(func_tree), -1, "Prototype",	 cell, "text", FUNCL_COLUMN_NAME, NULL);
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(func_tree), -1, "Description", cell, "text", FUNCL_COLUMN_DESCRIPTION, NULL);

	int i;
	for(i = 0; tmp_function_list[i].name != NULL; i++) {
		gtk_list_store_append(func_store, &iter);
#if defined(SYNGE_DEBUG)
		printf("Adding to function list: \"%s\" -> \"%s\"\n", tmp_function_list[i].prototype, tmp_function_list[i].description);
#endif /* SYNGE_DEBUG */
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

#if defined(SYNGE_DEBUG)
		g_print("selected expression is: %s\n", value);
		printf("'%s' => %d\n", value, (int) strlen(value));
#endif /* SYNGE_DEBUG */

		char *newstr = malloc((gtk_entry_get_text_length(GTK_ENTRY(input)) + strlen(value) + 1) * sizeof(char));
		sprintf(newstr, "%s%s", gtk_entry_get_text(GTK_ENTRY(input)), value);
		gtk_entry_set_text(GTK_ENTRY(input), newstr);

		free(newstr);
		g_free(value);
	}
} /* gui_add_function_to_expression() */

void gtk_default_settings(void) {
	struct synge_settings new = synge_get_settings();

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

#if defined(SYNGE_BAKE) && defined(SYNGE_GTK_XML_UI)
	gtk_builder_add_from_string(builder, SYNGE_GTK_XML_UI, -1, NULL);
#else
	if(!gtk_builder_add_from_file(builder, "synge-gtk.glade", NULL)) {
		puts("Couldn't open synge-gtk.glade");
		return 1;
	}
#endif /* SYNGE_BAKE && SYNGE_GIT_XML_UI */

	input = gtk_builder_get_object(builder, "input");
	output = gtk_builder_get_object(builder, "output");
	statusbar = gtk_builder_get_object(builder, "statusbar");
	func_window = gtk_builder_get_object(builder, "function_select");
	func_tree = gtk_builder_get_object(builder, "function_tree");

	gui_populate_function_list();

	if(!gtk_builder_get_object(builder, "basewindow")) {
		puts("Couldn't load base window");
		return 1;
	}

	window = GTK_WIDGET(gtk_builder_get_object(builder, "basewindow"));
	gtk_builder_connect_signals(builder, NULL);

#if defined(_UNIX)
	/* hint that the gui should be floating -- windows does stupid things with this hint */
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_type_hint(GTK_WINDOW(func_window), GDK_WINDOW_TYPE_HINT_DIALOG);
#endif /* _UNIX */

	struct synge_ver core = synge_get_version();
	char *template = "Core: %s\n"
					 "GUI: %s\n"
#if defined(SYNGE_REVISION)
					 "Revision: " SYNGE_REVISION;
#else
					 "Production Version";
#endif /* SYNGE_REVISION */
	int len = lenprintf(template, core.version, SYNGE_GTK_VERSION);
	char *comments = malloc(len);
	sprintf(comments, template, core.version, SYNGE_GTK_VERSION);

	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(gtk_builder_get_object(builder, "about_popup")), comments);
	gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(gtk_builder_get_object(builder, "about_popup")), SYNGE_GTK_LICENSE "\n\n" SYNGE_WARRANTY);
	free(comments);

	gtk_widget_show(GTK_WIDGET(window));
	gtk_main();
	g_object_unref(G_OBJECT(builder));

	synge_end();
	return 0;
}
