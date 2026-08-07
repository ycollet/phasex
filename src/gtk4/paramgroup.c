/*****************************************************************************
 *
 * gtk4/paramgroup.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * GTK4 port of create_param_group()/create_param_input(), scoped to the
 * "LFO-1" group. Simplifications made for this pass (all deferred to a
 * later, backend-wiring pass, not fundamental GTK4 limitations):
 *   - Value labels show the raw adjustment value ("%d") rather than the
 *     real list_labels[] strings (e.g. wave_labels, freq_base_labels) --
 *     pulling those arrays in means depending on param_strings.c, which
 *     pulls in a lot more of the backend than this pass needs.
 *   - PARAM_LFO1_POLARITY (the only PARAM_TYPE_BOOL param in this group)
 *     does use its real list_labels ("[-1,1]" / "[0,1]"), since that
 *     array (polarity_labels) is tiny and self-contained.
 *   - Adjustments are local GtkAdjustments, not wired to a real PARAM/
 *     PARAM_INFO or update_patch_state() callback.
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


#ifndef PHASEX_GTK4_PIXMAP_DIR
# define PHASEX_GTK4_PIXMAP_DIR "."
#endif


typedef enum {
    ROW_KNOB_PLAIN,     /* PARAM_TYPE_INT/REAL/RATE -- plain knob + numeric label */
    ROW_KNOB_DETENT,    /* PARAM_TYPE_DTNT -- detent knob + numeric label */
    ROW_BOOL2           /* PARAM_TYPE_BOOL, 2 choices -- radio buttons */
} RowKind;

typedef struct {
    const char  *label_text;
    RowKind     kind;
    double      lower;
    double      upper;
    double      value;
    const char  *bool_labels[2];   /* only used for ROW_BOOL2 */
} ParamRowSpec;


/* PARAM_LFO1_* row specs, values taken from src/param.c's init_param_info()
   calls (cc, lim, ccv/default, ofst): POLARITY(BOOL,0-1,0),
   FREQ_BASE(DTNT,0-8,8), WAVE(DTNT,0-27,0), RATE(RATE,0-127,64),
   INIT_PHASE(REAL,0-127,0), TRANSPOSE(INT,0-127,64),
   PITCHBEND(REAL,0-127,64), VOICE_AM(REAL,0-127,64). */
static const ParamRowSpec lfo1_rows[] = {
    { "Polarity",   ROW_BOOL2,        0,   1,   0, { "[-1,1]", "[0,1]" } },
    { "Source",     ROW_KNOB_DETENT,  0,   8,   8, { NULL, NULL } },
    { "Wave",       ROW_KNOB_DETENT,  0,  27,   0, { NULL, NULL } },
    { "Rate",       ROW_KNOB_PLAIN,   0, 127,  64, { NULL, NULL } },
    { "Init Phase", ROW_KNOB_PLAIN,   0, 127,   0, { NULL, NULL } },
    { "Transpose",  ROW_KNOB_PLAIN,   0, 127,  64, { NULL, NULL } },
    { "Pitchbend",  ROW_KNOB_PLAIN,   0, 127,  64, { NULL, NULL } },
    { "Voice AM",   ROW_KNOB_PLAIN,   0, 127,  64, { NULL, NULL } },
};

#define NUM_LFO1_ROWS (sizeof(lfo1_rows) / sizeof(lfo1_rows[0]))


static void
on_value_label_update(GtkAdjustment *adjustment, gpointer data) {
    GtkLabel    *label = GTK_LABEL(data);
    char        text[16];

    snprintf(text, sizeof(text), "%d", (int) gtk_adjustment_get_value(adjustment));
    gtk_label_set_text(label, text);
}


