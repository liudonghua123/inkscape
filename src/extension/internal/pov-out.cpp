/*
 * A quick hack to use the print output to write out a file.  This
 * then makes 'save as...' Postscript.
 *
 * Authors:
 *   Bob Jamison <rjamison@titan.com>
 *   Ted Gould <ted@gould.cx>
 *
 * Copyright (C) 2004 Authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "pov-out.h"
#include <inkscape.h>
#include <sp-path.h>
#include <style.h>
#include <color.h>
#include <display/curve.h>
#include <libnr/n-art-bpath.h>
#include <extension/system.h>
#include <extension/db.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <vector>
#include <string>

namespace Inkscape {
namespace Extension {
namespace Internal {

bool
PovOutput::check (Inkscape::Extension::Extension * module)
{
	//if (NULL == Inkscape::Extension::db.get(SP_MODULE_KEY_PRINT_PS))
	//	return FALSE;

	return TRUE;
}

static void
findElementsByTagName(std::vector<SPRepr *> &results, SPRepr *node, const char *name)
{
    if (!name)
        results.push_back(node);
    else if (strcmp(sp_repr_name(node), name) == 0)
        results.push_back(node);

    for (SPRepr *child = node->children; child ; child = child->next)
        findElementsByTagName ( results, child, name );

}


//used for saving information about shapes
class PovShapeInfo
{
    public:
    PovShapeInfo()
        {}
    virtual ~PovShapeInfo()
        {}
    std::string id;
    std::string color;
};

/**
 * Saves the <paths> of an Inkscape SVG file as PovRay spline definitions
*/
void
PovOutput::save (Inkscape::Extension::Output *mod, SPDocument *doc, const gchar *uri)
{
    std::vector<SPRepr *>results;
    //findElementsByTagName(results, SP_ACTIVE_DOCUMENT->rroot, "path");
    findElementsByTagName(results, SP_ACTIVE_DOCUMENT->rroot, NULL);//Check all nodes
    if (results.size() == 0)
        return;
    FILE *f = fopen(uri, "w");
    if (!f)
        return;

    time_t tim = time(NULL);
    fprintf(f, "/*#################################################\n");
    fprintf(f, "### This PovRay document was generated by Inkscape\n");
    fprintf(f, "### http://www.inkscape.org\n");
    fprintf(f, "### Created: %s", ctime(&tim));
    fprintf(f, "##################################################*/\n\n\n");

    std::vector<PovShapeInfo>povShapes; //A list for saving information about the shapes

    double bignum = 1000000.0;
    double minx  =  bignum;
    double maxx  = -bignum;
    double miny  =  bignum;
    double maxy  = -bignum;

    unsigned int indx;
    for (indx = 0; indx < results.size() ; indx++)
        {
        SPRepr *rpath = results[indx];
        gchar *id  = (gchar *)sp_repr_attr(rpath, "id");
        SPObject *reprobj = SP_ACTIVE_DOCUMENT->getObjectByRepr(rpath);
        if (!reprobj)
            continue;
        if (!SP_IS_SHAPE(reprobj))//Bulia's suggestion.  Allow all shapes
            {
            continue;
            }
        SPShape *shape = SP_SHAPE(reprobj);
        SPCurve *curve = shape->curve; 
        if (sp_curve_empty(curve))
            continue;

        PovShapeInfo shapeInfo;

        shapeInfo.id    = id;
        shapeInfo.color = "";

        //Try to get the fill color of the shape
        SPStyle *style = SP_OBJECT_STYLE(shape);
        if (style && (style->fill.type == SP_PAINT_TYPE_COLOR))
            {
            // see color.h for how to parse SPColor 
            SPColor spColor = style->fill.value.color;
            guint32 color = sp_color_get_rgba32_ualpha(&spColor, 0);
            int r = SP_RGBA32_R_U(color);
            int g = SP_RGBA32_G_U(color);
            int b = SP_RGBA32_B_U(color);
            gchar *str = g_strdup_printf("rgb < %d, %d, %d >", r, g, b);
            shapeInfo.color += str;
            g_free(str);

            //printf("got color for shape '%s': %s\n", id, shapeInfo.color.c_str());
            }

        povShapes.push_back(shapeInfo); //passed all tests.  save the info

        int curveNr;

        //Count the NR_CURVETOs/LINETOs
        int segmentCount=0;
        NArtBpath *bp = curve->bpath;
        for (curveNr=0 ; curveNr<curve->length ; curveNr++, bp++)
            if (bp->code == NR_CURVETO || bp->code == NR_LINETO)
                segmentCount++;

        bp = curve->bpath;
        double cminx  =  bignum;
        double cmaxx  = -bignum;
        double cminy  =  bignum;
        double cmaxy  = -bignum;
        double lastx  = 0.0;
        double lasty  = 0.0;

        fprintf(f, "/*##############################################\n");
        fprintf(f, "### PRISM:  %s\n", id);
        fprintf(f, "##############################################*/\n");
        fprintf(f, "#declare %s = prism {\n", id);
        fprintf(f, "    linear_sweep\n");
        fprintf(f, "    bezier_spline\n");
        fprintf(f, "    1.0, //top\n");
        fprintf(f, "    0.0, //bottom\n");
        fprintf(f, "    %d, //nr points\n", segmentCount * 4);
        int segmentNr = 0;
        for (bp = curve->bpath, curveNr=0 ; curveNr<curve->length ; curveNr++, bp++)
            {
            switch (bp->code)
                {
                case NR_MOVETO:
                case NR_MOVETO_OPEN:
                    //fprintf(f, "moveto: %f %f\n", bp->x3, bp->y3);
                break;
                case NR_CURVETO:
                    fprintf(f, "    /*%4d*/ <%f, %f>, <%f, %f>, <%f,%f>, <%f,%f>",
                        segmentNr++, lastx, lasty, bp->x1, bp->y1, 
                        bp->x2, bp->y2, bp->x3, bp->y3);
                    if (segmentNr < segmentCount)
                        fprintf(f, ",\n");
                    else
                        fprintf(f, "\n");
                    if (lastx < cminx)
                        cminx = lastx;
                    if (lastx > cmaxx)
                        cmaxx = lastx;
                    if (lasty < cminy)
                        cminy = lasty;
                    if (lasty > cmaxy)
                        cmaxy = lasty;
                break;
                case NR_LINETO:
                    fprintf(f, "    /*%4d*/ <%f, %f>, <%f, %f>, <%f,%f>, <%f,%f>",
                        segmentNr++, lastx, lasty, lastx, lasty, 
                        bp->x3, bp->y3, bp->x3, bp->y3);
                    if (segmentNr < segmentCount)
                        fprintf(f, ",\n");
                    else
                        fprintf(f, "\n");
                    //fprintf(f, "lineto\n");
                    if (lastx < cminx)
                        cminx = lastx;
                    if (lastx > cmaxx)
                        cmaxx = lastx;
                    if (lasty < cminy)
                        cminy = lasty;
                    if (lasty > cmaxy)
                        cmaxy = lasty;
                break;
                case NR_END:
                    //fprintf(f, "end\n");
                break;
                }
            lastx = bp->x3;
            lasty = bp->y3;
            }
        fprintf(f, "}\n");
        fprintf(f, "#declare %s_MIN_X    = %4.3f;\n", id, cminx);
        fprintf(f, "#declare %s_CENTER_X = %4.3f;\n", id, (cmaxx+cminx)/2.0);
        fprintf(f, "#declare %s_MAX_X    = %4.3f;\n", id, cmaxx);
        fprintf(f, "#declare %s_WIDTH    = %4.3f;\n", id, cmaxx-cminx);
        fprintf(f, "#declare %s_MIN_Y    = %4.3f;\n", id, cminy);
        fprintf(f, "#declare %s_CENTER_Y = %4.3f;\n", id, (cmaxy+cminy)/2.0);
        fprintf(f, "#declare %s_MAX_Y    = %4.3f;\n", id, cmaxy);
        fprintf(f, "#declare %s_HEIGHT   = %4.3f;\n", id, cmaxy-cminy);
        if (shapeInfo.color.length()>0)
            fprintf(f, "#declare %s_COLOR    = %s;\n",
                          id, shapeInfo.color.c_str());
        fprintf(f, "/*##############################################\n");
        fprintf(f, "### end %s\n", id);
        fprintf(f, "##############################################*/\n\n\n\n");
        if (cminx < minx)
            minx = cminx;
        else if (cmaxx > maxx)
            maxx = cmaxx;
        if (cminy < miny)
            miny = cminy;
        else if (cmaxy > maxy)
            maxy = cmaxy;


        }//for


    
    //## Let's make a union of all of the Shapes
    if (povShapes.size() > 0)
        {
        char *id = "AllShapes";
        fprintf(f, "/*##############################################\n");
        fprintf(f, "### UNION OF ALL SHAPES IN DOCUMENT\n");
        fprintf(f, "##############################################*/\n");
        fprintf(f, "\n\n");
        fprintf(f, "/**\n");
        fprintf(f, " * Allow the user to redefine the finish{}\n");
        fprintf(f, " * by declaring it before #including this file\n");
        fprintf(f, " */\n");
        fprintf(f, "#ifndef (%s_Finish)\n", id);
        fprintf(f, "#declare %s_Finish = finish {\n", id);
        fprintf(f, "    phong 0.5\n");
        fprintf(f, "    reflection 0.3\n");
        fprintf(f, "    specular 0.5\n");
        fprintf(f, "}\n");
        fprintf(f, "#end\n");
        fprintf(f, "\n\n");
        fprintf(f, "#declare %s = union {\n", id);
        for (unsigned int i=0 ; i<povShapes.size() ; i++)
            {
            fprintf(f, "    object { %s\n", povShapes[i].id.c_str());
            fprintf(f, "        texture { \n");
            if (povShapes[i].color.length()>0)
                fprintf(f, "            pigment { %s }\n", povShapes[i].color.c_str());
            else
                fprintf(f, "            pigment { rgb <0,0,0> }\n");
            fprintf(f, "            finish { %s_Finish }\n", id);
            fprintf(f, "            } \n");
            fprintf(f, "        } \n");
            }
        fprintf(f, "}\n\n\n");


        fprintf(f, "/* Same union, but with Z-diffs (actually Y in pov)*/\n");
        fprintf(f, "#declare %sZ = union {\n", id);
        double zinc   = 0.2 / (double)povShapes.size();
        double zscale = 1.0;
        double ztrans = 0.0;
        for (unsigned int i=0 ; i<povShapes.size() ; i++)
            {
            fprintf(f, "    object { %s\n", povShapes[i].id.c_str());
            fprintf(f, "        texture { \n");
            if (povShapes[i].color.length()>0)
                fprintf(f, "            pigment { %s }\n", povShapes[i].color.c_str());
            else
                fprintf(f, "            pigment { rgb <0,0,0> }\n");
            fprintf(f, "            finish { %s_Finish }\n", id);
            fprintf(f, "            } \n");
            fprintf(f, "        scale <1, %2.5f, 1>  translate <1, %2.5f, 1>\n", 
                                     zscale, ztrans);
            fprintf(f, "        } \n");
            zscale += zinc;
            ztrans -= zinc/2.0;
            }

        fprintf(f, "}\n");
        fprintf(f, "#declare %s_MIN_X    = %4.3f;\n", id, minx);
        fprintf(f, "#declare %s_CENTER_X = %4.3f;\n", id, (maxx+minx)/2.0);
        fprintf(f, "#declare %s_MAX_X    = %4.3f;\n", id, maxx);
        fprintf(f, "#declare %s_WIDTH    = %4.3f;\n", id, maxx-minx);
        fprintf(f, "#declare %s_MIN_Y    = %4.3f;\n", id, miny);
        fprintf(f, "#declare %s_CENTER_Y = %4.3f;\n", id, (maxy+miny)/2.0);
        fprintf(f, "#declare %s_MAX_Y    = %4.3f;\n", id, maxy);
        fprintf(f, "#declare %s_HEIGHT   = %4.3f;\n", id, maxy-miny);
        fprintf(f, "/*##############################################\n");
        fprintf(f, "### end %s\n", id);
        fprintf(f, "##############################################*/\n\n\n\n");
        }

    //All done
    fclose(f);
}

/**
	\brief   A function allocate a copy of this function.

	This is the definition of postscript out.  This function just
	calls the extension system with the memory allocated XML that
	describes the data.
*/
void
PovOutput::init (void)
{
	Inkscape::Extension::build_from_mem(
		"<inkscape-extension>\n"
			"<name>PovRay Output</name>\n"
			"<id>org.inkscape.output.pov</id>\n"
			"<output>\n"
				"<extension>.pov</extension>\n"
				"<mimetype>text/x-povray-script</mimetype>\n"
				"<filetypename>PovRay (*.pov) (export splines)</filetypename>\n"
				"<filetypetooltip>PovRay Raytracer File</filetypetooltip>\n"
			"</output>\n"
		"</inkscape-extension>", new PovOutput());

	return;
}

};};}; /* namespace Inkscape, Extension, Internal */
