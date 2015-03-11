/***************************************************************************
 *            nc_cluster_mass_plcl.c
 *
 *  Sun Mar 1 22:00:23 2015
 *  Copyright  2012  Mariana Penna Lima
 *  <pennalima@gmail.com>
 ****************************************************************************/
/*
 * numcosmo
 * Copyright (C) Mariana Penna Lima 2015 <pennalima@gmail.com>
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
 * SECTION:nc_cluster_mass_plcl
 * @title: NcClusterMassPlCL
 * @short_description: Planck-CLASH Cluster Mass Distribution
 *
 * FIXME Planck-CLASH Cluster Mass Distribution (SZ - Lensing).
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */
#include "build_cfg.h"

#include "lss/nc_cluster_mass_plcl.h"
#include "math/integral.h"
#include "math/memory_pool.h"
#include "math/ncm_cfg.h"

#include <gsl/gsl_randist.h>

G_DEFINE_TYPE (NcClusterMassPlCL, nc_cluster_mass_plcl, NC_TYPE_CLUSTER_MASS);

#define VECTOR (NCM_MODEL (mszl)->params)
#define A_SZ   (ncm_vector_get (VECTOR, NC_CLUSTER_MASS_PLCL_A_SZ))
#define B_SZ   (ncm_vector_get (VECTOR, NC_CLUSTER_MASS_PLCL_B_SZ))
#define SD_SZ  (ncm_vector_get (VECTOR, NC_CLUSTER_MASS_PLCL_SD_SZ))
#define A_L    (ncm_vector_get (VECTOR, NC_CLUSTER_MASS_PLCL_A_L))
#define B_L    (ncm_vector_get (VECTOR, NC_CLUSTER_MASS_PLCL_B_L))
#define SD_L   (ncm_vector_get (VECTOR, NC_CLUSTER_MASS_PLCL_SD_L))
#define COR    (ncm_vector_get (VECTOR, NC_CLUSTER_MASS_PLCL_COR)) 
#define MCUT   (ncm_vector_get (VECTOR, NC_CLUSTER_MASS_PLCL_MCUT))

enum
{
  PROP_0,
//  PROP_SIGNIFICANCE_OBS_MIN,
//  PROP_SIGNIFICANCE_OBS_MAX,
//  PROP_Z0,
//  PROP_M0,
  PROP_SIZE,
};

static gdouble
_SZ_lnmass_mean (NcClusterMassPlCL *mszl,  gdouble lnM)
{
  const gdouble lnMsz_mean = log (1.0 - B_SZ) + A_SZ * lnM;

  return lnMsz_mean;
}

static gdouble
_Lens_lnmass_mean (NcClusterMassPlCL *mszl, gdouble lnM)
{
  const gdouble lnMlens_mean = B_L + A_L * lnM;

  return lnMlens_mean;
}

static gdouble
_nc_cluster_mass_plcl_p (NcClusterMass *clusterm, NcHICosmo *model, gdouble lnM, gdouble z, gdouble *lnM_obs, gdouble *lnM_obs_params)
{
  NcClusterMassPlCL *mszl = NC_CLUSTER_MASS_PLCL (clusterm);
  const gdouble lnMobs_SZ = lnM_obs[0];
  const gdouble lnMobs_L = lnM_obs[1];
  const gdouble lnM_SZ_mean = _SZ_lnmass_mean (mszl, lnM);
  const gdouble lnM_L_mean = _Lens_lnmass_mean (mszl, lnM);
  const gdouble dlnM_SZ = lnMobs_SZ - lnM_SZ_mean;
  const gdouble dlnM_L = lnMobs_L - lnM_L_mean;
  const gdouble x1 = dlnM_SZ * dlnM_SZ / (SD_SZ * SD_SZ);
  const gdouble x2 = dlnM_L * dlnM_L / (SD_L * SD_L);
  const gdouble x12 = - 2.0 * COR * dlnM_SZ * dlnM_L / (SD_SZ * SD_L);
  const gdouble onemcor2 = 1.0 - COR * COR;
  const gdouble y = (x1 + x2 + x12) / (2.0 * onemcor2);
  
  NCM_UNUSED (model);
  NCM_UNUSED (z);
  /* What is the correct normalization factor? only exp (-y) is right */
  return M_2_SQRTPI / (2.0 * M_SQRT2) * exp (- y ) / (SD_SZ);
}

