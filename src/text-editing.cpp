/*
 * Parent class for text and flowtext
 *
 * Authors:
 *   bulia byak
 *   Richard Hughes
 *
 * Copyright (C) 2004-5 authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "config.h"

#include <algorithm>
#include <libnr/nr-matrix.h>
#include <libnr/nr-point-matrix-ops.h>

#include <glib.h>

#include "svg/svg.h"
#include "desktop.h"
#include "style.h"
#include "desktop-style.h"
#include "unit-constants.h"

#include "xml/repr.h"
#include "xml/attribute-record.h"

#include "sp-text.h"
#include "sp-textpath.h"
#include "sp-flowtext.h"
#include "sp-flowdiv.h"
#include "sp-flowregion.h"
#include "sp-tspan.h"
#include "sp-string.h"

#include "text-editing.h"

static bool tidy_xml_tree_recursively(SPObject *root);

Inkscape::Text::Layout const * te_get_layout (SPItem const *item)
{
    if (SP_IS_TEXT(item)) {
        return &(SP_TEXT(item)->layout);
    } else if (SP_IS_FLOWTEXT (item)) {
        return &(SP_FLOWTEXT(item)->layout);
    }
    return NULL;
}

static void te_update_layout_now (SPItem *item)
{
    if (SP_IS_TEXT(item))
        SP_TEXT(item)->rebuildLayout();
    else if (SP_IS_FLOWTEXT (item))
        SP_FLOWTEXT(item)->rebuildLayout();
}

/** Returns true if there are no visible characters on the canvas */
bool
sp_te_output_is_empty (SPItem const *item)
{
    Inkscape::Text::Layout const *layout = te_get_layout(item);
    return layout->begin() == layout->end();
}

/** Returns true if the user has typed nothing in the text box */
bool
sp_te_input_is_empty (SPObject const *item)
{
    if (SP_IS_STRING(item)) return SP_STRING(item)->string.empty();
    for (SPObject const *child = item->firstChild() ; child ; child = SP_OBJECT_NEXT(child))
        if (!sp_te_input_is_empty(child)) return false;
    return true;
}

Inkscape::Text::Layout::iterator
sp_te_get_position_by_coords (SPItem const *item, NR::Point &i_p)
{
    NR::Matrix  im=sp_item_i2d_affine (item);
    im = im.inverse();

    NR::Point p = i_p * im;
    Inkscape::Text::Layout const *layout = te_get_layout(item);
    return layout->getNearestCursorPositionTo(p);
}

std::vector<NR::Point> sp_te_create_selection_quads(SPItem const *item, Inkscape::Text::Layout::iterator const &start, Inkscape::Text::Layout::iterator const &end, NR::Matrix const &transform)
{
    if (start == end)
        return std::vector<NR::Point>();
    Inkscape::Text::Layout const *layout = te_get_layout(item);
    if (layout == NULL)
        return std::vector<NR::Point>();

    return layout->createSelectionShape(start, end, transform);
}

void
sp_te_get_cursor_coords (SPItem const *item, Inkscape::Text::Layout::iterator const &position, NR::Point &p0, NR::Point &p1)
{
    Inkscape::Text::Layout const *layout = te_get_layout(item);
    double height, rotation;
    layout->queryCursorShape(position, &p0, &height, &rotation);
    p1 = NR::Point(p0[NR::X] + height * sin(rotation), p0[NR::Y] - height * cos(rotation));
}

SPStyle const * sp_te_style_at_position(SPItem const *text, Inkscape::Text::Layout::iterator const &position)
{
    Inkscape::Text::Layout const *layout = te_get_layout(text);
    if (layout == NULL)
        return NULL;
    SPObject const *pos_obj = NULL;
    layout->getSourceOfCharacter(position, (void**)&pos_obj);
    if (pos_obj == NULL) pos_obj = text;
    while (SP_OBJECT_STYLE(pos_obj) == NULL)
        pos_obj = SP_OBJECT_PARENT(pos_obj);   // SPStrings don't have style
    return SP_OBJECT_STYLE(pos_obj);
}

/*
 * for debugging input
 *
char * dump_hexy(const gchar * utf8)
{
    static char buffer[1024];

    buffer[0]='\0';
    for (const char *ptr=utf8; *ptr; ptr++) {
        sprintf(buffer+strlen(buffer),"x%02X",(unsigned char)*ptr);
    }
    return buffer;
}
*/

Inkscape::Text::Layout::iterator sp_te_replace(SPItem *item, Inkscape::Text::Layout::iterator const &start, Inkscape::Text::Layout::iterator const &end, gchar const *utf8)
{
    Inkscape::Text::Layout::iterator new_start = sp_te_delete(item, start, end);
    return sp_te_insert(item, new_start, utf8);
}


/* ***************************************************************************************************/
//                             I N S E R T I N G   T E X T

static bool is_line_break_object(SPObject const *object)
{
    return    SP_IS_TEXT(object)
           || (SP_IS_TSPAN(object) && SP_TSPAN(object)->role != SP_TSPAN_ROLE_UNSPECIFIED)
           || SP_IS_TEXTPATH(object)
           || SP_IS_FLOWDIV(object)
           || SP_IS_FLOWPARA(object)
           || SP_IS_FLOWLINE(object)
           || SP_IS_FLOWREGIONBREAK(object);
}

/** returns the attributes for an object, or NULL if it isn't a text,
tspan or textpath. */
static TextTagAttributes* attributes_for_object(SPObject *object)
{
    if (SP_IS_TSPAN(object))
        return &SP_TSPAN(object)->attributes;
    if (SP_IS_TEXT(object))
        return &SP_TEXT(object)->attributes;
    if (SP_IS_TEXTPATH(object))
        return &SP_TEXTPATH(object)->attributes;
    return NULL;
}

static const char * span_name_for_text_object(SPObject const *object)
{
    if (SP_IS_TEXT(object)) return "svg:tspan";
    else if (SP_IS_FLOWTEXT(object)) return "svg:flowSpan";
    return NULL;
}

/** Recursively gets the length of all the SPStrings at or below the given
\a item. Also adds 1 for each line break encountered. */
unsigned sp_text_get_length(SPObject const *item)
{
    unsigned length = 0;

    if (SP_IS_STRING(item)) return SP_STRING(item)->string.length();
    if (is_line_break_object(item)) length++;
    for (SPObject const *child = item->firstChild() ; child ; child = SP_OBJECT_NEXT(child)) {
        if (SP_IS_STRING(child)) length += SP_STRING(child)->string.length();
        else length += sp_text_get_length(child);
    }
    return length;
}

static Inkscape::XML::Node* duplicate_node_without_children(Inkscape::XML::Node const *old_node)
{
    switch (old_node->type()) {
        case Inkscape::XML::ELEMENT_NODE: {
            Inkscape::XML::Node *new_node = sp_repr_new(old_node->name());
            Inkscape::Util::List<Inkscape::XML::AttributeRecord const> attributes = old_node->attributeList();
            GQuark const id_key = g_quark_from_string("id");
            for ( ; attributes ; attributes++) {
                if (attributes->key == id_key) continue;
                new_node->setAttribute(g_quark_to_string(attributes->key), attributes->value);
            }
            return new_node;
        }

        case Inkscape::XML::TEXT_NODE:
            return sp_repr_new_text(old_node->content());

        case Inkscape::XML::COMMENT_NODE:
            return sp_repr_new_comment(old_node->content());

        case Inkscape::XML::DOCUMENT_NODE:
            return NULL;   // this had better never happen
    }
    return NULL;
}

/** returns the sum of the (recursive) lengths of all the SPStrings prior
to \a item at the same level. */
static unsigned sum_sibling_text_lengths_before(SPObject const *item)
{
    unsigned char_index = 0;
    for (SPObject *sibling = SP_OBJECT_PARENT(item)->firstChild() ; sibling && sibling != item ; sibling = SP_OBJECT_NEXT(sibling))
        char_index += sp_text_get_length(sibling);
    return char_index;
}

/** splits the attributes for the first object at the given \a char_index
and moves the ones after that point into \a second_item. */
static void split_attributes(SPObject *first_item, SPObject *second_item, unsigned char_index)
{
    TextTagAttributes *first_attrs = attributes_for_object(first_item);
    TextTagAttributes *second_attrs = attributes_for_object(second_item);
    if (first_attrs && second_attrs)
        first_attrs->split(char_index, second_attrs);
}

