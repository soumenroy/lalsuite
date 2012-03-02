/*
 * Copyright (C) 2011 Drew Keppel, J. Creighton, S. Fairhurst, B. Krishnan, L. Santamaria, Stas Babak, David Churches, B.S. Sathyaprakash, Craig Robinson , Thomas Cokelaer
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with with program; see the file COPYING. If not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *  MA  02111-1307  USA
 */

#include <math.h>

#define LAL_USE_OLD_COMPLEX_STRUCTS
#include <lal/LALSimInspiral.h>
#define LAL_USE_COMPLEX_SHORT_MACROS
#include <lal/FindRoot.h>
#include <lal/LALComplex.h>
#include <lal/LALConstants.h>
#include <lal/LALStdlib.h>
#include <lal/TimeSeries.h>
#include <lal/Units.h>

#include "LALSimInspiralPNCoefficients.c"
#include "check_series_macros.h"

#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED
#endif

typedef struct
tagexpnCoeffsTaylorT2 {
   /* Taylor expansion coefficents in t(v)*/
   REAL8 tvaN, tva2, tva3, tva4, tva5, tva6, tva7, tvl6;
   /* Taylor expansion coefficents in phi(v)*/
   REAL8 pvaN, pva2, pva3, pva4, pva5, pva6, pva7, pvl6;

   /* sampling rate and interval*/
   REAL8 samplingrate, samplinginterval;
   /* symmetric mass ratio, total mass*/
   REAL8 eta, totalmass;

   /* initial and final values of frequency, time, velocity; lso
    values of velocity and frequency; final phase.*/
   REAL8 f0, fn, t0, tn, v0, vn, vf, vlso, flso, phiC;

   /* last stable orbit and pole defined by various Taylor and P-approximants*/
   REAL8 vlsoT0, vlsoT2, vlsoT4, vlsoT6;
}  expnCoeffsTaylorT2;

typedef REAL8 (SimInspiralPhasing2)(
   REAL8 td,
   expnCoeffsTaylorT2 *ak);

typedef REAL8 (SimInspiralTiming2)(
   REAL8 td,
   void *ak);

typedef struct
tagexpnFuncTaylorT2
{
   SimInspiralPhasing2 *phasing2;
   SimInspiralTiming2 *timing2;
} expnFuncTaylorT2;

typedef struct
tagSimInspiralToffInput
{
   REAL8 tN;
   REAL8 t2;
   REAL8 t3;
   REAL8 t4;
   REAL8 t5;
   REAL8 t6;
   REAL8 t7;
   REAL8 tl6;
   REAL8 piM;
   REAL8 tc;
   REAL8 t;
 } SimInspiralToffInput;

static REAL8
XLALSimInspiralTiming2_0PN (
   REAL8       f,
   void      *params
   )
{
  SimInspiralToffInput *toffIn;
  REAL8 v, v8;
  REAL8 toff;

  if (params == NULL)
    XLAL_ERROR_REAL8(XLAL_EFAULT);
  if (f <= 0)
    XLAL_ERROR_REAL8(XLAL_EDOM);

  toffIn = (SimInspiralToffInput *) params;

  if (toffIn->t < 0)
    XLAL_ERROR_REAL8(XLAL_EDOM);


  v = cbrt(toffIn->piM * f);
  v8 = pow(v,8.);

  toff = - toffIn->t + toffIn->tc
        + toffIn->tN / v8;

  return toff;
}

static REAL8
XLALSimInspiralTiming2_2PN (
   REAL8       f,
   void      *params
   )
{
  SimInspiralToffInput *toffIn;
  REAL8 v, v2, v8;
  REAL8 toff;

  if (params == NULL)
    XLAL_ERROR_REAL8(XLAL_EFAULT);
  if (f <= 0)
    XLAL_ERROR_REAL8(XLAL_EDOM);

  toffIn = (SimInspiralToffInput *) params;

  if (toffIn->t < 0)
    XLAL_ERROR_REAL8(XLAL_EDOM);


  v = cbrt(toffIn->piM * f);
  v2 = v*v;
  v8 = v2*v2*v2*v2;

  toff = - toffIn->t + toffIn->tc
        + toffIn->tN / v8 * (1.
        + toffIn->t2 * v2);

  return toff;
}

