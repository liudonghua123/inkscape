// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Theodore Janeczko 2012 <flutterguy317@gmail.com>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#include "live_effects/lpe-bounding-box.h"

#include "display/curve.h"

// TODO due to internal breakage in glibmm headers, this must be last:
#include <glibmm/i18n.h>


namespace Inkscape {
namespace LivePathEffect {

LPEBoundingBox::LPEBoundingBox(LivePathEffectObject *lpeobject) :
    Effect(lpeobject),
    linked_path(_("Linked path:"), _("Path from which to take the original path data"), "linkedpath", &wr, this),
    visual_bounds(_("Visual Bounds"), _("Uses the visual bounding box"), "visualbounds", &wr, this)
{
    registerParameter(&linked_path);
    registerParameter(&visual_bounds);
    //perceived_path = true;
}

LPEBoundingBox::~LPEBoundingBox()
= default;

void LPEBoundingBox::doEffect (SPCurve * curve)
{
    if (curve) {
        if ( linked_path.linksToPath() && linked_path.getObject() ) {
            SPItem * item = linked_path.getObject();
            Geom::OptRect bbox = visual_bounds.get_value() ? item->visualBounds() : item->geometricBounds();
            Geom::Path p;
            Geom::PathVector out;
            if (bbox) {
                p = Geom::Path(*bbox);
                out.push_back(p);
            }

            curve->set_pathvector(out);
        }
    }
}

} // namespace LivePathEffect
} /* namespace Inkscape */

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
