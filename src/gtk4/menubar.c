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
 * ever builds one fixed layout).
 *
 * ALSA and JACK list real devices/ports (device_enum.c), ported from
 * alsa_pcm.c/alsa_seq.c/rawmidi.c's enumeration functions without
 * linking those files whole. Every row is really wired up now:
 *
 *   - JACK MIDI ports (build_jack_menu()): each row is its own
 *     dynamically-created stateful boolean GAction, backed directly by
 *     jack.c's own jack_midi_ports list -- toggling one calls real
 *     jack_connect()/jack_disconnect() against the real
 *     jack_audio_client, the same pair gui_jack.c's
 *     on_select_jack_midi_port() calls (synchronously instead of via a
 *     queued watchdog-cycle request -- see that function's comment).
 *
 *   - ALSA PCM playback / ALSA rawmidi (build_alsa_menu()'s
 *     append_alsa_pcm_section()/append_alsa_rawmidi_section()): each is
 *     a one-device-at-a-time radio group (a single stateful *string*
 *     action, "win.alsa-pcm-device"/"win.alsa-rawmidi-device", one
 *     GMenuItem target per device), since only one can be the active
 *     device at a time -- selecting one calls alsa_wiring.c's real
 *     driver-switch (stop/select/init/start against the real
 *     alsa_pcm.c/rawmidi.c, now linked wholesale like jack.c).
 *
 *   - ALSA sequencer HW/SW (append_alsa_seq_section()): same
 *     per-row-own-action shape as JACK MIDI ports (any number can be
 *     subscribed at once), calling alsa_wiring.c's real
 *     alsa_seq_subscribe_port()/alsa_seq_unsubscribe_port() against the
 *     real alsa_seq_info.
 *
 * Only the "(none found)" fallback rows are disabled -- those really
 * are just informational text, not a device. See alsa_wiring.c's file
 * header for an important caveat: unlike the JACK wiring (verified
 * against a real jackd -d dummy server), the ALSA PCM/seq/rawmidi
 * wiring hasn't been runtime-tested at all -- this dev machine has no
 * real ALSA stack to test against.
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
#include <string.h>
#include "menubar.h"
#include "navbar.h"
#include "device_enum.h"
#include "jack.h"
#include "alsa_wiring.h"


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


/* Frees the g_strdup()'d port name captured at menu-build time (see
   build_jack_menu()) when the action itself is destroyed. */
static void
free_closure_data(gpointer data, GClosure *UNUSED_closure) {
    (void) UNUSED_closure;
    g_free(data);
}


/* One of these is created per real JACK MIDI port (build_jack_menu()),
   each with its own dynamically-generated action name -- not a single
   shared action -- since JACK MIDI connections aren't mutually
   exclusive the way a radio group is: any number of ports can be
   connected to midi_input_port at once, each needs its own independent
   checkmark.
   `data` is the port's name, captured as an owned copy rather than a
   JACK_PORT_INFO* at menu-build time: jack.c's port/client registration
   handlers rebuild the real jack_midi_ports list (freeing the old one)
   whenever JACK ports change, so a captured pointer could dangle. Look
   the port up by name against the *current* list instead, matching
   gui_jack.c's on_select_jack_midi_port() semantics but performed
   synchronously here (jack_connect()/jack_disconnect() are safe to call
   from any thread, and this preview has no separate watchdog thread to
   defer to the way the real app's jack_watchdog_cycle() does). */
static void
on_jack_midi_port_toggle(GSimpleAction *action, GVariant *UNUSED_param, gpointer data) {
    const char      *port_name = (const char *) data;
    JACK_PORT_INFO  *cur;

    (void) UNUSED_param;

    for (cur = jack_midi_ports; cur != NULL; cur = cur->next) {
        if (g_strcmp0(cur->name, port_name) != 0) {
            continue;
        }
        if (cur->connected) {
            jack_disconnect(jack_audio_client, cur->name, jack_port_name(midi_input_port));
        } else {
            jack_connect(jack_audio_client, cur->name, jack_port_name(midi_input_port));
        }
        cur->connected = jack_port_connected_to(midi_input_port, cur->name) ? 1 : 0;
        g_simple_action_set_state(action, g_variant_new_boolean(cur->connected ? TRUE : FALSE));
        return;
    }
    /* Port disappeared since the menu was built (unplugged/closed) --
       nothing to connect to, leave the action's state as it was. */
}


/* One of these is created per real ALSA sequencer port (see
   append_alsa_seq_section()), same shape as on_jack_midi_port_toggle()
   above and for the same reason: any number of ALSA sequencer ports
   can be subscribed at once, so each needs its own independent
   checkmark rather than sharing one action. `data` is the port's raw
   ALSA name ("client:port"), owned (g_strdup()'d at menu-build time,
   freed via free_closure_data when the action is destroyed). */
static void
on_alsa_seq_port_toggle(GSimpleAction *action, GVariant *UNUSED_param, gpointer data) {
    const char  *raw_alsa_name = (const char *) data;
    gboolean    connected;

    (void) UNUSED_param;

    connected = alsa_wiring_toggle_seq_port(raw_alsa_name);
    g_simple_action_set_state(action, g_variant_new_boolean(connected));
}


/* ALSA PCM playback and ALSA rawmidi are one-device-at-a-time radio
   groups (unlike JACK MIDI ports/ALSA sequencer ports above), so --
   unlike those -- these two really do share one stateful *string*
   action each across every row in their submenu (matching View/
   Notebook|One Page|WideScreen's on_radio_action() shape), with the
   real device switch (alsa_wiring.c) happening as a side effect of
   selecting one. */
static void
on_alsa_pcm_device_selected(GSimpleAction *action, GVariant *param, gpointer UNUSED_data) {
    (void) UNUSED_data;
    alsa_wiring_select_pcm_playback(g_variant_get_string(param, NULL));
    g_simple_action_set_state(action, param);
}


static void
on_alsa_rawmidi_selected(GSimpleAction *action, GVariant *param, gpointer UNUSED_data) {
    (void) UNUSED_data;
    alsa_wiring_select_rawmidi(g_variant_get_string(param, NULL));
    g_simple_action_set_state(action, param);
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
    /* Disabled below, right after the action group is built -- backs
       only the "(none found)" fallback row in the ALSA/JACK submenus
       (build_alsa_menu()/build_jack_menu()), which really is just
       informational text, not a device. Every real device/port row
       has its own real action now instead (per-row dynamically-created
       actions for JACK MIDI ports/ALSA sequencer ports, the two
       stateful string radio actions below for ALSA PCM/rawmidi) -- see
       the file header comment. */
    { "alsa-placeholder", placeholder_action, NULL, NULL,  NULL },
    { "jack-placeholder", placeholder_action, NULL, NULL,  NULL },
    /* ALSA PCM playback / ALSA rawmidi: one-device-at-a-time radio
       groups, unlike JACK MIDI ports/ALSA sequencer ports (each of
       those gets its own per-row action -- see build_jack_menu()/
       append_alsa_seq_section()). Initial state "''" (empty string)
       matches reality: nothing has been selected away from the
       default JACK audio/MIDI yet. */
    { "alsa-pcm-device",     on_alsa_pcm_device_selected, "s", "''", NULL },
    { "alsa-rawmidi-device", on_alsa_rawmidi_selected,    "s", "''", NULL },
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


/* GMenu item labels get mnemonic-parsed (a lone "_" underlines the
   next character and is itself dropped from the rendered text) --
   real device/port names routinely contain "_" (e.g. JACK's own
   "midi_out"), so passing one straight through as a label silently
   eats it: "dummy-midi-source:midi_out" rendered as "dummy-midi-
   source:midiout" before this existed. Escape by doubling, the
   standard GTK convention for a literal underscore. Caller frees the
   result -- g_menu_append()/g_menu_item_new() copy the string into
   the GMenuItem, so it doesn't need to outlive this call. */
static char *
escape_mnemonic_underscores(const char *label) {
    GString     *escaped = g_string_new(NULL);
    const char  *p;

    for (p = label; *p != '\0'; p++) {
        if (*p == '_') {
            g_string_append_c(escaped, '_');
        }
        g_string_append_c(escaped, *p);
    }
    return g_string_free(escaped, FALSE);
}


/* device_enum.c always formats its display strings as "[raw] rest...",
   e.g. "[hw:0,0] MyCard: MyDevice" or "[0:1] MyClient: MyPort" -- the
   bracketed prefix is exactly the raw ALSA identifier
   (ALSA_PCM_HW_INFO/ALSA_SEQ_PORT/ALSA_RAWMIDI_HW_INFO's own
   `alsa_name` field, verified by reading alsa_pcm.c/alsa_seq.c/
   rawmidi.c's real hw-list builders) that alsa_wiring.c's functions
   need to actually open/subscribe/switch to that device -- device_enum.c
   only ever needed the display string, so it doesn't expose this
   separately. Caller frees the result. */
static char *
extract_raw_alsa_name(const char *display) {
    const char  *start = strchr(display, '[');
    const char  *end;

    if (start == NULL) {
        return g_strdup(display);
    }
    start++;
    end = strchr(start, ']');
    if (end == NULL) {
        return g_strdup(display);
    }
    return g_strndup(start, (gsize) (end - start));
}


/* ALSA PCM playback: one-device-at-a-time radio group (see
   on_alsa_pcm_device_selected()). */
static void
append_alsa_pcm_section(GMenu *menu, GPtrArray *devices) {
    GMenu   *section = g_menu_new();
    guint   i;

    if (devices->len == 0) {
        g_menu_append(section, "(no ALSA PCM playback devices found)", "win.alsa-placeholder");
    } else {
        for (i = 0; i < devices->len; i++) {
            const char  *display = g_ptr_array_index(devices, i);
            char        *raw     = extract_raw_alsa_name(display);
            char        *label   = escape_mnemonic_underscores(display);
            GMenuItem   *item    = g_menu_item_new(label, NULL);

            g_menu_item_set_action_and_target_value(item, "win.alsa-pcm-device", g_variant_new_string(raw));
            g_menu_append_item(section, item);
            g_object_unref(item);
            g_free(label);
            g_free(raw);
        }
    }
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);
    g_ptr_array_free(devices, TRUE);
}


/* ALSA rawmidi: same one-device-at-a-time shape as ALSA PCM above (see
   on_alsa_rawmidi_selected()) -- rawmidi.c's own real device switch
   (setting_alsa_raw_midi_device + a driver restart) only ever has one
   device open at a time, unlike ALSA sequencer's multi-port subscribe
   model just below. */
static void
append_alsa_rawmidi_section(GMenu *menu, GPtrArray *devices) {
    GMenu   *section = g_menu_new();
    guint   i;

    if (devices->len == 0) {
        g_menu_append(section, "(no ALSA raw MIDI devices found)", "win.alsa-placeholder");
    } else {
        for (i = 0; i < devices->len; i++) {
            const char  *display = g_ptr_array_index(devices, i);
            char        *raw     = extract_raw_alsa_name(display);
            char        *label   = escape_mnemonic_underscores(display);
            GMenuItem   *item    = g_menu_item_new(label, NULL);

            g_menu_item_set_action_and_target_value(item, "win.alsa-rawmidi-device", g_variant_new_string(raw));
            g_menu_append_item(section, item);
            g_object_unref(item);
            g_free(label);
            g_free(raw);
        }
    }
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);
    g_ptr_array_free(devices, TRUE);
}


