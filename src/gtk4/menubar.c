/*****************************************************************************
 *
 * gtk4/menubar.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * GTK4 port of src/gui_menubar.c.  Architecturally, this is the piece
 * that changed the most going from GTK2 to GTK4:
 *
 *   - GtkMenuBar/GtkMenu/GtkMenuItem/GtkCheckMenuItem/GtkRadioMenuItem
 *     and the (already-deprecated-in-GTK2) GtkItemFactory table gui_
 *     menubar.c builds its menu from are all gone in GTK4. The
 *     replacement is a declarative GMenu model (built here with
 *     g_menu_append*()) rendered by a GtkPopoverMenuBar, wired to
 *     GAction objects registered on the window rather than signal-
 *     connected widgets.
 *
 *   - Radio and check items are no longer separate widget types you
 *     toggle by hand (gtk_check_menu_item_set_active(), maintaining a
 *     GSList of radio-group members). They're a single GSimpleAction
 *     with GVariant state: a boolean-state action for a check item, a
 *     string-state action (with each GMenuItem carrying a "target"
 *     matching one state value) for a radio group. GTK draws the
 *     radio dot/checkmark and handles mutual exclusion automatically
 *     from the state.
 *
 * Scope for this pass: the static menu structure (File, View, Patch,
 * MIDI, Help) for the PHASEX_NUM_PARTS=1 layout, wired to real actions
 * where the already-linked backend supports them (all-notes-off, load/
 * save patch -- reusing navbar.c's dialog code so there's exactly one
 * implementation). Not wired to anything real: Reset Patch, MIDI map
 * load/save (needs midimap.c, which isn't linked -- see backend_init.c
 * for why), and window-layout switching (View/Notebook|One Page|
 * WideScreen and the Fit WxH items are cosmetic here since main.c only
 * ever builds one fixed layout). ALSA and JACK now list real devices/
 * ports (device_enum.c), ported from alsa_pcm.c/alsa_seq.c/rawmidi.c/
 * jack.c's enumeration functions without linking those files whole --
 * each row is still informational only, not yet wired to a real
 * connect/subscribe action (gui_alsa.c's on_select_alsa_... family,
 * gui_jack.c's on_select_jack_midi_port).
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
#include "menubar.h"
#include "navbar.h"
#include "device_enum.h"


static void
placeholder_action(GSimpleAction *action, GVariant *UNUSED_param, gpointer UNUSED_data) {
    (void) UNUSED_param;
    (void) UNUSED_data;
    g_printerr("[gtk4-preview] win.%s activated (not wired to the backend yet)\n",
               g_action_get_name(G_ACTION(action)));
}


static void
on_quit(GSimpleAction *UNUSED_action, GVariant *UNUSED_param, gpointer data) {
    GtkWindow *window = GTK_WINDOW(data);

    (void) UNUSED_action;
    (void) UNUSED_param;

    g_application_quit(G_APPLICATION(gtk_window_get_application(window)));
}


static void
on_load_patch(GSimpleAction *UNUSED_action, GVariant *UNUSED_param, gpointer data) {
    (void) UNUSED_action;
    (void) UNUSED_param;
    navbar_trigger_load_patch(GTK_WINDOW(data));
}


static void
on_save_patch(GSimpleAction *UNUSED_action, GVariant *UNUSED_param, gpointer data) {
    (void) UNUSED_action;
    (void) UNUSED_param;
    navbar_trigger_save_patch(GTK_WINDOW(data));
}


static void
on_all_notes_off(GSimpleAction *UNUSED_action, GVariant *UNUSED_param, gpointer UNUSED_data) {
    (void) UNUSED_action;
    (void) UNUSED_param;
    (void) UNUSED_data;
    navbar_trigger_all_notes_off();
}


/* View/Notebook|One Page|WideScreen and Patch/Bank Memory Autosave|Warn|
   Protect: stateful radio actions. The state visibly updates (GTK draws
   the radio dot on whichever GMenuItem's target matches), but nothing
   downstream reads it yet -- see the file header comment. */
static void
on_radio_action(GSimpleAction *action, GVariant *param, gpointer UNUSED_data) {
    (void) UNUSED_data;
    g_simple_action_set_state(action, param);
}


static void
on_fullscreen(GSimpleAction *action, GVariant *UNUSED_param, gpointer data) {
    GtkWindow   *window = GTK_WINDOW(data);
    GVariant    *old_state;
    gboolean    new_state;

    (void) UNUSED_param;

    old_state = g_action_get_state(G_ACTION(action));
    new_state = !g_variant_get_boolean(old_state);
    g_variant_unref(old_state);

    if (new_state) {
        gtk_window_fullscreen(window);
    } else {
        gtk_window_unfullscreen(window);
    }
    g_simple_action_set_state(action, g_variant_new_boolean(new_state));
}


static void
on_fit_size(GSimpleAction *UNUSED_action, GVariant *param, gpointer data) {
    GtkWindow   *window = GTK_WINDOW(data);
    int         width;
    int         height;

    (void) UNUSED_action;

    if (sscanf(g_variant_get_string(param, NULL), "%dx%d", &width, &height) == 2) {
        gtk_window_set_default_size(window, width, height);
    }
}


