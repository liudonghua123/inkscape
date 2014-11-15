#ifndef INKSCAPE_LPE_MIRROR_SYMMETRY_H
#define INKSCAPE_LPE_MIRROR_SYMMETRY_H

/** \file
 * LPE <mirror_symmetry> implementation: mirrors a path with respect to a given line.
 */
/*
 * Authors:
 *   Maximilian Albert
 *   Johan Engelen
 *
 * Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 * Copyright (C) Maximilin Albert 2008 <maximilian.albert@gmail.com>
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "live_effects/effect.h"
#include "live_effects/parameter/parameter.h"
#include "live_effects/parameter/point.h"
#include "live_effects/parameter/path.h"
#include "live_effects/parameter/enum.h"
#include "live_effects/lpegroupbbox.h"

namespace Inkscape {
namespace LivePathEffect {

namespace MS {
  // we need a separate namespace to avoid clashes with LPEPerpBisector
  class KnotHolderEntityCenterMirrorSymmetry;
}

enum ModeType {
    MT_FREE,
    MT_X,
    MT_Y,
    MT_END
};

class LPEMirrorSymmetry : public Effect, GroupBBoxEffect{
public:
    LPEMirrorSymmetry(LivePathEffectObject *lpeobject);
    virtual ~LPEMirrorSymmetry();

    virtual void doOnApply (SPLPEItem const* lpeitem);

    virtual void doBeforeEffect (SPLPEItem const* lpeitem);

    virtual int pointSideOfLine(Geom::Point A, Geom::Point B, Geom::Point X);

    virtual std::vector<Geom::Path> doEffect_path (std::vector<Geom::Path> const & path_in);

    /* the knotholder entity classes must be declared friends */
    friend class MS::KnotHolderEntityCenterMirrorSymmetry;
    void addKnotHolderEntities(KnotHolder *knotholder, SPDesktop *desktop, SPItem *item);

protected:
    virtual void addCanvasIndicators(SPLPEItem const *lpeitem, std::vector<Geom::PathVector> &hp_vec);

private:
    EnumParam<ModeType> mode;
    BoolParam discard_orig_path;
    BoolParam fusionPaths;
    BoolParam reverseFusion;
    PathParam reflection_line;
    Geom::Line lineSeparation;
    PointParam center;

    LPEMirrorSymmetry(const LPEMirrorSymmetry&);
    LPEMirrorSymmetry& operator=(const LPEMirrorSymmetry&);
};

} //namespace LivePathEffect
} //namespace Inkscape

#endif
