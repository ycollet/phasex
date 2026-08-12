/*****************************************************************************
 *
 * gtk4/backend_init.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Minimal, audio-free initialization of the real session/bank/patch
 * backend for the GTK4 preview.
 *
 * phasex.c's real main() can't be linked in directly: it owns main()
 * itself, spins up the GTK2 GUI thread, parses CLI args, and --
 * critically -- calls init_audio()/init_midi() before init_patch_bank(),
 * which block waiting for a real sample rate from a running JACK/ALSA
 * server. Checked bank.c/session.c/patch.c: none of them reference
 * sample_rate, so the patch/session/param data layer inits fine on its
 * own, skipping audio and MIDI I/O entirely (this preview doesn't play
 * sound, so that's not a loss).
 *
 * The path globals below (user_patch_dir, sys_default_patch, etc.) are
 * normally *defined* in phasex.c and populated by check_user_data_dirs()
 * + read_settings() during startup. Since phasex.c isn't linked, this
 * file provides their storage instead, pointed straight at the real
 * files already in the source tree (patches/, misc/patchbank) -- so
 * patch/bank loading is genuine, not fabricated, without needing an
 * install step or PATCH_DIR (an install-path macro that doesn't exist
 * yet in a dev build).
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
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <glib.h>
#include "phasex.h"
#include "config.h"
#include "patch.h"
#include "param.h"
#include "engine.h"
#include "bank.h"
#include "session.h"
#include "settings.h"
#include "timekeeping.h"
#include "buffer.h"
#include "gui_layout.h"
#include "backend_init.h"


#ifndef PHASEX_GTK4_REPO_DIR
# define PHASEX_GTK4_REPO_DIR "."
#endif


/* Storage for the small set of setting_* globals actually referenced by
   the backend files linked into this preview (bank.c, engine.c,
   timekeeping.c, param_cb.c, bpm.c, midi_process.c). settings.c itself
   isn't linked in -- it directly calls GTK2 functions (gtk_window_set_
   decorated(), gtk_check_menu_item_set_active(), etc.) to push settings
   changes live into the GTK2 GUI, so it's not toolkit-agnostic the way
   bank.c/session.c/patch.c are. Values below match settings.c's own
   defaults. */
int         setting_ignore_midi_program_change = 0;
int         setting_sample_rate_mode           = SAMPLE_RATE_NORMAL;
int         setting_polyphony                  = DEFAULT_POLYPHONY;
int         setting_engine_priority            = ENGINE_THREAD_PRIORITY;
int         setting_sched_policy               = PHASEX_SCHED_POLICY;
timecalc_t  setting_audio_phase_lock           = DEFAULT_AUDIO_PHASE_LOCK;
timecalc_t  setting_clock_constant             = 1.0;


/* Storage for the extern path globals declared in phasex.h -- see the
   file header comment above for why these live here instead of in
   phasex.c. */
char        user_data_dir[PATH_MAX];
char        user_patch_dir[PATH_MAX];
char        user_midimap_dir[PATH_MAX];
char        user_session_dir[PATH_MAX];
char        user_bank_file[PATH_MAX];
char        user_session_bank_file[PATH_MAX];
char        user_patchdump_file[MAX_PARTS][PATH_MAX];
char        user_session_dump_dir[PATH_MAX];
char        user_midimap_dump_file[PATH_MAX];
char        user_config_file[PATH_MAX];
char        user_default_patch[PATH_MAX];
char        sys_default_patch[PATH_MAX];
char        sys_bank_file[PATH_MAX];


/* Several backend files (bank.c, session.c, patch.c, param.c, ...) call
   this on unrecoverable errors (e.g. malloc failure). Normally defined
   in phasex.c alongside a full teardown of audio/MIDI/GUI threads; here
   there's nothing else running, so just report and exit. */
void
phasex_shutdown(const char *msg) {
    fprintf(stderr, "phasex-gtk4-preview: fatal: %s", msg);
    exit(1);
}