static void
on_about(GSimpleAction *UNUSED_action, GVariant *UNUSED_param, gpointer data) {
    (void) UNUSED_action;
    (void) UNUSED_param;

    gtk_show_about_dialog(GTK_WINDOW(data),
                          "program-name", "phasex",
                          "version", "0.14.97 (GTK4 preview)",
                          "comments", "Phase Harmonic Advanced Synthesis Experiment",
                          "license-type", GTK_LICENSE_GPL_3_0,
                          NULL);
}


static void
on_help(GSimpleAction *UNUSED_action, GVariant *UNUSED_param, gpointer data) {
    GtkAlertDialog *dialog = gtk_alert_dialog_new("Using PHASEX");

    (void) UNUSED_action;
    (void) UNUSED_param;

    gtk_alert_dialog_set_detail(dialog,
            "See help/parameters.help in the phasex source tree for full "
            "parameter documentation (this preview doesn't link help.c).");
    gtk_alert_dialog_show(dialog, GTK_WINDOW(data));
    g_object_unref(dialog);
}


static const GActionEntry win_actions[] = {
    { "quit",           on_quit,          NULL, NULL,      NULL },
    { "load-patch",     on_load_patch,    NULL, NULL,      NULL },
    { "save-patch",     on_save_patch,    NULL, NULL,      NULL },
    { "save-patch-as",  on_save_patch,    NULL, NULL,      NULL },
    { "reset-patch",    placeholder_action, NULL, NULL,    NULL },
    { "view-layout",    on_radio_action,  "s",  "'notebook'", NULL },
    { "fullscreen",     on_fullscreen,    NULL, "false",   NULL },
    { "fit-size",       on_fit_size,      "s",  NULL,      NULL },
    { "bank-mem-mode",  on_radio_action,  "s",  "'autosave'", NULL },
    { "all-notes-off",  on_all_notes_off, NULL, NULL,      NULL },
    { "load-midimap",   placeholder_action, NULL, NULL,    NULL },
    { "save-midimap",   placeholder_action, NULL, NULL,    NULL },
    { "save-midimap-as", placeholder_action, NULL, NULL,   NULL },
    { "about",          on_about,         NULL, NULL,      NULL },
    { "help",           on_help,          NULL, NULL,      NULL },
    /* Disabled below, right after the action group is built. Every
       row in the ALSA/JACK submenus (build_alsa_menu()/build_jack_
       menu(), real device/port names or the empty-category fallback)
       uses one of these -- a GMenuItem with a NULL action doesn't
       reliably render at all in GtkPopoverMenuBar, it needs a real
       action reference to show up as a (greyed-out, informational-
       only for now) row with its label. */
    { "alsa-placeholder", placeholder_action, NULL, NULL,  NULL },
    { "jack-placeholder", placeholder_action, NULL, NULL,  NULL },
};


static GMenuModel *
build_file_menu(void) {
    GMenu *menu = g_menu_new();
    GMenu *section;

    section = g_menu_new();
    g_menu_append(section, "Open", "win.load-patch");
    g_menu_append(section, "Save", "win.save-patch");
    g_menu_append(section, "Save As", "win.save-patch-as");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    section = g_menu_new();
    g_menu_append(section, "Quit", "win.quit");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    return G_MENU_MODEL(menu);
}


static GMenuModel *
build_view_menu(void) {
    GMenu       *menu = g_menu_new();
    GMenu       *section;
    GMenuItem   *item;

    section = g_menu_new();
    item = g_menu_item_new("Notebook", NULL);
    g_menu_item_set_action_and_target_value(item, "win.view-layout", g_variant_new_string("notebook"));
    g_menu_append_item(section, item);
    g_object_unref(item);
    item = g_menu_item_new("One Page", NULL);
    g_menu_item_set_action_and_target_value(item, "win.view-layout", g_variant_new_string("one-page"));
    g_menu_append_item(section, item);
    g_object_unref(item);
    item = g_menu_item_new("WideScreen", NULL);
    g_menu_item_set_action_and_target_value(item, "win.view-layout", g_variant_new_string("widescreen"));
    g_menu_append_item(section, item);
    g_object_unref(item);
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    section = g_menu_new();
    g_menu_append(section, "FullScreen", "win.fullscreen");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    section = g_menu_new();
    {
        static const char *const sizes[] = {
            "800x600", "1024x768", "1280x960", "1440x900", "1680x1050", "1920x1080",
        };
        gsize   i;

        for (i = 0; i < G_N_ELEMENTS(sizes); i++) {
            char label[32];

            snprintf(label, sizeof(label), "Fit %s", sizes[i]);
            item = g_menu_item_new(label, NULL);
            g_menu_item_set_action_and_target_value(item, "win.fit-size", g_variant_new_string(sizes[i]));
            g_menu_append_item(section, item);
            g_object_unref(item);
        }
    }
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    return G_MENU_MODEL(menu);
}


