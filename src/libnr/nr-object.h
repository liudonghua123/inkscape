#ifndef __NR_OBJECT_H__
#define __NR_OBJECT_H__

/*
 * RGBA display list system for inkscape
 *
 * Author:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * This code is in public domain
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>

typedef guint32 NRType;

struct NRObject;
struct NRObjectClass;

#define NR_TYPE_OBJECT (nr_object_get_type ())
#define NR_OBJECT(o) (NR_CHECK_INSTANCE_CAST ((o), NR_TYPE_OBJECT, NRObject))
#define NR_IS_OBJECT(o) (NR_CHECK_INSTANCE_TYPE ((o), NR_TYPE_OBJECT))

#define NR_TYPE_ACTIVE_OBJECT (nr_active_object_get_type ())
#define NR_ACTIVE_OBJECT(o) (NR_CHECK_INSTANCE_CAST ((o), NR_TYPE_ACTIVE_OBJECT, NRActiveObject))
#define NR_IS_ACTIVE_OBJECT(o) (NR_CHECK_INSTANCE_TYPE ((o), NR_TYPE_ACTIVE_OBJECT))

#define nr_return_if_fail(expr) if (!(expr) && nr_emit_fail_warning (__FILE__, __LINE__, "?", #expr)) return
#define nr_return_val_if_fail(expr,val) if (!(expr) && nr_emit_fail_warning (__FILE__, __LINE__, "?", #expr)) return (val)

unsigned int nr_emit_fail_warning (const gchar *file, unsigned int line, const gchar *method, const gchar *expr);

#ifndef NR_DISABLE_CAST_CHECKS
#define NR_CHECK_INSTANCE_CAST(ip, tc, ct) ((ct *) nr_object_check_instance_cast (ip, tc))
#else
#define NR_CHECK_INSTANCE_CAST(ip, tc, ct) ((ct *) ip)
#endif

#define NR_CHECK_INSTANCE_TYPE(ip, tc) nr_object_check_instance_type (ip, tc)
#define NR_OBJECT_GET_CLASS(ip) (((NRObject *) ip)->klass)

NRType nr_type_is_a (NRType type, NRType test);

void *nr_object_check_instance_cast (void *ip, NRType tc);
unsigned int nr_object_check_instance_type (void *ip, NRType tc);

NRType nr_object_register_type (NRType parent,
				      gchar const *name,
				      unsigned int csize,
				      unsigned int isize,
				      void (* cinit) (NRObjectClass *),
				      void (* iinit) (NRObject *));

/* NRObject */

struct NRObject {
	NRObjectClass *klass;
	unsigned int refcount;
};

struct NRObjectClass {
	NRType type;
	NRObjectClass *parent;

	gchar *name;
	unsigned int csize;
	unsigned int isize;
	void (* cinit) (NRObjectClass *);
	void (* iinit) (NRObject *);

	void (* finalize) (NRObject *object);
};

NRType nr_object_get_type (void);

/* Dynamic lifecycle */

NRObject *nr_object_new (NRType type);
NRObject *nr_object_delete (NRObject *object);

NRObject *nr_object_ref (NRObject *object);
NRObject *nr_object_unref (NRObject *object);

/* Automatic lifecycle */

NRObject *nr_object_setup (NRObject *object, NRType type);
NRObject *nr_object_release (NRObject *object);

/* NRActiveObject */

struct NRObjectEventVector {
	void (* dispose) (NRObject *object, void *data);
};

struct NRObjectListener {
	const NRObjectEventVector *vector;
	unsigned int size;
	void *data;
};

struct NRObjectCallbackBlock {
	unsigned int size;
	unsigned int length;
	NRObjectListener listeners[1];
};

struct NRActiveObject {
	NRObject object;
	NRObjectCallbackBlock *callbacks;
};

struct NRActiveObjectClass {
	NRObjectClass parent_class;
};

NRType nr_active_object_get_type (void);

void nr_active_object_add_listener (NRActiveObject *object, const NRObjectEventVector *vector, unsigned int size, void *data);
void nr_active_object_remove_listener_by_data (NRActiveObject *object, void *data);

#endif

