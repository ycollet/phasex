/*****************************************************************************
 *
 * gtk4/paramgroup.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * GTK4 port of create_param_group()/create_param_input(), generalized
 * to any real param group (see param_groups_data.c) using real
 * PARAM_INFO/PARAM state -- get_param_info_by_id() for type/label/
 * range/list_labels, gp->param[id] for the current value, both from
 * param.c, already linked (see backend_init.c).
 *
 * Simplifications made for this pass (all deferred to a later pass,
 * not fundamental GTK4 limitations):
 *   - Numeric (INT/REAL/RATE/DTNT/LIST) rows show the raw adjustment
 *     value ("%d") rather than info->list_labels[cc_val] -- unlike
 *     BOOL/BBOX below, matching the exact numeric-vs-list-label
 *     distinction create_param_input() makes would need re-deriving
 *     cc_val from a changed int_val (info->cc_offset math), which
 *     isn't essential to prove the pattern.
 *   - Adjustments are local GtkAdjustments seeded from the real
 *     current value, not connected back to the real
 *     update_patch_state() callback (param_cb.c) that would actually
 *     apply a change to the synth engine's state.
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
#include "paramgroup.h"
#include "gtkknob.h"
#include "gui_layout.h"
#include "param.h"
#include "patch.h"


#ifndef PHASEX_GTK4_PIXMAP_DIR
# define PHASEX_GTK4_PIXMAP_DIR "."
#endif


static PhasexKnobAnim *plain_anim  = NULL;
static PhasexKnobAnim *detent_anim = NULL;


static void
ensure_knob_anims(void) {
    char knob_file[1024];

    if (plain_anim != NULL) {
        return;
    }

    snprintf(knob_file, sizeof(knob_file), "%s/Dark/knob-28x28.png", PHASEX_GTK4_PIXMAP_DIR);
    plain_anim = phasex_knob_animation_new_from_file(knob_file, 28, -1, 28);

    snprintf(knob_file, sizeof(knob_file), "%s/Dark/detent-knob-28x28.png", PHASEX_GTK4_PIXMAP_DIR);
    detent_anim = phasex_knob_animation_new_from_file(knob_file, 28, -1, 28);
}


static void
on_value_label_update(GtkAdjustment *adjustment, gpointer data) {
    GtkLabel    *label = GTK_LABEL(data);
    char        text[16];

    snprintf(text, sizeof(text), "%d", (int) gtk_adjustment_get_value(adjustment));
    gtk_label_set_text(label, text);
}


/* PARAM_TYPE_INT/REAL/RATE/DTNT/LIST: knob (detent image for DTNT,
   plain for everything else, matching create_param_input()) + a
   numeric value label, seeded from the real current value.
   create_param_input() fixes the value label's width (4 chars for
   INT/REAL, 5 for RATE/DTNT) and left-justifies it so the label
   doesn't shift the rest of the row around as its digit count
   changes -- matched here via gtk_label_set_width_chars()/xalign. */
static void
add_knob_row(GtkGrid *grid, guint row, PARAM_INFO *info, PARAM *param) {
    GtkWidget       *label;
    GtkWidget       *knob;
    GtkWidget       *value_label;
    GtkAdjustment   *adj;
    char            text[16];
    double          lower = (double) info->cc_offset;
    double          upper = (double) (info->cc_limit + info->cc_offset);
    double          value = (double) param->value.int_val;

    label = gtk_label_new(info->label_text);
    gtk_widget_add_css_class(label, "param-name");
    gtk_label_set_xalign(GTK_LABEL(label), 1.0);
    gtk_grid_attach(grid, label, 0, (int) row, 1, 1);

    adj  = gtk_adjustment_new(value, lower, upper, 1, (info->leap > 0) ? info->leap : 1, 0);
    knob = phasex_knob_new(adj, (info->type == PARAM_TYPE_DTNT) ? detent_anim : plain_anim);
    gtk_widget_set_halign(knob, GTK_ALIGN_CENTER);
    gtk_grid_attach(grid, knob, 1, (int) row, 1, 1);

    snprintf(text, sizeof(text), "%d", (int) value);
    value_label = gtk_label_new(text);
    gtk_widget_add_css_class(value_label, "numeric-label");
    gtk_label_set_xalign(GTK_LABEL(value_label), 0.0);
    gtk_label_set_width_chars(GTK_LABEL(value_label),
            ((info->type == PARAM_TYPE_RATE) || (info->type == PARAM_TYPE_DTNT)) ? 5 : 4);
    gtk_grid_attach(grid, value_label, 2, (int) row, 1, 1);

    g_signal_connect(adj, "value-changed", G_CALLBACK(on_value_label_update), value_label);
}


/* PARAM_TYPE_BOOL: a row of radio-style buttons, each with its label
   beside it (button then label, left to right), one pair per
   info->list_labels[] entry -- matches create_param_input()'s
   PARAM_TYPE_BOOL case (button and label packed side by side in the
   same hbox), e.g. "Polarity" showing "( ) [-1,1]  (o) [0,1]". */