/* ALSA sequencer HW/SW: same per-row-own-action shape as
   build_jack_menu()'s real JACK MIDI ports (see on_alsa_seq_port_toggle()
   for why), used for both the HW and SW submenus -- action_prefix keeps
   their dynamically-generated action names from colliding with each
   other. `actions` must still be alive (create_menubar() unrefs its own
   reference only after both calls return) since each port's toggle
   action is registered into it here. */
static void
append_alsa_seq_section(GMenu *menu, GSimpleActionGroup *actions, GPtrArray *devices,
                        const char *action_prefix, const char *empty_label) {
    GMenu   *section = g_menu_new();
    guint   i;

    if (devices->len == 0) {
        g_menu_append(section, empty_label, "win.alsa-placeholder");
    } else {
        for (i = 0; i < devices->len; i++) {
            const char      *display = g_ptr_array_index(devices, i);
            char            *raw     = extract_raw_alsa_name(display);
            char            *label   = escape_mnemonic_underscores(display);
            GSimpleAction   *action;
            char            action_name[48];
            char            win_action_name[56];

            snprintf(action_name, sizeof(action_name), "%s-%u", action_prefix, i);
            action = g_simple_action_new_stateful(action_name, NULL, g_variant_new_boolean(FALSE));
            /* `raw` is transferred to the closure -- freed by
               free_closure_data when the action itself is destroyed,
               same as build_jack_menu()'s per-port actions below. */
            g_signal_connect_data(action, "activate", G_CALLBACK(on_alsa_seq_port_toggle),
                    raw, free_closure_data, (GConnectFlags) 0);
            g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(action));
            g_object_unref(action);

            snprintf(win_action_name, sizeof(win_action_name), "win.%s", action_name);
            g_menu_append(section, label, win_action_name);
            g_free(label);
        }
    }
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);
    g_ptr_array_free(devices, TRUE);
}