static gdouble
_nc_cluster_mass_plcl_intp (NcClusterMass *clusterm, NcHICosmo *model, gdouble lnM, gdouble z)
{
  NcClusterMassPlCL *mszl = NC_CLUSTER_MASS_PLCL (clusterm);
  /* Lognormal cluster distribution - copy*/
  /* 
  const gdouble sqrt2_sigma = M_SQRT2 * SIGMA;
  const gdouble x_min = (lnM - mszl->lnMobs_min) / sqrt2_sigma;
  const gdouble x_max = (lnM - mszl->lnMobs_max) / sqrt2_sigma;

  NCM_UNUSED (model);
  NCM_UNUSED (z);
  
  if (x_max > 4.0)
    return -(erfc (x_min) - erfc (x_max)) / 2.0;
  else
    return (erf (x_min) - erf (x_max)) / 2.0;
  */
  NCM_UNUSED (mszl);
  return 0.0;
}


static gboolean
_nc_cluster_mass_plcl_resample (NcClusterMass *clusterm, NcHICosmo *model, gdouble lnM, gdouble z, gdouble *lnMobs, gdouble *lnMobs_params, NcmRNG *rng)
{
  NcClusterMassPlCL *mszl = NC_CLUSTER_MASS_PLCL (clusterm);
  gdouble r_SZ, r_L;

  const gdouble lnM_SZ = _SZ_lnmass_mean (mszl, lnM);
  const gdouble lnM_L = _Lens_lnmass_mean (mszl, lnM);
  
  NCM_UNUSED (lnMobs_params);
  NCM_UNUSED (clusterm);
  NCM_UNUSED (model);
  NCM_UNUSED (z);
  
  ncm_rng_lock (rng);
  gsl_ran_bivariate_gaussian (rng->r, SD_SZ, SD_L, COR, &r_SZ, &r_L);
  lnMobs[0] = lnM_SZ + r_SZ;
  lnMobs[1] = lnM_L + r_L;
  ncm_rng_unlock (rng);

  return FALSE;
  //return (lnMobs[0] >= mszl->lnMobs_min) && (lnMobs[1] >= mszl->lnMobs_min); 
}

/*
static void
_nc_cluster_mass_plcl_p_limits (NcClusterMass *clusterm, NcHICosmo *model, gdouble *xi, gdouble *xi_params, gdouble *lnM_lower, gdouble *lnM_upper)
{
  NcClusterMassBenson *msz = NC_CLUSTER_MASS_BENSON (clusterm);
  const gdouble xil = GSL_MAX (xi[0] - 7.0, msz->signif_obs_min);
  const gdouble zetal = _significance_to_zeta (clusterm, model, 2.0, xil) - 7.0 * D_SZ;
  const gdouble lnMl = GSL_MAX (_zeta_to_mass (clusterm, model, 2.0, zetal), log (NC_CLUSTER_MASS_BENSON_M_LOWER_BOUND));

  const gdouble xiu = xi[0] + 7.0;
  const gdouble zetau = _significance_to_zeta (clusterm, model, 0.0, xiu) + 7.0 * D_SZ;
  const gdouble lnMu = _zeta_to_mass (clusterm, model, 0.0, zetau);

  NCM_UNUSED (xi_params);
  
  *lnM_lower = lnMl;
  *lnM_upper = lnMu;

  return;
}
*/

/*
static void
_nc_cluster_mass_plcl_n_limits (NcClusterMass *clusterm, NcHICosmo *model, gdouble *lnM_lower, gdouble *lnM_upper)
{
  NcClusterMassBenson *msz = NC_CLUSTER_MASS_BENSON (clusterm);

  const gdouble xil = msz->signif_obs_min;
  const gdouble zetal = _significance_to_zeta (clusterm, model, 2.0, xil) - 7.0 * D_SZ;
  const gdouble lnMl = GSL_MAX (_zeta_to_mass (clusterm, model, 2.0, zetal), log (NC_CLUSTER_MASS_BENSON_M_LOWER_BOUND));

  const gdouble xiu = msz->signif_obs_max;
  const gdouble zetau = _significance_to_zeta (clusterm, model, 0.0, xiu) + 7.0 * D_SZ;
  const gdouble lnMu = _zeta_to_mass (clusterm, model, 0.0, zetau);

  *lnM_lower = lnMl;
  *lnM_upper = lnMu;

  return;  
}
*/