/* Everything below stands in for symbols normally provided by driver.c
   (real engine-thread lifecycle, tied to the audio driver we're not
   starting) or the GTK2 GUI layer (gui_patch.c, gui_param.c,
   gui_midimap.c, settings.c), so bank.c/session.c/patch.c/engine.c/
   midi_process.c link without pulling in either. None of the thread-
   lifecycle functions below (start_engine_threads/stop_engine/etc.) are
   ever called by phasex_gtk4_backend_init() -- they just need to
   *resolve* since they're referenced from functions elsewhere in the
   same object files. update_gui_patch() is the one exception worth
   noting: it does real work (updating `gp`, the pointer patch.c reads
   for "the currently active patch"), since that's cheap and correct to
   do regardless of GUI toolkit. Redraw-triggering hooks are true
   no-ops for now -- the GTK4 navbar reads state instead of being
   pushed updates, until a later pass adds live notification. */
pthread_t       engine_thread_p[MAX_PARTS];
int             gtkui_ready                    = 0;
int             pending_shutdown                = 0;
PATCH           *gp                            = NULL;
PATCH           *pending_visible_patch         = NULL;
int             cc_edit_active                 = 0;
int             cc_edit_ignore_midi            = 0;
int             cc_edit_cc_num                 = 0;
int             ccmatrix[128][16];


void
gui_param_midi_update(PARAM *param, int cc_val) {
    (void) param;
    (void) cc_val;
}


void
update_gui_patch_name(void) {
}


void
update_gui_session_name(void) {
}


void
update_gui_patch_modified(void) {
}


void
update_gui_patch_changed(PATCH *patch, int part_switch) {
    (void) patch;
    (void) part_switch;
}


void
update_gui_patch(PATCH *patch, int part_switch) {
    (void) part_switch;
    gp = patch;
}


int
save_midimap(char *filename) {
    (void) filename;
    return 0;
}


int
save_settings(char *filename) {
    (void) filename;
    return 0;
}


void
phasex_gtk4_backend_init(void) {
    unsigned int    part_num;

    snprintf(user_patch_dir, PATH_MAX, "%s/patches", PHASEX_GTK4_REPO_DIR);
    snprintf(user_midimap_dir, PATH_MAX, "%s/midimaps", PHASEX_GTK4_REPO_DIR);
    snprintf(sys_default_patch, PATH_MAX, "%s/patches/phasex-default.phx", PHASEX_GTK4_REPO_DIR);
    snprintf(user_default_patch, PATH_MAX, "%s/patches/phasex-default.phx", PHASEX_GTK4_REPO_DIR);
    snprintf(sys_bank_file, PATH_MAX, "%s/misc/patchbank", PHASEX_GTK4_REPO_DIR);
    snprintf(user_bank_file, PATH_MAX, "%s/misc/patchbank", PHASEX_GTK4_REPO_DIR);

    user_data_dir[0]          = '\0';
    user_session_dir[0]       = '\0';
    user_session_bank_file[0] = '\0';
    user_session_dump_dir[0]  = '\0';
    user_midimap_dump_file[0] = '\0';
    user_config_file[0]       = '\0';
    for (part_num = 0; part_num < MAX_PARTS; part_num++) {
        user_patchdump_file[part_num][0] = '\0';
    }

    init_params();
    init_param_groups();
    init_param_pages();
}


/* Split out from phasex_gtk4_backend_init() (see backend_init.h) --
   must run after audio_init.c's phasex_gtk4_audio_init() has negotiated
   a real sample rate with JACK, since init_engine_internals() computes
   several real values straight from f_sample_rate (aftertouch_smooth_len,
   pitch_bend_smooth_len, global.dcR_const, delay->size) and reads
   env_table[] (built by build_env_tables(), also part of
   phasex_gtk4_audio_init()) -- both were previously either fake
   (a hardcoded DEFAULT_SAMPLE_RATE) or simply never built, since no real
   audio driver was ever wired up before now. init_buffer_indices()/
   start_midi_clock() also moved out of here: jack_start() (src/jack.c,
   called from phasex_gtk4_audio_start()) already does both for real once
   the audio driver is actually running, the same way it does in the real
   app's main() -- doing it twice would just mean the second (real) call
   overwrites this preview's fake seed value, so there's no reason to
   keep the fake one now that real audio is wired up. */
void
phasex_gtk4_backend_init_patch_data(void) {
    init_engine_internals();
    init_patch_param_data();
    init_patch_bank(NULL);
    init_session_bank(NULL);
}