static void
add_knob_row(GtkGrid *grid, guint row, const ParamRowSpec *spec, PhasexKnobAnim *anim) {
    GtkWidget       *label;
    GtkWidget       *knob;
    GtkWidget       *value_label;
    GtkAdjustment   *adj;
    char            text[16];

    label = gtk_label_new(spec->label_text);
    gtk_widget_add_css_class(label, "param-name");
    gtk_label_set_xalign(GTK_LABEL(label), 1.0);
    gtk_grid_attach(grid, label, 0, (int) row, 1, 1);

    adj  = gtk_adjustment_new(spec->value, spec->lower, spec->upper, 1, 1, 0);
    knob = phasex_knob_new(adj, anim);
    gtk_grid_attach(grid, knob, 1, (int) row, 1, 1);

    snprintf(text, sizeof(text), "%d", (int) spec->value);
    value_label = gtk_label_new(text);
    gtk_widget_add_css_class(value_label, "numeric-label");
    gtk_grid_attach(grid, value_label, 2, (int) row, 1, 1);

    g_signal_connect(adj, "value-changed", G_CALLBACK(on_value_label_update), value_label);
}


static void
add_bool2_row(GtkGrid *grid, guint row, const ParamRowSpec *spec) {
    GtkWidget   *label;
    GtkWidget   *hbox;
    GtkWidget   *button;
    GtkWidget   *first_button = NULL;
    GtkWidget   *button_label;
    int         j;

    label = gtk_label_new(spec->label_text);
    gtk_widget_add_css_class(label, "param-name");
    gtk_label_set_xalign(GTK_LABEL(label), 1.0);
    gtk_grid_attach(grid, label, 0, (int) row, 1, 1);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_grid_attach(grid, hbox, 1, (int) row, 2, 1);

    for (j = 0; j < 2 && spec->bool_labels[j] != NULL; j++) {
        button = gtk_check_button_new();
        if (first_button == NULL) {
            first_button = button;
        } else {
            gtk_check_button_set_group(GTK_CHECK_BUTTON(button), GTK_CHECK_BUTTON(first_button));
        }
        gtk_widget_add_css_class(button, (j == (int) spec->value) ? "off-button" : "param-button");
        gtk_check_button_set_active(GTK_CHECK_BUTTON(button), (j == (int) spec->value));
        gtk_box_append(GTK_BOX(hbox), button);

        button_label = gtk_label_new(spec->bool_labels[j]);
        gtk_widget_add_css_class(button_label, "button-label");
        gtk_box_append(GTK_BOX(hbox), button_label);
    }
}


GtkWidget *
create_lfo1_group(void) {
    GtkWidget       *frame;
    GtkWidget       *grid;
    GtkWidget       *title;
    PhasexKnobAnim  *plain_anim;
    PhasexKnobAnim  *detent_anim;
    char            knob_file[1024];
    guint           row;

    snprintf(knob_file, sizeof(knob_file), "%s/Dark/knob-28x28.png", PHASEX_GTK4_PIXMAP_DIR);
    plain_anim = phasex_knob_animation_new_from_file(knob_file, 28, -1, 28);

    snprintf(knob_file, sizeof(knob_file), "%s/Dark/detent-knob-28x28.png", PHASEX_GTK4_PIXMAP_DIR);
    detent_anim = phasex_knob_animation_new_from_file(knob_file, 28, -1, 28);

    frame = gtk_frame_new(NULL);
    gtk_widget_add_css_class(frame, "param-group-frame");

    title = gtk_label_new("<b>LFO-1</b>");
    gtk_label_set_use_markup(GTK_LABEL(title), TRUE);
    gtk_widget_add_css_class(title, "group-name");
    gtk_frame_set_label_widget(GTK_FRAME(frame), title);

    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 0);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 4);
    gtk_frame_set_child(GTK_FRAME(frame), grid);

    for (row = 0; row < NUM_LFO1_ROWS; row++) {
        const ParamRowSpec *spec = &lfo1_rows[row];

        if (spec->kind == ROW_BOOL2) {
            add_bool2_row(GTK_GRID(grid), row, spec);
        } else {
            add_knob_row(GTK_GRID(grid), row, spec,
                        (spec->kind == ROW_KNOB_DETENT) ? detent_anim : plain_anim);
        }
    }

    return frame;
}
