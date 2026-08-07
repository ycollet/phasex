/*****************************************************************************
 *
 * gtk4/navbar.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * GTK4 port of src/gui_navbar.c's create_navbar(), for the
 * PHASEX_NUM_PARTS=1 layout (the default build) -- matches the widgets
 * actually present in gtk2_debug_dump.txt, so the two can be diffed.
 *
 * Scope for this first pass: layout, sizing, and color parity only.  The
 * widgets are wired to local state (a GtkAdjustment, a couple of labels)
 * rather than the real session/bank/patch backend -- get_current_session(),
 * get_visible_patch(), select_session(), load_program(), etc. all reach
 * deep into global engine/session state that a future pass will wire up
 * once the rest of the GUI (and its CMake target) exists to host it.  The
 * button/entry callbacks here just print to stderr as placeholders.
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
#include "navbar.h"
#include "gtkknob.h"


#ifndef PHASEX_GTK4_PIXMAP_DIR
# define PHASEX_GTK4_PIXMAP_DIR "."
#endif


static GtkWidget *midi_channel_label = NULL;


static GtkWidget *
make_patch_param_label(const char *text) {
    GtkWidget *label = gtk_label_new(text);

    gtk_widget_add_css_class(label, "patch-param");
    return label;
}


static GtkWidget *
make_phasex_button(const char *markup_text, GCallback callback) {
    GtkWidget *button = gtk_button_new();
    GtkWidget *label  = gtk_label_new(markup_text);

    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_widget_add_css_class(label, "phasex-button-label");
    gtk_button_set_child(GTK_BUTTON(button), label);
    gtk_widget_add_css_class(button, "phasex-button");
    g_signal_connect(button, "clicked", callback, NULL);

    return button;
}


static void
on_placeholder_clicked(GtkButton *button, gpointer UNUSED_data) {
    (void) UNUSED_data;
    g_printerr("[gtk4-preview] %s clicked (not wired to the backend yet)\n",
               gtk_widget_get_name(GTK_WIDGET(button)));
}


static void
on_midi_channel_changed(GtkAdjustment *adjustment, gpointer UNUSED_data) {
    char    text[8];
    int     channel = (int) gtk_adjustment_get_value(adjustment);

    (void) UNUSED_data;

    snprintf(text, sizeof(text), "%-3d", channel + 1);
    gtk_label_set_text(GTK_LABEL(midi_channel_label), text);
}


/*****************************************************************************
 * create_navbar()
 *
 * Builds the "PatchGroup" frame: title bar + a single-row grid of program
 * selector, patch name, patch load/save, patch-modified indicator,
 * test-note/notes-off, and the MIDI channel knob -- the same set of
 * controls gui_navbar.c's create_navbar() builds when MAX_PARTS == 1.
 *****************************************************************************/
GtkWidget *
create_navbar(void) {
    GtkWidget       *frame;
    GtkWidget       *content;
    GtkWidget       *grid;
    GtkWidget       *box;
    GtkWidget       *vbox;
    GtkWidget       *label;
    GtkWidget       *entry;
    GtkWidget       *spin;
    GtkWidget       *knob;
    GtkAdjustment   *program_adj;
    GtkAdjustment   *midi_channel_adj;
    PhasexKnobAnim  *anim;
    char            knob_file[1024];
    int             col = 0;

    frame = gtk_frame_new(NULL);
    gtk_widget_add_css_class(frame, "patch-group");

    label = gtk_label_new("<b>phasex v0.14.97 (GTK4 preview)</b>");
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_widget_add_css_class(label, "group-name");
    gtk_frame_set_label_widget(GTK_FRAME(frame), label);
    gtk_frame_set_label_align(GTK_FRAME(frame), 1.0);

    content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(content, "patch-group-content");
    gtk_frame_set_child(GTK_FRAME(frame), content);

    grid = gtk_grid_new();
    gtk_widget_set_hexpand(grid, TRUE);
    gtk_box_append(GTK_BOX(content), grid);

    /* *** Program selector (label + spin) *** */
    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), make_patch_param_label("Program #:"));

    program_adj = gtk_adjustment_new(1, 1, 128, 1, 8, 0);
    spin = gtk_spin_button_new(program_adj, 0, 0);
    gtk_widget_add_css_class(spin, "numeric-entry");
    gtk_box_append(GTK_BOX(box), spin);

    gtk_grid_attach(GTK_GRID(grid), box, col++, 0, 1, 1);

    /* *** Patch name (label + entry) *** */
    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), make_patch_param_label("Patch:"));

    entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(entry), 32);
    gtk_editable_set_text(GTK_EDITABLE(entry), "untitled");
    gtk_editable_set_width_chars(GTK_EDITABLE(entry), 32);
    gtk_widget_add_css_class(entry, "numeric-entry");
    gtk_box_append(GTK_BOX(box), entry);

    gtk_grid_attach(GTK_GRID(grid), box, col++, 0, 1, 1);

    /* *** Patch load/save buttons *** */
    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_name(box, "load-save-patch");
    gtk_box_append(GTK_BOX(box), make_phasex_button("<small>Load\nPatch</small>",
                   G_CALLBACK(on_placeholder_clicked)));
    gtk_box_append(GTK_BOX(box), make_phasex_button("<small>Save\nPatch</small>",
                   G_CALLBACK(on_placeholder_clicked)));
    gtk_grid_attach(GTK_GRID(grid), box, col++, 0, 1, 1);

    /* *** Patch-modified indicator *** */
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    label = gtk_label_new(" ");
    gtk_widget_add_css_class(label, "indicator-label");
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(vbox), label);
    gtk_grid_attach(GTK_GRID(grid), vbox, col++, 0, 1, 1);

    /* *** Test note / notes off buttons *** */
    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_name(box, "test-panic");
    gtk_box_append(GTK_BOX(box), make_phasex_button("<small>Test\nNote</small>",
                   G_CALLBACK(on_placeholder_clicked)));
    gtk_box_append(GTK_BOX(box), make_phasex_button("<small>Notes\nOff</small>",
                   G_CALLBACK(on_placeholder_clicked)));
    gtk_grid_attach(GTK_GRID(grid), box, col++, 0, 1, 1);

    /* *** MIDI channel selector (label + knob + value label) *** */
    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), make_patch_param_label("MIDI Ch:"));

    midi_channel_adj = gtk_adjustment_new(0, 0, 16, 1, 1, 0);

    snprintf(knob_file, sizeof(knob_file), "%s/Dark/detent-knob-28x28.png", PHASEX_GTK4_PIXMAP_DIR);
    anim = phasex_knob_animation_new_from_file(knob_file, 28, -1, 28);
    if (anim != NULL) {
        knob = phasex_knob_new(midi_channel_adj, anim);
        gtk_box_append(GTK_BOX(box), knob);
    } else {
        gtk_box_append(GTK_BOX(box), gtk_label_new("(no knob image)"));
    }

    midi_channel_label = gtk_label_new("1  ");
    gtk_widget_add_css_class(midi_channel_label, "detent-label");
    gtk_widget_set_valign(midi_channel_label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(box), midi_channel_label);
    g_signal_connect(midi_channel_adj, "value-changed", G_CALLBACK(on_midi_channel_changed), NULL);

    gtk_grid_attach(GTK_GRID(grid), box, col++, 0, 1, 1);

    return frame;
}
