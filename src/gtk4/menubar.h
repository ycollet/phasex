/*****************************************************************************
 *
 * gtk4/menubar.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * GTK4 port of src/gui_menubar.c. See menubar.c for the architectural
 * gap this closes: GtkMenuBar/GtkMenu/GtkItemFactory don't exist in
 * GTK4 at all.
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
#ifndef _PHASEX_GTK4_MENUBAR_H_
#define _PHASEX_GTK4_MENUBAR_H_

#include <gtk/gtk.h>

/* Registers the "win.*" actions on `window` and returns a GtkPopoverMenuBar
   built from the corresponding GMenu model, ready to be packed at the top
   of the main vbox. */
GtkWidget *create_menubar(GtkWindow *window);

#endif /* _PHASEX_GTK4_MENUBAR_H_ */