static void
add_bool_row(GtkGrid *grid, guint row, PARAM_INFO *info, PARAM *param) {
    GtkWidget   *label;
    GtkWidget   *hbox;
    GtkWidget   *button;
    GtkWidget   *first_button = NULL;
    GtkWidget   *button_label;
    int         j;

    label = gtk_label_new(info->label_text);
    gtk_widget_add_css_class(label, "param-name");
    gtk_label_set_xalign(GTK_LABEL(label), 1.0);
    gtk_grid_attach(grid, label, 0, (int) row, 1, 1);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_grid_attach(grid, hbox, 1, (int) row, 2, 1);

    for (j = 0; (info->list_labels != NULL) && (info->list_labels[j] != NULL); j++) {
        button = gtk_check_button_new();
        if (first_button == NULL) {
            first_button = button;
        } else {
            gtk_check_button_set_group(GTK_CHECK_BUTTON(button), GTK_CHECK_BUTTON(first_button));
        }
        gtk_check_button_set_active(GTK_CHECK_BUTTON(button), (j == param->value.cc_val));
        gtk_widget_add_css_class(button, "param-button");
        gtk_box_append(GTK_BOX(hbox), button);

        button_label = gtk_label_new(info->list_labels[j]);
        gtk_label_set_use_markup(GTK_LABEL(button_label), TRUE);
        gtk_widget_add_css_class(button_label, "button-label");
        gtk_box_append(GTK_BOX(hbox), button_label);
    }
}


/* PARAM_TYPE_BBOX: one column per info->list_labels[] entry, each
   column its label stacked directly above its radio button -- matches
   create_param_input()'s PARAM_TYPE_BBOX case (a 2-row table per
   option: label in row 0, button in row 1), e.g. "Mix Mod" showing
   "Off  Mix  AM  Mod" as a label row over a row of radio buttons,
   rather than BOOL's side-by-side button-then-label pairs. */
static void
add_bbox_row(GtkGrid *grid, guint row, PARAM_INFO *info, PARAM *param) {
    GtkWidget   *label;
    GtkWidget   *hbox;
    GtkWidget   *vbox;
    GtkWidget   *button;
    GtkWidget   *first_button = NULL;
    GtkWidget   *button_label;
    int         j;

    label = gtk_label_new(info->label_text);
    gtk_widget_add_css_class(label, "param-name");
    gtk_label_set_xalign(GTK_LABEL(label), 1.0);
    gtk_grid_attach(grid, label, 0, (int) row, 1, 1);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_grid_attach(grid, hbox, 1, (int) row, 2, 1);

    for (j = 0; (info->list_labels != NULL) && (info->list_labels[j] != NULL); j++) {
        vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

        button_label = gtk_label_new(info->list_labels[j]);
        gtk_label_set_use_markup(GTK_LABEL(button_label), TRUE);
        gtk_widget_add_css_class(button_label, "button-label");
        gtk_widget_set_halign(button_label, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(vbox), button_label);

        button = gtk_check_button_new();
        if (first_button == NULL) {
            first_button = button;
        } else {
            gtk_check_button_set_group(GTK_CHECK_BUTTON(button), GTK_CHECK_BUTTON(first_button));
        }
        gtk_check_button_set_active(GTK_CHECK_BUTTON(button), (j == param->value.cc_val));
        gtk_widget_add_css_class(button, "param-button");
        gtk_widget_set_halign(button, GTK_ALIGN_CENTER);
        gtk_box_append(GTK_BOX(vbox), button);

        gtk_box_append(GTK_BOX(hbox), vbox);
    }
}


GtkWidget *
create_param_group_view(int group_index) {
    PARAM_GROUP *group = &param_group[group_index];
    GtkWidget   *frame;
    GtkWidget   *grid;
    GtkWidget   *title;
    PATCH       *patch = get_visible_patch();
    guint       row;
    int         k;

    ensure_knob_anims();

    frame = gtk_frame_new(NULL);
    gtk_widget_add_css_class(frame, "param-group-frame");
    /* Each group should hug its own content height rather than being
       stretched by whatever container packs it (a GtkFlowBox row's
       tallest neighbor, a GtkBox's leftover space, ...). */
    gtk_widget_set_valign(frame, GTK_ALIGN_START);

    title = gtk_label_new(group->label);
    gtk_label_set_use_markup(GTK_LABEL(title), TRUE);
    gtk_widget_add_css_class(title, "group-name");
    gtk_frame_set_label_widget(GTK_FRAME(frame), title);

    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 0);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 4);
    gtk_frame_set_child(GTK_FRAME(frame), grid);

    for (k = 0, row = 0; (k < 16) && (group->param_list[k] > -1); k++, row++) {
        unsigned int    param_id = (unsigned int) group->param_list[k];
        PARAM_INFO      *info    = get_param_info_by_id(param_id);
        PARAM           *param   = &patch->param[param_id];

        if (info->type == PARAM_TYPE_BOOL) {
            add_bool_row(GTK_GRID(grid), row, info, param);
        } else if (info->type == PARAM_TYPE_BBOX) {
            add_bbox_row(GTK_GRID(grid), row, info, param);
        } else {
            add_knob_row(GTK_GRID(grid), row, info, param);
        }
    }

    return frame;
}
