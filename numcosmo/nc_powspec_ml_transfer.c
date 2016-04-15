/***************************************************************************
 *            nc_powspec_ml_transfer.c
 *
 *  Thu March 17 14:57:40 2016
 *  Copyright  2016  Sandro Dias Pinto Vitenti
 *  <sandro@isoftware.com.br>
 ****************************************************************************/
/*
 * nc_powspec_ml_transfer.c
 * Copyright (C) 2016 Sandro Dias Pinto Vitenti <sandro@isoftware.com.br>
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
 * SECTION:nc_powspec_ml_transfer
 * @title: NcPowspecMLTransfer
 * @short_description: Class for linear matter power spectrum from a transfer function.
 * 
 * Provides a linear matter power spectrum using a transfer function #NcTransferFunc.
 * 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */
#include "build_cfg.h"

#include "nc_powspec_ml_transfer.h"
#include "nc_hiprim.h"

enum
{
  PROP_0,
  PROP_TRANSFER,
  PROP_GROWTH
};

G_DEFINE_TYPE (NcPowspecMLTransfer, nc_powspec_ml_transfer, NC_TYPE_POWSPEC_ML);

static void
nc_powspec_ml_transfer_init (NcPowspecMLTransfer *ps_mlt)
{
  ps_mlt->tf         = NULL;
  ps_mlt->gf         = NULL;
  ps_mlt->Pm_k2Pzeta = 0.0;
}

