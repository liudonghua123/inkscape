#define __NR_MATRIX_C__

/*
 * Pixel buffer rendering library
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * This code is in public domain
 */

#include "nr-matrix.h"

NRMatrix *
nr_matrix_d_from_f (NRMatrix *d, const NRMatrix *s)
{
	if (s) {
		d->c[0] = s->c[0];
		d->c[1] = s->c[1];
		d->c[2] = s->c[2];
		d->c[3] = s->c[3];
		d->c[4] = s->c[4];
		d->c[5] = s->c[5];
	} else {
		nr_matrix_d_set_identity (d);
	}

	return d;
}

NRMatrix *
nr_matrix_f_from_d (NRMatrix *d, const NRMatrix *s)
{
	if (s) {
		d->c[0] = (float) s->c[0];
		d->c[1] = (float) s->c[1];
		d->c[2] = (float) s->c[2];
		d->c[3] = (float) s->c[3];
		d->c[4] = (float) s->c[4];
		d->c[5] = (float) s->c[5];
	} else {
		nr_matrix_f_set_identity (d);
	}

	return d;
}

NRMatrix *
nr_matrix_multiply_ddd (NRMatrix *d, const NRMatrix *m0, const NRMatrix *m1)
{
	if (m0) {
		if (m1) {
			double d0, d1, d2, d3, d4, d5;

			d0 = m0->c[0] * m1->c[0] + m0->c[1] * m1->c[2];
			d1 = m0->c[0] * m1->c[1] + m0->c[1] * m1->c[3];
			d2 = m0->c[2] * m1->c[0] + m0->c[3] * m1->c[2];
			d3 = m0->c[2] * m1->c[1] + m0->c[3] * m1->c[3];
			d4 = m0->c[4] * m1->c[0] + m0->c[5] * m1->c[2] + m1->c[4];
			d5 = m0->c[4] * m1->c[1] + m0->c[5] * m1->c[3] + m1->c[5];
			d->c[0] = d0;
			d->c[1] = d1;
			d->c[2] = d2;
			d->c[3] = d3;
			d->c[4] = d4;
			d->c[5] = d5;
		} else {
			*d = *m0;
		}
	} else {
		if (m1) {
			*d = *m1;
		} else {
			nr_matrix_d_set_identity (d);
		}
	}

	return d;
}

NRMatrix *
nr_matrix_multiply_fff (NRMatrix *d, const NRMatrix *m0, const NRMatrix *m1)
{
	if (m0) {
		if (m1) {
			float d0, d1, d2, d3, d4, d5;

			d0 = m0->c[0] * m1->c[0] + m0->c[1] * m1->c[2];
			d1 = m0->c[0] * m1->c[1] + m0->c[1] * m1->c[3];
			d2 = m0->c[2] * m1->c[0] + m0->c[3] * m1->c[2];
			d3 = m0->c[2] * m1->c[1] + m0->c[3] * m1->c[3];
			d4 = m0->c[4] * m1->c[0] + m0->c[5] * m1->c[2] + m1->c[4];
			d5 = m0->c[4] * m1->c[1] + m0->c[5] * m1->c[3] + m1->c[5];
			d->c[0] = d0;
			d->c[1] = d1;
			d->c[2] = d2;
			d->c[3] = d3;
			d->c[4] = d4;
			d->c[5] = d5;
		} else {
			*d = *m0;
		}
	} else {
		if (m1) {
			*d = *m1;
		} else {
			nr_matrix_f_set_identity (d);
		}
	}

	return d;
}

NRMatrix *
nr_matrix_multiply_fdd (NRMatrix *d, const NRMatrix *m0, const NRMatrix *m1)
{
	if (m0) {
		if (m1) {
			d->c[0] = (float) (m0->c[0] * m1->c[0] + m0->c[1] * m1->c[2]);
			d->c[1] = (float) (m0->c[0] * m1->c[1] + m0->c[1] * m1->c[3]);
			d->c[2] = (float) (m0->c[2] * m1->c[0] + m0->c[3] * m1->c[2]);
			d->c[3] = (float) (m0->c[2] * m1->c[1] + m0->c[3] * m1->c[3]);
			d->c[4] = (float) (m0->c[4] * m1->c[0] + m0->c[5] * m1->c[2] + m1->c[4]);
			d->c[5] = (float) (m0->c[4] * m1->c[1] + m0->c[5] * m1->c[3] + m1->c[5]);
		} else {
			d->c[0] = (float) m0->c[0];
			d->c[1] = (float) m0->c[1];
			d->c[2] = (float) m0->c[2];
			d->c[3] = (float) m0->c[3];
			d->c[4] = (float) m0->c[4];
			d->c[5] = (float) m0->c[5];
		}
	} else {
		if (m1) {
			d->c[0] = (float) m1->c[0];
			d->c[1] = (float) m1->c[1];
			d->c[2] = (float) m1->c[2];
			d->c[3] = (float) m1->c[3];
			d->c[4] = (float) m1->c[4];
			d->c[5] = (float) m1->c[5];
		} else {
			nr_matrix_f_set_identity (d);
		}
	}

	return d;
}