static REAL8
XLALSimInspiralTiming2_3PN (
   REAL8       f,
   void      *params
   )
{
  SimInspiralToffInput *toffIn;
  REAL8 v, v2, v3, v8;
  REAL8 toff;

  if (params == NULL)
    XLAL_ERROR_REAL8(XLAL_EFAULT);
  if (f <= 0)
    XLAL_ERROR_REAL8(XLAL_EDOM);

  toffIn = (SimInspiralToffInput *) params;

  if (toffIn->t < 0)
    XLAL_ERROR_REAL8(XLAL_EDOM);


  v = cbrt(toffIn->piM * f);
  v2 = v*v;
  v3 = v2*v;
  v8 = v3*v3*v2;

  toff = - toffIn->t + toffIn->tc
        + toffIn->tN / v8 * (1.
        + toffIn->t2 * v2
        + toffIn->t3 * v3);

  return toff;
}

static REAL8
XLALSimInspiralTiming2_4PN (
   REAL8       f,
   void      *params
   )
{
  SimInspiralToffInput *toffIn;
  REAL8 v, v2, v3, v4, v8;
  REAL8 toff;

  if (params == NULL)
    XLAL_ERROR_REAL8(XLAL_EFAULT);
  if (f <= 0)
    XLAL_ERROR_REAL8(XLAL_EDOM);

  toffIn = (SimInspiralToffInput *) params;

  if (toffIn->t < 0)
    XLAL_ERROR_REAL8(XLAL_EDOM);


  v = cbrt(toffIn->piM * f);
  v2 = v*v;
  v3 = v2*v;
  v4 = v3*v;
  v8 = v4*v4;

  toff = - toffIn->t + toffIn->tc
        + toffIn->tN / v8 * (1.
        + toffIn->t2 * v2
        + toffIn->t3 * v3
        + toffIn->t4 * v4);

  return toff;
}

static REAL8
XLALSimInspiralTiming2_5PN (
   REAL8       f,
   void      *params
   )
{
  SimInspiralToffInput *toffIn;
  REAL8 v, v2, v3, v4, v5, v8;
  REAL8 toff;

  if (params == NULL)
    XLAL_ERROR_REAL8(XLAL_EFAULT);
  if (f <= 0)
    XLAL_ERROR_REAL8(XLAL_EDOM);

  toffIn = (SimInspiralToffInput *) params;

  if (toffIn->t < 0)
    XLAL_ERROR_REAL8(XLAL_EDOM);


  v = cbrt(toffIn->piM * f);
  v2 = v*v;
  v3 = v2*v;
  v4 = v3*v;
  v5 = v4*v;
  v8 = v4*v4;

  toff = - toffIn->t + toffIn->tc
        + toffIn->tN / v8 * (1.
        + toffIn->t2 * v2
        + toffIn->t3 * v3
        + toffIn->t4 * v4
        + toffIn->t5 * v5);

  return toff;
}

static REAL8
XLALSimInspiralTiming2_6PN (
   REAL8       f,
   void      *params
   )
{

  SimInspiralToffInput *toffIn;
  REAL8 v, v2, v3, v4, v5, v6, v8;
  REAL8 toff;

  if (params == NULL)
    XLAL_ERROR_REAL8(XLAL_EFAULT);
  if (f <= 0)
    XLAL_ERROR_REAL8(XLAL_EDOM);

  toffIn = (SimInspiralToffInput *) params;

  if (toffIn->t < 0)
    XLAL_ERROR_REAL8(XLAL_EDOM);


  v = cbrt(toffIn->piM * f);
  v2 = v*v;
  v3 = v2*v;
  v4 = v3*v;
  v5 = v4*v;
  v6 = v5*v;
  v8 = v6*v2;

  toff = - toffIn->t + toffIn->tc
        + toffIn->tN / v8 * (1.
        + toffIn->t2 * v2
        + toffIn->t3 * v3
        + toffIn->t4 * v4
        + toffIn->t5 * v5
        + (toffIn->t6 + toffIn->tl6 * log(16.*v2)) * v6);

  return toff;
}

