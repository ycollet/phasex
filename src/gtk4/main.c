/*****************************************************************************
 *
 * gtk4/main.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Entry point for the experimental GTK4 preview (PHASEX_GTK4=ON).  Builds
 * just enough of a real window to compare against the GTK2 GUI_DEBUG
 * baseline (gtk2_debug_dump.txt): the "PatchGroup" navbar frame, styled via
 * theme-dark.css.  Not the full app yet -- see src/gtk4/navbar.c for what's
 * in scope for this pass.
 *
 * PHASEX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PHASEX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PHASEX.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/
#include <gtk/gtk.h>
#include "navbar.h"
#include "paramgroup.h"
#include "gui_debug4.h"


#ifndef PHASEX_GTK4_CSS_DIR
# define PHASEX_GTK4_CSS_DIR "."
#endif


static void
activate(GtkApplication *app, gpointer UNUSED_data) {
    GtkWidget           *window;
    GtkWidget           *vbox;
    GtkCssProvider      *css;
    char                css_path[1024];

    (void) UNUSED_data;

    css = gtk_css_provider_new();
    snprintf(css_path, sizeof(css_path), "%s/theme-dark.css", PHASEX_GTK4_CSS_DIR);
    gtk_css_provider_load_from_path(css, css_path);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
            GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "phasex (GTK4 preview)");

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(vbox), create_navbar());
    gtk_box_append(GTK_BOX(vbox), create_lfo1_group());
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    gtk_window_set_default_size(GTK_WINDOW(window), 969, 500);
    gtk_window_present(GTK_WINDOW(window));

    gui_debug4_dump_window(window);

    if (g_getenv("PHASEX_GTK4_DUMP_AND_EXIT") != NULL) {
        g_application_quit(G_APPLICATION(app));
    }
}


int
main(int argc, char **argv) {
    GtkApplication  *app;
    int             status;

    app = gtk_application_new("org.phasex.gtk4preview", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
