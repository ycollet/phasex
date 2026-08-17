/*****************************************************************************
 *
 * gtk4/alsa_wiring.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Real ALSA PCM/sequencer/rawmidi driver-switch and connect actions for
 * the GTK4 preview's menubar (menubar.c), using the real alsa_pcm.c/
 * alsa_seq.c/rawmidi.c (see CMakeLists.txt's phasex_gtk4_driver_SOURCES,
 * and audio_init.c for the settings.c-owned globals those files need
 * storage for). Those three files are GTK-free, same as jack.c/
 * driver.c, so they're linked wholesale rather than re-implemented --
 * unlike device_enum.c, which only ever needed read-only enumeration
 * and deliberately avoided linking these same files for exactly that
 * reason (before real audio existed at all, linking them just for
 * enumeration wasn't worth the settings.c-globals stubbing cost; now
 * that real driver-switching needs the real thing anyway, that
 * tradeoff no longer applies).
 *
 * audio_driver and midi_driver are independent selections (driver.c):
 * switching one doesn't touch the other, so e.g. JACK audio output can
 * keep running while MIDI input switches to ALSA sequencer, or ALSA
 * PCM audio output can run alongside JACK MIDI input. Only one driver
 * can be active per role at a time, though -- selecting an ALSA
 * sequencer port stops JACK MIDI (if that was active) the same way
 * selecting rawmidi would stop ALSA sequencer.
 *
 * IMPORTANT CAVEAT: unlike jack.c (verified end to end against a real
 * jackd -d dummy server -- see audio_init.c's commit history), none of
 * this has been runtime-tested: this development machine has no real
 * ALSA hardware or software stack, and building a stub complete enough
 * to even compile alsa_pcm.c's full API surface (hw_params, mmap
 * access, writei, ...) here wasn't practical the way the earlier
 * throwaway JACK dummy-server tests were. This file is written from a
 * careful reading of the real alsa_pcm.c/alsa_seq.c/rawmidi.c/
 * gui_alsa.c, matching their real semantics as closely as possible,
 * but needs real testing against real ALSA hardware to be trusted.
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
#include <string.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include "phasex.h"
#include "config.h"
#include "driver.h"
#include "engine.h"
#include "alsa_pcm.h"
#include "alsa_seq.h"
#include "rawmidi.h"
#include "settings.h"
#include "wave.h"
#include "filter.h"
#include "alsa_wiring.h"


/*****************************************************************************
 * restart_audio_on_driver()
 *
 * Mirrors phasex_gtk4_audio_init()'s (audio_init.c) initial startup
 * sequence for the sample-rate-dependent parts, since switching to a
 * different piece of audio hardware can mean a different native sample
 * rate -- the filter/env tables built from the *old* rate would
 * otherwise silently go stale.
 *****************************************************************************/
static void
restart_audio_on_driver(int new_driver) {
    int old_sample_rate = sample_rate;

    stop_audio();
    wait_audio_stop();

    select_audio_driver(NULL, new_driver);
    sample_rate = 0;
    init_audio();
    while (sample_rate == 0) {
        init_audio();
    }

    if (sample_rate != old_sample_rate) {
        build_filter_tables();
        build_env_tables();
    }

    start_audio();
    wait_audio_start();
}


/*****************************************************************************
 * restart_midi_on_driver()
 *****************************************************************************/
static void
restart_midi_on_driver(int new_driver) {
    stop_midi();
    wait_midi_stop();

    select_midi_driver(NULL, new_driver);
    init_midi();

    start_midi();
    wait_midi_start();
}


/*****************************************************************************
 * alsa_wiring_select_pcm_playback()
 *****************************************************************************/
void
alsa_wiring_select_pcm_playback(const char *raw_alsa_name) {
    free(setting_alsa_pcm_device);
    setting_alsa_pcm_device = strdup(raw_alsa_name);

    restart_audio_on_driver(AUDIO_DRIVER_ALSA_PCM);
}


/*****************************************************************************
 * alsa_wiring_select_rawmidi()
 *****************************************************************************/
void
alsa_wiring_select_rawmidi(const char *raw_alsa_name) {
    free(setting_alsa_raw_midi_device);
    setting_alsa_raw_midi_device = strdup(raw_alsa_name);

    restart_midi_on_driver(MIDI_DRIVER_RAW_ALSA);
}


/*****************************************************************************
 * alsa_wiring_toggle_seq_port()
 *
 * Real subscribe/unsubscribe against the real alsa_seq_info, mirroring
 * gui_alsa.c's on_select_alsa_seq_port() + alsa_seq_watchdog_cycle()'s
 * pair of alsa_seq_subscribe_port()/alsa_seq_unsubscribe_port() calls,
 * but synchronous instead of via a queued watchdog-cycle request (same
 * simplification as menubar.c's real JACK MIDI port wiring): this
 * preview has no separate watchdog thread to defer to, and these calls
 * are plain ALSA client-side API calls, safe from any thread.
 *
 * alsa_seq_get_port_list() is called with the *existing*
 * alsa_seq_info->capture_ports as its list argument rather than NULL
 * (unlike alsa_seq_watchdog_cycle()'s own always-fresh-then-
 * compare refresh): passing the old list lets alsa_seq_get_port_list()
 * (via alsa_seq_port_in_list()'s existing-entry check) reuse the same
 * ALSA_SEQ_PORT nodes -- and their already-populated `subs` handles --
 * for ports still present, only allocating new nodes for ports that
 * newly appeared. That's what lets this function tell whether the
 * targeted port was already subscribed *before* refreshing the list.
 *****************************************************************************/
gboolean
alsa_wiring_toggle_seq_port(const char *raw_alsa_name) {
    ALSA_SEQ_PORT   *cur;
    gboolean        was_connected = FALSE;

    if (midi_driver != MIDI_DRIVER_ALSA_SEQ) {
        restart_midi_on_driver(MIDI_DRIVER_ALSA_SEQ);
    }
    if (alsa_seq_info == NULL) {
        return FALSE;
    }

    for (cur = alsa_seq_info->capture_ports; cur != NULL; cur = cur->next) {
        if ((strcmp(cur->alsa_name, raw_alsa_name) == 0) && (cur->subs != NULL)) {
            was_connected = TRUE;
            break;
        }
    }

    alsa_seq_info->capture_ports = alsa_seq_get_port_list(alsa_seq_info,
            (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ),
            alsa_seq_info->capture_ports);

    for (cur = alsa_seq_info->capture_ports; cur != NULL; cur = cur->next) {
        if (strcmp(cur->alsa_name, raw_alsa_name) != 0) {
            continue;
        }
        if (was_connected) {
            alsa_seq_unsubscribe_port(alsa_seq_info, cur);
        } else {
            alsa_seq_subscribe_port(alsa_seq_info, cur, NULL);
        }
        return (cur->subs != NULL) ? TRUE : FALSE;
    }

    /* Port disappeared since the menu was built (unplugged/closed). */
    return FALSE;
}
