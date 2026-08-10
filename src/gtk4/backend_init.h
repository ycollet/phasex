/*****************************************************************************
 *
 * gtk4/backend_init.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Minimal, audio-free initialization of the real (toolkit-agnostic)
 * session/bank/patch backend, for the GTK4 preview. See backend_init.c for
 * why this doesn't just call phasex.c's main() startup sequence directly.
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
#ifndef _PHASEX_GTK4_BACKEND_INIT_H_
#define _PHASEX_GTK4_BACKEND_INIT_H_

void phasex_gtk4_backend_init(void);

#endif /* _PHASEX_GTK4_BACKEND_INIT_H_ */
