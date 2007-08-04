/*
 * feMorphology filter primitive renderer
 *
 * Authors:
 *   Felipe Corrêa da Silva Sanches <felipe.sanches@gmail.com>
 *
 * Copyright (C) 2007 authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "display/nr-filter-morphology.h"
namespace NR {

FilterMorphology::FilterMorphology()
{
    g_warning("FilterMorphology::render not implemented.");
}

FilterPrimitive * FilterMorphology::create() {
    return new FilterMorphology();
}

FilterMorphology::~FilterMorphology()
{}

int FilterMorphology::render(FilterSlot &slot, Matrix const &trans) {
    NRPixBlock *in = slot.get(_input);
    NRPixBlock *out = new NRPixBlock;

    nr_pixblock_setup_fast(out, in->mode,
                           in->area.x0, in->area.y0, in->area.x1, in->area.y1,
                           true);

    unsigned char *in_data = NR_PIXBLOCK_PX(in);
    unsigned char *out_data = NR_PIXBLOCK_PX(out);

//IMPLEMENT ME!
    
    out->empty = FALSE;
    slot.set(_output, out);
    return 0;
}

void FilterMorphology::area_enlarge(NRRectL &area, Matrix const &trans)
{
}

} /* namespace NR */

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=99 :