guint _nc_cluster_mass_plcl_obs_len (NcClusterMass *clusterm) { NCM_UNUSED (clusterm); return 1; }
guint _nc_cluster_mass_plcl_obs_params_len (NcClusterMass *clusterm) { NCM_UNUSED (clusterm); return 0; }

static void
_nc_cluster_mass_plcl_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
  NcClusterMassPlCL *mszl = NC_CLUSTER_MASS_PLCL (object);
  g_return_if_fail (NC_IS_CLUSTER_MASS_PLCL (object));

  NCM_UNUSED (mszl);
/*
  switch (prop_id)
  {
    case PROP_SIGNIFICANCE_OBS_MIN:
      mszl->signif_obs_min = g_value_get_double (value);
      break;
    case PROP_SIGNIFICANCE_OBS_MAX:
      mszl->signif_obs_max = g_value_get_double (value);
      break;
    case PROP_M0:
      mszl->M0 = g_value_get_double (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }*/
}

static void
_nc_cluster_mass_plcl_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  NcClusterMassPlCL *mszl = NC_CLUSTER_MASS_PLCL (object);
  g_return_if_fail (NC_IS_CLUSTER_MASS_PLCL (object));

  NCM_UNUSED (mszl);
/*
  switch (prop_id)
  {
    case PROP_SIGNIFICANCE_OBS_MIN:
      g_value_set_double (value, mszl->signif_obs_min);
      break;
    case PROP_SIGNIFICANCE_OBS_MAX:
      g_value_set_double (value, mszl->signif_obs_max);
      break;
    case PROP_M0:
      g_value_set_double (value, mszl->M0);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  */
}

static void
nc_cluster_mass_plcl_init (NcClusterMassPlCL *mszl)
{
  //mszl->signif_obs_min = 0.0;
  //mszl->signif_obs_max = 0.0;
  //mszl->M0 = 0.0;
}

static void
_nc_cluster_mass_plcl_finalize (GObject *object)
{

  /* Chain up : end */
  G_OBJECT_CLASS (nc_cluster_mass_plcl_parent_class)->finalize (object);
}

