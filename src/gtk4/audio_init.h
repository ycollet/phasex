/*****************************************************************************
 *
 * gtk4/audio_init.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
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
#ifndef _PHASEX_GTK4_AUDIO_INIT_H_
#define _PHASEX_GTK4_AUDIO_INIT_H_

/* Blocks until a real JACK client is open and a real sample rate is
   negotiated (see audio_init.c for why this can block indefinitely if
   no JACK/ALSA server is reachable -- same as the real app). Must be
   called after phasex_gtk4_backend_init(). */
void phasex_gtk4_audio_init(void);

/* Starts the engine threads, the real audio driver, and real MIDI
   input. Must be called after phasex_gtk4_audio_init(). */
void phasex_gtk4_audio_start(void);

/* Cleanly stops audio/MIDI/engine threads and closes the JACK client.
   Call before process exit so PHASEX doesn't leave a stale client
   registered with the JACK server / session manager. */
void phasex_gtk4_audio_stop(void);

#endif /* _PHASEX_GTK4_AUDIO_INIT_H_ */
