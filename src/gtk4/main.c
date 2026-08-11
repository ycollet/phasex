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
 * Param groups are packed into fixed columns inside a scrolled window,
 * the same shape as the real app's create_param_one_page() (gui_layout.c):
 * one vbox per PARAM_GROUP.full_x value, groups stacked top-to-bottom
 * within their column. An earlier pass used a GtkFlowBox instead (groups
 * wrapped left-to-right, as many as fit per row) as a stand-in for "some
 * multi-column layout" without picking one of the real notebook/one-page/
 * widescreen layouts prematurely -- but GtkFlowBox allocates every child
 * in a row the height of that row's tallest child, so a short group next
 * to a tall one got stretched into mostly empty frame. Fixed columns
 * don't have that problem: each column's height follows its own content,
 * independent of its neighbors, matching what create_param_one_page()
 * actually looks like. Only full_x is used (not notebook_x/wide_x) since
 * setting_window_layout isn't wired to anything real yet (see menubar.c's
 * View menu) and one_page's column count/shape is a reasonable default.
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
    GtkWidget           *columns;
    GtkWidget           *column;
    GtkCssProvider      *css;
    char                css_path[1024];
    int                 i;
    int                 x;
    int                 max_x = 0;

    (void) UNUSED_data;

    css = gtk_css_provider_new();
    snprintf(css_path, sizeof(css_path), "%s/theme-dark.css", PHASEX_GTK4_CSS_DIR);
    gtk_css_provider_load_from_path(css, css_path);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
            GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "phasex (GTK4 preview)");

    /* Same fixed-column packing as gui_layout.c's create_param_one_page():
       one vbox per PARAM_GROUP.full_x value, groups appended to their
       column in param_group[] order (== one_page_order[], which is just
       the identity permutation there). Each column stacks its groups
       tightly, independently of how tall the other columns end up -- no
       GtkFlowBox row to force same-row groups to a shared height. */
    for (i = 0; i < NUM_PARAM_GROUPS; i++) {
        if (param_group[i].full_x > max_x) {
            max_x = param_group[i].full_x;
        }
    }

    columns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_set_homogeneous(GTK_BOX(columns), TRUE);

    for (x = 0; x <= max_x; x++) {
        column = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_hexpand(column, TRUE);
        for (i = 0; i < NUM_PARAM_GROUPS; i++) {
            if ((param_group[i].param_list[0] > -1) && (param_group[i].full_x == x)) {
                gtk_box_append(GTK_BOX(column), create_param_group_view(i));
            }
        }
        gtk_box_append(GTK_BOX(columns), column);
    }

    scroller = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), columns);

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
