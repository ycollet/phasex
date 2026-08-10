/*****************************************************************************
 *
 * gtk4/device_enum.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Real ALSA/JACK device and port enumeration, ported from
 * alsa_pcm.c:alsa_pcm_get_hw_list(), alsa_seq.c:alsa_seq_get_port_list()
 * (plus gui_alsa.c:on_alsa_menu_activate()'s HW/SW split and PHASEX-
 * client filtering), rawmidi.c:alsa_rawmidi_get_hw_list(), and
 * jack.c:jack_get_midi_port_list().
 *
 * Those files aren't linked into this preview -- alsa_pcm.c/alsa_seq.c/
 * rawmidi.c/jack.c each carry a lot of driver-lifecycle state
 * (capture/playback handles, watchdog threads, settings.c/driver.c
 * globals) that would need extensive stubbing to even link, the same
 * problem backend_init.c already documents for jack.c as a whole. The
 * actual enumeration logic doesn't touch any of that -- alsa_pcm_get_
 * hw_list()/alsa_rawmidi_get_hw_list() are self-contained (they open
 * their own per-card handles), and alsa_seq_get_port_list()/jack_get_
 * midi_port_list() only need a live snd_seq_t/jack_client_t handle,
 * which this file opens its own throwaway one for -- so the logic is
 * ported here directly against the real ALSA/JACK headers (already on
 * this target's include path, see CMakeLists.txt) instead.
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
#include <alsa/asoundlib.h>
#include <jack/jack.h>
#include "device_enum.h"


/* PARAM_TYPE_BOOL/BBOX: matches gui_alsa.c's "[alsa_name] card/client:
   device/port" label format, so a Linux user comparing this preview
   against the real app's ALSA/JACK menus sees the same identifiers. */

GPtrArray *
device_enum_alsa_pcm_playback(void) {
    GPtrArray           *result = g_ptr_array_new();
    char                name[32];
    snd_ctl_t           *handle;
    snd_ctl_card_info_t *cardinfo;
    snd_pcm_info_t      *pcminfo;
    int                 card_num;
    int                 device_num;

    snd_ctl_card_info_alloca(&cardinfo);
    snd_pcm_info_alloca(&pcminfo);

    card_num = -1;
    while (snd_card_next(&card_num) >= 0) {
        if (card_num < 0) {
            break;
        }
        snprintf(name, sizeof(name), "hw:%d", card_num);
        if (snd_ctl_open(&handle, name, 0) < 0) {
            continue;
        }
        if (snd_ctl_card_info(handle, cardinfo) < 0) {
            snd_ctl_close(handle);
            continue;
        }
        device_num = -1;
        while (snd_ctl_pcm_next_device(handle, &device_num) >= 0) {
            if (device_num < 0) {
                break;
            }
            snd_pcm_info_set_device(pcminfo, (unsigned int) device_num);
            snd_pcm_info_set_subdevice(pcminfo, 0);
            snd_pcm_info_set_stream(pcminfo, SND_PCM_STREAM_PLAYBACK);
            if (snd_ctl_pcm_info(handle, pcminfo) < 0) {
                continue;
            }
            g_ptr_array_add(result, g_strdup_printf("[hw:%d,%d] %s: %s",
                    card_num, device_num,
                    snd_ctl_card_info_get_name(cardinfo),
                    snd_pcm_info_get_name(pcminfo)));
        }
        snd_ctl_close(handle);
    }

    return result;
}


GPtrArray *
device_enum_alsa_rawmidi(void) {
    GPtrArray           *result = g_ptr_array_new();
    char                name[32];
    snd_ctl_t           *handle;
    snd_rawmidi_info_t  *info;
    int                 card_num;
    int                 device_num;

    card_num = -1;
    while (snd_card_next(&card_num) >= 0) {
        if (card_num < 0) {
            break;
        }
        snprintf(name, sizeof(name), "hw:%d", card_num);
        if (snd_ctl_open(&handle, name, 0) < 0) {
            continue;
        }
        device_num = -1;
        while (snd_ctl_rawmidi_next_device(handle, &device_num) >= 0) {
            if (device_num < 0) {
                break;
            }
            snd_rawmidi_info_alloca(&info);
            memset(info, 0, snd_rawmidi_info_sizeof());
            snd_rawmidi_info_set_device(info, (unsigned int) device_num);
            snd_rawmidi_info_set_subdevice(info, 0);
            snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_INPUT);
            if (snd_ctl_rawmidi_info(handle, info) < 0) {
                continue;
            }
            g_ptr_array_add(result, g_strdup_printf("[hw:%d,%d] %s: %s",
                    card_num, device_num,
                    snd_rawmidi_info_get_name(info),
                    snd_rawmidi_info_get_subdevice_name(info)));
        }
        snd_ctl_close(handle);
    }

    return result;
}