/** recursively divides the XML node tree into two objects: the original will
contain all objects up to and including \a split_obj and the returned value
will be the new leaf which represents the copy of \a split_obj and extends
down the tree with new elements all the way to the common root which is the
parent of the first line break node encountered.
*/
static SPObject* split_text_object_tree_at(SPObject *split_obj, unsigned char_index)
{
    if (is_line_break_object(split_obj)) {
        Inkscape::XML::Node *new_node = duplicate_node_without_children(SP_OBJECT_REPR(split_obj));
        SP_OBJECT_REPR(SP_OBJECT_PARENT(split_obj))->addChild(new_node, SP_OBJECT_REPR(split_obj));
        Inkscape::GC::release(new_node);
        split_attributes(split_obj, SP_OBJECT_NEXT(split_obj), char_index);
        return SP_OBJECT_NEXT(split_obj);
    }

    unsigned char_count_before = sum_sibling_text_lengths_before(split_obj);
    SPObject *duplicate_obj = split_text_object_tree_at(SP_OBJECT_PARENT(split_obj), char_index + char_count_before);
    // copy the split node
    Inkscape::XML::Node *new_node = duplicate_node_without_children(SP_OBJECT_REPR(split_obj));
    SP_OBJECT_REPR(duplicate_obj)->appendChild(new_node);
    Inkscape::GC::release(new_node);

    // sort out the copied attributes (x/y/dx/dy/rotate)
    split_attributes(split_obj, duplicate_obj->firstChild(), char_index);

    // then move all the subsequent nodes
    split_obj = SP_OBJECT_NEXT(split_obj);
    while (split_obj) {
        Inkscape::XML::Node *move_repr = SP_OBJECT_REPR(split_obj);
        SPObject *next_obj = SP_OBJECT_NEXT(split_obj);  // this is about to become invalidated by removeChild()
        Inkscape::GC::anchor(move_repr);
        SP_OBJECT_REPR(SP_OBJECT_PARENT(split_obj))->removeChild(move_repr);
        SP_OBJECT_REPR(duplicate_obj)->appendChild(move_repr);
        Inkscape::GC::release(move_repr);

        split_obj = next_obj;
    }
    return duplicate_obj->firstChild();
}

/** inserts a new line break at the given position in a text or flowtext
object. If the position is in the middle of a span, the XML tree must be
chopped in two such that the line can be created at the root of the text
element. Returns an iterator pointing just after the inserted break. */
Inkscape::Text::Layout::iterator sp_te_insert_line (SPItem *item, Inkscape::Text::Layout::iterator const &position)
{
    // Disable newlines in a textpath; TODO: maybe on Enter in a textpath, separate it into two
    // texpaths attached to the same path, with a vertical shift
    if (SP_IS_TEXT_TEXTPATH (item))
        return position;

    Inkscape::Text::Layout const *layout = te_get_layout(item);
    SPObject *split_obj;
    Glib::ustring::iterator split_text_iter;
    if (position == layout->end())
        split_obj = NULL;
    else
        layout->getSourceOfCharacter(position, (void**)&split_obj, &split_text_iter);

    if (split_obj == NULL || is_line_break_object(split_obj)) {
        if (split_obj == NULL) split_obj = item->lastChild();
        if (split_obj) {
            Inkscape::XML::Node *new_node = duplicate_node_without_children(SP_OBJECT_REPR(split_obj));
            SP_OBJECT_REPR(SP_OBJECT_PARENT(split_obj))->addChild(new_node, SP_OBJECT_REPR(split_obj));
            Inkscape::GC::release(new_node);
        }
    } else if (SP_IS_STRING(split_obj)) {
        Glib::ustring *string = &SP_STRING(split_obj)->string;
        unsigned char_index = 0;
        for (Glib::ustring::iterator it = string->begin() ; it != split_text_iter ; it++)
            char_index++;
        // we need to split the entire text tree into two
        SPString *new_string = SP_STRING(split_text_object_tree_at(split_obj, char_index));
        SP_OBJECT_REPR(new_string)->setContent(&*split_text_iter.base());   // a little ugly
        string->erase(split_text_iter, string->end());
        SP_OBJECT_REPR(split_obj)->setContent(string->c_str());
        // TODO: if the split point was at the beginning of a span we have a whole load of empty elements to clean up
    } else {
        // TODO
        // I think the only case to put here is arbitrary gaps, which nobody uses yet
    }
    item->updateRepr(SP_OBJECT_REPR(item),SP_OBJECT_WRITE_EXT);
    unsigned char_index = layout->iteratorToCharIndex(position);
    te_update_layout_now(item);
    item->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
    return layout->charIndexToIterator(char_index + 1);
}

/** finds the first SPString after the given position, including children, excluding parents */
static SPString* sp_te_seek_next_string_recursive(SPObject *start_obj)
{
    while (start_obj) {
        if (start_obj->hasChildren()) {
            SPString *found_string = sp_te_seek_next_string_recursive(start_obj->firstChild());
            if (found_string) return found_string;
        }
        if (SP_IS_STRING(start_obj)) return SP_STRING(start_obj);
        start_obj = SP_OBJECT_NEXT(start_obj);
        if (is_line_break_object(start_obj))
            break;   // don't cross line breaks
    }
    return NULL;
}

/** inserts the given characters into the given string and inserts
corresponding new x/y/dx/dy/rotate attributes into all its parents. */
static void insert_into_spstring(SPString *string_item, Glib::ustring::iterator iter_at, gchar const *utf8)
{
    unsigned char_index = 0;
    unsigned char_count = g_utf8_strlen(utf8, -1);
    Glib::ustring *string = &SP_STRING(string_item)->string;

    for (Glib::ustring::iterator it = string->begin() ; it != iter_at ; it++)
        char_index++;
    string->replace(iter_at, iter_at, utf8);

    SPObject *parent_item = string_item;
    for ( ; ; ) {
        char_index += sum_sibling_text_lengths_before(parent_item);
        parent_item = SP_OBJECT_PARENT(parent_item);
        TextTagAttributes *attributes = attributes_for_object(parent_item);
        if (!attributes) break;
        attributes->insert(char_index, char_count);
    }
}

/** Inserts the given text into a text or flowroot object. Line breaks
cannot be inserted using this function, see sp_te_insert_line(). Returns
an iterator pointing just after the inserted text. */
Inkscape::Text::Layout::iterator
sp_te_insert(SPItem *item, Inkscape::Text::Layout::iterator const &position, gchar const *utf8)
{
    if (!g_utf8_validate(utf8,-1,NULL)) {
        g_warning("Trying to insert invalid utf8");
        return position;
    }

    Inkscape::Text::Layout const *layout = te_get_layout(item);
    SPObject *source_obj;
    Glib::ustring::iterator iter_text;
    // we want to insert after the previous char, not before the current char.
    // it makes a difference at span boundaries
    Inkscape::Text::Layout::iterator it_prev_char = position;
    bool cursor_at_start = !it_prev_char.prevCharacter();
    bool cursor_at_end = position == layout->end();
    layout->getSourceOfCharacter(it_prev_char, (void**)&source_obj, &iter_text);
    if (SP_IS_STRING(source_obj)) {
        // the simple case
        if (!cursor_at_start) iter_text++;
        SPString *string_item = SP_STRING(source_obj);
        insert_into_spstring(string_item, cursor_at_end ? string_item->string.end() : iter_text, utf8);
    } else {
        // the not-so-simple case where we're at a line break or other control char; add to the next child/sibling SPString
        if (cursor_at_start) {
            source_obj = item;
            if (source_obj->hasChildren()) {
                source_obj = source_obj->firstChild();
                if (SP_IS_FLOWTEXT(item)) {
                    while (SP_IS_FLOWREGION(source_obj) || SP_IS_FLOWREGIONEXCLUDE(source_obj))
                        source_obj = SP_OBJECT_NEXT(source_obj);
                    if (source_obj == NULL)
                        source_obj = item;
                }
            }
            if (source_obj == item && SP_IS_FLOWTEXT(item)) {
                Inkscape::XML::Node *para = sp_repr_new("svg:flowPara");
                SP_OBJECT_REPR(item)->appendChild(para);
                source_obj = item->lastChild();
            }
        } else
            source_obj = SP_OBJECT_NEXT(source_obj);

        if (source_obj) {  // never fails
            SPString *string_item = sp_te_seek_next_string_recursive(source_obj);
            if (string_item == NULL) {
                // need to add an SPString in this (pathological) case
                Inkscape::XML::Node *rstring = sp_repr_new_text("");
                SP_OBJECT_REPR(source_obj)->addChild(rstring, NULL);
                Inkscape::GC::release(rstring);
                g_assert(SP_IS_STRING(source_obj->firstChild()));
                string_item = SP_STRING(source_obj->firstChild());
            }
            insert_into_spstring(string_item, cursor_at_end ? string_item->string.end() : string_item->string.begin(), utf8);
        }
    }

    item->updateRepr(SP_OBJECT_REPR(item),SP_OBJECT_WRITE_EXT);
    unsigned char_index = layout->iteratorToCharIndex(position);
    te_update_layout_now(item);
    item->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
    return layout->charIndexToIterator(char_index + g_utf8_strlen(utf8, -1));
}


/* ***************************************************************************************************/
//                            D E L E T I N G   T E X T

