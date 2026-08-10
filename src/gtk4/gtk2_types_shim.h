/*****************************************************************************
 *
 * gtk4/gtk2_types_shim.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Some shared backend headers (param.h, settings.h, src/gtkknob.h) declare
 * a few fields using GTK2-only types (GtkObject, GdkBitmap, GdkGC) that are
 * purely GUI bookkeeping -- e.g. PARAM_INFO::adj/knob, GtkKnob::mask/mask_gc
 * -- and are never read or written by the pure backend files linked into
 * this preview (bank.c, session.c, patch.c, param.c, ...). Those types don't
 * exist in GTK4 at all (GtkObject was removed in GTK3; GdkBitmap/GdkGC are
 * GTK2-only), so this force-included shim (see CMakeLists.txt's -include
 * flag) provides forward-declared, never-defined stand-ins: enough for the
 * pointer-only fields to compile, without needing to touch any shared
 * header (which would affect the GTK2 build too) or link real GTK2/GDK.
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
#ifndef _PHASEX_GTK4_GTK2_TYPES_SHIM_H_
#define _PHASEX_GTK4_GTK2_TYPES_SHIM_H_

typedef struct _GtkObject           GtkObject;
typedef struct _GdkBitmap           GdkBitmap;
typedef struct _GdkGC               GdkGC;
typedef struct _GdkEventButton      GdkEventButton;
typedef struct _GdkEventFocus       GdkEventFocus;
typedef struct _GdkEventWindowState GdkEventWindowState;
typedef struct _GtkTable            GtkTable;
typedef int                         GtkAttachOptions;

#endif /* _PHASEX_GTK4_GTK2_TYPES_SHIM_H_ */
