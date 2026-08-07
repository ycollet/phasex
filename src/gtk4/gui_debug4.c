/*****************************************************************************
 *
 * gtk4/gui_debug4.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * GTK4 counterpart to src/gui_debug.c.  Two real API gaps vs. the GTK2
 * dumper, discovered while porting -- both are inherent to GTK4, not
 * oversights here:
 *
 *   1. No parent-relative x/y.  GTK2 exposed widget->allocation.x/y (the
 *      position within the immediate parent's coordinate space) directly.
 *      GTK4 has no public equivalent -- gtk_widget_get_allocation() is
 *      deprecated and, even where it still works, no longer means the same
 *      thing under GTK4's layout model.  This dumper only reports abs_x/
 *      abs_y (position relative to the toplevel, via
 *      gtk_widget_compute_point()), which was already the primary
 *      comparison metric on the GTK2 side for exactly this reason.
 *
 *   2. No resolved background color.  gtk_widget_get_color() (GTK 4.10+)
 *      returns the resolved foreground/text color, but there is no
 *      equivalent for background: GTK4 backgrounds are arbitrary CSS
 *      (gradients, images, multiple layers), not necessarily reducible to
 *      one color, so the API doesn't offer to reduce it to one. bg/base
 *      fields are omitted here rather than faked.
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
#include <stdio.h>
#include "gui_debug4.h"


static void
gui_debug4_format_color(GtkWidget *widget, char *buf, size_t buflen) {
    GdkRGBA color;

    gtk_widget_get_color(widget, &color);
    snprintf(buf, buflen, "#%02X%02X%02X",
              (unsigned) (CLAMP(color.red,   0.0, 1.0) * 255.0 + 0.5),
              (unsigned) (CLAMP(color.green, 0.0, 1.0) * 255.0 + 0.5),
              (unsigned) (CLAMP(color.blue,  0.0, 1.0) * 255.0 + 0.5));
}


static void
gui_debug4_dump_widget(GtkWidget *widget, GtkWidget *toplevel, const char *parent_path, int sibling_index) {
    GtkWidget           *child;
    const char          *name = gtk_widget_get_name(widget);
    char                path[1024];
    char                fg_buf[8];
    graphene_point_t    origin  = GRAPHENE_POINT_INIT(0.0f, 0.0f);
    graphene_point_t    abs_point;
    int                 index = 0;

    snprintf(path, sizeof(path), "%s/%s[%d]", parent_path, G_OBJECT_TYPE_NAME(widget), sibling_index);

    if (!gtk_widget_compute_point(widget, toplevel, &origin, &abs_point)) {
        abs_point.x = -1.0f;
        abs_point.y = -1.0f;
    }

    gui_debug4_format_color(widget, fg_buf, sizeof(fg_buf));

    g_print("[GUI_DEBUG4] path=%s type=%s name=%s visible=%d "
            "abs_x=%.0f abs_y=%.0f w=%d h=%d fg=%s",
            path,
            G_OBJECT_TYPE_NAME(widget),
            name != NULL ? name : "(none)",
            gtk_widget_get_visible(widget),
            (double) abs_point.x, (double) abs_point.y,
            gtk_widget_get_width(widget), gtk_widget_get_height(widget),
            fg_buf);

    if (GTK_IS_LABEL(widget)) {
        const char *text = gtk_label_get_text(GTK_LABEL(widget));

        g_print(" text=\"%s\"", text != NULL ? text : "");
    }

    if (GTK_IS_EDITABLE(widget)) {
        g_print(" entry_text=\"%s\"", gtk_editable_get_text(GTK_EDITABLE(widget)));
    }

    if (GTK_IS_SPIN_BUTTON(widget)) {
        g_print(" spin_value=%.4f", gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)));
    }

    g_print("\n");

    for (child = gtk_widget_get_first_child(widget); child != NULL;
            child = gtk_widget_get_next_sibling(child), index++) {
        gui_debug4_dump_widget(child, toplevel, path, index);
    }
}


void
gui_debug4_dump_window(GtkWidget *toplevel) {
    while (g_main_context_pending(NULL)) {
        g_main_context_iteration(NULL, FALSE);
    }

    g_print("[GUI_DEBUG4] ==== dump start: GTK+ %u.%u.%u window=%dx%d ====\n",
            gtk_get_major_version(), gtk_get_minor_version(), gtk_get_micro_version(),
            gtk_widget_get_width(toplevel), gtk_widget_get_height(toplevel));

    gui_debug4_dump_widget(toplevel, toplevel, "", 0);

    g_print("[GUI_DEBUG4] ==== dump end ====\n");
}
