#ifndef __SP_OBJECT_ATTRIBUTES_H__
#define __SP_OBJECT_ATTRIBUTES_H__

/*
 * Generic object attribute editor
 *
 * Author:
 *   Lauris Kaplinski <lauris@ximian.com>
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Licensed under GNU GPL
 */

#include <glib.h>



#include <gtk/gtkwidget.h>
#include "../forward.h"

void sp_object_attributes_dialog (SPObject *object, const gchar *tag);



#endif