NRMatrix *
nr_matrix_multiply_fdf (NRMatrix *d, const NRMatrix *m0, const NRMatrix *m1)
{
	if (m0) {
		if (m1) {
			float d0, d1, d2, d3, d4, d5;

			d0 = (float) (m0->c[0] * m1->c[0] + m0->c[1] * m1->c[2]);
			d1 = (float) (m0->c[0] * m1->c[1] + m0->c[1] * m1->c[3]);
			d2 = (float) (m0->c[2] * m1->c[0] + m0->c[3] * m1->c[2]);
			d3 = (float) (m0->c[2] * m1->c[1] + m0->c[3] * m1->c[3]);
			d4 = (float) (m0->c[4] * m1->c[0] + m0->c[5] * m1->c[2] + m1->c[4]);
			d5 = (float) (m0->c[4] * m1->c[1] + m0->c[5] * m1->c[3] + m1->c[5]);
			d->c[0] = d0;
			d->c[1] = d1;
			d->c[2] = d2;
			d->c[3] = d3;
			d->c[4] = d4;
			d->c[5] = d5;
		} else {
			d->c[0] = (float) m0->c[0];
			d->c[1] = (float) m0->c[1];
			d->c[2] = (float) m0->c[2];
			d->c[3] = (float) m0->c[3];
			d->c[4] = (float) m0->c[4];
			d->c[5] = (float) m0->c[5];
		}
	} else {
		if (m1) {
			*d = *m1;
		} else {
			nr_matrix_f_set_identity (d);
		}
	}

	return d;
}

NRMatrix *
nr_matrix_multiply_ffd (NRMatrix *d, const NRMatrix *m0, const NRMatrix *m1)
{
	if (m0) {
		if (m1) {
			float d0, d1, d2, d3, d4, d5;

			d0 = (float) (m0->c[0] * m1->c[0] + m0->c[1] * m1->c[2]);
			d1 = (float) (m0->c[0] * m1->c[1] + m0->c[1] * m1->c[3]);
			d2 = (float) (m0->c[2] * m1->c[0] + m0->c[3] * m1->c[2]);
			d3 = (float) (m0->c[2] * m1->c[1] + m0->c[3] * m1->c[3]);
			d4 = (float) (m0->c[4] * m1->c[0] + m0->c[5] * m1->c[2] + m1->c[4]);
			d5 = (float) (m0->c[4] * m1->c[1] + m0->c[5] * m1->c[3] + m1->c[5]);
			d->c[0] = d0;
			d->c[1] = d1;
			d->c[2] = d2;
			d->c[3] = d3;
			d->c[4] = d4;
			d->c[5] = d5;
		} else {
			*d = *m0;
		}
	} else {
		if (m1) {
			d->c[0] = (float) m1->c[0];
			d->c[1] = (float) m1->c[1];
			d->c[2] = (float) m1->c[2];
			d->c[3] = (float) m1->c[3];
			d->c[4] = (float) m1->c[4];
			d->c[5] = (float) m1->c[5];
		} else {
			nr_matrix_f_set_identity (d);
		}
	}

	return d;
}

NRMatrix *
nr_matrix_multiply_ddf (NRMatrix *d, const NRMatrix *m0, const NRMatrix *m1)
{
	if (m0) {
		if (m1) {
			double d0, d1, d2, d3, d4, d5;

			d0 = m0->c[0] * m1->c[0] + m0->c[1] * m1->c[2];
			d1 = m0->c[0] * m1->c[1] + m0->c[1] * m1->c[3];
			d2 = m0->c[2] * m1->c[0] + m0->c[3] * m1->c[2];
			d3 = m0->c[2] * m1->c[1] + m0->c[3] * m1->c[3];
			d4 = m0->c[4] * m1->c[0] + m0->c[5] * m1->c[2] + m1->c[4];
			d5 = m0->c[4] * m1->c[1] + m0->c[5] * m1->c[3] + m1->c[5];
			d->c[0] = d0;
			d->c[1] = d1;
			d->c[2] = d2;
			d->c[3] = d3;
			d->c[4] = d4;
			d->c[5] = d5;
		} else {
			*d = *m0;
		}
	} else {
		if (m1) {
			d->c[0] = m1->c[0];
			d->c[1] = m1->c[1];
			d->c[2] = m1->c[2];
			d->c[3] = m1->c[3];
			d->c[4] = m1->c[4];
			d->c[5] = m1->c[5];
		} else {
			nr_matrix_d_set_identity (d);
		}
	}

	return d;
}

