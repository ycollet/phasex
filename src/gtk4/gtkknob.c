/*****************************************************************************
 *
 * gtk4/gtkknob.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * GTK4 port of src/gtkknob.c.  The drawing (pixbuf-blit of a pre-rendered
 * frame from a sprite-sheet PNG) and the mouse-drag math are carried over
 * essentially unchanged; only the widget boilerplate and event plumbing had
 * to change for GTK4:
 *   - size_request/size_allocate signals    -> measure() vfunc
 *   - expose_event + GdkGC                  -> snapshot() vfunc, via
 *                                               gtk_snapshot_append_cairo()
 *                                               (still real Cairo, so the
 *                                               original gdk_cairo_set_source
 *                                               _pixbuf()/cairo_paint() call
 *                                               carries over verbatim)
 *   - button_press/release/motion signals   -> GtkGestureDrag (one per
 *                                               mouse button, since each
 *                                               button has different
 *                                               semantics) + GtkGestureClick
 *                                               for the middle-button reset
 *   - scroll_event signal                   -> GtkEventControllerScroll
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
#include <math.h>
#include "gtkknob.h"


#ifndef M_1_PI
# define M_1_PI 0.31830988618379067154
#endif


struct _PhasexKnob {
    GtkWidget       parent_instance;

    PhasexKnobAnim  *anim;
    int             width;
    int             height;
    int             frame_offset;

    GtkAdjustment   *adjustment;
    double          old_value;
    double          old_lower;
    double          old_upper;

    /* drag bookkeeping, shared by the left- and right-button drag
       gestures (only one is ever active at a time in practice) */
    double          saved_x;
    double          saved_y;
    gboolean        drag_moved;
};

G_DEFINE_TYPE(PhasexKnob, phasex_knob, GTK_TYPE_WIDGET)


static void phasex_knob_set_frame_offset(PhasexKnob *knob, double value);
static void phasex_knob_update(PhasexKnob *knob);
static void phasex_knob_update_mouse(PhasexKnob *knob, double x, double y, gboolean absolute);


/*****************************************************************************
 * adjustment signal handlers
 *****************************************************************************/
static void
on_adjustment_changed(GtkAdjustment *adjustment, gpointer data) {
    PhasexKnob  *knob = PHASEX_KNOB(data);
    double      value  = gtk_adjustment_get_value(adjustment);
    double      lower  = gtk_adjustment_get_lower(adjustment);
    double      upper  = gtk_adjustment_get_upper(adjustment);

    if ((knob->old_value != value) || (knob->old_lower != lower) || (knob->old_upper != upper)) {
        phasex_knob_update(knob);
        knob->old_value = value;
        knob->old_lower = lower;
        knob->old_upper = upper;
    }
}


static void
on_adjustment_value_changed(GtkAdjustment *adjustment, gpointer data) {
    PhasexKnob  *knob  = PHASEX_KNOB(data);
    double      value  = gtk_adjustment_get_value(adjustment);
    double      lower  = gtk_adjustment_get_lower(adjustment);
    double      upper  = gtk_adjustment_get_upper(adjustment);

    if (value > upper) {
        value = upper;
        gtk_adjustment_set_value(adjustment, value);
    } else if (value < lower) {
        value = lower;
        gtk_adjustment_set_value(adjustment, value);
    }
    if (knob->old_value != value) {
        phasex_knob_update(knob);
        knob->old_value = value;
    }
}


/*****************************************************************************
 * drag / click / scroll gesture handlers
 *****************************************************************************/
static void
on_drag_begin(GtkGestureDrag *drag, double UNUSED_x, double UNUSED_y, gpointer data) {
    PhasexKnob  *knob = PHASEX_KNOB(data);
    double      start_x;
    double      start_y;

    (void) UNUSED_x;
    (void) UNUSED_y;

    gtk_gesture_drag_get_start_point(drag, &start_x, &start_y);
    knob->saved_x   = start_x;
    knob->saved_y   = start_y;
    knob->drag_moved = FALSE;
}


