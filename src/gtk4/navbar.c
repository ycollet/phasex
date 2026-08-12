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
 * Every control here reads and drives the real backend (see
 * backend_init.c): initial values come from get_visible_patch()/
 * get_visible_part(), the program spinner calls the real
 * set_active_patch()/init_patch_state() (the core of gui_bank.c's
 * select_program(), minus its modified-patch warning dialog and
 * BANK_MEM_* handling -- deferred to a later pass), Test Note/Notes
 * Off call the same timekeeping.c/midi_event.c/midi_process.c
 * functions gui_navbar.c's queue_test_note() and broadcast_notes_off()
 * do, and Load/Save Patch use GTK4's async GtkFileDialog (there's no
 * GTK2 equivalent to port -- gui_bank.c's dialogs are GTK2-native)
 * around the real read_patch()/save_patch().
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
#include <time.h>
#include "navbar.h"
#include "gtkknob.h"
#include "bank.h"
#include "patch.h"
#include "engine.h"
#include "mididefs.h"
#include "midi_event.h"
#include "midi_process.h"
#include "timekeeping.h"
#include "buffer.h"


#ifndef PHASEX_GTK4_PIXMAP_DIR
# define PHASEX_GTK4_PIXMAP_DIR "."
#endif


static GtkWidget *midi_channel_label = NULL;
static GtkWidget *patch_name_entry   = NULL;


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


/* Same body as gui_navbar.c's queue_test_note(): entirely toolkit-
   agnostic (timekeeping.c/buffer.c/midi_event.c), just missing the
   unused GtkWidget parameter GTK2's signal signature required. */
static void
on_test_note_clicked(GtkButton *UNUSED_button, gpointer UNUSED_data) {
    MIDI_EVENT      event;
    timecalc_t      delta_nsec;
    struct timespec now;
    unsigned int    m_index;
    unsigned int    tmp_index;
    unsigned int    cycle_frame;
    PART            *part = get_visible_part();

    (void) UNUSED_button;
    (void) UNUSED_data;

    event.state    = -1;
    event.type     = MIDI_EVENT_NOTE_ON;
    event.channel  = (unsigned char) part->midi_channel;
    event.note     = 64;
    event.velocity = 64;
    event.next     = NULL;

    tmp_index   = get_midi_index();
    delta_nsec  = get_time_delta(&now);
    cycle_frame = get_midi_cycle_frame(delta_nsec);

    if (tmp_index != (m_index = get_midi_index())) {
        cycle_frame = 0;
    }

    queue_midi_event(visible_part_num, &event, cycle_frame, m_index);

    if (delta_nsec >= 0.0) {
        inc_midi_index();
    }
}


static void
on_notes_off_clicked(GtkButton *UNUSED_button, gpointer UNUSED_data) {
    (void) UNUSED_button;
    (void) UNUSED_data;
    broadcast_notes_off();
}


static void
on_save_patch_finish(GObject *source, GAsyncResult *result, gpointer UNUSED_data) {
    GtkFileDialog   *dialog = GTK_FILE_DIALOG(source);
    GFile           *file;
    GError          *error  = NULL;

    (void) UNUSED_data;

    file = gtk_file_dialog_save_finish(dialog, result, &error);
    if (file == NULL) {
        g_clear_error(&error);
        return;
    }

    {
        char    *path  = g_file_get_path(file);
        PATCH   *patch = get_visible_patch();

        save_patch(path, patch);
        g_free(path);
    }
    g_object_unref(file);
}


void
navbar_trigger_save_patch(GtkWindow *parent) {
    GtkFileDialog   *dialog = gtk_file_dialog_new();

    gtk_file_dialog_set_title(dialog, "Save Patch");
    gtk_file_dialog_save(dialog, parent, NULL, on_save_patch_finish, NULL);
}


static void
on_save_patch_clicked(GtkButton *button, gpointer UNUSED_data) {
    (void) UNUSED_data;
    navbar_trigger_save_patch(GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button))));
}


