/***************************************************************************
 *            ncm_mset_trans_kern_flat.c
 *
 *  Thu October 02 13:37:11 2014
 *  Copyright  2014  Sandro Dias Pinto Vitenti
 *  <sandro@isoftware.com.br>
 ****************************************************************************/
/*
 * ncm_mset_trans_kern_flat.c
 * Copyright (C) 2014 Sandro Dias Pinto Vitenti <sandro@isoftware.com.br>
 *
 * numcosmo is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * numcosmo is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:ncm_mset_trans_kern_flat
 * @title: Markov Chain Multivariate Flat Sampler 
 * @short_description: Object implementing a multivariate flat sampler.
 *
 * FIXME
 * 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */
#include "build_cfg.h"

#include "math/ncm_mset_trans_kern_flat.h"
#include "math/ncm_c.h"
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_randist.h>

enum
{
  PROP_0,
};

G_DEFINE_TYPE (NcmMSetTransKernFlat, ncm_mset_trans_kern_flat, NCM_TYPE_MSET_TRANS_KERN);

static void
ncm_mset_trans_kern_flat_init (NcmMSetTransKernFlat *tkernf)
{
  tkernf->parea = 0.0;
}

static void
_ncm_mset_trans_kern_flat_finalize (GObject *object)
{

  /* Chain up : end */
  G_OBJECT_CLASS (ncm_mset_trans_kern_flat_parent_class)->finalize (object);
}

static void _ncm_mset_trans_kern_flat_set_mset (NcmMSetTransKern *tkern, NcmMSet *mset);
static void _ncm_mset_trans_kern_flat_generate (NcmMSetTransKern *tkern, NcmVector *theta, NcmVector *thetastar, NcmRNG *rng);
static gdouble _ncm_mset_trans_kern_flat_pdf (NcmMSetTransKern *tkern, NcmVector *theta, NcmVector *thetastar);
static const gchar *_ncm_mset_trans_kern_flat_get_name (NcmMSetTransKern *tkern);

static void
ncm_mset_trans_kern_flat_class_init (NcmMSetTransKernFlatClass *klass)
{
  GObjectClass* object_class = G_OBJECT_CLASS (klass);
  NcmMSetTransKernClass *tkern_class = NCM_MSET_TRANS_KERN_CLASS (klass);

  object_class->finalize     = &_ncm_mset_trans_kern_flat_finalize;

  tkern_class->set_mset = &_ncm_mset_trans_kern_flat_set_mset;
  tkern_class->generate = &_ncm_mset_trans_kern_flat_generate;
  tkern_class->pdf      = &_ncm_mset_trans_kern_flat_pdf;
  tkern_class->get_name = &_ncm_mset_trans_kern_flat_get_name;
}

static void 
_ncm_mset_trans_kern_flat_set_mset (NcmMSetTransKern *tkern, NcmMSet *mset)
{
  NcmMSetTransKernFlat *tkernf = NCM_MSET_TRANS_KERN_FLAT (tkern);
  guint fparam_len = ncm_mset_fparam_len (tkern->mset);
  guint i;

  tkernf->parea = 1.0;
  
  for (i = 0; i < fparam_len; i++)
  {
    const gdouble lb = ncm_mset_fparam_get_lower_bound (tkern->mset, i);
    const gdouble ub = ncm_mset_fparam_get_upper_bound (tkern->mset, i);
    tkernf->parea *= ub - lb;
    g_assert_cmpfloat (tkernf->parea, >, 0.0);
  }
}

static void 
_ncm_mset_trans_kern_flat_generate (NcmMSetTransKern *tkern, NcmVector *theta, NcmVector *thetastar, NcmRNG *rng)
{
  guint fparam_len = ncm_mset_fparam_len (tkern->mset);
  guint i;
  
  for (i = 0; i < fparam_len; i++)
  {
    const gdouble lb  = ncm_mset_fparam_get_lower_bound (tkern->mset, i);
    const gdouble ub  = ncm_mset_fparam_get_upper_bound (tkern->mset, i);
    const gdouble val = lb + (ub - lb) * gsl_rng_uniform_pos (rng->r);
    ncm_vector_set (thetastar, i, val);
  }
}

static gdouble 
_ncm_mset_trans_kern_flat_pdf (NcmMSetTransKern *tkern, NcmVector *theta, NcmVector *thetastar)
{
  return 1.0 / NCM_MSET_TRANS_KERN_FLAT (tkern)->parea;
}

static const gchar *
_ncm_mset_trans_kern_flat_get_name (NcmMSetTransKern *tkern)
{
  return "Multivariate Flat Sampler";
}

/**
 * ncm_mset_trans_kern_flat_new:
 *
 * New NcmMSetTransKern flat.
 * 
 * Returns: (transfer full): a new #NcmMSetTransKernFlat.
 * 
 */
NcmMSetTransKernFlat *
ncm_mset_trans_kern_flat_new (void)
{
  NcmMSetTransKernFlat *tkernf = g_object_new (NCM_TYPE_MSET_TRANS_KERN_FLAT,
                                               NULL);
  return tkernf;
}