static void
on_drag_update_absolute(GtkGestureDrag *drag, double offset_x, double offset_y, gpointer data) {
    PhasexKnob  *knob = PHASEX_KNOB(data);
    double      start_x;
    double      start_y;

    gtk_gesture_drag_get_start_point(drag, &start_x, &start_y);
    if ((offset_x != 0.0) || (offset_y != 0.0)) {
        knob->drag_moved = TRUE;
    }
    phasex_knob_update_mouse(knob, start_x + offset_x, start_y + offset_y, TRUE);
}


static void
on_drag_update_relative(GtkGestureDrag *drag, double offset_x, double offset_y, gpointer data) {
    PhasexKnob  *knob = PHASEX_KNOB(data);
    double      start_x;
    double      start_y;

    gtk_gesture_drag_get_start_point(drag, &start_x, &start_y);
    if ((offset_x != 0.0) || (offset_y != 0.0)) {
        knob->drag_moved = TRUE;
    }
    phasex_knob_update_mouse(knob, start_x + offset_x, start_y + offset_y, FALSE);
}


static void
on_drag_end_decrement(GtkGestureDrag *UNUSED_drag, double UNUSED_x, double UNUSED_y, gpointer data) {
    PhasexKnob  *knob = PHASEX_KNOB(data);

    (void) UNUSED_drag;
    (void) UNUSED_x;
    (void) UNUSED_y;

    /* A plain click (no drag motion) steps the value down by a page
       increment, matching the GTK2 button-release behavior. */
    if (!knob->drag_moved) {
        double value = gtk_adjustment_get_value(knob->adjustment);
        double page  = gtk_adjustment_get_page_increment(knob->adjustment);

        gtk_adjustment_set_value(knob->adjustment, value - page);
    }
}


static void
on_drag_end_increment(GtkGestureDrag *UNUSED_drag, double UNUSED_x, double UNUSED_y, gpointer data) {
    PhasexKnob  *knob = PHASEX_KNOB(data);

    (void) UNUSED_drag;
    (void) UNUSED_x;
    (void) UNUSED_y;

    if (!knob->drag_moved) {
        double value = gtk_adjustment_get_value(knob->adjustment);
        double page  = gtk_adjustment_get_page_increment(knob->adjustment);

        gtk_adjustment_set_value(knob->adjustment, value + page);
    }
}


static void
on_middle_pressed(GtkGestureClick *UNUSED_gesture, int UNUSED_n_press,
                  double UNUSED_x, double UNUSED_y, gpointer data) {
    PhasexKnob  *knob  = PHASEX_KNOB(data);
    double      lower  = gtk_adjustment_get_lower(knob->adjustment);
    double      upper  = gtk_adjustment_get_upper(knob->adjustment);

    (void) UNUSED_gesture;
    (void) UNUSED_n_press;
    (void) UNUSED_x;
    (void) UNUSED_y;

    gtk_adjustment_set_value(knob->adjustment, floor((lower + upper + 1.0) * 0.5));
}


static gboolean
on_scroll(GtkEventControllerScroll *UNUSED_controller, double UNUSED_dx, double dy, gpointer data) {
    PhasexKnob  *knob = PHASEX_KNOB(data);
    double      value = gtk_adjustment_get_value(knob->adjustment);
    double      step  = gtk_adjustment_get_step_increment(knob->adjustment);

    (void) UNUSED_controller;
    (void) UNUSED_dx;

    /* dy < 0 is "scroll up" in GTK4's convention. */
    if (dy < 0.0) {
        gtk_adjustment_set_value(knob->adjustment, value + step);
    } else if (dy > 0.0) {
        gtk_adjustment_set_value(knob->adjustment, value - step);
    }
    return TRUE;
}


/*****************************************************************************
 * knob math -- ported verbatim from src/gtkknob.c's
 * gtk_knob_update_mouse()/gtk_knob_update()/gtk_knob_set_frame_offset()
 *****************************************************************************/