/** moves all the children of \a from_repr to \a to_repr, either before
the existing children or after them. Order is maintained. The empty
\a from_repr is not deleted. */
static void move_child_nodes(Inkscape::XML::Node *from_repr, Inkscape::XML::Node *to_repr, bool prepend = false)
{
    while (from_repr->childCount()) {
        Inkscape::XML::Node *child = prepend ? from_repr->lastChild() : from_repr->firstChild();
        Inkscape::GC::anchor(child);
        from_repr->removeChild(child);
        if (prepend) to_repr->addChild(child, NULL);
        else to_repr->appendChild(child);
        Inkscape::GC::release(child);
    }
}

/** returns the object in the tree which is the closest ancestor of both
\a one and \a two. It will never return anything higher than \a text. */
static SPObject* get_common_ancestor(SPObject *text, SPObject *one, SPObject *two)
{
    if (one == NULL || two == NULL)
        return text;
    SPObject *common_ancestor = one;
    if (SP_IS_STRING(common_ancestor))
        common_ancestor = SP_OBJECT_PARENT(common_ancestor);
    while (!(common_ancestor == two || common_ancestor->isAncestorOf(two))) {
        g_assert(common_ancestor != text);
        common_ancestor = SP_OBJECT_PARENT(common_ancestor);
    }
    return common_ancestor;
}

/** positions \a para_obj and \a text_iter to be pointing at the end
of the last string in the last leaf object of \a para_obj. If the last
leaf is not an SPString then \a text_iter will be unchanged. */
static void move_to_end_of_paragraph(SPObject **para_obj, Glib::ustring::iterator *text_iter)
{
    while ((*para_obj)->hasChildren())
        *para_obj = (*para_obj)->lastChild();
    if (SP_IS_STRING(*para_obj))
        *text_iter = SP_STRING(*para_obj)->string.end();
}

/** delete the line break pointed to by \a item by merging its children into
the next suitable object and deleting \a item. Returns the object after the
ones that have just been moved and sets \a next_is_sibling accordingly. */
static SPObject* delete_line_break(SPObject *root, SPObject *item, bool *next_is_sibling)
{
    Inkscape::XML::Node *this_repr = SP_OBJECT_REPR(item);
    SPObject *next_item = NULL;
    unsigned moved_char_count = sp_text_get_length(item) - 1;   // the -1 is because it's going to count the line break

    /* some sample cases (the div is the item to be deleted, the * represents where to put the new span):
      <div></div><p>*text</p>
      <p><div></div>*text</p>
      <p><div></div></p><p>*text</p>
    */
    Inkscape::XML::Node *new_span_repr = sp_repr_new(span_name_for_text_object(root));

    if (gchar const *a = this_repr->attribute("dx"))
        new_span_repr->setAttribute("dx", a);
    if (gchar const *a = this_repr->attribute("dy"))
        new_span_repr->setAttribute("dy", a);
    if (gchar const *a = this_repr->attribute("rotate"))
        new_span_repr->setAttribute("rotate", a);

    SPObject *following_item = item;
    while (SP_OBJECT_NEXT(following_item) == NULL) {
        following_item = SP_OBJECT_PARENT(following_item);
        g_assert(following_item != root);
    }
    following_item = SP_OBJECT_NEXT(following_item);

    SPObject *new_parent_item;
    if (SP_IS_STRING(following_item)) {
        new_parent_item = SP_OBJECT_PARENT(following_item);
        SP_OBJECT_REPR(new_parent_item)->addChild(new_span_repr, SP_OBJECT_PREV(following_item) ? SP_OBJECT_REPR(SP_OBJECT_PREV(following_item)) : NULL);
        next_item = following_item;
        *next_is_sibling = true;
    } else {
        new_parent_item = following_item;
        next_item = new_parent_item->firstChild();
        *next_is_sibling = true;
        if (next_item == NULL) {
            next_item = new_parent_item;
            *next_is_sibling = false;
        }
        SP_OBJECT_REPR(new_parent_item)->addChild(new_span_repr, NULL);
    }

    // work around a bug in sp_style_write_difference() which causes the difference
    // not to be written if the second param has a style set which the first does not
    // by causing the first param to have everything set
    SPCSSAttr *dest_node_attrs = sp_repr_css_attr(SP_OBJECT_REPR(new_parent_item), "style");
    SPCSSAttr *this_node_attrs = sp_repr_css_attr(this_repr, "style");
    SPCSSAttr *this_node_attrs_inherited = sp_repr_css_attr_inherited(this_repr, "style");
    Inkscape::Util::List<Inkscape::XML::AttributeRecord const> attrs = dest_node_attrs->attributeList();
    for ( ; attrs ; attrs++) {
        gchar const *key = g_quark_to_string(attrs->key);
        gchar const *this_attr = this_node_attrs_inherited->attribute(key);
        if ((this_attr == NULL || strcmp(attrs->value, this_attr)) && this_node_attrs->attribute(key) == NULL)
            this_node_attrs->setAttribute(key, this_attr);
    }
    sp_repr_css_attr_unref(this_node_attrs_inherited);
    sp_repr_css_attr_unref(this_node_attrs);
    sp_repr_css_attr_unref(dest_node_attrs);
    sp_repr_css_change(new_span_repr, this_node_attrs, "style");

    TextTagAttributes *attributes = attributes_for_object(new_parent_item);
    if (attributes)
        attributes->insert(0, moved_char_count);
    move_child_nodes(this_repr, new_span_repr);
    this_repr->parent()->removeChild(this_repr);
    return next_item;
}

/** erases the given characters from the given string and deletes the
corresponding x/y/dx/dy/rotate attributes from all its parents. */
static void erase_from_spstring(SPString *string_item, Glib::ustring::iterator iter_from, Glib::ustring::iterator iter_to)
{
    unsigned char_index = 0;
    unsigned char_count = 0;
    Glib::ustring *string = &SP_STRING(string_item)->string;

    for (Glib::ustring::iterator it = string->begin() ; it != iter_from ; it++)
        char_index++;
    for (Glib::ustring::iterator it = iter_from ; it != iter_to ; it++)
        char_count++;
    string->erase(iter_from, iter_to);
    SP_OBJECT_REPR(string_item)->setContent(string->c_str());

    SPObject *parent_item = string_item;
    for ( ; ; ) {
        char_index += sum_sibling_text_lengths_before(parent_item);
        parent_item = SP_OBJECT_PARENT(parent_item);
        TextTagAttributes *attributes = attributes_for_object(parent_item);
        if (attributes == NULL) break;

        attributes->erase(char_index, char_count);
        attributes->writeTo(SP_OBJECT_REPR(parent_item));
    }
}

/* Deletes the given characters from a text or flowroot object. This is
quite a complicated operation, partly due to the cleanup that is done if all
the text in a subobject has been deleted, and partly due to the difficulty
of figuring out what is a line break and how to delete one. Returns the
lesser of \a start and \a end, because that is where the cursor should be
put after the deletion is done. */
Inkscape::Text::Layout::iterator
sp_te_delete (SPItem *item, Inkscape::Text::Layout::iterator const &start, Inkscape::Text::Layout::iterator const &end)
{
    if (start == end) return start;
    Inkscape::Text::Layout::iterator first, last;
    if (start < end) {
        first = start;
        last = end;
    } else {
        first = end;
        last = start;
    }
    Inkscape::Text::Layout const *layout = te_get_layout(item);
    SPObject *start_item, *end_item;
    Glib::ustring::iterator start_text_iter, end_text_iter;
    layout->getSourceOfCharacter(first, (void**)&start_item, &start_text_iter);
    layout->getSourceOfCharacter(last, (void**)&end_item, &end_text_iter);
    if (start_item == NULL)
        return first;   // start is at end of text
    if (is_line_break_object(start_item))
        move_to_end_of_paragraph(&start_item, &start_text_iter);
    if (end_item == NULL) {
        end_item = item->lastChild();
        move_to_end_of_paragraph(&end_item, &end_text_iter);
    }
    else if (is_line_break_object(end_item))
        move_to_end_of_paragraph(&end_item, &end_text_iter);

    SPObject *common_ancestor = get_common_ancestor(item, start_item, end_item);

    if (start_item == end_item) {
        // the quick case where we're deleting stuff all from the same string
        if (SP_IS_STRING(start_item)) {     // always true (if it_start != it_end anyway)
            erase_from_spstring(SP_STRING(start_item), start_text_iter, end_text_iter);
        }
    } else {
        SPObject *sub_item = start_item;
        // walk the tree from start_item to end_item, deleting as we go
        while (sub_item != item) {
            if (sub_item == end_item) {
                if (SP_IS_STRING(sub_item)) {
                    Glib::ustring *string = &SP_STRING(sub_item)->string;
                    erase_from_spstring(SP_STRING(sub_item), string->begin(), end_text_iter);
                }
                break;
            }
            if (SP_IS_STRING(sub_item)) {
                SPString *string = SP_STRING(sub_item);
                if (sub_item == start_item)
                    erase_from_spstring(string, start_text_iter, string->string.end());
                else
                    erase_from_spstring(string, string->string.begin(), string->string.end());
            }
            // walk to the next item in the tree
            if (sub_item->hasChildren())
                sub_item = sub_item->firstChild();
            else {
                SPObject *next_item;
                do {
                    bool is_sibling = true;
                    next_item = SP_OBJECT_NEXT(sub_item);
                    if (next_item == NULL) {
                        next_item = SP_OBJECT_PARENT(sub_item);
                        is_sibling = false;
                    }

                    if (is_line_break_object(sub_item))
                        next_item = delete_line_break(item, sub_item, &is_sibling);

                    sub_item = next_item;
                    if (is_sibling) break;
                    // no more siblings, go up a parent
                } while (sub_item != item && sub_item != end_item);
            }
        }
    }

    while (tidy_xml_tree_recursively(common_ancestor));
    te_update_layout_now(item);
    item->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
    layout->validateIterator(&first);
    return first;
}


