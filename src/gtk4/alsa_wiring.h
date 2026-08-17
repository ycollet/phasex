/*****************************************************************************
 *
 * gtk4/alsa_wiring.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Real ALSA PCM/sequencer/rawmidi connect and driver-switch actions for
 * the GTK4 preview's menubar. See alsa_wiring.c for the real driver
 * lifecycle these wrap (stop/select/init/start), and device_enum.c for
 * where the raw_alsa_name strings these take actually come from (the
 * bracketed prefix of each menu row's display string).
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
#ifndef _PHASEX_GTK4_ALSA_WIRING_H_
#define _PHASEX_GTK4_ALSA_WIRING_H_

#include <glib.h>

/* Switches real audio output to the ALSA PCM device named
   raw_alsa_name ("hw:C,D", matching ALSA_PCM_HW_INFO::alsa_name) --
   stops whatever audio driver is currently running (e.g. JACK) and
   restarts on ALSA PCM. MIDI is untouched: audio_driver/midi_driver
   are independent selections (e.g. JACK MIDI keeps working with ALSA
   PCM audio, or vice versa). */
void alsa_wiring_select_pcm_playback(const char *raw_alsa_name);

/* Switches real MIDI input to raw ALSA rawmidi, using the single
   device named raw_alsa_name ("hw:C,D" or "hw:C,D,S", matching
   ALSA_RAWMIDI_HW_INFO::alsa_name) -- rawmidi is a one-device-at-a-time
   driver (like ALSA PCM above), not a multi-port subscribe model like
   ALSA sequencer below. */
void alsa_wiring_select_rawmidi(const char *raw_alsa_name);

/* Toggles a real ALSA sequencer MIDI subscription on/off for the port
   named raw_alsa_name ("client:port", matching
   ALSA_SEQ_PORT::alsa_name). Switches midi_driver to
   MIDI_DRIVER_ALSA_SEQ first if it isn't already (stopping whatever
   MIDI driver -- e.g. JACK -- was previously active, since ALSA
   sequencer input and JACK MIDI input can't both be the active MIDI
   driver at once), unlike JACK MIDI ports, which can all be connected
   at once alongside everything else. Returns the port's new connected
   state (TRUE if now subscribed, FALSE if now unsubscribed or if the
   port couldn't be found/subscribed at all). */
gboolean alsa_wiring_toggle_seq_port(const char *raw_alsa_name);

#endif /* _PHASEX_GTK4_ALSA_WIRING_H_ */