static REAL8
XLALSimInspiralTiming2_7PN (
   REAL8       f,
   void      *params
   )
{
  SimInspiralToffInput *toffIn;
  REAL8 v, v2, v3, v4, v5, v6, v7, v8;
  REAL8 toff;

  if (params == NULL)
    XLAL_ERROR_REAL8(XLAL_EFAULT);
  if (f <= 0)
    XLAL_ERROR_REAL8(XLAL_EDOM);

  toffIn = (SimInspiralToffInput *) params;

  if (toffIn->t < 0)
    XLAL_ERROR_REAL8(XLAL_EDOM);


  v = cbrt(toffIn->piM*f);
  v2 = v*v;
  v3 = v2*v;
  v4 = v3*v;
  v5 = v4*v;
  v6 = v5*v;
  v7 = v6*v;
  v8 = v7*v;

  toff = - toffIn->t + toffIn->tc
        + toffIn->tN / v8 * (1.
        + toffIn->t2 * v2
        + toffIn->t3 * v3
        + toffIn->t4 * v4
        + toffIn->t5 * v5
        + (toffIn->t6 + toffIn->tl6 * log(16.*v2)) * v6
        + toffIn->t7 * v7);

  return toff;
}


static REAL8
XLALSimInspiralPhasing2_0PN (
   REAL8       v,
   expnCoeffsTaylorT2 *ak
   )
{
  REAL8 v5;
  REAL8 phase;

  if (ak == NULL)
    XLAL_ERROR_REAL8(XLAL_EFAULT);

  v5 = pow(v,5.);
  phase = ak->phiC
         + ak->pvaN / v5;

  return phase;
}

static REAL8
XLALSimInspiralPhasing2_2PN (
   REAL8       v,
   expnCoeffsTaylorT2 *ak
   )
{
  REAL8 v2,v5;
  REAL8 phase;

  if (ak == NULL)
    XLAL_ERROR_REAL8(XLAL_EFAULT);

  v2 = v*v;
  v5 = v2*v2*v;
  phase = ak->phiC
         + ak->pvaN / v5 * ( 1. +
         + ak->pva2 * v2);

  return phase;
}

static REAL8
XLALSimInspiralPhasing2_3PN (
   REAL8       v,
   expnCoeffsTaylorT2 *ak
   )
{
  REAL8 v2,v3,v5;
  REAL8 phase;

  if (ak == NULL)
    XLAL_ERROR_REAL8(XLAL_EFAULT);

  v2 = v*v;
  v3 = v2*v;
  v5 = v3*v2;
  phase = ak->phiC
         + ak->pvaN / v5 * ( 1. +
         + ak->pva2 * v2
         + ak->pva3 * v3);

  return phase;
}

static REAL8
XLALSimInspiralPhasing2_4PN (
   REAL8       v,
   expnCoeffsTaylorT2 *ak
   )
{
  REAL8 v2,v3,v4,v5;
  REAL8 phase;

  if (ak == NULL)
    XLAL_ERROR_REAL8(XLAL_EFAULT);

  v2 = v*v;
  v3 = v2*v;
  v4 = v3*v;
  v5 = v4*v;
  phase = ak->phiC
         + ak->pvaN / v5 * ( 1. +
         + ak->pva2 * v2
         + ak->pva3 * v3
         + ak->pva4 * v4);

  return phase;
}

static REAL8
XLALSimInspiralPhasing2_5PN (
   REAL8       v,
   expnCoeffsTaylorT2 *ak
   )
{
  REAL8 v2,v3,v4,v5;
  REAL8 phase;

  if (ak == NULL)
    XLAL_ERROR_REAL8(XLAL_EFAULT);

  v2 = v*v;
  v3 = v2*v;
  v4 = v3*v;
  v5 = v4*v;
  phase = ak->phiC
         + ak->pvaN / v5 * ( 1. +
         + ak->pva2 * v2
         + ak->pva3 * v3
         + ak->pva4 * v4
         + ak->pva5 * log(v/ak->vlso) * v5);

  return phase;
}

static REAL8
XLALSimInspiralPhasing2_6PN (
   REAL8       v,
   expnCoeffsTaylorT2 *ak
   )
{
  REAL8 v2,v3,v4,v5,v6;
  REAL8 phase;

  if (ak == NULL)
    XLAL_ERROR_REAL8(XLAL_EFAULT);

  v2 = v*v;
  v3 = v2*v;
  v4 = v3*v;
  v5 = v4*v;
  v6 = v5*v;
  phase = ak->phiC
         + ak->pvaN / v5 * ( 1. +
         + ak->pva2 * v2
         + ak->pva3 * v3
         + ak->pva4 * v4
         + ak->pva5 * log(v/ak->vlso) * v5
         + (ak->pva6 + ak->pvl6*log(16.*v2)) * v6);

  return phase;
}