static void
phasex_knob_update_mouse(PhasexKnob *knob, double x, double y, gboolean absolute) {
    double  old_value;
    double  new_value;
    double  lower;
    double  upper;
    double  range;
    double  scale;
    double  angle;

    if (knob->adjustment == NULL) {
        return;
    }

    old_value = gtk_adjustment_get_value(knob->adjustment);
    lower     = gtk_adjustment_get_lower(knob->adjustment);
    upper     = gtk_adjustment_get_upper(knob->adjustment);

    range = upper - lower + 1.0;
    scale = range * range / 16384.0;

    angle = atan2(-y + (knob->height >> 1) + 2, x - (knob->width >> 1) - 1);

    if (absolute) {
        /* map [1.25pi, -0.25pi] onto [0, 1] */
        angle *= M_1_PI;
        if (angle < -0.5) {
            angle += 2.0;
        }
        new_value  = 0.66666666666666666666 * (1.25 - angle);
        new_value *= (upper - lower);
        new_value += lower;
    } else {
        double dv = knob->saved_y - y;
        double dh = x - knob->saved_x;

        new_value = old_value + ((dv + dh) * scale * gtk_adjustment_get_step_increment(knob->adjustment));
    }

    new_value = CLAMP(new_value, lower, upper);

    if (floor(new_value + 0.5) != floor(old_value + 0.5)) {
        gtk_adjustment_set_value(knob->adjustment, new_value);
        knob->saved_x = x;
        knob->saved_y = y;
    }
}


static void
phasex_knob_set_frame_offset(PhasexKnob *knob, double value) {
    double  lower;
    double  upper;

    if ((knob->anim == NULL) || (knob->adjustment == NULL)) {
        return;
    }

    lower = gtk_adjustment_get_lower(knob->adjustment);
    upper = gtk_adjustment_get_upper(knob->adjustment);

    knob->frame_offset = (int) ((((double) knob->anim->width / (double) knob->anim->frame_width) - 1) *
                                (value - lower) * (1.0 / (upper - lower))) * knob->width;
}


static void
phasex_knob_update(PhasexKnob *knob) {
    double  new_value;
    double  lower;
    double  upper;
    double  step;
    double  value;

    if (knob->adjustment == NULL) {
        return;
    }

    step  = gtk_adjustment_get_step_increment(knob->adjustment);
    value = gtk_adjustment_get_value(knob->adjustment);
    lower = gtk_adjustment_get_lower(knob->adjustment);
    upper = gtk_adjustment_get_upper(knob->adjustment);

    if (step == 1.0) {
        new_value = floor(value + 0.5);
    } else {
        new_value = value;
    }

    new_value = CLAMP(new_value, lower, upper);

    phasex_knob_set_frame_offset(knob, new_value);

    if (new_value != value) {
        gtk_adjustment_set_value(knob->adjustment, new_value);
    }

    gtk_widget_queue_draw(GTK_WIDGET(knob));
}


/*****************************************************************************
 * GtkWidget vfuncs
 *****************************************************************************/
static void
phasex_knob_measure(GtkWidget *widget, GtkOrientation orientation, int UNUSED_for_size,
                    int *minimum, int *natural, int *minimum_baseline, int *natural_baseline) {
    PhasexKnob  *knob = PHASEX_KNOB(widget);
    int         size  = (orientation == GTK_ORIENTATION_HORIZONTAL) ? knob->width : knob->height;

    (void) UNUSED_for_size;

    *minimum = *natural = size;
    if (minimum_baseline != NULL) {
        *minimum_baseline = -1;
    }
    if (natural_baseline != NULL) {
        *natural_baseline = -1;
    }
}


static void
phasex_knob_snapshot(GtkWidget *widget, GtkSnapshot *snapshot) {
    PhasexKnob  *knob   = PHASEX_KNOB(widget);
    int         width   = gtk_widget_get_width(widget);
    int         height  = gtk_widget_get_height(widget);
    cairo_t     *cr;

    if ((knob->anim == NULL) || (knob->anim->pixbuf == NULL)) {
        return;
    }

    cr = gtk_snapshot_append_cairo(snapshot, &GRAPHENE_RECT_INIT(0, 0, (float) width, (float) height));
    gdk_cairo_set_source_pixbuf(cr, knob->anim->pixbuf, (double) (0 - knob->frame_offset), 0.0);
    cairo_paint(cr);
    cairo_destroy(cr);
}


