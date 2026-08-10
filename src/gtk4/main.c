/*****************************************************************************
 *
 * gtk4/main.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Entry point for the experimental GTK4 preview (PHASEX_GTK4=ON).  Builds
 * a real window with the menubar, the "PatchGroup" navbar frame, and
 * every real param group (see param_groups_data.c), styled via
 * theme-dark.css.  Calls phasex_gtk4_backend_init() first to bring up
 * the real session/bank/patch backend (see backend_init.c) before
 * building any widgets, so the navbar and param groups can read and
 * drive real state.
 *
 * Param groups are packed into a GtkFlowBox inside a scrolled window
 * rather than the real app's notebook/one-page/widescreen multi-column
 * layouts (gui_layout.c's create_param_notebook()/_one_page()/
 * _widescreen()) -- those depend on setting_window_layout, which isn't
 * wired to anything real yet (see menubar.c's View menu). A flow box
 * that wraps groups left-to-right, as many as fit per row, is a
 * reasonable stand-in for "some multi-column layout" without picking
 * one of the three prematurely.
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
#include "menubar.h"
#include "paramgroup.h"
#include "gui_debug4.h"
#include "backend_init.h"
#include "session.h"
#include "patch.h"
#include "bank.h"
#include "gui_layout.h"


#ifndef PHASEX_GTK4_CSS_DIR
# define PHASEX_GTK4_CSS_DIR "."
#endif


static void
activate(GtkApplication *app, gpointer UNUSED_data) {
    GtkWidget           *window;
    GtkWidget           *vbox;
    GtkWidget           *scroller;
    GtkWidget           *flowbox;
    GtkCssProvider      *css;
    char                css_path[1024];
    int                 i;

    (void) UNUSED_data;

    css = gtk_css_provider_new();
    snprintf(css_path, sizeof(css_path), "%s/theme-dark.css", PHASEX_GTK4_CSS_DIR);
    gtk_css_provider_load_from_path(css, css_path);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
            GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "phasex (GTK4 preview)");

    flowbox = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flowbox), GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flowbox), 6);
    gtk_widget_set_valign(flowbox, GTK_ALIGN_START);
    for (i = 0; i < NUM_PARAM_GROUPS; i++) {
        if (param_group[i].param_list[0] > -1) {
            gtk_flow_box_insert(GTK_FLOW_BOX(flowbox), create_param_group_view(i), -1);
        }
    }

    scroller = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), flowbox);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(vbox), create_menubar(GTK_WINDOW(window)));
    gtk_box_append(GTK_BOX(vbox), create_navbar());
    gtk_box_append(GTK_BOX(vbox), scroller);
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    gtk_window_set_default_size(GTK_WINDOW(window), 1400, 900);
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

    phasex_gtk4_backend_init();

    g_print("[backend] real state after init: visible_sess_num=%u visible_part_num=%u "
            "program=%u patch_name=\"%s\"\n",
            visible_sess_num, visible_part_num, get_visible_program_number(),
            (get_visible_patch()->name != NULL) ? get_visible_patch()->name : "(null)");

    app = gtk_application_new("org.phasex.gtk4preview", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
