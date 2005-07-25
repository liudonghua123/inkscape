#ifndef INKSCAPE_LIVAROT_INT_LINE_H
#define INKSCAPE_LIVAROT_INT_LINE_H

#include "livarot/livarot-forward.h"
#include "livarot/LivarotDefs.h"

/** \file
 * Coverage with integer boundaries.
 *
 * This is what we want for actual rasterization. It contains the same 
 * stuff as FloatLigne, but technically only the Copy() functions are used.
 */

struct int_ligne_run {
    int st;
    int en;
    float vst;
    float ven;
};

struct int_ligne_bord {
    int pos;
    bool start;
    float val;
    int other;
    int prev;
    int next;
};

/// Coverage with integer boundaries.
class IntLigne {
public:
    
    int nbBord;
    int maxBord;
    int_ligne_bord* bords;

    int nbRun;
    int maxRun;
    int_ligne_run* runs;

    int firstAc;
    int lastAc;

    IntLigne();
    ~IntLigne();

    void Reset();
    int AddBord(int spos, float sval, int epos, float eval);

    void Flatten();

    void Affiche();

    int AddRun(int st, int en, float vst, float ven);

    void Booleen(IntLigne* a, IntLigne* b, BooleanOp mod);

    // copy another IntLigne
    void Copy(IntLigne* a);
    // copy a FloatLigne's runs, ie compute non-overlapping runs with integer boundaries from a set
    // of runs with floating-point boundaries
    void Copy(FloatLigne* a);
    // transform a line of bits into pixel coverage values.
    // this is where you go from supersampled data to alpha values
    void Copy(BitLigne* a);
    void Copy(int nbSub,BitLigne **a); // nbSub is the number of BitLigne in the array "a"

    void Enqueue(int no);
    void Dequeue(int no);
    float RemainingValAt(int at);

    static int CmpBord(void const *p1, void const *p2) {
        int_ligne_bord const *d1 = reinterpret_cast<int_ligne_bord const *>(p1);
        int_ligne_bord const *d2 = reinterpret_cast<int_ligne_bord const *>(p2);
        
        if ( d1->pos == d2->pos ) {
            if ( d1->start && !(d2->start) ) {
                return 1;
            }
            if ( !(d1->start) && d2->start ) {
                return -1;
            }
            return 0;
        }
        return (( d1->pos < d2->pos ) ? -1 : 1);
    };

    inline float ValAt(int at, int ps, int pe, float vs, float ve) {
        return ((at - ps) * ve + (pe - at) * vs) / (pe - ps);
    };

    // rasterization.
    // the parameters have the same meaning as in the AlphaLigne class
    void Raster(raster_info &dest, void *color, RasterInRunFunc worker);
};


#endif

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