static void
phasex_knob_dispose(GObject *object) {
    PhasexKnob *knob = PHASEX_KNOB(object);

    if (knob->adjustment != NULL) {
        g_signal_handlers_disconnect_by_func(knob->adjustment, on_adjustment_changed, knob);
        g_signal_handlers_disconnect_by_func(knob->adjustment, on_adjustment_value_changed, knob);
        g_clear_object(&knob->adjustment);
    }

    G_OBJECT_CLASS(phasex_knob_parent_class)->dispose(object);
}


static void
phasex_knob_class_init(PhasexKnobClass *class) {
    GObjectClass    *object_class = G_OBJECT_CLASS(class);
    GtkWidgetClass  *widget_class = GTK_WIDGET_CLASS(class);

    object_class->dispose  = phasex_knob_dispose;
    widget_class->measure  = phasex_knob_measure;
    widget_class->snapshot = phasex_knob_snapshot;
}


static void
phasex_knob_init(PhasexKnob *knob) {
    GtkGesture          *drag;
    GtkGesture          *click;
    GtkEventController  *scroll;

    knob->anim         = NULL;
    knob->adjustment   = NULL;
    knob->frame_offset = 0;
    knob->old_value    = 0.0;
    knob->old_lower    = 0.0;
    knob->old_upper    = 0.0;
    knob->drag_moved   = FALSE;

    gtk_widget_set_focusable(GTK_WIDGET(knob), TRUE);

    /* This is a fixed-size sprite-sheet widget (one wide strip of
       pre-rendered rotation frames, see phasex_knob_snapshot()) --
       it must never be allocated more than measure()'s returned
       size. GtkWidget defaults to GTK_ALIGN_FILL/hexpand=vexpand=
       FALSE... but FALSE only means "don't request extra space", not
       "don't accept it": a GtkGrid still stretches FILL children out
       to their column/row's full size when a sibling cell (e.g. a
       BOOL/BBOX param's row of several buttons) makes that column
       wider than 28px. Once stretched, cairo_paint() in snapshot()
       paints into that wider area, and since the source image is one
       wide strip of frames, it shows a slice of *several* adjacent
       frames at once instead of clipping to one -- looking exactly
       like extra, stuck-together knobs that all move in sync (they're
       really the same knob's neighboring animation frames). START
       alignment stops the stretch at the source. */
    gtk_widget_set_halign(GTK_WIDGET(knob), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(knob), GTK_ALIGN_START);
    gtk_widget_set_hexpand(GTK_WIDGET(knob), FALSE);
    gtk_widget_set_vexpand(GTK_WIDGET(knob), FALSE);

    /* left button: absolute (click-to-angle) drag, page-decrement on a
       plain click */
    drag = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_PRIMARY);
    g_signal_connect(drag, "drag-begin",  G_CALLBACK(on_drag_begin), knob);
    g_signal_connect(drag, "drag-update", G_CALLBACK(on_drag_update_absolute), knob);
    g_signal_connect(drag, "drag-end",    G_CALLBACK(on_drag_end_decrement), knob);
    gtk_widget_add_controller(GTK_WIDGET(knob), GTK_EVENT_CONTROLLER(drag));

    /* right button: relative (delta-based) drag, page-increment on a
       plain click */
    drag = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_SECONDARY);
    g_signal_connect(drag, "drag-begin",  G_CALLBACK(on_drag_begin), knob);
    g_signal_connect(drag, "drag-update", G_CALLBACK(on_drag_update_relative), knob);
    g_signal_connect(drag, "drag-end",    G_CALLBACK(on_drag_end_increment), knob);
    gtk_widget_add_controller(GTK_WIDGET(knob), GTK_EVENT_CONTROLLER(drag));

    /* middle button: jump straight to the midpoint value */
    click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), GDK_BUTTON_MIDDLE);
    g_signal_connect(click, "pressed", G_CALLBACK(on_middle_pressed), knob);
    gtk_widget_add_controller(GTK_WIDGET(knob), GTK_EVENT_CONTROLLER(click));

    scroll = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL |
            GTK_EVENT_CONTROLLER_SCROLL_DISCRETE);
    g_signal_connect(scroll, "scroll", G_CALLBACK(on_scroll), knob);
    gtk_widget_add_controller(GTK_WIDGET(knob), scroll);
}