static void
nc_cluster_mass_plcl_class_init (NcClusterMassPlCLClass *klass)
{
  GObjectClass* object_class = G_OBJECT_CLASS (klass);
  NcClusterMassClass* parent_class = NC_CLUSTER_MASS_CLASS (klass);
  NcmModelClass *model_class = NCM_MODEL_CLASS (klass);

  parent_class->P = &_nc_cluster_mass_plcl_p;
  parent_class->intP = &_nc_cluster_mass_plcl_intp;
  //parent_class->P_limits = &_nc_cluster_mass_plcl_p_limits;
  //parent_class->N_limits = &_nc_cluster_mass_plcl_n_limits;
  parent_class->resample = &_nc_cluster_mass_plcl_resample;
  parent_class->obs_len = &_nc_cluster_mass_plcl_obs_len;
  parent_class->obs_params_len = &_nc_cluster_mass_plcl_obs_params_len;

  parent_class->impl = NC_CLUSTER_MASS_IMPL_ALL;

  object_class->finalize = _nc_cluster_mass_plcl_finalize;

  model_class->set_property = &_nc_cluster_mass_plcl_set_property;
  model_class->get_property = &_nc_cluster_mass_plcl_get_property;

  ncm_model_class_set_name_nick (model_class, "Planck - CLASH cluster mass distribution", "Planck_CLASH");
  ncm_model_class_add_params (model_class, NC_CLUSTER_MASS_PLCL_SPARAM_LEN, 0, PROP_SIZE);

  /*
   * SZ signal-mass scaling parameter: Asz.
   * FIXME Set correct values (limits)
   */
  ncm_model_class_set_sparam (model_class, NC_CLUSTER_MASS_PLCL_A_SZ, "A_{SZ}", "Asz",
                              1e-8,  10.0, 1.0e-2,
                              NC_CLUSTER_MASS_PLCL_DEFAULT_PARAMS_ABSTOL, NC_CLUSTER_MASS_PLCL_DEFAULT_A_SZ,
                              NCM_PARAM_TYPE_FIXED);

  /*
   * SZ signal-mass scaling parameter: Bsz.
   * FIXME Set correct values (limits)
   */
  ncm_model_class_set_sparam (model_class, NC_CLUSTER_MASS_PLCL_B_SZ, "B_{SZ}", "Bsz",
                              1e-8,  10.0, 1.0e-2,
                              NC_CLUSTER_MASS_PLCL_DEFAULT_PARAMS_ABSTOL, NC_CLUSTER_MASS_PLCL_DEFAULT_B_SZ,
                              NCM_PARAM_TYPE_FIXED);

  /*
   * SZ signal-mass scaling parameter: SDsz.
   * FIXME Set correct values (limits)
   */
  ncm_model_class_set_sparam (model_class, NC_CLUSTER_MASS_PLCL_SD_SZ, "SD_{SZ}", "SDsz",
                              1e-8,  10.0, 1.0e-2,
                              NC_CLUSTER_MASS_PLCL_DEFAULT_PARAMS_ABSTOL, NC_CLUSTER_MASS_PLCL_DEFAULT_SD_SZ,
                              NCM_PARAM_TYPE_FIXED);
  /*
   * Lensing signal-mass scaling parameter: Al.
   * FIXME Set correct values (limits)
   */
  ncm_model_class_set_sparam (model_class, NC_CLUSTER_MASS_PLCL_A_L, "A_{L}", "Al",
                              1e-8,  10.0, 1.0e-2,
                              NC_CLUSTER_MASS_PLCL_DEFAULT_PARAMS_ABSTOL, NC_CLUSTER_MASS_PLCL_DEFAULT_A_L,
                              NCM_PARAM_TYPE_FIXED);

  /*
   * Lensing signal-mass scaling parameter: Bl.
   * FIXME Set correct values (limits)
   */
  ncm_model_class_set_sparam (model_class, NC_CLUSTER_MASS_PLCL_B_L, "B_{L}", "Bl",
                              1e-8,  10.0, 1.0e-2,
                              NC_CLUSTER_MASS_PLCL_DEFAULT_PARAMS_ABSTOL, NC_CLUSTER_MASS_PLCL_DEFAULT_B_L,
                              NCM_PARAM_TYPE_FIXED);

  /*
   * Lensing signal-mass scaling parameter: SDl.
   * FIXME Set correct values (limits)
   */
  ncm_model_class_set_sparam (model_class, NC_CLUSTER_MASS_PLCL_SD_L, "SD_{L}", "SDl",
                              1e-8,  10.0, 1.0e-2,
                              NC_CLUSTER_MASS_PLCL_DEFAULT_PARAMS_ABSTOL, NC_CLUSTER_MASS_PLCL_DEFAULT_SD_L,
                              NCM_PARAM_TYPE_FIXED);
  /*
   * SZ-Lensing signal-mass correlation: COR.
   * FIXME Set correct values (limits)
   */
  ncm_model_class_set_sparam (model_class, NC_CLUSTER_MASS_PLCL_COR, "COR", "COR",
                              1e-8,  10.0, 1.0e-2,
                              NC_CLUSTER_MASS_PLCL_DEFAULT_PARAMS_ABSTOL, NC_CLUSTER_MASS_PLCL_DEFAULT_COR,
                              NCM_PARAM_TYPE_FIXED);
  /*
   * Lower mass cut-off: MCUT.
   * FIXME Set correct values (limits)
   */
  ncm_model_class_set_sparam (model_class, NC_CLUSTER_MASS_PLCL_MCUT, "MCUT", "MCUT",
                              1e-8,  10.0, 1.0e-2,
                              NC_CLUSTER_MASS_PLCL_DEFAULT_PARAMS_ABSTOL, NC_CLUSTER_MASS_PLCL_DEFAULT_MCUT,
                              NCM_PARAM_TYPE_FIXED);

  
  /* Check for errors in parameters initialization */
  ncm_model_class_check_params_info (model_class);
}