/* Shared by device_enum_alsa_seq_hw()/_sw(): opens its own throwaway
   sequencer client (same open call as alsa_seq.c's open_alsa_seq_in())
   just long enough to enumerate, then closes it -- this preview never
   keeps a real MIDI input connection open. want_hardware selects the
   SND_SEQ_PORT_TYPE_HARDWARE split gui_alsa.c's on_alsa_menu_activate()
   makes between the "ALSA Seq HW" and "ALSA Seq SW" submenus. */
static GPtrArray *
enum_alsa_seq_ports(gboolean want_hardware) {
    GPtrArray               *result = g_ptr_array_new();
    snd_seq_t               *seq;
    snd_seq_client_info_t   *cinfo;
    snd_seq_port_info_t     *pinfo;
    int                     client;

    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK) < 0) {
        return result;
    }

    snd_seq_client_info_alloca(&cinfo);
    snd_seq_port_info_alloca(&pinfo);

    snd_seq_client_info_set_client(cinfo, -1);
    while (snd_seq_query_next_client(seq, cinfo) >= 0) {
        const char *client_name = snd_seq_client_info_get_name(cinfo);

        client = snd_seq_client_info_get_client(cinfo);

        snd_seq_port_info_set_client(pinfo, client);
        snd_seq_port_info_set_port(pinfo, -1);
        while (snd_seq_query_next_port(seq, pinfo) >= 0) {
            unsigned int    type = snd_seq_port_info_get_type(pinfo);
            unsigned int    caps = snd_seq_port_info_get_capability(pinfo);
            gboolean        is_hardware;

            if (!(type & SND_SEQ_PORT_TYPE_MIDI_GENERIC)) {
                continue;
            }
            if ((caps & (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ)) !=
                    (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ)) {
                continue;
            }
            if ((strstr(client_name, "PHASEX") != NULL) || (strstr(client_name, "phasex") != NULL)) {
                continue;
            }
            if (snd_seq_port_info_get_client(pinfo) == SND_SEQ_ADDRESS_SUBSCRIBERS) {
                continue;
            }

            is_hardware = (type & SND_SEQ_PORT_TYPE_HARDWARE) == SND_SEQ_PORT_TYPE_HARDWARE;
            if (is_hardware != want_hardware) {
                continue;
            }

            g_ptr_array_add(result, g_strdup_printf("[%d:%d] %s: %s",
                    client, snd_seq_port_info_get_port(pinfo),
                    client_name, snd_seq_port_info_get_name(pinfo)));
        }
    }

    snd_seq_close(seq);

    return result;
}


GPtrArray *
device_enum_alsa_seq_hw(void) {
    return enum_alsa_seq_ports(TRUE);
}


GPtrArray *
device_enum_alsa_seq_sw(void) {
    return enum_alsa_seq_ports(FALSE);
}


/* Opens its own throwaway JACK client (JackNoStartServer: query
   whatever's running, never launch a new jackd just to list ports) --
   matches jack_get_midi_port_list()'s "any output MIDI port" query via
   jack_get_ports(), simplified to pass JACK_DEFAULT_MIDI_TYPE straight
   into jack_get_ports()'s type filter instead of the original's
   separate jack_port_type()-per-port check. */
GPtrArray *
device_enum_jack_midi(void) {
    GPtrArray       *result = g_ptr_array_new();
    jack_client_t   *client;
    jack_status_t   status;
    const char      **port_names;
    int             i;

    client = jack_client_open("phasex-gtk4-preview-enum", JackNoStartServer, &status);
    if (client == NULL) {
        return result;
    }

    port_names = jack_get_ports(client, NULL, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput);
    if (port_names != NULL) {
        for (i = 0; port_names[i] != NULL; i++) {
            g_ptr_array_add(result, g_strdup(port_names[i]));
        }
        free(port_names);
    }

    jack_client_close(client);

    return result;
}
