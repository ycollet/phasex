/*****************************************************************************
 *
 * gui_debug.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 2012-2013 William Weston <whw@linuxmail.org>
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
#include "gui_debug.h"

#ifdef GUI_DEBUG

#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>

#include "gtkknob.h"
#include "settings.h"


/* Renders a GdkColor the same way regardless of GTK version, so dumps taken
   from a GTK2 build and a future GTK4 build can be diffed line-for-line. */
static void
gui_debug_format_color(const GdkColor *color, char *buf, size_t buflen) {
    snprintf(buf, buflen, "#%02X%02X%02X",
              (color->red   >> 8) & 0xFF,
              (color->green >> 8) & 0xFF,
              (color->blue  >> 8) & 0xFF);
}


static void
gui_debug_dump_widget(GtkWidget *widget, GtkWidget *toplevel, const char *parent_path, int sibling_index) {
    GtkStyle    *style = widget->style;
    const char  *name  = gtk_widget_get_name(widget);
    char        path[1024];
    char        fg_buf[8];
    char        bg_buf[8];
    char        base_buf[8];
    char        text_buf[8];
    gint        abs_x = 0;
    gint        abs_y = 0;

    snprintf(path, sizeof(path), "%s/%s[%d]", parent_path, G_OBJECT_TYPE_NAME(widget), sibling_index);

    /* Absolute position relative to the toplevel window, regardless of how
       deeply the widget is nested in boxes/tables/event-boxes -- this is
       what actually needs to match between a GTK2 and a GTK4 layout, since
       intermediate container structure is likely to change during a port. */
    gtk_widget_translate_coordinates(widget, toplevel, 0, 0, &abs_x, &abs_y);

    gui_debug_format_color(&style->fg[GTK_STATE_NORMAL],   fg_buf,   sizeof(fg_buf));
    gui_debug_format_color(&style->bg[GTK_STATE_NORMAL],   bg_buf,   sizeof(bg_buf));
    gui_debug_format_color(&style->base[GTK_STATE_NORMAL], base_buf, sizeof(base_buf));
    gui_debug_format_color(&style->text[GTK_STATE_NORMAL], text_buf, sizeof(text_buf));

    g_print("[GUI_DEBUG] path=%s type=%s name=%s visible=%d "
            "x=%d y=%d abs_x=%d abs_y=%d w=%d h=%d "
            "fg=%s bg=%s base=%s text_color=%s",
            path,
            G_OBJECT_TYPE_NAME(widget),
            name ? name : "(none)",
            gtk_widget_get_visible(widget),
            widget->allocation.x, widget->allocation.y, abs_x, abs_y,
            widget->allocation.width, widget->allocation.height,
            fg_buf, bg_buf, base_buf, text_buf);

    if (GTK_IS_LABEL(widget)) {
        const char              *text = gtk_label_get_text(GTK_LABEL(widget));
        PangoFontDescription    *desc = style->font_desc;
        char                    *font_str = desc ? pango_font_description_to_string(desc) : NULL;

        g_print(" text=\"%s\" font=\"%s\"", text ? text : "", font_str ? font_str : "(default)");
        if (font_str != NULL) {
            g_free(font_str);
        }
    }

    if (GTK_IS_KNOB(widget)) {
        GtkKnob    *knob = GTK_KNOB(widget);

        g_print(" knob_w=%d knob_h=%d frame_offset=%d",
                knob->width, knob->height, knob->frame_offset);
        if (knob->adjustment != NULL) {
            g_print(" value=%.4f lower=%.4f upper=%.4f",
                    (double) knob->adjustment->value,
                    (double) knob->adjustment->lower,
                    (double) knob->adjustment->upper);
        }
    }

    if (GTK_IS_TOGGLE_BUTTON(widget)) {
        g_print(" active=%d", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
    }

    if (GTK_IS_BUTTON(widget)) {
        const char *label = gtk_button_get_label(GTK_BUTTON(widget));

        if (label != NULL) {
            g_print(" label=\"%s\"", label);
        }
    }

    if (GTK_IS_ENTRY(widget)) {
        g_print(" entry_text=\"%s\"", gtk_entry_get_text(GTK_ENTRY(widget)));
    }

    if (GTK_IS_SPIN_BUTTON(widget)) {
        g_print(" spin_value=%.4f", gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)));
    }

    g_print("\n");

    if (GTK_IS_CONTAINER(widget)) {
        GList   *children = gtk_container_get_children(GTK_CONTAINER(widget));
        GList   *node;
        int     index = 0;

        for (node = children; node != NULL; node = node->next, index++) {
            gui_debug_dump_widget(GTK_WIDGET(node->data), toplevel, path, index);
        }
        g_list_free(children);
    }
}


void
gui_debug_dump_window(GtkWidget *toplevel) {
    gint    width  = 0;
    gint    height = 0;

    /* Force GTK to finish the pending size-allocate pass before reading
       any widget allocation, so we dump final on-screen geometry rather
       than a stale request. */
    while (gtk_events_pending()) {
        gtk_main_iteration();
    }

    if (GTK_IS_WINDOW(toplevel)) {
        gtk_window_get_size(GTK_WINDOW(toplevel), &width, &height);
    }

    g_print("[GUI_DEBUG] ==== dump start: GTK+ %u.%u.%u theme=%s screen=%dx%d window=%dx%d ====\n",
            gtk_major_version, gtk_minor_version, gtk_micro_version,
            theme_names[setting_theme],
            gdk_screen_width(), gdk_screen_height(),
            width, height);

    gui_debug_dump_widget(toplevel, toplevel, "", 0);

    g_print("[GUI_DEBUG] ==== dump end ====\n");
}


#else /* !GUI_DEBUG */


void
gui_debug_dump_window(GtkWidget *toplevel) {
    (void) toplevel;
}


#endif /* GUI_DEBUG */
