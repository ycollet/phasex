/*****************************************************************************
 *
 * gtk4/device_enum.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Real ALSA device and port enumeration for the GTK4 preview's
 * menubar, ported from alsa_pcm.c/alsa_seq.c/rawmidi.c. See
 * device_enum.c for why those files aren't linked directly (and for
 * why JACK MIDI port enumeration isn't here at all anymore).
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
#ifndef _PHASEX_GTK4_DEVICE_ENUM_H_
#define _PHASEX_GTK4_DEVICE_ENUM_H_

#include <glib.h>

/* Each returns a newly-allocated GPtrArray of newly-allocated display
   strings (caller frees with g_ptr_array_free(arr, TRUE)) -- empty
   (never NULL) if none are found, or if the driver/library can't be
   reached at all (e.g. no ALSA on this machine). */
GPtrArray *device_enum_alsa_pcm_playback(void);
GPtrArray *device_enum_alsa_seq_hw(void);
GPtrArray *device_enum_alsa_seq_sw(void);
GPtrArray *device_enum_alsa_rawmidi(void);

#endif /* _PHASEX_GTK4_DEVICE_ENUM_H_ */