NRMatrix *
nr_matrix_multiply_dfd (NRMatrix *d, const NRMatrix *m0, const NRMatrix *m1)
{
	if (m0) {
		if (m1) {
			double d0, d1, d2, d3, d4, d5;

			d0 = m0->c[0] * m1->c[0] + m0->c[1] * m1->c[2];
			d1 = m0->c[0] * m1->c[1] + m0->c[1] * m1->c[3];
			d2 = m0->c[2] * m1->c[0] + m0->c[3] * m1->c[2];
			d3 = m0->c[2] * m1->c[1] + m0->c[3] * m1->c[3];
			d4 = m0->c[4] * m1->c[0] + m0->c[5] * m1->c[2] + m1->c[4];
			d5 = m0->c[4] * m1->c[1] + m0->c[5] * m1->c[3] + m1->c[5];
			d->c[0] = d0;
			d->c[1] = d1;
			d->c[2] = d2;
			d->c[3] = d3;
			d->c[4] = d4;
			d->c[5] = d5;
		} else {
			d->c[0] = m0->c[0];
			d->c[1] = m0->c[1];
			d->c[2] = m0->c[2];
			d->c[3] = m0->c[3];
			d->c[4] = m0->c[4];
			d->c[5] = m0->c[5];
		}
	} else {
		if (m1) {
			*d = *m1;
		} else {
			nr_matrix_d_set_identity (d);
		}
	}

	return d;
}

NRMatrix *
nr_matrix_d_invert (NRMatrix *d, const NRMatrix *m)
{
	if (m) {
		double det;
		det = m->c[0] * m->c[3] - m->c[1] * m->c[2];
		if (!NR_DF_TEST_CLOSE (det, 0.0, NR_EPSILON_D)) {
			double rdet, t;
			rdet = 1.0 / det;
			t = m->c[3] * rdet;
			d->c[3] = m->c[0] * rdet;
			d->c[0] = t;
			t = -m->c[1] * rdet;
			d->c[1] = -m->c[1] * rdet;
			d->c[2] = -m->c[2] * rdet;
			d->c[4] = -m->c[4] * d->c[0] - m->c[5] * d->c[2];
			d->c[5] = -m->c[4] * d->c[1] - m->c[5] * d->c[3];
		} else {
			nr_matrix_d_set_identity (d);
		}
	} else {
		nr_matrix_d_set_identity (d);
	}

	return d;
}

NRMatrix *
nr_matrix_f_invert (NRMatrix *d, const NRMatrix *m)
{
	if (m) {
		float det;
		det = m->c[0] * m->c[3] - m->c[1] * m->c[2];
		if (!NR_DF_TEST_CLOSE (det, 0.0, NR_EPSILON_F)) {
			float rdet, t;
			rdet = 1.0F / det;
			t = m->c[3] * rdet;
			d->c[3] = m->c[0] * rdet;
			d->c[0] = t;
			t = -m->c[1] * rdet;
			d->c[1] = -m->c[1] * rdet;
			d->c[2] = -m->c[2] * rdet;
			d->c[4] = -m->c[4] * d->c[0] - m->c[5] * d->c[2];
			d->c[5] = -m->c[4] * d->c[1] - m->c[5] * d->c[3];
		} else {
			nr_matrix_f_set_identity (d);
		}
	} else {
		nr_matrix_f_set_identity (d);
	}

	return d;
}

NRMatrix *
nr_matrix_d_set_translate (NRMatrix *m, double x, double y)
{
	m->c[0] = 1.0;
	m->c[1] = 0.0;
	m->c[2] = 0.0;
	m->c[3] = 1.0;
	m->c[4] = x;
	m->c[5] = y;

	return m;
}

NRMatrix *
nr_matrix_f_set_translate (NRMatrix *m, float x, float y)
{
	m->c[0] = 1.0;
	m->c[1] = 0.0;
	m->c[2] = 0.0;
	m->c[3] = 1.0;
	m->c[4] = x;
	m->c[5] = y;

	return m;
}

NRMatrix *
nr_matrix_d_set_scale (NRMatrix *m, double sx, double sy)
{
	m->c[0] = sx;
	m->c[1] = 0.0;
	m->c[2] = 0.0;
	m->c[3] = sy;
	m->c[4] = 0.0;
	m->c[5] = 0.0;

	return m;
}

NRMatrix *
nr_matrix_f_set_scale (NRMatrix *m, float sx, float sy)
{
	m->c[0] = sx;
	m->c[1] = 0.0;
	m->c[2] = 0.0;
	m->c[3] = sy;
	m->c[4] = 0.0;
	m->c[5] = 0.0;

	return m;
}

NRMatrix *
nr_matrix_d_set_rotate (NRMatrix *m, double theta)
{
	double s, c;
	s = sin (theta);
	c = cos (theta);
	m->c[0] = c;
	m->c[1] = s;
	m->c[2] = -s;
	m->c[3] = c;
	m->c[4] = 0.0;
	m->c[5] = 0.0;
	return m;
}

NRMatrix *
nr_matrix_f_set_rotate (NRMatrix *m, float theta)
{
	float s, c;
	s = (float) sin (theta);
	c = (float) cos (theta);
	m->c[0] = c;
	m->c[1] = s;
	m->c[2] = -s;
	m->c[3] = c;
	m->c[4] = 0.0;
	m->c[5] = 0.0;
	return m;
}