static GMenuModel *
build_alsa_menu(GSimpleActionGroup *actions) {
    GMenu *menu = g_menu_new();

    append_alsa_pcm_section(menu, device_enum_alsa_pcm_playback());
    append_alsa_seq_section(menu, actions, device_enum_alsa_seq_hw(), "alsa-seq-hw",
            "(no ALSA sequencer hardware ports found)");
    append_alsa_seq_section(menu, actions, device_enum_alsa_seq_sw(), "alsa-seq-sw",
            "(no ALSA sequencer software ports found)");
    append_alsa_rawmidi_section(menu, device_enum_alsa_rawmidi());

    return G_MENU_MODEL(menu);
}


/* JACK MIDI ports are backed directly by jack.c's own jack_midi_ports
   list -- already populated for real by jack_audio_init() (audio_init.c's
   phasex_gtk4_audio_init(), called well before create_menubar()) --
   rather than device_enum.c's throwaway-client enumeration, since
   jack_midi_ports already carries real per-port `connected` state and
   is what jack_connect()/jack_disconnect() actually need a name to act
   on. `actions` must still be alive (create_menubar() unrefs its own
   reference only after this returns) since each port's toggle action
   is registered into it here, dynamically, one real GAction per port --
   see on_jack_midi_port_toggle() for why a single shared action won't
   do. */