static REAL8
XLALSimInspiralPhasing2_7PN (
   REAL8       v,
   expnCoeffsTaylorT2 *ak
   )
{
  REAL8 v2,v3,v4,v5,v6,v7;
  REAL8 phase;

  if (ak == NULL)
    XLAL_ERROR_REAL8(XLAL_EFAULT);

  v2 = v*v;
  v3 = v2*v;
  v4 = v3*v;
  v5 = v4*v;
  v6 = v5*v;
  v7 = v6*v;
  phase = ak->phiC
         + ak->pvaN / v5 * ( 1. +
         + ak->pva2 * v2
         + ak->pva3 * v3
         + ak->pva4 * v4
         + ak->pva5 * log(v/ak->vlso) * v5
         + (ak->pva6 + ak->pvl6*log(16.*v2)) * v6
         + ak->pva7 * v7);

  return phase;
}


/**
 * Set up the expnCoeffsTaylorT3 and expnFuncTaylorT3 structures for
 * generating a TaylorT3 waveform.
 *
 * Inputs given in SI units.
 */
static int XLALSimInspiralTaylorT2Setup(
		expnCoeffsTaylorT2 *ak,	/**< coefficients for TaylorT2 evolution [modified] */
		expnFuncTaylorT2 *f,	/**< functions for TaylorT2 evolution [modified] */
	       	REAL8 deltaT,		/**< sampling interval */
		REAL8 m1,		/**< mass of companion 1 */
		REAL8 m2,		/**< mass of companion 2 */
		REAL8 f_min,		/**< start frequency */
		int O			/**< twice post-Newtonian order */
		)
{
  REAL8 eta, lso;
  REAL8 oneby6 = 1./6.;

  ak->t0 = 0;
  ak->totalmass = m1 + m2;
  eta = ak->eta = m1 * m2 / (ak->totalmass * ak->totalmass);
  ak->totalmass *= LAL_G_SI / pow(LAL_C_SI, 3.0); /* convert m from kilograms to seconds */

  ak->f0 = f_min;
  ak->samplinginterval = deltaT;
  ak->fn = 1. / (2. * ak->samplinginterval);
  ak->v0 = cbrt(LAL_PI * ak->totalmass * f_min);

  ak->tvaN = XLALSimInspiralTaylorT2Timing_0PNCoeff(m1+m2, eta);
  ak->tva2 = XLALSimInspiralTaylorT2Timing_2PNCoeff(eta);
  ak->tva3 = XLALSimInspiralTaylorT2Timing_3PNCoeff(eta);
  ak->tva4 = XLALSimInspiralTaylorT2Timing_4PNCoeff(eta);
  ak->tva5 = XLALSimInspiralTaylorT2Timing_5PNCoeff(eta);
  ak->tva6 = XLALSimInspiralTaylorT2Timing_6PNCoeff(eta);
  ak->tva7 = XLALSimInspiralTaylorT2Timing_7PNCoeff(eta);
  ak->tvl6 = XLALSimInspiralTaylorT2Timing_6PNLogCoeff(eta);

  ak->pvaN = XLALSimInspiralTaylorT2Phasing_0PNCoeff(eta);
  ak->pva2 = XLALSimInspiralTaylorT2Phasing_2PNCoeff(eta);
  ak->pva3 = XLALSimInspiralTaylorT2Phasing_3PNCoeff(eta);
  ak->pva4 = XLALSimInspiralTaylorT2Phasing_4PNCoeff(eta);
  ak->pva5 = XLALSimInspiralTaylorT2Phasing_5PNCoeff(eta);
  ak->pva6 = XLALSimInspiralTaylorT2Phasing_6PNCoeff(eta);
  ak->pva7 = XLALSimInspiralTaylorT2Phasing_7PNCoeff(eta);
  ak->pvl6 = XLALSimInspiralTaylorT2Phasing_6PNLogCoeff(eta);

  lso = sqrt(oneby6);

/* Location of the 0PN and 1PN T- and P-approximant last stable orbit: */
  ak->vlsoT0 = lso;

/*
  vlsoT2 =  6./(9.+eta);
  This correct value makes vlso too large for vlsoT2 hence use 1/sqrt(6)
*/
  ak->vlsoT2 = lso;

  switch (O)
  {
     case 0:
           ak->vlso = ak->vlsoT0;
           f->phasing2 = &XLALSimInspiralPhasing2_0PN;
           f->timing2 = &XLALSimInspiralTiming2_0PN;
           break;
     case 1:
       XLALPrintError("XLAL Error - %s: PN approximant not supported for requested PN order\n", __func__);
       XLAL_ERROR(XLAL_EINVAL);
       break;
     case 2:
           ak->vlso = ak->vlsoT2;
           f->phasing2 = &XLALSimInspiralPhasing2_2PN;
           f->timing2 = &XLALSimInspiralTiming2_2PN;
           break;
     case 3:
           ak->vlso = ak->vlsoT2;
           f->phasing2 = &XLALSimInspiralPhasing2_3PN;
           f->timing2 = &XLALSimInspiralTiming2_3PN;
           break;
     case 4:
/*
   The value vlsoT4 is too large and doesn't work sometimes;
   so we use vlsoT2.
*/
           ak->vlso = ak->vlsoT2;
           f->phasing2 = &XLALSimInspiralPhasing2_4PN;
           f->timing2 = &XLALSimInspiralTiming2_4PN;
           break;
     case 5:
/*
   The value vlsoT4 is too large and doesn't work with 2.5 PN
   Taylor approximant; so we use vlsoT2.
*/
           ak->vlso = ak->vlsoT2;
           f->phasing2 = &XLALSimInspiralPhasing2_5PN;
           f->timing2 = &XLALSimInspiralTiming2_5PN;
           break;
     case 6:
/*
   vlsoT6 is as yet undetermined and vlsoT4 is too large in
   certain cases (TaylorT2 crashes for (1.4,10)); using vlsoT2;
*/
           ak->vlso = ak->vlsoT2;
           f->phasing2 = &XLALSimInspiralPhasing2_6PN;
           f->timing2 = &XLALSimInspiralTiming2_6PN;
           break;
     case 7:
           ak->vlso = ak->vlsoT2;
           f->phasing2 = &XLALSimInspiralPhasing2_7PN;
           f->timing2 = &XLALSimInspiralTiming2_7PN;
           break;
     case 8:
           XLALPrintError("XLAL Error - %s: PN approximant not supported for requested PN order\n", __func__);
           XLAL_ERROR(XLAL_EINVAL);
           break;
     default:
        XLALPrintError("XLAL Error - %s: Unknown PN order in switch\n", __func__);
        XLAL_ERROR(XLAL_EINVAL);
  }

  return XLAL_SUCCESS;
}