/*****************************************************************************
 * public API
 *****************************************************************************/
GtkAdjustment *
phasex_knob_get_adjustment(PhasexKnob *knob) {
    return knob->adjustment;
}


static void
phasex_knob_set_adjustment(PhasexKnob *knob, GtkAdjustment *adjustment) {
    if (knob->adjustment != NULL) {
        g_signal_handlers_disconnect_by_func(knob->adjustment, on_adjustment_changed, knob);
        g_signal_handlers_disconnect_by_func(knob->adjustment, on_adjustment_value_changed, knob);
        g_clear_object(&knob->adjustment);
    }

    knob->adjustment = g_object_ref(adjustment);

    g_signal_connect(adjustment, "changed",       G_CALLBACK(on_adjustment_changed), knob);
    g_signal_connect(adjustment, "value-changed", G_CALLBACK(on_adjustment_value_changed), knob);

    knob->old_value = gtk_adjustment_get_value(adjustment);
    knob->old_lower = gtk_adjustment_get_lower(adjustment);
    knob->old_upper = gtk_adjustment_get_upper(adjustment);

    phasex_knob_set_frame_offset(knob, knob->old_value);
    phasex_knob_update(knob);
}


static void
phasex_knob_set_animation(PhasexKnob *knob, PhasexKnobAnim *anim) {
    knob->anim   = anim;
    knob->width  = anim->frame_width;
    knob->height = anim->height;
    phasex_knob_set_frame_offset(knob, (knob->adjustment == NULL) ? 0.0 : knob->old_value);
    gtk_widget_queue_resize(GTK_WIDGET(knob));
}


GtkWidget *
phasex_knob_new(GtkAdjustment *adjustment, PhasexKnobAnim *anim) {
    PhasexKnob *knob;

    g_return_val_if_fail(anim != NULL, NULL);
    g_return_val_if_fail(GDK_IS_PIXBUF(anim->pixbuf), NULL);

    knob = g_object_new(PHASEX_TYPE_KNOB, NULL);

    phasex_knob_set_animation(knob, anim);

    if (adjustment == NULL) {
        adjustment = gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    }
    phasex_knob_set_adjustment(knob, adjustment);

    return GTK_WIDGET(knob);
}


PhasexKnobAnim *
phasex_knob_animation_new_from_file(const char *filename, int frame_width, int width, int height) {
    PhasexKnobAnim  *anim = g_new0(PhasexKnobAnim, 1);
    GError          *error = NULL;

    g_return_val_if_fail(filename != NULL, NULL);

    anim->pixbuf = gdk_pixbuf_new_from_file_at_size(filename, width, height, &error);
    if (anim->pixbuf == NULL) {
        g_warning("Unable to load knob animation '%s': %s", filename,
                  (error != NULL) ? error->message : "unknown error");
        g_clear_error(&error);
        g_free(anim);
        return NULL;
    }

    anim->height      = gdk_pixbuf_get_height(anim->pixbuf);
    anim->width       = gdk_pixbuf_get_width(anim->pixbuf);
    anim->frame_width = (frame_width != -1) ? frame_width : anim->height;

    return anim;
}


void
phasex_knob_animation_free(PhasexKnobAnim *anim) {
    if (anim == NULL) {
        return;
    }
    g_clear_object(&anim->pixbuf);
    g_free(anim);
}
