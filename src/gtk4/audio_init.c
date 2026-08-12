/*****************************************************************************
 *
 * gtk4/audio_init.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Real JACK audio/MIDI startup for the GTK4 preview, using the real
 * driver.c/jack.c/jack_midi.c/jack_transport.c (see CMakeLists.txt's
 * phasex_gtk4_jack_SOURCES) instead of the fake DEFAULT_SAMPLE_RATE seed
 * backend_init.c used before this file existed. Those four files are
 * GTK-free (checked: zero gtk_... / Gtk... references) and mostly
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
 *   - Stand-ins for the ALSA PCM/sequencer/rawmidi driver entry points
 *     (alsa_pcm_init/_thread/_watchdog_cycle/_get_hw_list/_hw_list_free,
 *     alsa_seq_init/_thread/_watchdog_cycle/_get_port_list/_port_free,
 *     rawmidi_init/_thread/_watchdog_cycle,
 *     alsa_rawmidi_get_hw_list/_hw_info_free). select_audio_driver()/
 *     select_midi_driver() (driver.c) take the *address* of every
 *     driver's entry points unconditionally, to populate their
 *     DRIVER_FUNC/THREAD_FUNC/DRIVER_VOID_FUNC tables for whichever
 *     driver ID gets selected at runtime -- so all of them need to
 *     exist at link time even though phasex_gtk4_audio_init() below
 *     always force-selects JACK for both audio and MIDI, and none of
 *     these ALSA entry points are ever actually called. The real
 *     implementations live in alsa_pcm.c/alsa_seq.c/rawmidi.c, which
 *     (like settings.c) carry a lot of driver-lifecycle state this
 *     preview has no use for -- same tradeoff device_enum.c documents
 *     for why it re-implements enumeration instead of linking those
 *     files wholesale.
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


/* ALSA PCM/sequencer/rawmidi dead-path stand-ins -- see file header
   comment for why these must exist even though they're never called.
   Struct tags match the real ones (alsa_pcm.h/alsa_seq.h/rawmidi.h) for
   readability, but are kept opaque here rather than including those
   headers: alsa_seq.h pulls in a bare "asoundlib.h" (unlike alsa_pcm.h's
   "alsa/asoundlib.h"), an existing inconsistency in the real headers
   that isn't this file's concern to fix, and none of these stand-ins
   need the real struct layout since they never touch one. */
struct alsa_pcm_info;
struct alsa_pcm_hw_info;
struct alsa_seq_port;
struct alsa_seq_info;
struct alsa_rawmidi_hw_info;
struct rawmidi_info;

/* Data globals (not just function pointers) that scan_audio_and_midi()/
   query_audio_driver_status() (driver.c -- informational status/scan
   helpers this preview never calls) read directly. */
struct alsa_pcm_info            *alsa_pcm_info          = NULL;
char                             *alsa_pcm_device        = NULL;
struct alsa_pcm_hw_info         *alsa_pcm_capture_hw     = NULL;
struct alsa_pcm_hw_info         *alsa_pcm_playback_hw    = NULL;
int                              alsa_pcm_hw_changed     = 0;
unsigned long                    alsa_pcm_buffer_size    = 0;
unsigned long                    alsa_pcm_period_size    = 0;
unsigned int                     alsa_pcm_format_bits    = 0;
struct alsa_seq_info            *alsa_seq_info           = NULL;
int                              alsa_seq_ports_changed  = 0;
struct rawmidi_info              *rawmidi_info            = NULL;
struct alsa_rawmidi_hw_info      *alsa_rawmidi_hw         = NULL;
int                              alsa_rawmidi_hw_changed = 0;

int
alsa_pcm_init(void) {
    return -1;
}


void *
alsa_pcm_thread(void *arg) {
    (void) arg;
    return NULL;
}


void
alsa_pcm_watchdog_cycle(void) {
}


struct alsa_pcm_hw_info *
alsa_pcm_get_hw_list(int stream) {
    (void) stream;
    return NULL;
}


void
alsa_pcm_hw_list_free(struct alsa_pcm_hw_info *hw_list) {
    (void) hw_list;
}


int
alsa_seq_init(void) {
    return -1;
}


void *
alsa_seq_thread(void *arg) {
    (void) arg;
    return NULL;
}


void
alsa_seq_watchdog_cycle(void) {
}


struct alsa_seq_port *
alsa_seq_get_port_list(struct alsa_seq_info *midi, unsigned int caps, struct alsa_seq_port *orig_list) {
    (void) midi;
    (void) caps;
    (void) orig_list;
    return NULL;
}


void
alsa_seq_port_free(struct alsa_seq_port *portinfo) {
    (void) portinfo;
}


int
rawmidi_init(void) {
    return -1;
}


void *
rawmidi_thread(void *arg) {
    (void) arg;
    return NULL;
}


void
rawmidi_watchdog_cycle(void) {
}


struct alsa_rawmidi_hw_info *
alsa_rawmidi_get_hw_list(void) {
    return NULL;
}


void
alsa_rawmidi_hw_info_free(struct alsa_rawmidi_hw_info *hwinfo) {
    (void) hwinfo;
}


/*****************************************************************************
 * phasex_gtk4_audio_init()
 *
 * Force-selects JACK for both audio and MIDI (this preview has no
 * config UI to choose a driver yet -- see menubar.c's ALSA/JACK device
 * menus, still read-only), then mirrors phasex.c's real main() sequence
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
