/*****************************************************************************
 *
 * gtk4/audio_init.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Real JACK audio/MIDI startup for the GTK4 preview, using the real
 * driver.c/jack.c/jack_midi.c/jack_transport.c (see CMakeLists.txt's
 * phasex_gtk4_driver_SOURCES) instead of the fake DEFAULT_SAMPLE_RATE
 * seed backend_init.c used before this file existed. Those four files
 * are GTK-free (checked: zero gtk_... / Gtk... references) and mostly
 * self-contained, but they still need a few things phasex.c/settings.c
 * would normally provide:
 *
 *   - Storage for phasex.c-owned thread handles and CLI port-name
 *     globals (audio_thread_p, midi_thread_p, jack_thread_p,
 *     audio_input_ports, audio_output_ports) -- phasex.c itself isn't
 *     linked, same reasoning as backend_init.c's path globals.
 *
 *   - Storage for the handful of settings.c-owned config globals
 *     driver.c/jack.c read directly (setting_jack_autoconnect,
 *     setting_jack_multi_out, setting_jack_transport_mode,
 *     setting_buffer_latency, setting_audio_driver, setting_midi_driver)
 *     -- settings.c itself is GTK2-coupled (pushes changes live into
 *     GTK2 widgets), not linked. Values match settings.c's own
 *     defaults, then get overwritten for real by select_audio_driver()/
 *     select_midi_driver() below.
 *
 *   - init_rt_mutex(): a 10-line phasex.c helper that driver.c calls
 *     directly, copied verbatim rather than pulling in all of phasex.c
 *     for one function.
 *
 *   - build_ccmatrix(): populates the CC-to-param lookup table
 *     midi_process.c consults for every incoming MIDI CC message.
 *     Normally lives in midimap.c and is called from driver.c's
 *     init_midi(), but midimap.c has one GTK2 call elsewhere in the
 *     file (set_midi_channel_for_part()) we don't need, and everything
 *     build_ccmatrix() itself touches (get_param_info_by_id(), the
 *     ccmatrix[] storage already stubbed in backend_init.c) is already
 *     linked -- so it's copied here rather than linking all of
 *     midimap.c for one self-contained function.
 *
 *   - Storage for the settings.c-owned config globals alsa_pcm.c/
 *     alsa_seq.c/rawmidi.c read directly (setting_alsa_pcm_device,
 *     setting_alsa_seq_port, setting_alsa_raw_midi_device,
 *     setting_oss_midi_device, setting_generic_midi_device,
 *     setting_sample_rate, setting_buffer_period_size,
 *     setting_force_16bit, setting_enable_mmap, setting_enable_inputs,
 *     setting_audio_priority, setting_midi_priority) -- same reasoning
 *     as the JACK-specific ones above. alsa_pcm.c/alsa_seq.c/rawmidi.c
 *     are linked wholesale too (like jack.c -- see CMakeLists.txt),
 *     since they're just as GTK-free; alsa_wiring.c is where
 *     select_audio_driver(ALSA_PCM)/select_midi_driver(ALSA_SEQ/
 *     RAW_ALSA) actually get used.
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
#include <sched.h>
#include <pthread.h>
#include "phasex.h"
#include "config.h"
#include "driver.h"
#include "jack_transport.h"
#include "engine.h"
#include "wave.h"
#include "filter.h"
#include "param.h"
#include "midimap.h"
#include "settings.h"
#include "audio_init.h"


/* Storage for phasex.c-owned globals jack.c/driver.c reference as
   extern (phasex.h) -- phasex.c itself isn't linked (see
   backend_init.c's file header for why). */
pthread_t   audio_thread_p         = 0;
pthread_t   midi_thread_p          = 0;
pthread_t   jack_thread_p          = 0;
char        *audio_input_ports     = NULL;
char        *audio_output_ports    = NULL;


/* Storage for settings.c-owned config globals driver.c/jack.c read
   directly. Values match settings.c's own defaults; setting_audio_driver/
   setting_midi_driver get overwritten for real by select_audio_driver()/
   select_midi_driver() in phasex_gtk4_audio_init() below. */
int             setting_audio_driver           = AUDIO_DRIVER_NONE;
int             setting_midi_driver             = MIDI_DRIVER_NONE;
unsigned int    setting_buffer_latency          = DEFAULT_LATENCY_PERIODS;
int             setting_jack_multi_out          = 0;
int             setting_jack_autoconnect        = 1;
int             setting_jack_transport_mode     = JACK_TRANSPORT_OFF;


/* Storage for a few more settings.c/phasex.c-owned globals driver.c's
   dead-but-linked config-change/status-reporting code paths (never
   reached here: config_changed never gets set to 1, since there's no
   config dialog to change anything in) reference directly. */
int             config_changed                 = 0;
int             sample_rate_changed             = 0;
GtkWidget       *config_dialog                  = NULL;
GtkWidget       *audio_status_label             = NULL;
char            audio_driver_status_msg[256]    = "";


/* phasex_watchdog() (driver.c, never called here -- see
   phasex_gtk4_audio_start()) unconditionally polls this every cycle for
   NSM (Non/New Session Manager) support. Real implementation lives in
   nsm.c, which pulls in liblo -- not worth linking for a function this
   preview never actually calls. */
void
nsm_check_events(void) {
}