/* ***************************************************************************************************/
//                            P L A I N   T E X T   F U N C T I O N S

/** Gets a text-only representation of the given text or flowroot object,
replacing line break elements with '\n'. */
static void sp_te_get_ustring_multiline(SPObject const *root, Glib::ustring *string, bool *pending_line_break)
{
    if (*pending_line_break)
        *string += '\n';
    for (SPObject const *child = root->firstChild() ; child ; child = SP_OBJECT_NEXT(child)) {
        if (SP_IS_STRING(child))
            *string += SP_STRING(child)->string;
        else
            sp_te_get_ustring_multiline(child, string, pending_line_break);
    }
    if (!SP_IS_TEXT(root) && !SP_IS_TEXTPATH(root) && is_line_break_object(root))
        *pending_line_break = true;
}

/** Gets a text-only representation of the given text or flowroot object,
replacing line break elements with '\n'. The return value must be free()d. */
gchar *
sp_te_get_string_multiline (SPItem const *text)
{
    Glib::ustring string;
    bool pending_line_break = false;

    if (!SP_IS_TEXT(text) && !SP_IS_FLOWTEXT(text)) return NULL;
    sp_te_get_ustring_multiline(text, &string, &pending_line_break);
    if (string.empty()) return NULL;
    return strdup(string.data());
}

/** Gets a text-only representation of the characters in a text or flowroot
object from \a start to \a end only. Line break elements are replaced with
'\n'. */
Glib::ustring
sp_te_get_string_multiline (SPItem const *text, Inkscape::Text::Layout::iterator const &start, Inkscape::Text::Layout::iterator const &end)
{
    if (start == end) return "";
    Inkscape::Text::Layout::iterator first, last;
    if (start < end) {
        first = start;
        last = end;
    } else {
        first = end;
        last = start;
    }
    Inkscape::Text::Layout const *layout = te_get_layout(text);
    Glib::ustring result;
    // not a particularly fast piece of code. I'll optimise it if people start to notice.
    for ( ; first < last ; first.nextCharacter()) {
        SPObject *char_item;
        Glib::ustring::iterator text_iter;
        layout->getSourceOfCharacter(first, (void**)&char_item, &text_iter);
        if (SP_IS_STRING(char_item))
            result += *text_iter;
        else
            result += '\n';
    }
    return result;
}

void
sp_te_set_repr_text_multiline(SPItem *text, gchar const *str)
{
    g_return_if_fail (text != NULL);
    g_return_if_fail (SP_IS_TEXT(text) || SP_IS_FLOWTEXT(text));

    Inkscape::XML::Node *repr;
    SPObject *object;
    bool is_textpath = false;
    if (SP_IS_TEXT_TEXTPATH (text)) {
        repr = SP_OBJECT_REPR (sp_object_first_child(SP_OBJECT (text)));
        object = sp_object_first_child(SP_OBJECT (text));
        is_textpath = true;
    } else {
        repr = SP_OBJECT_REPR (text);
        object = SP_OBJECT (text);
    }

    if (!str) str = "";
    gchar *content = g_strdup (str);

    repr->setContent("");
    SPObject *child = object->firstChild();
    while (child) {
        SPObject *next = SP_OBJECT_NEXT(child);
        if (!SP_IS_FLOWREGION(child) && !SP_IS_FLOWREGIONEXCLUDE(child))
            repr->removeChild(SP_OBJECT_REPR(child));
        child = next;
    }

    gchar *p = content;
    while (p) {
        gchar *e = strchr (p, '\n');
        if (is_textpath) {
            if (e) *e = ' '; // no lines for textpath, replace newlines with spaces
        } else {
            if (e) *e = '\0';
            Inkscape::XML::Node *rtspan;
            if (SP_IS_TEXT(text)) { // create a tspan for each line
                rtspan = sp_repr_new ("svg:tspan");
                rtspan->setAttribute("sodipodi:role", "line");
            } else { // create a flowPara for each line
                rtspan = sp_repr_new ("svg:flowPara");
            }
            Inkscape::XML::Node *rstr = sp_repr_new_text(p);
            rtspan->addChild(rstr, NULL);
            Inkscape::GC::release(rstr);
            repr->appendChild(rtspan);
            Inkscape::GC::release(rtspan);
        }
        p = (e) ? e + 1 : NULL;
    }
    if (is_textpath) {
        Inkscape::XML::Node *rstr = sp_repr_new_text(content);
        repr->addChild(rstr, NULL);
        Inkscape::GC::release(rstr);
    }

    g_free (content);
}

/* ***************************************************************************************************/
//                           K E R N I N G   A N D   S P A C I N G

/** Returns the attributes block and the character index within that block
which represents the iterator \a position. */
static TextTagAttributes*
text_tag_attributes_at_position(SPItem *item, Inkscape::Text::Layout::iterator const &position, unsigned *char_index)
{
    if (item == NULL || char_index == NULL || !SP_IS_TEXT(item))
        return NULL;   // flowtext doesn't support kerning yet
    SPText *text = SP_TEXT(item);

    SPObject *source_item;
    Glib::ustring::iterator source_text_iter;
    text->layout.getSourceOfCharacter(position, (void**)&source_item, &source_text_iter);

    if (!SP_IS_STRING(source_item)) return NULL;
    Glib::ustring *string = &SP_STRING(source_item)->string;
    *char_index = sum_sibling_text_lengths_before(source_item);
    for (Glib::ustring::iterator it = string->begin() ; it != source_text_iter ; it++)
        ++*char_index;

    return attributes_for_object(SP_OBJECT_PARENT(source_item));
}