static void
on_load_patch_finish(GObject *source, GAsyncResult *result, gpointer UNUSED_data) {
    GtkFileDialog   *dialog = GTK_FILE_DIALOG(source);
    GFile           *file;
    GError          *error  = NULL;

    (void) UNUSED_data;

    file = gtk_file_dialog_open_finish(dialog, result, &error);
    if (file == NULL) {
        g_clear_error(&error);
        return;
    }

    {
        char    *path  = g_file_get_path(file);
        PATCH   *patch = get_visible_patch();

        if (read_patch(path, patch) == 0) {
            gtk_editable_set_text(GTK_EDITABLE(patch_name_entry),
                                  (patch->name != NULL) ? patch->name : "untitled");
        }
        g_free(path);
    }
    g_object_unref(file);
}


void
navbar_trigger_load_patch(GtkWindow *parent) {
    GtkFileDialog   *dialog = gtk_file_dialog_new();

    gtk_file_dialog_set_title(dialog, "Load Patch");
    gtk_file_dialog_open(dialog, parent, NULL, on_load_patch_finish, NULL);
}


static void
on_load_patch_clicked(GtkButton *button, gpointer UNUSED_data) {
    (void) UNUSED_data;
    navbar_trigger_load_patch(GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button))));
}


void
navbar_trigger_all_notes_off(void) {
    broadcast_notes_off();
}


/* Core of gui_bank.c's select_program(): switches the real active patch
   and refreshes the patch-name entry. Skips the BANK_MEM_WARN/AUTOSAVE
   modified-patch handling and the update_gui_session_modified()/
   session->modified bookkeeping select_program() also does -- this
   preview doesn't yet have a save path for the modification to matter. */