static GMenuModel *
build_jack_menu(GSimpleActionGroup *actions) {
    GMenu           *menu    = g_menu_new();
    GMenu           *section = g_menu_new();
    JACK_PORT_INFO  *cur;
    GSimpleAction   *action;
    char            action_name[40];
    char            win_action_name[48];
    int             index = 0;

    if ((midi_driver == MIDI_DRIVER_JACK) && (jack_midi_ports != NULL)) {
        for (cur = jack_midi_ports; cur != NULL; cur = cur->next, index++) {
            char *label;

            snprintf(action_name, sizeof(action_name), "jack-midi-port-%d", index);
            action = g_simple_action_new_stateful(action_name, NULL,
                    g_variant_new_boolean(cur->connected ? TRUE : FALSE));
            g_signal_connect_data(action, "activate", G_CALLBACK(on_jack_midi_port_toggle),
                    g_strdup(cur->name), free_closure_data, (GConnectFlags) 0);
            g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(action));
            g_object_unref(action);

            snprintf(win_action_name, sizeof(win_action_name), "win.%s", action_name);
            label = escape_mnemonic_underscores(cur->name);
            g_menu_append(section, label, win_action_name);
            g_free(label);
        }
    } else {
        g_menu_append(section, "(no JACK MIDI ports found -- is a JACK server running?)",
                "win.jack-placeholder");
    }
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

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
    GMenuModel          *alsa_menu;
    GMenuModel          *jack_menu;

    g_action_map_add_action_entries(G_ACTION_MAP(actions), win_actions,
                                    G_N_ELEMENTS(win_actions), window);
    gtk_widget_insert_action_group(GTK_WIDGET(window), "win", G_ACTION_GROUP(actions));

    /* Disabled so they render as greyed-out rows rather than being
       clickable no-ops -- see the win_actions[] comment above. */
    g_simple_action_set_enabled(
            G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(actions), "alsa-placeholder")), FALSE);
    g_simple_action_set_enabled(
            G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(actions), "jack-placeholder")), FALSE);

    /* Register one real GAction per ALSA sequencer port / JACK MIDI
       port into `actions`, so both calls must run before it's unreffed
       below -- see append_alsa_seq_section()/build_jack_menu(). */
    alsa_menu = build_alsa_menu(actions);
    jack_menu = build_jack_menu(actions);
    g_object_unref(actions);

    g_menu_append_submenu(menubar, "File", build_file_menu());
    g_menu_append_submenu(menubar, "View", build_view_menu());
    g_menu_append_submenu(menubar, "Patch", build_patch_menu());
    g_menu_append_submenu(menubar, "MIDI", build_midi_menu());
    g_menu_append_submenu(menubar, "ALSA", alsa_menu);
    g_menu_append_submenu(menubar, "JACK", jack_menu);
    g_menu_append_submenu(menubar, "Help", build_help_menu());

    bar = gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(menubar));
    g_object_unref(menubar);

    return bar;
}