static GMenuModel *
build_patch_menu(void) {
    GMenu       *menu = g_menu_new();
    GMenu       *section;
    GMenuItem   *item;

    section = g_menu_new();
    g_menu_append(section, "Load", "win.load-patch");
    g_menu_append(section, "Save", "win.save-patch");
    g_menu_append(section, "Save As", "win.save-patch-as");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    section = g_menu_new();
    g_menu_append(section, "Reset Patch", "win.reset-patch");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    section = g_menu_new();
    item = g_menu_item_new("Bank Memory Autosave", NULL);
    g_menu_item_set_action_and_target_value(item, "win.bank-mem-mode", g_variant_new_string("autosave"));
    g_menu_append_item(section, item);
    g_object_unref(item);
    item = g_menu_item_new("Bank Memory Warn", NULL);
    g_menu_item_set_action_and_target_value(item, "win.bank-mem-mode", g_variant_new_string("warn"));
    g_menu_append_item(section, item);
    g_object_unref(item);
    item = g_menu_item_new("Bank Memory Protect", NULL);
    g_menu_item_set_action_and_target_value(item, "win.bank-mem-mode", g_variant_new_string("protect"));
    g_menu_append_item(section, item);
    g_object_unref(item);
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    return G_MENU_MODEL(menu);
}


static GMenuModel *
build_midi_menu(void) {
    GMenu *menu = g_menu_new();
    GMenu *section;

    section = g_menu_new();
    g_menu_append(section, "All Notes Off", "win.all-notes-off");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    section = g_menu_new();
    g_menu_append(section, "Load MIDI Map", "win.load-midimap");
    g_menu_append(section, "Save MIDI Map", "win.save-midimap");
    g_menu_append(section, "Save MIDI Map As", "win.save-midimap-as");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    return G_MENU_MODEL(menu);
}


/* Real ALSA/JACK enumeration (device_enum.c), one section per device/
   port category -- falls back to a single disabled "(none found)" row
   per category so an empty category still renders as a visible row
   rather than an empty section, matching the ALSA/JACK top-level
   placeholder fix. */
static void
append_device_list_section(GMenu *menu, GPtrArray *devices, const char *empty_label,
                           const char *detailed_action) {
    GMenu   *section = g_menu_new();
    guint   i;

    if (devices->len == 0) {
        g_menu_append(section, empty_label, detailed_action);
    } else {
        for (i = 0; i < devices->len; i++) {
            g_menu_append(section, g_ptr_array_index(devices, i), detailed_action);
        }
    }
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);
    g_ptr_array_free(devices, TRUE);
}


static GMenuModel *
build_alsa_menu(void) {
    GMenu *menu = g_menu_new();

    append_device_list_section(menu, device_enum_alsa_pcm_playback(),
            "(no ALSA PCM playback devices found)", "win.alsa-placeholder");
    append_device_list_section(menu, device_enum_alsa_seq_hw(),
            "(no ALSA sequencer hardware ports found)", "win.alsa-placeholder");
    append_device_list_section(menu, device_enum_alsa_seq_sw(),
            "(no ALSA sequencer software ports found)", "win.alsa-placeholder");
    append_device_list_section(menu, device_enum_alsa_rawmidi(),
            "(no ALSA raw MIDI devices found)", "win.alsa-placeholder");

    return G_MENU_MODEL(menu);
}


static GMenuModel *
build_jack_menu(void) {
    GMenu *menu = g_menu_new();

    append_device_list_section(menu, device_enum_jack_midi(),
            "(no JACK MIDI ports found -- is a JACK server running?)", "win.jack-placeholder");

    return G_MENU_MODEL(menu);
}


static GMenuModel *
build_help_menu(void) {
    GMenu *menu = g_menu_new();

    g_menu_append(menu, "About PHASEX", "win.about");
    g_menu_append(menu, "Using PHASEX", "win.help");
    return G_MENU_MODEL(menu);
}


GtkWidget *
create_menubar(GtkWindow *window) {
    GMenu               *menubar = g_menu_new();
    GSimpleActionGroup  *actions = g_simple_action_group_new();
    GtkWidget           *bar;

    g_action_map_add_action_entries(G_ACTION_MAP(actions), win_actions,
                                    G_N_ELEMENTS(win_actions), window);
    gtk_widget_insert_action_group(GTK_WIDGET(window), "win", G_ACTION_GROUP(actions));

    /* Disabled so they render as greyed-out rows rather than being
       clickable no-ops -- see the win_actions[] comment above. */
    g_simple_action_set_enabled(
            G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(actions), "alsa-placeholder")), FALSE);
    g_simple_action_set_enabled(
            G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(actions), "jack-placeholder")), FALSE);
    g_object_unref(actions);

    g_menu_append_submenu(menubar, "File", build_file_menu());
    g_menu_append_submenu(menubar, "View", build_view_menu());
    g_menu_append_submenu(menubar, "Patch", build_patch_menu());
    g_menu_append_submenu(menubar, "MIDI", build_midi_menu());
    g_menu_append_submenu(menubar, "ALSA", build_alsa_menu());
    g_menu_append_submenu(menubar, "JACK", build_jack_menu());
    g_menu_append_submenu(menubar, "Help", build_help_menu());

    bar = gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(menubar));
    g_object_unref(menubar);

    return bar;
}