void
sp_te_adjust_kerning_screen (SPItem *item, Inkscape::Text::Layout::iterator const &start, Inkscape::Text::Layout::iterator const &end, SPDesktop *desktop, NR::Point by)
{
    // divide increment by zoom
    // divide increment by matrix expansion
    gdouble factor = 1 / desktop->current_zoom();
    NR::Matrix t = sp_item_i2doc_affine(item);
    factor = factor / NR::expansion(t);
    by = factor * by;

    unsigned char_index;
    TextTagAttributes *attributes = text_tag_attributes_at_position(item, std::min(start, end), &char_index);
    if (attributes) attributes->addToDxDy(char_index, by);
    if (start != end) {
        attributes = text_tag_attributes_at_position(item, std::max(start, end), &char_index);
        if (attributes) attributes->addToDxDy(char_index, -by);
    }

    item->updateRepr();
    item->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

void
sp_te_adjust_rotation_screen(SPItem *text, Inkscape::Text::Layout::iterator const &start, Inkscape::Text::Layout::iterator const &end, SPDesktop *desktop, gdouble pixels)
{
    // divide increment by zoom
    // divide increment by matrix expansion
    gdouble factor = 1 / desktop->current_zoom();
    NR::Matrix t = sp_item_i2doc_affine(text);
    factor = factor / NR::expansion(t);
    SPObject *source_item;
    Inkscape::Text::Layout const *layout = te_get_layout(text);
    if (layout == NULL) return;
    layout->getSourceOfCharacter(std::min(start, end), (void**)&source_item);
    if (source_item == NULL) return;
    gdouble degrees = (180/M_PI) * atan2(pixels, SP_OBJECT_PARENT(source_item)->style->font_size.computed / factor);

    sp_te_adjust_rotation(text, start, end, desktop, degrees);
}

void
sp_te_adjust_rotation(SPItem *text, Inkscape::Text::Layout::iterator const &start, Inkscape::Text::Layout::iterator const &end, SPDesktop *desktop, gdouble degrees)
{
    unsigned char_index;
    TextTagAttributes *attributes = text_tag_attributes_at_position(text, std::min(start, end), &char_index);
    if (attributes == NULL) return;

    if (start != end) {
        for (Inkscape::Text::Layout::iterator it = std::min(start, end) ; it != std::max(start, end) ; it.nextCharacter()) {
            attributes = text_tag_attributes_at_position(text, it, &char_index);
            if (attributes) attributes->addToRotate(char_index, degrees);
        }
    } else
        attributes->addToRotate(char_index, degrees);

    text->updateRepr();
    text->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

void
sp_te_adjust_tspan_letterspacing_screen(SPItem *text, Inkscape::Text::Layout::iterator const &start, Inkscape::Text::Layout::iterator const &end, SPDesktop *desktop, gdouble by)
{
    g_return_if_fail (text != NULL);
    g_return_if_fail (SP_IS_TEXT(text) || SP_IS_FLOWTEXT(text));

    Inkscape::Text::Layout const *layout = te_get_layout(text);

    gdouble val;
    SPObject *source_obj;
    unsigned nb_let;
    layout->getSourceOfCharacter(std::min(start, end), (void**)&source_obj);
    if (source_obj == NULL)    // end of text
        source_obj = text->lastChild();
    else if (SP_IS_STRING(source_obj))
        source_obj = source_obj->parent;

    SPStyle *style = SP_OBJECT_STYLE (source_obj);

    // calculate real value
    /* TODO: Consider calculating val unconditionally, i.e. drop the first `if' line, and
       get rid of the `else val = 0.0'.  Similarly below and in sp-string.cpp. */
    if (style->letter_spacing.value != 0 && style->letter_spacing.computed == 0) { // set in em or ex
        if (style->letter_spacing.unit == SP_CSS_UNIT_EM) {
            val = style->font_size.computed * style->letter_spacing.value;
        } else if (style->letter_spacing.unit == SP_CSS_UNIT_EX) {
            val = style->font_size.computed * style->letter_spacing.value * 0.5;
        } else { // unknown unit - should not happen
            val = 0.0;
        }
    } else { // there's a real value in .computed, or it's zero
        val = style->letter_spacing.computed;
    }

    if (start == end) {
        while (!is_line_break_object(source_obj))     // move up the tree so we apply to the closest paragraph
            source_obj = SP_OBJECT_PARENT(source_obj);
        nb_let = sp_text_get_length(source_obj);
    } else {
        nb_let = abs(layout->iteratorToCharIndex(end) - layout->iteratorToCharIndex(start));
    }

    // divide increment by zoom and by the number of characters in the line,
    // so that the entire line is expanded by by pixels, no matter what its length
    gdouble const zoom = desktop->current_zoom();
    gdouble const zby = (by
                         / (zoom * (nb_let > 1 ? nb_let - 1 : 1))
                         / NR::expansion(sp_item_i2doc_affine(SP_ITEM(source_obj))));
    val += zby;

    if (start == end) {
        // set back value to entire paragraph
        style->letter_spacing.normal = FALSE;
        if (style->letter_spacing.value != 0 && style->letter_spacing.computed == 0) { // set in em or ex
            if (style->letter_spacing.unit == SP_CSS_UNIT_EM) {
                style->letter_spacing.value = val / style->font_size.computed;
            } else if (style->letter_spacing.unit == SP_CSS_UNIT_EX) {
                style->letter_spacing.value = val / style->font_size.computed * 2;
            }
        } else {
            style->letter_spacing.computed = val;
        }

        style->letter_spacing.set = TRUE;
    } else {
        // apply to selection only
        SPCSSAttr *css = sp_repr_css_attr_new();
        char string_val[40];
        g_snprintf(string_val, sizeof(string_val), "%f", val);
        sp_repr_css_set_property(css, "letter-spacing", string_val);
        sp_te_apply_style(text, start, end, css);
        sp_repr_css_attr_unref(css);
    }

    text->updateRepr();
    text->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG | SP_TEXT_LAYOUT_MODIFIED_FLAG);
}

void
sp_te_adjust_linespacing_screen (SPItem *text, Inkscape::Text::Layout::iterator const &start, Inkscape::Text::Layout::iterator const &end, SPDesktop *desktop, gdouble by)
{
    // TODO: use start and end iterators to delineate the area to be affected
    g_return_if_fail (text != NULL);
    g_return_if_fail (SP_IS_TEXT(text) || SP_IS_FLOWTEXT(text));

    Inkscape::Text::Layout const *layout = te_get_layout(text);
    SPStyle *style = SP_OBJECT_STYLE (text);

    if (!style->line_height.set || style->line_height.inherit || style->line_height.normal) {
        style->line_height.set = TRUE;
        style->line_height.inherit = FALSE;
        style->line_height.normal = FALSE;
        style->line_height.unit = SP_CSS_UNIT_PERCENT;
        style->line_height.value = style->line_height.computed = Inkscape::Text::Layout::LINE_HEIGHT_NORMAL;
    }

    unsigned line_count = layout->lineIndex(layout->end());
    double all_lines_height = layout->characterAnchorPoint(layout->end())[NR::Y] - layout->characterAnchorPoint(layout->begin())[NR::Y];
    double average_line_height = all_lines_height / (line_count == 0 ? 1 : line_count);
    if (fabs(average_line_height) < 0.001) average_line_height = 0.001;

    // divide increment by zoom and by the number of lines,
    // so that the entire object is expanded by by pixels
    gdouble zby = by / (desktop->current_zoom() * (line_count == 0 ? 1 : line_count));

    // divide increment by matrix expansion
    NR::Matrix t = sp_item_i2doc_affine (SP_ITEM(text));
    zby = zby / NR::expansion(t);

    switch (style->line_height.unit) {
        case SP_CSS_UNIT_NONE:
        default:
            // multiplier-type units, stored in computed
            if (fabs(style->line_height.computed) < 0.001) style->line_height.computed = by < 0.0 ? -0.001 : 0.001;    // the formula below could get stuck at zero
            else style->line_height.computed *= (average_line_height + zby) / average_line_height;
            style->line_height.value = style->line_height.computed;
            break;
        case SP_CSS_UNIT_EM:
        case SP_CSS_UNIT_EX:
        case SP_CSS_UNIT_PERCENT:
            // multiplier-type units, stored in value
            if (fabs(style->line_height.value) < 0.001) style->line_height.value = by < 0.0 ? -0.001 : 0.001;
            else style->line_height.value *= (average_line_height + zby) / average_line_height;
            break;
            // absolute-type units
	    case SP_CSS_UNIT_PX:
            style->line_height.computed += zby;
            style->line_height.value = style->line_height.computed;
            break;
	    case SP_CSS_UNIT_PT:
            style->line_height.computed += zby * PT_PER_PX;
            style->line_height.value = style->line_height.computed;
            break;
	    case SP_CSS_UNIT_PC:
            style->line_height.computed += zby * (PT_PER_PX / 12);
            style->line_height.value = style->line_height.computed;
            break;
	    case SP_CSS_UNIT_MM:
            style->line_height.computed += zby * MM_PER_PX;
            style->line_height.value = style->line_height.computed;
            break;
	    case SP_CSS_UNIT_CM:
            style->line_height.computed += zby * CM_PER_PX;
            style->line_height.value = style->line_height.computed;
            break;
	    case SP_CSS_UNIT_IN:
            style->line_height.computed += zby * IN_PER_PX;
            style->line_height.value = style->line_height.computed;
            break;
    }
    text->updateRepr();
    text->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG | SP_TEXT_LAYOUT_MODIFIED_FLAG);
}


/* ***************************************************************************************************/
//                           S T Y L E   A P P L I C A T I O N


/** converts an iterator to a character index, mainly because ustring::substr()
doesn't have a version that takes iterators as parameters. */
static unsigned char_index_of_iterator(Glib::ustring const &string, Glib::ustring::const_iterator text_iter)
{
    unsigned n = 0;
    for (Glib::ustring::const_iterator it = string.begin() ; it != string.end() && it != text_iter ; it++)
        n++;
    return n;
}

/** applies the given style string on top of the existing styles for \a item,
as opposed to sp_style_merge_from_style_string which merges its parameter
underneath the existing styles (ie ignoring already set properties). */
static void overwrite_style_with_string(SPObject *item, gchar const *style_string)
{
    SPStyle *new_style = sp_style_new();
    sp_style_merge_from_style_string(new_style, style_string);
    gchar const *item_style_string = SP_OBJECT_REPR(item)->attribute("style");
    if (item_style_string && *item_style_string)
        sp_style_merge_from_style_string(new_style, item_style_string);
    gchar *new_style_string = sp_style_write_string(new_style);
    sp_style_unref(new_style);
    SP_OBJECT_REPR(item)->setAttribute("style", new_style_string && *new_style_string ? new_style_string : NULL);
    g_free(new_style_string);
}