/**
 * Computes a post-Newtonian orbit using the Taylor T2 method.
 */
int XLALSimInspiralTaylorT2PNEvolveOrbit(
		REAL8TimeSeries **V,   /**< post-Newtonian parameter [returned] */
	       	REAL8TimeSeries **phi, /**< orbital phase [returned] */
	       	REAL8 phi0,            /**< initial phase */
	       	REAL8 deltaT,          /**< sampling interval */
		REAL8 m1,              /**< mass of companion 1 */
		REAL8 m2,              /**< mass of companion 2 */
		REAL8 f_min,           /**< start frequency */
		int O                  /**< twice post-Newtonian order */
		)
{
	const UINT4 blocklen = 1024;
	REAL8 m = m1 + m2;
	m *= LAL_G_SI / pow(LAL_C_SI, 3.0); /* convert m from kilograms to seconds */
	REAL8 tC, xmin, xmax, xacc, v, phase;
	REAL8 (*timing2)(REAL8, void *);
	UINT4 j;
	LIGOTimeGPS tc = LIGOTIMEGPSZERO;
	REAL8 f, fLso;
	SimInspiralToffInput toffIn;
	void *funcParams;

	expnFuncTaylorT2 expnfunc;
	expnCoeffsTaylorT2 ak;

	/* allocate memory */

	*V = XLALCreateREAL8TimeSeries("ORBITAL_FREQUENCY_PARAMETER", &tc, 0.0, deltaT, &lalDimensionlessUnit,
		blocklen);
	*phi = XLALCreateREAL8TimeSeries("ORBITAL_PHASE", &tc, 0.0, deltaT, &lalDimensionlessUnit, blocklen);
	if (!V || !phi)
		XLAL_ERROR(XLAL_EFUNC);

	/* initialize expnCoeffsTaylorT2 and expnFuncTaylorT2 structures */
	if (XLALSimInspiralTaylorT2Setup(&ak, &expnfunc, deltaT, m1, m2, f_min, O))
		XLAL_ERROR(XLAL_EFUNC);

	timing2 = expnfunc.timing2; /* function to solve for v, given t:*/

	toffIn.tN = ak.tvaN;
	toffIn.t2 = ak.tva2;
	toffIn.t3 = ak.tva3;
	toffIn.t4 = ak.tva4;
	toffIn.t5 = ak.tva5;
	toffIn.t6 = ak.tva6;
	toffIn.t7 = ak.tva7;
	toffIn.tl6 = ak.tvl6;
	toffIn.piM = ak.totalmass * LAL_PI;

	/* Determine the total chirp-time tC: the total chirp time is
	 * timing2(v0;tC,t) with t=tc=0*/

 	toffIn.t = 0.;
	toffIn.tc = 0.;
	funcParams = (void *) &toffIn;
	tC = timing2(f_min, funcParams);
	if (XLAL_IS_REAL8_FAIL_NAN(tC))
		XLAL_ERROR(XLAL_EFUNC);
	/* Reset chirp time in toffIn structure */
	toffIn.tc = -tC;

	/* If flso is less than the user inputted upper frequency cutoff fu */

	fLso = ak.fn;

	/* Is the sampling rate large enough? */

	if (fLso > 0.5/deltaT)
		XLAL_ERROR(XLAL_EDOM);
	if (fLso <= f_min)
		XLAL_ERROR(XLAL_EDOM);

	xmax = 1.5*fLso;
	xacc = 1.0e-8;
	xmin = 0.999999*f_min;

	/* start waveform generation */


	/* Now cast the input structure to argument 4 of BisectionFindRoot so
	 * that it of type void * rather than SimInspiralToffInput  */

	funcParams = (void *) &toffIn;

	toffIn.t = 0.0;
	f = f_min;
	j = 0;
	do {
		/* make sure we don't write past end of vectors */

		if ( j >= (*V)->data->length ) {
			if ( ! XLALResizeREAL8TimeSeries(*V, 0, (*V)->data->length + blocklen) )
				XLAL_ERROR(XLAL_EFUNC);
			if ( ! XLALResizeREAL8TimeSeries(*phi, 0, (*phi)->data->length + blocklen) )
				XLAL_ERROR(XLAL_EFUNC);
		}

		/* compute values at this step */

		v = cbrt(f*toffIn.piM);
		phase = expnfunc.phasing2(v, &ak); /* phase at given v */
		if (XLAL_IS_REAL8_FAIL_NAN(phase))
			XLAL_ERROR(XLAL_EFUNC);

		(*V)->data->data[j] = v;
		(*phi)->data->data[j] = phase;

		/* make next step */

		++j;
		toffIn.t=j*deltaT;

		/* Determine the frequency at the current time by solving
		 * timing2(v;tC,t)=0 */

		xmin = 0.8*f;
		f = XLALDBisectionFindRoot(timing2, xmin, xmax, xacc, funcParams);
		if (XLAL_IS_REAL8_FAIL_NAN(f))
			XLAL_ERROR(XLAL_EFUNC);
	} while (f < fLso && toffIn.t < -tC);

	/* check termination conditions */

	if (toffIn.t >= -tC) {
		XLALPrintInfo("XLAL Info - %s: PN inspiral terminated at coalesence time\n", __func__);
	}
	if (f >= fLso) {
		XLALPrintInfo("XLAL Info - %s: PN inspiral terminated at ISCO\n", __func__);
	}

	/* make the correct length */

	if ( ! XLALResizeREAL8TimeSeries(*V, 0, j) )
		XLAL_ERROR(XLAL_EFUNC);
	if ( ! XLALResizeREAL8TimeSeries(*phi, 0, j) )
		XLAL_ERROR(XLAL_EFUNC);

	/* adjust to correct time */

	XLALGPSAdd(&(*phi)->epoch, -1.0*j*deltaT);
	XLALGPSAdd(&(*V)->epoch, -1.0*j*deltaT);

	/* adjust so initial phase is phi0 */

	phi0 -= (*phi)->data->data[0];
	for (j = 0; j < (*phi)->data->length; ++j)
		(*phi)->data->data[j] += phi0;

	return (int)(*V)->data->length;
}


