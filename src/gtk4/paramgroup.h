/*****************************************************************************
 *
 * gtk4/paramgroup.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * GTK4 port of gui_layout.c's create_param_group() + gui_param.c's
 * create_param_input(), generalized to build any of the real param
 * groups (see param_groups_data.c) from real PARAM_INFO/PARAM state
 * (param.c, already linked) rather than hardcoded specs. See
 * paramgroup.c for what's simplified in this pass.
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

/* Builds the Nth real param group (0..NUM_PARAM_GROUPS-1, see
   param_groups_data.c's init_param_groups()) as a framed grid of
   knobs/buttons, one row per parameter. */
GtkWidget *create_param_group_view(int group_index);

#endif /* _PHASEX_GTK4_PARAMGROUP_H_ */