/** Returns true if the style of \a parent and the style of \a child are
equivalent (and hence the children of both will appear the same). It is a
limitation of the current implementation that \a parent must be a (not
necessarily immediate) ancestor of \a child. */
static bool objects_have_equal_style(SPObject const *parent, SPObject const *child)
{
    // the obvious implementation of strcmp(style_write_all(parent), style_write_all(child))
    // will not work. Firstly because of an inheritance bug in style.cpp that has
    // implications too large for me to feel safe fixing, but mainly because the css spec
    // requires that the computed value is inherited, not the specified value.
    g_assert(parent->isAncestorOf(child));
    gchar *parent_style = sp_style_write_string(parent->style, SP_STYLE_FLAG_ALWAYS);
    // we have to write parent_style then read it again, because some properties format their values
    // differently depending on whether they're set or not (*cough*dash-offset*cough*)
    SPStyle *parent_spstyle = sp_style_new();
    sp_style_merge_from_style_string(parent_spstyle, parent_style);
    g_free(parent_style);
    parent_style = sp_style_write_string(parent_spstyle, SP_STYLE_FLAG_ALWAYS);
    sp_style_unref(parent_spstyle);

    Glib::ustring child_style_construction(parent_style);
    while (child != parent) {
        char const *style_text = SP_OBJECT_REPR(child)->attribute("style");
        if (style_text && *style_text) {
            child_style_construction += ';';
            child_style_construction += style_text;
        }
        child = SP_OBJECT_PARENT(child);
    }
    SPStyle *child_spstyle = sp_style_new();
    sp_style_merge_from_style_string(child_spstyle, child_style_construction.c_str());
    gchar *child_style = sp_style_write_string(child_spstyle, SP_STYLE_FLAG_ALWAYS);
    sp_style_unref(child_spstyle);
    bool equal = !strcmp(child_style, parent_style);
    g_free(child_style);
    g_free(parent_style);
    return equal;
}

/** returns true if \a first and \a second contain all the same attributes
with the same values as each other. Note that we have to compare both
forwards and backwards to make sure we don't miss any attributes that are
in one but not the other. */
static bool css_attrs_are_equal(SPCSSAttr const *first, SPCSSAttr const *second)
{
    Inkscape::Util::List<Inkscape::XML::AttributeRecord const> attrs = first->attributeList();
    for ( ; attrs ; attrs++) {
        gchar const *other_attr = second->attribute(g_quark_to_string(attrs->key));
        if (other_attr == NULL || strcmp(attrs->value, other_attr))
            return false;
    }
    attrs = second->attributeList();
    for ( ; attrs ; attrs++) {
        gchar const *other_attr = first->attribute(g_quark_to_string(attrs->key));
        if (other_attr == NULL || strcmp(attrs->value, other_attr))
            return false;
    }
    return true;
}

/** sets the given css attribute on this object and all its descendants.
Annoyingly similar to sp_desktop_apply_css_recursive(), except without the
transform stuff. */
static void apply_css_recursive(SPObject *o, SPCSSAttr const *css)
{
    sp_repr_css_change(SP_OBJECT_REPR(o), const_cast<SPCSSAttr*>(css), "style");

    for (SPObject *child = sp_object_first_child(SP_OBJECT(o)) ; child != NULL ; child = SP_OBJECT_NEXT(child) ) {
        if (sp_repr_css_property(const_cast<SPCSSAttr*>(css), "opacity", NULL) != NULL) {
            // Unset properties which are accumulating and thus should not be set recursively.
            // For example, setting opacity 0.5 on a group recursively would result in the visible opacity of 0.25 for an item in the group.
            SPCSSAttr *css_recurse = sp_repr_css_attr_new();
            sp_repr_css_merge(css_recurse, const_cast<SPCSSAttr*>(css));
            sp_repr_css_set_property(css_recurse, "opacity", NULL);
            apply_css_recursive(child, css_recurse);
            sp_repr_css_attr_unref(css_recurse);
        } else {
            apply_css_recursive(child, const_cast<SPCSSAttr*>(css));
        }
    }
}

/** applies the given style to all the objects at the given level and below
which are between \a start_item and \a end_item, creating spans as necessary.
If \a start_item or \a end_item are NULL then the style is applied to all
objects to the beginning or end respectively. \a span_object_name is the
name of the xml for a text span (ie tspan or flowspan). */
static void recursively_apply_style(SPObject *common_ancestor, SPCSSAttr const *css, SPObject *start_item, Glib::ustring::iterator start_text_iter, SPObject *end_item, Glib::ustring::iterator end_text_iter, char const *span_object_name)
{
    bool passed_start = start_item == NULL ? true : false;

    for (SPObject *child = common_ancestor->firstChild() ; child != NULL ; child = SP_OBJECT_NEXT(child)) {
        if (start_item == child)
            passed_start = true;

        if (passed_start) {
            if (end_item && child->isAncestorOf(end_item)) {
                recursively_apply_style(child, css, NULL, start_text_iter, end_item, end_text_iter, span_object_name);
                break;
            }
            // apply style

            // note that when adding stuff we must make sure that 'child' stays valid so the for loop keeps working.
            // often this means that new spans are created before child and child is modified only
            if (SP_IS_STRING(child)) {
                SPString *string_item = SP_STRING(child);
                bool surround_entire_string = true;

                Inkscape::XML::Node *child_span = sp_repr_new(span_object_name);
                sp_repr_css_set(child_span, const_cast<SPCSSAttr*>(css), "style");   // better hope that prototype wasn't nonconst for a good reason
                SPObject *prev_item = SP_OBJECT_PREV(child);
                Inkscape::XML::Node *prev_repr = prev_item ? SP_OBJECT_REPR(prev_item) : NULL;

                if (child == start_item || child == end_item) {
                    surround_entire_string = false;
                    if (start_item == end_item && start_text_iter != string_item->string.begin()) {
                        // eg "abcDEFghi"  -> "abc"<span>"DEF"</span>"ghi"
                        unsigned start_char_index = char_index_of_iterator(string_item->string, start_text_iter);
                        unsigned end_char_index = char_index_of_iterator(string_item->string, end_text_iter);

                        Inkscape::XML::Node *text_before = sp_repr_new_text(string_item->string.substr(0, start_char_index).c_str());
                        SP_OBJECT_REPR(common_ancestor)->addChild(text_before, prev_repr);
                        SP_OBJECT_REPR(common_ancestor)->addChild(child_span, text_before);
                        Inkscape::GC::release(text_before);
                        Inkscape::XML::Node *text_in_span = sp_repr_new_text(string_item->string.substr(start_char_index, end_char_index - start_char_index).c_str());
                        child_span->appendChild(text_in_span);
                        Inkscape::GC::release(text_in_span);
                        SP_OBJECT_REPR(child)->setContent(string_item->string.substr(end_char_index).c_str());

                    } else if (child == end_item) {
                        // eg "ABCdef" -> <span>"ABC"</span>"def"
                        //  (includes case where start_text_iter == begin())
                        // NB: we might create an empty string here. Doesn't matter, it'll get cleaned up later
                        unsigned end_char_index = char_index_of_iterator(string_item->string, end_text_iter);

                        SP_OBJECT_REPR(common_ancestor)->addChild(child_span, prev_repr);
                        Inkscape::XML::Node *text_in_span = sp_repr_new_text(string_item->string.substr(0, end_char_index).c_str());
                        child_span->appendChild(text_in_span);
                        Inkscape::GC::release(text_in_span);
                        SP_OBJECT_REPR(child)->setContent(string_item->string.substr(end_char_index).c_str());

                    } else if (start_text_iter != string_item->string.begin()) {
                        // eg "abcDEF" -> "abc"<span>"DEF"</span>
                        unsigned start_char_index = char_index_of_iterator(string_item->string, start_text_iter);

                        Inkscape::XML::Node *text_before = sp_repr_new_text(string_item->string.substr(0, start_char_index).c_str());
                        SP_OBJECT_REPR(common_ancestor)->addChild(text_before, prev_repr);
                        SP_OBJECT_REPR(common_ancestor)->addChild(child_span, text_before);
                        Inkscape::GC::release(text_before);
                        Inkscape::XML::Node *text_in_span = sp_repr_new_text(string_item->string.substr(start_char_index).c_str());
                        child_span->appendChild(text_in_span);
                        Inkscape::GC::release(text_in_span);
                        child->deleteObject();
                        child = sp_object_get_child_by_repr(common_ancestor, child_span);

                    } else
                        surround_entire_string = true;
                }
                if (surround_entire_string) {
                    Inkscape::XML::Node *child_repr = SP_OBJECT_REPR(child);
                    SP_OBJECT_REPR(common_ancestor)->addChild(child_span, child_repr);
                    Inkscape::GC::anchor(child_repr);
                    SP_OBJECT_REPR(common_ancestor)->removeChild(child_repr);
                    child_span->appendChild(child_repr);
                    Inkscape::GC::release(child_repr);
                    child = sp_object_get_child_by_repr(common_ancestor, child_span);
                }
                Inkscape::GC::release(child_span);

            } else if (child != end_item) {   // not a string and we're applying to the entire object. This is easy
                apply_css_recursive(child, css);
            }

        } else {  // !passed_start
            if (child->isAncestorOf(start_item)) {
                recursively_apply_style(child, css, start_item, start_text_iter, end_item, end_text_iter, span_object_name);
                if (end_item && child->isAncestorOf(end_item))
                    break;   // only happens when start_item == end_item (I think)
                passed_start = true;
            }
        }

        if (end_item == child)
            break;
    }
}

