#ifndef __SP_LIVAROT_H__
#define __SP_LIVAROT_H__

/*
 * boolops and outlines
 *
 * public domain
 */

#include "forward.h"

// boolean operations
// work on the current selection
// selection has 2 contain exactly 2 items
void sp_selected_path_union ();
void sp_selected_path_intersect ();
void sp_selected_path_diff ();
void sp_selected_path_symdiff ();
void sp_selected_path_cut ();
void sp_selected_path_slice ();

// offset/inset of a curve
// takes the fill-rule in consideration
// offset amount is the stroke-width of the curve
void sp_selected_path_offset ();
void sp_selected_path_offset_screen (double pixels);
void sp_selected_path_inset ();
void sp_selected_path_inset_screen (double pixels);
void sp_selected_path_create_offset ();
void sp_selected_path_create_inset ();
void sp_selected_path_create_updating_offset ();
void sp_selected_path_create_updating_inset ();

void sp_selected_path_create_offset_object_zero ();
void sp_selected_path_create_updating_offset_object_zero ();

// outline of a curve
// uses the stroke-width
void sp_selected_path_outline ();

// simplifies a path (removes small segments and the like)
void sp_selected_path_simplify ();
// treshhold= like the name says
// justCoalesce= only tries to merge successive path elements
// angleLimit= treshhold when breakableAngles=true (not implemented)
// breakableAngles= make angles less than angleLimit easier to create a control point (not implemented)
void sp_selected_path_simplify_withparams (float treshhold,bool justCoalesce,float angleLimit,bool breakableAngles);

#endif

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=c++:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