/**
 * Driver routine to compute the post-Newtonian inspiral waveform.
 *
 * This routine allows the user to specify different pN orders
 * for phasing calcuation vs. amplitude calculations.
 */
int XLALSimInspiralTaylorT2PNGenerator(
		REAL8TimeSeries **hplus,  /**< +-polarization waveform */
	       	REAL8TimeSeries **hcross, /**< x-polarization waveform */
	       	REAL8 phic,               /**< coalescence phase */
	       	REAL8 v0,                 /**< tail-term gauge choice thing (default = 1) */
	       	REAL8 deltaT,             /**< sampling interval */
	       	REAL8 m1,                 /**< mass of companion 1 */
	       	REAL8 m2,                 /**< mass of companion 2 */
	       	REAL8 f_min,              /**< start frequency */
	       	REAL8 r,                  /**< distance of source */
	       	REAL8 i,                  /**< inclination of source (rad) */
	       	int amplitudeO,           /**< twice post-Newtonian amplitude order */
	       	int phaseO                /**< twice post-Newtonian phase order */
		)
{
	REAL8TimeSeries *V;
	REAL8TimeSeries *phi;
	int status;
	int n;
	n = XLALSimInspiralTaylorT2PNEvolveOrbit(&V, &phi, phic, deltaT, m1, m2, f_min, phaseO);
	if ( n < 0 )
		XLAL_ERROR(XLAL_EFUNC);
	status = XLALSimInspiralPNPolarizationWaveforms(hplus, hcross, V, phi, v0, m1, m2, r, i, amplitudeO);
	XLALDestroyREAL8TimeSeries(phi);
	XLALDestroyREAL8TimeSeries(V);
	if ( status < 0 )
		XLAL_ERROR(XLAL_EFUNC);
	return n;
}


