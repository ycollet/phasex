/*****************************************************************************
 *
 * gui_debug.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 2012-2013 William Weston <whw@linuxmail.org>
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
#ifndef _PHASEX_GUI_DEBUG_H_
#define _PHASEX_GUI_DEBUG_H_

#include <gtk/gtk.h>


/* Recursively dumps the position, size, and colors of every widget under
   `toplevel` to stdout, one line per widget.  Intended to be diffed against
   an equivalent dump taken from a future GTK4 build, to check that a
   GTK2->GTK4 port hasn't changed the look of the UI.  Compiled in only when
   the CMake option PHASEX_GUI_DEBUG is ON; a no-op otherwise, so call sites
   never need an #ifdef. */
void gui_debug_dump_window(GtkWidget *toplevel);


#endif /* _PHASEX_GUI_DEBUG_H_ */
