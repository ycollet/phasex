/*****************************************************************************
 *
 * gtk4/navbar.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * GTK4 port of src/gui_navbar.c's create_navbar(), scoped to the
 * PHASEX_NUM_PARTS=1 layout (the default build), for a visual/layout/color
 * parity comparison against the GTK2 GUI_DEBUG baseline. Not yet wired to
 * the real session/bank/patch backend -- see navbar.c for details.
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
#ifndef _PHASEX_GTK4_NAVBAR_H_
#define _PHASEX_GTK4_NAVBAR_H_

#include <gtk/gtk.h>

GtkWidget *create_navbar(void);

#endif /* _PHASEX_GTK4_NAVBAR_H_ */