/* if item is at the beginning of a tree it doesn't matter which element
it points to so for neatness we would like it to point to the highest
possible child of \a common_ancestor. There is no iterator return because
a string can never be an ancestor.

eg: <span><span>*ABC</span>DEFghi</span> where * is the \a item. We would
like * to point to the inner span because we can apply style to that whole
span. */
static SPObject* ascend_while_first(SPObject *item, Glib::ustring::iterator text_iter, SPObject *common_ancestor)
{
    if (item == common_ancestor)
        return item;
    if (SP_IS_STRING(item))
        if (text_iter != SP_STRING(item)->string.begin())
            return item;
    for ( ; ; ) {
        SPObject *parent = SP_OBJECT_PARENT(item);
        if (parent == common_ancestor)
            break;
        if (item != parent->firstChild())
            break;
        item = parent;
    }
    return item;
}


/**     empty spans: abc<span></span>def
                      -> abcdef                  */
static bool tidy_operator_empty_spans(SPObject **item)
{
    if ((*item)->hasChildren()) return false;
    if (is_line_break_object(*item)) return false;
    if (SP_IS_STRING(*item) && !SP_STRING(*item)->string.empty()) return false;
    SPObject *next = SP_OBJECT_NEXT(*item);
    (*item)->deleteObject();
    *item = next;
    return true;
}

/**    inexplicable spans: abc<span style="">def</span>ghi
                            -> "abc""def""ghi"
the repeated strings will be merged by another operator. */
static bool tidy_operator_inexplicable_spans(SPObject **item)
{
    if (SP_IS_STRING(*item)) return false;
    if (is_line_break_object(*item)) return false;
    TextTagAttributes *attrs = attributes_for_object(*item);
    if (attrs && attrs->anyAttributesSet()) return false;
    if (!objects_have_equal_style(SP_OBJECT_PARENT(*item), *item)) return false;
    SPObject *next = *item;
    while ((*item)->hasChildren()) {
        Inkscape::XML::Node *repr = SP_OBJECT_REPR((*item)->firstChild());
        Inkscape::GC::anchor(repr);
        SP_OBJECT_REPR(*item)->removeChild(repr);
        SP_OBJECT_REPR(SP_OBJECT_PARENT(*item))->addChild(repr, SP_OBJECT_REPR(next));
        Inkscape::GC::release(repr);
        next = SP_OBJECT_NEXT(next);
    }
    (*item)->deleteObject();
    *item = next;
    return true;
}

/**    repeated spans: <font a>abc</font><font a>def</font>
                        -> <font a>abcdef</font>            */
static bool tidy_operator_repeated_spans(SPObject **item)
{
    SPObject *first = *item;
    SPObject *second = SP_OBJECT_NEXT(first);
    if (second == NULL) return false;

    Inkscape::XML::Node *first_repr = SP_OBJECT_REPR(first);
    Inkscape::XML::Node *second_repr = SP_OBJECT_REPR(second);

    if (first_repr->type() != second_repr->type()) return false;

    if (SP_IS_STRING(first) && SP_IS_STRING(second)) {
        // also amalgamate consecutive SPStrings into one
        Glib::ustring merged_string = SP_STRING(first)->string + SP_STRING(second)->string;
        SP_OBJECT_REPR(first)->setContent(merged_string.c_str());
        second_repr->parent()->removeChild(second_repr);
        return true;
    }

    // merge consecutive spans with identical styles into one
    if (first_repr->type() != Inkscape::XML::ELEMENT_NODE) return false;
    if (strcmp(first_repr->name(), second_repr->name()) != 0) return false;
    if (is_line_break_object(second)) return false;
    gchar const *first_style = first_repr->attribute("style");
    gchar const *second_style = second_repr->attribute("style");
    if (!((first_style == NULL && second_style == NULL)
          || (first_style != NULL && second_style != NULL && !strcmp(first_style, second_style))))
        return false;

    // all our tests passed: do the merge
    TextTagAttributes *attributes_first = attributes_for_object(first);
    TextTagAttributes *attributes_second = attributes_for_object(second);
    if (attributes_first && attributes_second && attributes_second->anyAttributesSet()) {
        TextTagAttributes attributes_first_copy = *attributes_first;
        attributes_first->join(attributes_first_copy, *attributes_second, sp_text_get_length(first));
    }
    move_child_nodes(second_repr, first_repr);
    second_repr->parent()->removeChild(second_repr);
    return true;
    // *item is still the next object to process
}

/**    redundant nesting: <font a><font b>abc</font></font>
                           -> <font b>abc</font>
       excessive nesting: <font a><size 1>abc</size></font>
                           -> <font a,size 1>abc</font>      */
static bool tidy_operator_excessive_nesting(SPObject **item)
{
    if (!(*item)->hasChildren()) return false;
    if ((*item)->firstChild() != (*item)->lastChild()) return false;
    if (SP_IS_FLOWREGION((*item)->firstChild()) || SP_IS_FLOWREGIONEXCLUDE((*item)->firstChild()))
        return false;
    if (SP_IS_STRING((*item)->firstChild())) return false;
    if (is_line_break_object((*item)->firstChild())) return false;
    TextTagAttributes *attrs = attributes_for_object((*item)->firstChild());
    if (attrs && attrs->anyAttributesSet()) return false;
    gchar const *child_style = SP_OBJECT_REPR((*item)->firstChild())->attribute("style");
    if (child_style && *child_style)
        overwrite_style_with_string(*item, child_style);
    move_child_nodes(SP_OBJECT_REPR((*item)->firstChild()), SP_OBJECT_REPR(*item));
    (*item)->firstChild()->deleteObject();
    return true;
}

/** helper for tidy_operator_redundant_double_nesting() */
static bool redundant_double_nesting_processor(SPObject **item, SPObject *child, bool prepend)
{
    if (SP_IS_FLOWREGION(child) || SP_IS_FLOWREGIONEXCLUDE(child))
        return false;
    if (SP_IS_STRING(child)) return false;
    if (is_line_break_object(child)) return false;
    if (is_line_break_object(*item)) return false;
    TextTagAttributes *attrs = attributes_for_object(child);
    if (attrs && attrs->anyAttributesSet()) return false;
    if (!objects_have_equal_style(SP_OBJECT_PARENT(*item), child)) return false;

    Inkscape::XML::Node *insert_after_repr;
    if (prepend) insert_after_repr = SP_OBJECT_REPR(SP_OBJECT_PREV(*item));
    else insert_after_repr = SP_OBJECT_REPR(*item);
    while (SP_OBJECT_REPR(child)->childCount()) {
        Inkscape::XML::Node *move_repr = SP_OBJECT_REPR(child)->firstChild();
        Inkscape::GC::anchor(move_repr);
        SP_OBJECT_REPR(child)->removeChild(move_repr);
        SP_OBJECT_REPR(SP_OBJECT_PARENT(*item))->addChild(move_repr, insert_after_repr);
        Inkscape::GC::release(move_repr);
        insert_after_repr = move_repr;      // I think this will stay valid long enough. It's garbage collected these days.
    }
    child->deleteObject();
    return true;
}

/**    redundant double nesting: <font b><font a><font b>abc</font>def</font>ghi</font>
                                -> <font b>abc<font a>def</font>ghi</font>
this function does its work when the parameter is the <font a> tag in the
example. You may note that this only does its work when the doubly-nested
child is the first or last. The other cases are called 'style inversion'
below, and I'm not yet convinced that the result of that operation will be
tidier in all cases. */
static bool tidy_operator_redundant_double_nesting(SPObject **item)
{
    if (!(*item)->hasChildren()) return false;
    if ((*item)->firstChild() == (*item)->lastChild()) return false;     // this is excessive nesting, done above
    if (redundant_double_nesting_processor(item, (*item)->firstChild(), true))
        return true;
    if (redundant_double_nesting_processor(item, (*item)->lastChild(), false))
        return true;
    return false;
}

/** helper for tidy_operator_redundant_semi_nesting(). Checks a few things,
then compares the styles for item+child versus just child. If they're equal,
tidying is possible. */
static bool redundant_semi_nesting_processor(SPObject **item, SPObject *child, bool prepend)
{
    if (SP_IS_FLOWREGION(child) || SP_IS_FLOWREGIONEXCLUDE(child))
        return false;
    if (SP_IS_STRING(child)) return false;
    if (is_line_break_object(child)) return false;
    if (is_line_break_object(*item)) return false;
    TextTagAttributes *attrs = attributes_for_object(child);
    if (attrs && attrs->anyAttributesSet()) return false;
    attrs = attributes_for_object(*item);
    if (attrs && attrs->anyAttributesSet()) return false;

    SPCSSAttr *css_child_and_item = sp_repr_css_attr_new();
    SPCSSAttr *css_child_only = sp_repr_css_attr_new();
    gchar const *child_style = SP_OBJECT_REPR(child)->attribute("style");
    if (child_style && *child_style) {
        sp_repr_css_attr_add_from_string(css_child_and_item, child_style);
        sp_repr_css_attr_add_from_string(css_child_only, child_style);
    }
    gchar const *item_style = SP_OBJECT_REPR(*item)->attribute("style");
    if (item_style && *item_style) {
        sp_repr_css_attr_add_from_string(css_child_and_item, item_style);
    }
    bool equal = css_attrs_are_equal(css_child_only, css_child_and_item);
    sp_repr_css_attr_unref(css_child_and_item);
    sp_repr_css_attr_unref(css_child_only);
    if (!equal) return false;

    Inkscape::XML::Node *new_span = sp_repr_new(SP_OBJECT_REPR(*item)->name());
    if (prepend) {
        SPObject *prev = SP_OBJECT_PREV(*item);
        SP_OBJECT_REPR(SP_OBJECT_PARENT(*item))->addChild(new_span, prev ? SP_OBJECT_REPR(prev) : NULL);
    } else
        SP_OBJECT_REPR(SP_OBJECT_PARENT(*item))->addChild(new_span, SP_OBJECT_REPR(*item));
    new_span->setAttribute("style", SP_OBJECT_REPR(child)->attribute("style"));
    move_child_nodes(SP_OBJECT_REPR(child), new_span);
    Inkscape::GC::release(new_span);
    child->deleteObject();
    return true;
}

