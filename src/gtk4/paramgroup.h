/*****************************************************************************
 *
 * gtk4/paramgroup.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * GTK4 port of gui_layout.c's create_param_group() + gui_param.c's
 * create_param_input(), scoped to the "LFO-1" group (src/gui_layout.c's
 * param_group[] entry with param_list = { PARAM_LFO1_POLARITY,
 * PARAM_LFO1_FREQ_BASE, PARAM_LFO1_WAVE, PARAM_LFO1_RATE,
 * PARAM_LFO1_INIT_PHASE, PARAM_LFO1_TRANSPOSE, PARAM_LFO1_PITCHBEND,
 * PARAM_LFO1_VOICE_AM }) as a second, denser test of the ported knob
 * widget and the table-of-controls layout pattern used by every other
 * param group. See paramgroup.c for what's simplified in this pass.
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
#ifndef _PHASEX_GTK4_PARAMGROUP_H_
#define _PHASEX_GTK4_PARAMGROUP_H_

#include <gtk/gtk.h>

GtkWidget *create_lfo1_group(void);

#endif /* _PHASEX_GTK4_PARAMGROUP_H_ */