static void
nc_powspec_ml_transfer_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  NcPowspecMLTransfer *ps_mlt = NC_POWSPEC_ML_TRANSFER (object);
  g_return_if_fail (NC_IS_POWSPEC_ML_TRANSFER (object));

  switch (prop_id)
  {
    case PROP_TRANSFER:
      nc_powspec_ml_transfer_set_tf (ps_mlt, g_value_get_object (value));
      break;
    case PROP_GROWTH:
      nc_powspec_ml_transfer_set_gf (ps_mlt, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nc_powspec_ml_transfer_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  NcPowspecMLTransfer *ps_mlt = NC_POWSPEC_ML_TRANSFER (object);
  g_return_if_fail (NC_IS_POWSPEC_ML_TRANSFER (object));

  switch (prop_id)
  {
    case PROP_TRANSFER:
      g_value_set_object (value, nc_powspec_ml_transfer_peek_tf (ps_mlt));
      break;
    case PROP_GROWTH:
      g_value_set_object (value, nc_powspec_ml_transfer_peek_gf (ps_mlt));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nc_powspec_ml_transfer_constructed (GObject *object)
{
  /* Chain up : start */
  G_OBJECT_CLASS (nc_powspec_ml_transfer_parent_class)->constructed (object);
  {
    NcPowspecMLTransfer *ps_mlt = NC_POWSPEC_ML_TRANSFER (object);
    if (ps_mlt->gf == NULL)
    {
      ps_mlt->gf = nc_growth_func_new ();
    }
  }
}

static void
nc_powspec_ml_transfer_dispose (GObject *object)
{
  NcPowspecMLTransfer *ps_mlt = NC_POWSPEC_ML_TRANSFER (object);

  nc_transfer_func_clear (&ps_mlt->tf);
  nc_growth_func_clear (&ps_mlt->gf);
  
  /* Chain up : end */
  G_OBJECT_CLASS (nc_powspec_ml_transfer_parent_class)->dispose (object);
}

static void
nc_powspec_ml_transfer_finalize (GObject *object)
{

  /* Chain up : end */
  G_OBJECT_CLASS (nc_powspec_ml_transfer_parent_class)->finalize (object);
}

void _nc_powspec_ml_transfer_prepare (NcmPowspec *powspec, NcmModel *model);
gdouble _nc_powspec_ml_transfer_eval (NcmPowspec *powspec, NcmModel *model, const gdouble z, const gdouble k);

static void
nc_powspec_ml_transfer_class_init (NcPowspecMLTransferClass *klass)
{
  GObjectClass *object_class     = G_OBJECT_CLASS (klass);
  NcmPowspecClass *powspec_class = NCM_POWSPEC_CLASS (klass);

  object_class->set_property = nc_powspec_ml_transfer_set_property;
  object_class->get_property = nc_powspec_ml_transfer_get_property;

  object_class->constructed  = nc_powspec_ml_transfer_constructed;
  object_class->dispose      = nc_powspec_ml_transfer_dispose;
  object_class->finalize     = nc_powspec_ml_transfer_finalize;

  g_object_class_install_property (object_class,
                                   PROP_TRANSFER,
                                   g_param_spec_object ("transfer",
                                                        NULL,
                                                        "Transfer function",
                                                        NC_TYPE_TRANSFER_FUNC,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class,
                                   PROP_GROWTH,
                                   g_param_spec_object ("growth",
                                                        NULL,
                                                        "Growth function",
                                                        NC_TYPE_GROWTH_FUNC,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB));

  powspec_class->prepare = &_nc_powspec_ml_transfer_prepare;
  powspec_class->eval    = &_nc_powspec_ml_transfer_eval;
}

void 
_nc_powspec_ml_transfer_prepare (NcmPowspec *powspec, NcmModel *model)
{
  NcHICosmo *cosmo            = NC_HICOSMO (model);
  NcPowspecMLTransfer *ps_mlt = NC_POWSPEC_ML_TRANSFER (powspec);

  g_assert (NC_IS_HICOSMO (model));

  nc_growth_func_prepare_if_needed (ps_mlt->gf, cosmo);
  nc_transfer_func_prepare_if_needed (ps_mlt->tf, cosmo);

  {
    const gdouble RH   = nc_hicosmo_RH_Mpc (cosmo);    
    ps_mlt->Pm_k2Pzeta = (2.0 * M_PI * M_PI) * gsl_pow_2 ((9.0 / 10.0) / (2.0 * (3.0 / 2.0) * nc_hicosmo_Omega_m0 (cosmo))) * gsl_pow_4 (RH);
  }
}

gdouble 
_nc_powspec_ml_transfer_eval (NcmPowspec *powspec, NcmModel *model, const gdouble z, const gdouble k)
{
  NcHICosmo *cosmo            = NC_HICOSMO (model);
  NcHIPrim *prim              = NC_HIPRIM (ncm_model_peek_submodel_by_mid (model, nc_hiprim_id ()));
  NcPowspecMLTransfer *ps_mlt = NC_POWSPEC_ML_TRANSFER (powspec);
  const gdouble kh            = k / nc_hicosmo_h (cosmo);
  const gdouble growth        = nc_growth_func_eval (ps_mlt->gf, cosmo, z);
  const gdouble tf            = nc_transfer_func_eval (ps_mlt->tf, cosmo, kh);
  const gdouble tfz           = growth * tf;
  const gdouble tfz2          = tfz * tfz;
  const gdouble Delta_zeta_k  = nc_hiprim_SA_powspec_k (prim, k);

  return k * Delta_zeta_k * ps_mlt->Pm_k2Pzeta * tfz2;
}

/**
 * nc_powspec_ml_transfer_new:
 * @tf: a #NcTransferFunc
 * 
 * Creates a new #NcPowspecMLTransfer from the transfer 
 * function @tf.
 * 
 * Returns: (transfer full): the newly created #NcPowspecMLTransfer.
 */
NcPowspecMLTransfer *
nc_powspec_ml_transfer_new (NcTransferFunc *tf)
{
  NcPowspecMLTransfer *ps_mlt = g_object_new (NC_TYPE_POWSPEC_ML_TRANSFER,
                                              "transfer", tf,
                                              NULL);

  return ps_mlt;
}

/**
 * nc_powspec_ml_transfer_set_tf:
 * @ps_mlt: a #NcPowspecMLTransfer
 * @tf: a #NcTransferFunc
 * 
 * Sets the #NcTransferFunc to @tf.
 * 
 */
void 
nc_powspec_ml_transfer_set_tf (NcPowspecMLTransfer *ps_mlt, NcTransferFunc *tf)
{
  g_clear_object (&ps_mlt->tf);
  ps_mlt->tf = nc_transfer_func_ref (tf);
}

/**
 * nc_powspec_ml_transfer_set_gf:
 * @ps_mlt: a #NcPowspecMLTransfer
 * @gf: a #NcGrowthFunc
 * 
 * Sets the #NcGrowthFunc to @gf.
 * 
 */
void 
nc_powspec_ml_transfer_set_gf (NcPowspecMLTransfer *ps_mlt, NcGrowthFunc *gf)
{
  g_clear_object (&ps_mlt->gf);
  ps_mlt->gf = nc_growth_func_ref (gf);
}

/**
 * nc_powspec_ml_transfer_peek_tf:
 * @ps_mlt: a #NcPowspecMLTransfer
 * 
 * Peeks the #NcTransferFunc inside @ps_mlt.
 * 
 * Returns: (transfer none): the #NcTransferFunc inside @ps_mlt.
 */
NcTransferFunc *
nc_powspec_ml_transfer_peek_tf (NcPowspecMLTransfer *ps_mlt)
{
  return ps_mlt->tf;
}

/**
 * nc_powspec_ml_transfer_peek_gf:
 * @ps_mlt: a #NcPowspecMLTransfer
 * 
 * Peeks the #NcGrowthFunc inside @ps_mlt.
 * 
 * Returns: (transfer none): the #NcGrowthFunc inside @ps_mlt.
 */
NcGrowthFunc *
nc_powspec_ml_transfer_peek_gf (NcPowspecMLTransfer *ps_mlt)
{
  return ps_mlt->gf;
}