/* Real implementation, minus the GTK2 config-dialog branch -- ported
   from settings.c's set_engine_priority(widget, data), which only reads
   `data` (a GtkSpinButton) when non-NULL. start_engine_threads()
   (engine.c) always calls this as set_engine_priority(NULL, NULL), so
   that branch never applies here. This one actually matters, unlike
   backend_init.c's other never-called stand-ins: without it, the real
   engine thread never gets real-time scheduling priority at all, so it
   can't keep up with JACK's per-cycle deadline once real audio is
   running -- every single process cycle XRuns ("client = phasex was
   not finished, state = Triggered" in the JACK server's own log). */
void
set_engine_priority(GtkWidget *widget, gpointer data) {
    struct sched_param  schedparam;
    unsigned int         i;

    (void) widget;
    (void) data;

    memset(&schedparam, 0, sizeof(struct sched_param));
    schedparam.sched_priority = setting_engine_priority;

    for (i = 0; i < MAX_PARTS; i++) {
        pthread_setschedparam(engine_thread_p[i], setting_sched_policy, &schedparam);
    }
}


/* Copied verbatim from phasex.c -- see file header comment. */
void
init_rt_mutex(pthread_mutex_t *mutex, int rt) {
    pthread_mutexattr_t     attr;

    pthread_mutexattr_init(&attr);
#ifdef HAVE_PTHREAD_MUTEXATTR_SETPROTOCOL
    if (rt) {
        pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
    }
#endif
    pthread_mutex_init(mutex, &attr);
}


/* Copied from midimap.c's build_ccmatrix() -- see file header comment. */
void
build_ccmatrix(void) {
    PARAM_INFO      *param_info;
    int             cc;
    unsigned int    id;
    unsigned int    j;

    for (cc = 0; cc < 128; cc++) {
        for (j = 0; j < 16; j++) {
            ccmatrix[cc][j] = -1;
        }
    }

    for (id = 0; id < NUM_PARAMS; id++) {
        param_info = get_param_info_by_id(id);
        cc = param_info->cc_num;
        if ((cc >= 0) && (cc < 128)) {
            j = 0;
            while ((ccmatrix[cc][j] >= 0) && (j < 16)) {
                j++;
            }
            if (j < 16) {
                ccmatrix[cc][j] = (int) id;
            }
        }
    }
}


/* Storage for settings.c-owned config globals alsa_pcm.c/alsa_seq.c/
   rawmidi.c read directly -- see file header comment. Values match
   settings.c's own defaults. The four device/port-name strings
   (initially NULL, meaning "use the driver's own default") are set for
   real by alsa_wiring.c when the user picks a specific device/port
   from the ALSA menu. */
char            *setting_alsa_pcm_device       = NULL;
char            *setting_alsa_seq_port         = NULL;
char            *setting_alsa_raw_midi_device  = NULL;
char            *setting_oss_midi_device       = NULL;
char            *setting_generic_midi_device   = NULL;
int             setting_sample_rate            = DEFAULT_SAMPLE_RATE;
unsigned int    setting_buffer_period_size     = DEFAULT_BUFFER_PERIOD_SIZE;
int             setting_force_16bit            = 0;
int             setting_enable_mmap            = 0;
int             setting_enable_inputs          = 0;
int             setting_audio_priority         = AUDIO_THREAD_PRIORITY;
int             setting_midi_priority          = MIDI_THREAD_PRIORITY;


/*****************************************************************************
 * phasex_gtk4_audio_init()
 *
 * Force-selects JACK for both audio and MIDI at startup (this preview
 * has no config file/dialog to remember a prior choice -- the user can
 * still switch to ALSA PCM/sequencer/rawmidi afterward from the
 * ALSA menu, see alsa_wiring.c), then mirrors phasex.c's real main()
 * sequence
 * for everything sample-rate related: build the sample-rate-independent
 * lookup tables first, block until a real JACK connection supplies a
 * real sample rate (init_audio() internally retries JACK for ~3s, falls
 * back to ALSA, then to AUDIO_DRIVER_NONE -- looping here matches
 * phasex.c's own `while (sample_rate == 0) init_audio();`, including
 * that it can block indefinitely if no JACK/ALSA server is ever
 * reachable, same as the real app), then init_midi() (which also builds
 * the real ccmatrix via build_ccmatrix() above), then finally the two
 * lookup tables that need a real sample rate.
 *****************************************************************************/
void
phasex_gtk4_audio_init(void) {
    select_audio_driver(NULL, AUDIO_DRIVER_JACK);
    select_midi_driver(NULL, MIDI_DRIVER_JACK);

    init_rt_mutex(&sample_rate_mutex, 1);

    build_freq_table();
    build_freq_shift_table();
    build_waveform_tables();
    build_mix_table();
    build_pan_table();
    build_gain_table();
    build_velocity_gain_table();
    build_keyfollow_table();

    init_audio();
    while (sample_rate == 0) {
        init_audio();
    }

    init_midi();

    build_filter_tables();
    build_env_tables();
}


/*****************************************************************************
 * phasex_gtk4_audio_start()
 *****************************************************************************/
void
phasex_gtk4_audio_start(void) {
    start_engine_threads();
    start_audio();
    wait_audio_start();
    start_midi();
    wait_midi_start();
}


/*****************************************************************************
 * phasex_gtk4_audio_stop()
 *****************************************************************************/
void
phasex_gtk4_audio_stop(void) {
    stop_midi();
    wait_midi_stop();
    stop_audio();
    stop_engine();
}