/**
 * Driver routine to compute the post-Newtonian inspiral waveform.
 *
 * This routine uses the same pN order for phasing and amplitude
 * (unless the order is -1 in which case the highest available
 * order is used for both of these -- which might not be the same).
 *
 * Constant log term in amplitude set to 1.  This is a gauge choice.
 */
int XLALSimInspiralTaylorT2PN(
		REAL8TimeSeries **hplus,  /**< +-polarization waveform */
	       	REAL8TimeSeries **hcross, /**< x-polarization waveform */
	       	REAL8 phic,               /**< coalescence phase */
	       	REAL8 deltaT,             /**< sampling interval */
	       	REAL8 m1,                 /**< mass of companion 1 */
	       	REAL8 m2,                 /**< mass of companion 2 */
	       	REAL8 f_min,              /**< start frequency */
	       	REAL8 r,                  /**< distance of source */
	       	REAL8 i,                  /**< inclination of source (rad) */
	       	int O                     /**< twice post-Newtonian order */
		)
{
	/* set v0 to default value 1 */
	return XLALSimInspiralTaylorT2PNGenerator(hplus, hcross, phic, 1.0, deltaT, m1, m2, f_min, r, i, O, O);
}


/**
 * Driver routine to compute the restricted post-Newtonian inspiral waveform.
 *
 * This routine computes the phasing to the specified order, but
 * only computes the amplitudes to the Newtonian (quadrupole) order.
 *
 * Constant log term in amplitude set to 1.  This is a gauge choice.
 */
int XLALSimInspiralTaylorT2PNRestricted(
		REAL8TimeSeries **hplus,  /**< +-polarization waveform */
	       	REAL8TimeSeries **hcross, /**< x-polarization waveform */
	       	REAL8 phic,               /**< coalescence phase */
	       	REAL8 deltaT,             /**< sampling interval */
	       	REAL8 m1,                 /**< mass of companion 1 */
	       	REAL8 m2,                 /**< mass of companion 2 */
	       	REAL8 f_min,              /**< start frequency */
	       	REAL8 r,                  /**< distance of source */
	       	REAL8 i,                  /**< inclination of source (rad) */
	       	int O                     /**< twice post-Newtonian phase order */
		)
{
	/* use Newtonian order for amplitude */
	/* set v0 to default value 1 */
	return XLALSimInspiralTaylorT2PNGenerator(hplus, hcross, phic, 1.0, deltaT, m1, m2, f_min, r, i, 0, O);
}