static void
on_program_changed(GtkAdjustment *adjustment, gpointer UNUSED_data) {
    unsigned int    prog = (unsigned int) gtk_adjustment_get_value(adjustment) - 1;
    PATCH           *patch;

    (void) UNUSED_data;

    visible_prog_num[visible_part_num] = prog;
    patch = set_active_patch(visible_sess_num, visible_part_num, prog);
    init_patch_state(patch);

    gtk_editable_set_text(GTK_EDITABLE(patch_name_entry),
                          (patch->name != NULL) ? patch->name : "untitled");
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

    /* No frame title here -- "phasex v0.14.97 (GTK4 preview)" used to sit
       in its own reserved label band above this row, adding a visible gap
       above Program #/Patch/Load/Save for no benefit: the same string
       (name in the window titlebar, full version in Help > About) is
       already shown elsewhere, so this copy was purely redundant. */
    frame = gtk_frame_new(NULL);
    gtk_widget_add_css_class(frame, "patch-group");

    content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(content, "patch-group-content");
    gtk_frame_set_child(GTK_FRAME(frame), content);

    grid = gtk_grid_new();
    gtk_widget_set_hexpand(grid, TRUE);
    gtk_box_append(GTK_BOX(content), grid);

    /* *** Program selector (label + spin) *** */
    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), make_patch_param_label("Program #:"));

    program_adj = gtk_adjustment_new((double) (get_visible_program_number() + 1),
                                     1, PATCH_BANK_SIZE, 1, 8, 0);
    spin = gtk_spin_button_new(program_adj, 0, 0);
    gtk_widget_add_css_class(spin, "numeric-entry");
    gtk_box_append(GTK_BOX(box), spin);

    /* Matches gui_navbar.c's real create_navbar(): each of these
       clusters is its own GTK_EXPAND table cell, right/left/center-
       justified within it (table_add_widget()'s JUSTIFY_* argument) --
       not packed tight left-to-right the way a plain GtkBox would.
       Ported here as hexpand + halign per grid column, since a
       GtkGrid distributes leftover width across every hexpand column
       the same way GTK2's table distributed it across every EXPAND
       cell. Without this, every cluster bunches up on the left with
       all the navbar's extra width going unused on the right. */
    gtk_widget_set_hexpand(box, TRUE);
    gtk_widget_set_halign(box, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), box, col++, 0, 1, 1);

    /* *** Patch name (label + entry) *** */
    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), make_patch_param_label("Patch:"));

    entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(entry), 32);
    {
        PATCH *visible_patch = get_visible_patch();

        gtk_editable_set_text(GTK_EDITABLE(entry),
                              (visible_patch->name != NULL) ? visible_patch->name : "untitled");
    }
    gtk_editable_set_width_chars(GTK_EDITABLE(entry), 32);
    gtk_widget_add_css_class(entry, "numeric-entry");
    gtk_box_append(GTK_BOX(box), entry);
    patch_name_entry = entry;

    g_signal_connect(program_adj, "value-changed", G_CALLBACK(on_program_changed), NULL);

    gtk_widget_set_hexpand(box, TRUE);
    gtk_widget_set_halign(box, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), box, col++, 0, 1, 1);

    /* *** Patch load/save buttons *** */
    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_name(box, "load-save-patch");
    gtk_box_append(GTK_BOX(box), make_phasex_button("<small>Load\nPatch</small>",
                   G_CALLBACK(on_load_patch_clicked)));
    gtk_box_append(GTK_BOX(box), make_phasex_button("<small>Save\nPatch</small>",
                   G_CALLBACK(on_save_patch_clicked)));
    gtk_widget_set_hexpand(box, TRUE);
    gtk_widget_set_halign(box, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), box, col++, 0, 1, 1);

    /* *** Patch-modified indicator *** */
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    label = gtk_label_new(" ");
    gtk_widget_add_css_class(label, "indicator-label");
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(vbox), label);
    gtk_widget_set_halign(vbox, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), vbox, col++, 0, 1, 1);

    /* *** Test note / notes off buttons *** */
    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_name(box, "test-panic");
    gtk_box_append(GTK_BOX(box), make_phasex_button("<small>Test\nNote</small>",
                   G_CALLBACK(on_test_note_clicked)));
    gtk_box_append(GTK_BOX(box), make_phasex_button("<small>Notes\nOff</small>",
                   G_CALLBACK(on_notes_off_clicked)));
    gtk_widget_set_hexpand(box, TRUE);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_grid_attach(GTK_GRID(grid), box, col++, 0, 1, 1);

    /* *** MIDI channel selector (label + knob + value label) *** */
    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), make_patch_param_label("MIDI Ch:"));

    midi_channel_adj = gtk_adjustment_new((double) get_visible_part()->midi_channel, 0, 16, 1, 1, 0);

    snprintf(knob_file, sizeof(knob_file), "%s/Dark/detent-knob-28x28.png", PHASEX_GTK4_PIXMAP_DIR);
    anim = phasex_knob_animation_new_from_file(knob_file, 28, -1, 28);
    if (anim != NULL) {
        knob = phasex_knob_new(midi_channel_adj, anim);
        gtk_box_append(GTK_BOX(box), knob);
    } else {
        gtk_box_append(GTK_BOX(box), gtk_label_new("(no knob image)"));
    }

    {
        char    initial_text[8];

        snprintf(initial_text, sizeof(initial_text), "%-3d",
                get_visible_part()->midi_channel + 1);
        midi_channel_label = gtk_label_new(initial_text);
    }
    gtk_widget_add_css_class(midi_channel_label, "detent-label");
    gtk_widget_set_valign(midi_channel_label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(box), midi_channel_label);
    g_signal_connect(midi_channel_adj, "value-changed", G_CALLBACK(on_midi_channel_changed), NULL);

    gtk_widget_set_hexpand(box, TRUE);
    gtk_widget_set_halign(box, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), box, col++, 0, 1, 1);

    return frame;
}
