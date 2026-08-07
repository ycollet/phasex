/*****************************************************************************
 *
 * gtk4/gtkknob.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * GTK4 port of src/gtkknob.c (original GtkKnob from gAlan 0.2.0, Copyright
 * (C) 1999 Tony Garnock-Jones; modifications Copyright (C) 2004-2013 Sean
 * Bolton, Pete Shorthose, William Weston).
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
#ifndef _PHASEX_GTK4_KNOB_H_
#define _PHASEX_GTK4_KNOB_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHASEX_TYPE_KNOB (phasex_knob_get_type())
G_DECLARE_FINAL_TYPE(PhasexKnob, phasex_knob, PHASEX, KNOB, GtkWidget)


/* Same sprite-sheet animation concept as the GTK2 GtkKnobAnim: one PNG
   containing every rotation frame side by side, frame_width wide each. */
typedef struct _PhasexKnobAnim {
    GdkPixbuf   *pixbuf;
    int         width;
    int         height;
    int         frame_width;
} PhasexKnobAnim;


GtkWidget      *phasex_knob_new(GtkAdjustment *adjustment, PhasexKnobAnim *anim);
GtkAdjustment  *phasex_knob_get_adjustment(PhasexKnob *knob);

PhasexKnobAnim *phasex_knob_animation_new_from_file(const char *filename,
        int frame_width, int width, int height);
void            phasex_knob_animation_free(PhasexKnobAnim *anim);

G_END_DECLS

#endif /* _PHASEX_GTK4_KNOB_H_ */
