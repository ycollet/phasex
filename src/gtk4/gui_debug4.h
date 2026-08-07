/*****************************************************************************
 *
 * gtk4/gui_debug4.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * GTK4 counterpart to src/gui_debug.c, for GTK2/GTK4 visual-parity dumps.
 * See gui_debug4.c for the API differences this had to work around.
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
#ifndef _PHASEX_GTK4_GUI_DEBUG4_H_
#define _PHASEX_GTK4_GUI_DEBUG4_H_

#include <gtk/gtk.h>

void gui_debug4_dump_window(GtkWidget *toplevel);

#endif /* _PHASEX_GTK4_GUI_DEBUG4_H_ */