/**    redundant semi-nesting: <font a><font b>abc</font>def</font>
                                -> <font b>abc</font><font>def</font>
test this by applying a colour to a region, then a different colour to
a partially-overlapping region. */
static bool tidy_operator_redundant_semi_nesting(SPObject **item)
{
    if (!(*item)->hasChildren()) return false;
    if ((*item)->firstChild() == (*item)->lastChild()) return false;     // this is redundant nesting, done above
    if (redundant_semi_nesting_processor(item, (*item)->firstChild(), true))
        return true;
    if (redundant_semi_nesting_processor(item, (*item)->lastChild(), false))
        return true;
    return false;
}

/** helper for tidy_operator_styled_whitespace(), finds the last string object
in a paragraph which is not \a not_obj. */
static SPString* find_last_string_child_not_equal_to(SPObject *root, SPObject *not_obj)
{
    for (SPObject *child = root->lastChild() ; child ; child = SP_OBJECT_PREV(child))
    {
        if (child == not_obj) continue;
        if (child->hasChildren()) {
            SPString *ret = find_last_string_child_not_equal_to(child, not_obj);
            if (ret) return ret;
        } else if (SP_IS_STRING(child))
            return SP_STRING(child);
    }
    return NULL;
}

/** whitespace-only spans: abc<font> </font>def
                            -> abc<font></font> def
                           abc<b><i>def</i> </b>ghi
                            -> abc<b><i>def</i></b> ghi   */
static bool tidy_operator_styled_whitespace(SPObject **item)
{
    if (!SP_IS_STRING(*item)) return false;
    Glib::ustring const &str = SP_STRING(*item)->string;
    for (Glib::ustring::const_iterator it = str.begin() ; it != str.end() ; ++it)
        if (!g_unichar_isspace(*it)) return false;

    SPObject *test_item = *item;
    SPString *next_string;
    for ( ; ; ) {  // find the next string
        next_string = sp_te_seek_next_string_recursive(SP_OBJECT_NEXT(test_item));
        if (next_string) {
            next_string->string.insert(0, str);
            break;
        }
        for ( ; ; ) {   // go up one item in the xml
            test_item = SP_OBJECT_PARENT(test_item);
            if (is_line_break_object(test_item)) break;
            SPObject *next = SP_OBJECT_NEXT(test_item);
            if (next) {
                test_item = next;
                break;
            }
        }
        if (is_line_break_object(test_item)) {  // no next string, see if there's a prev string
            next_string = find_last_string_child_not_equal_to(test_item, *item);
            if (next_string == NULL) return false;   // an empty paragraph
            next_string->string += str;
            break;
        }
    }
    SP_OBJECT_REPR(next_string)->setContent(next_string->string.c_str());
    SPObject *delete_obj = *item;
    *item = SP_OBJECT_NEXT(*item);
    delete_obj->deleteObject();
    return true;
}

/* possible tidy operators that are not yet implemented, either because
they are difficult, occur infrequently, or because I'm not sure that the
output is tidier in all cases:
    duplicate styles in line break elements: <div italic><para italic>abc</para></div>
                                              -> <div italic><para>abc</para></div>
    style inversion: <font a>abc<font b>def<font a>ghi</font>jkl</font>mno</font>
                      -> <font a>abc<font b>def</font>ghi<font b>jkl</font>mno</font>
    mistaken precedence: <font a,size 1>abc</font><size 1>def</size>
                          -> <size 1><font a>abc</font>def</size>
*/

/** Recursively walks the xml tree calling a set of cleanup operations on
every child. Returns true if any changes were made to the tree.

All the tidy operators return true if they made changes, and alter their
parameter to point to the next object that should be processed, or NULL.
They must not significantly alter (ie delete) any ancestor elements of the
one they are passed.

It may be that some of the later tidy operators that I wrote are actually
general cases of the earlier operators, and hence the special-case-only
versions can be removed. I haven't analysed my work in detail to figure
out if this is so. */
static bool tidy_xml_tree_recursively(SPObject *root)
{
    static bool (* const tidy_operators[])(SPObject**) = {
        tidy_operator_empty_spans,
        tidy_operator_inexplicable_spans,
        tidy_operator_repeated_spans,
        tidy_operator_excessive_nesting,
        tidy_operator_redundant_double_nesting,
        tidy_operator_redundant_semi_nesting,
        tidy_operator_styled_whitespace
    };
    bool changes = false;

    for (SPObject *child = root->firstChild() ; child != NULL ; ) {
        if (SP_IS_FLOWREGION(child) || SP_IS_FLOWREGIONEXCLUDE(child)) {
            child = SP_OBJECT_NEXT(child);
            continue;
        }
        if (child->hasChildren())
            changes |= tidy_xml_tree_recursively(child);

        unsigned i;
        for (i = 0 ; i < sizeof(tidy_operators) / sizeof(tidy_operators[0]) ; i++) {
            if (tidy_operators[i](&child)) {
                changes = true;
                break;
            }
        }
        if (i == sizeof(tidy_operators) / sizeof(tidy_operators[0]))
            child = SP_OBJECT_NEXT(child);
    }
    return changes;
}

/** Applies the given CSS fragment to the characters of the given text or
flowtext object between \a start and \a end, creating or removing span
elements as necessary and optimal. */
void sp_te_apply_style(SPItem *text, Inkscape::Text::Layout::iterator const &start, Inkscape::Text::Layout::iterator const &end, SPCSSAttr const *css)
{
    // in the comments in the code below, capital letters are inside the application region, lowercase are outside
    if (start == end) return;
    Inkscape::Text::Layout::iterator first, last;
    if (start < end) {
        first = start;
        last = end;
    } else {
        first = end;
        last = start;
    }
    Inkscape::Text::Layout const *layout = te_get_layout(text);
    SPObject *start_item, *end_item;
    Glib::ustring::iterator start_text_iter, end_text_iter;
    layout->getSourceOfCharacter(first, (void**)&start_item, &start_text_iter);
    layout->getSourceOfCharacter(last, (void**)&end_item, &end_text_iter);
    if (start_item == NULL)
        return;   // start is at end of text
    if (is_line_break_object(start_item))
        start_item = SP_OBJECT_NEXT(start_item);
    if (is_line_break_object(end_item))
        end_item = SP_OBJECT_NEXT(end_item);
    if (end_item == NULL) end_item = text;

    /* stage 1: applying the style. Go up to the closest common ancestor of
    start and end and then semi-recursively apply the style to all the
    objects in between. The semi-recursion is because it's only necessary
    at the beginning and end; the style can just be applied to the root
    child in the middle.
    eg: <span>abcDEF</span><span>GHI</span><span>JKLmno</span>
    The recursion may involve creating new spans.
    */
    SPObject *common_ancestor = get_common_ancestor(text, start_item, end_item);
    start_item = ascend_while_first(start_item, start_text_iter, common_ancestor);
    end_item = ascend_while_first(end_item, end_text_iter, common_ancestor);
    recursively_apply_style(common_ancestor, css, start_item, start_text_iter, end_item, end_text_iter, span_name_for_text_object(text));

    /* stage 2: cleanup the xml tree (of which there are multiple passes) */
    /* discussion: this stage requires a certain level of inventiveness because
    it's not clear what the best representation is in many cases. An ideal
    implementation would provide some sort of scoring function to rate the
    ugliness of a given xml tree and try to reduce said function, but providing
    the various possibilities to be rated is non-trivial. Instead, I have opted
    for a multi-pass technique which simply recognises known-ugly patterns and
    has matching routines for optimising the patterns it finds. It's reasonably
    easy to add new pattern matching processors. If everything gets disastrous
    and neither option can be made to work, a fallback could be to reduce
    everything to a single level of nesting and drop all pretence of
    roundtrippability. */
    while (tidy_xml_tree_recursively(common_ancestor));

    // if we only modified subobjects this won't have been automatically sent
    text->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG);
}

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
