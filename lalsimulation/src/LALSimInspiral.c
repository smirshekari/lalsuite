/*
 * Copyright (C) 2008 J. Creighton, S. Fairhurst, B. Krishnan, L. Santamaria, D. Keppel, Evan Ochsner
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

#include <gsl/gsl_const.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_odeiv.h>

#define LAL_USE_OLD_COMPLEX_STRUCTS
#include <lal/LALSimInspiral.h>
#define LAL_USE_COMPLEX_SHORT_MACROS
#include <lal/LALComplex.h>
#include <lal/LALConstants.h>
#include <lal/LALStdlib.h>
#include <lal/TimeSeries.h>
#include <lal/Units.h>
#include <lal/SphericalHarmonics.h>

#include "check_series_macros.h"

#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED
#endif

#define MAX_NONPRECESSING_AMP_PN_ORDER 5
#define MAX_PRECESSING_AMP_PN_ORDER 3

/**
 * Macro functions to rotate the components of a vector about an axis
 */
#define ROTATEZ(angle, vx, vy, vz)\
	tmp1 = vx*cos(angle) - vy*sin(angle);\
	tmp2 = vx*sin(angle) + vy*cos(angle);\
	vx = tmp1;\
	vy = tmp2;

#define ROTATEY(angle, vx, vy, vz)\
	tmp1 = vx*cos(angle) - vz*sin(angle);\
	tmp2 = vx*sin(angle) + vz*cos(angle);\
	vx = tmp1;\
	vz = tmp2;


/**
 * Multiplies a mode h(l,m) by a spin-2 weighted spherical harmonic
 * to obtain hplus - i hcross, which is added to the time series.
 *
 * Implements the sum of a single term of Eq. (11) of:
 * Lawrence E. Kidder, \"Using Full Information When Computing Modes of
 * Post-Newtonian Waveforms From Inspiralling Compact Binaries in Circular
 * Orbit\", Physical Review D 77, 044016 (2008), arXiv:0710.0614v1 [gr-qc].
 *
 * If sym is non-zero, symmetrically add the m and -m terms assuming
 * that \f$h(l,-m) = (-1)^l h(l,m)*\f$; see Eq. (78) ibid.
 */
int XLALSimAddMode(
		REAL8TimeSeries *hplus,      /**< +-polarization waveform */
	       	REAL8TimeSeries *hcross,     /**< x-polarization waveform */
	       	COMPLEX16TimeSeries *hmode,  /**< complex mode h(l,m) */
	       	REAL8 theta,                 /**< polar angle (rad) */
	       	REAL8 phi,                   /**< azimuthal angle (rad) */
	       	int l,                       /**< mode number l */
	       	int m,                       /**< mode number m */
	       	int sym                      /**< flag to add -m mode too */
		)
{
	COMPLEX16 Y;
	UINT4 j;

	LAL_CHECK_VALID_SERIES(hmode, XLAL_FAILURE);
	LAL_CHECK_VALID_SERIES(hplus, XLAL_FAILURE);
	LAL_CHECK_VALID_SERIES(hcross, XLAL_FAILURE);
	LAL_CHECK_CONSISTENT_TIME_SERIES(hplus, hmode, XLAL_FAILURE);
	LAL_CHECK_CONSISTENT_TIME_SERIES(hcross, hmode, XLAL_FAILURE);

	Y = XLALSpinWeightedSphericalHarmonic(theta, phi, -2, l, m);
	for ( j = 0; j < hmode->data->length; ++j ) {
		COMPLEX16 hpc;
		hpc = cmul(Y, hmode->data->data[j]);
		hplus->data->data[j] += creal(hpc);
		hcross->data->data[j] += -cimag(hpc);
	}
	if ( sym ) { /* equatorial symmetry: add in -m mode */
		Y = XLALSpinWeightedSphericalHarmonic(theta, phi, -2, l, -m);
		if ( l % 2 ) /* l is odd */
			Y = cneg(Y);
		for ( j = 0; j < hmode->data->length; ++j ) {
			COMPLEX16 hpc;
			hpc = cmul(Y, conj(hmode->data->data[j]));
			hplus->data->data[j] += creal(hpc);
			hcross->data->data[j] += -cimag(hpc);
		}
	}
	return 0;
}


/**
 * Computes h(l,m) mode timeseries of spherical harmonic decomposition of
 * the post-Newtonian inspiral waveform.
 *
 * See Eqns. (79)-(116) of:
 * Lawrence E. Kidder, \"Using Full Information When Computing Modes of
 * Post-Newtonian Waveforms From Inspiralling Compact Binaries in Circular
 * Orbit\", Physical Review D 77, 044016 (2008), arXiv:0710.0614v1 [gr-qc].
 */
COMPLEX16TimeSeries *XLALCreateSimInspiralPNModeCOMPLEX16TimeSeries(
		REAL8TimeSeries *v,   /**< post-Newtonian parameter */
	       	REAL8TimeSeries *phi, /**< orbital phase */
	       	REAL8 v0,             /**< tail-term gauge choice (default = 1) */
	       	REAL8 m1,             /**< mass of companion 1 */
	       	REAL8 m2,             /**< mass of companion 2 */
	       	REAL8 r,              /**< distance of source */
	       	int O,                /**< twice post-Newtonain order */
	       	int l,                /**< mode number l */
	       	int m                 /**< mode number m */
		)
{
	COMPLEX16TimeSeries *h;
	UINT4 j;
	LAL_CHECK_VALID_SERIES(v, NULL);
	LAL_CHECK_VALID_SERIES(phi, NULL);
	LAL_CHECK_CONSISTENT_TIME_SERIES(v, phi, NULL);
	h = XLALCreateCOMPLEX16TimeSeries( "H_MODE", &v->epoch, 0.0, v->deltaT, &lalStrainUnit, v->data->length );
	if ( !h )
		XLAL_ERROR_NULL(XLAL_EFUNC);
	if ( l == 2 && abs(m) == 2 )
		for ( j = 0; j < h->data->length; ++j )
			h->data->data[j] = XLALSimInspiralPNMode22(v->data->data[j], phi->data->data[j], v0, m1, m2, r, O);
	else if ( l == 2 && abs(m) == 1 )
		for ( j = 0; j < h->data->length; ++j )
			h->data->data[j] = XLALSimInspiralPNMode21(v->data->data[j], phi->data->data[j], v0, m1, m2, r, O);
	else if ( l == 3 && abs(m) == 3 )
		for ( j = 0; j < h->data->length; ++j )
			h->data->data[j] = XLALSimInspiralPNMode33(v->data->data[j], phi->data->data[j], v0, m1, m2, r, O);
	else if ( l == 3 && abs(m) == 2 )
		for ( j = 0; j < h->data->length; ++j )
			h->data->data[j] = XLALSimInspiralPNMode32(v->data->data[j], phi->data->data[j], v0, m1, m2, r, O);
	else if ( l == 3 && abs(m) == 1 )
		for ( j = 0; j < h->data->length; ++j )
			h->data->data[j] = XLALSimInspiralPNMode31(v->data->data[j], phi->data->data[j], v0, m1, m2, r, O);
	else {
		XLALDestroyCOMPLEX16TimeSeries(h);
		XLALPrintError("XLAL Error - %s: Unsupported mode l=%d, m=%d\n", __func__, l, m );
		XLAL_ERROR_NULL(XLAL_EINVAL);
	}
	if ( m < 0 ) {
		REAL8 sign = l % 2 ? -1.0 : 1.0;
		for ( j = 0; j < h->data->length; ++j )
			h->data->data[j] = cmulr(conj(h->data->data[j]), sign);
	}
	return h;
}


/**
 * Given time series for a binary's orbital dynamical variables,
 * construct the waveform polarizations h+ and hx as a sum of
 * -2 spin-weighted spherical harmonic modes, h_lm.
 * NB: Valid only for non-precessing systems!
 *
 * Implements Equation (11) of:
 * Lawrence E. Kidder, \"Using Full Information When Computing Modes of
 * Post-Newtonian Waveforms From Inspiralling Compact Binaries in Circular
 * Orbit\", Physical Review D 77, 044016 (2008), arXiv:0710.0614v1 [gr-qc].
 */
int XLALSimInspiralPNPolarizationWaveformsFromModes(
		REAL8TimeSeries **hplus,  /**< +-polarization waveform [returned] */
	       	REAL8TimeSeries **hcross, /**< x-polarization waveform [returned] */
	       	REAL8TimeSeries *v,       /**< post-Newtonian parameter */
	       	REAL8TimeSeries *phi,     /**< orbital phase */
	       	REAL8 v0,                 /**< tail-term gauge choice (default = 1) */
	       	REAL8 m1,                 /**< mass of companion 1 */
	       	REAL8 m2,                 /**< mass of companion 2 */
	       	REAL8 r,                  /**< distance of source */
	       	REAL8 i,                  /**< inclination of source (rad) */
	       	int O                     /**< twice post-Newtonian order */
		)
{
	int l, m;
	LAL_CHECK_VALID_SERIES(v, XLAL_FAILURE);
	LAL_CHECK_VALID_SERIES(phi, XLAL_FAILURE);
	LAL_CHECK_CONSISTENT_TIME_SERIES(v, phi, XLAL_FAILURE);
	*hplus = XLALCreateREAL8TimeSeries( "H_PLUS", &v->epoch, 0.0, v->deltaT, &lalStrainUnit, v->data->length );
	*hcross = XLALCreateREAL8TimeSeries( "H_CROSS", &v->epoch, 0.0, v->deltaT, &lalStrainUnit, v->data->length );
	if ( ! hplus || ! hcross )
		XLAL_ERROR(XLAL_EFUNC);
	memset((*hplus)->data->data, 0, (*hplus)->data->length*sizeof(*(*hplus)->data->data));
	memset((*hcross)->data->data, 0, (*hcross)->data->length*sizeof(*(*hcross)->data->data));
	for ( l = 2; l <= LAL_PN_MODE_L_MAX; ++l ) {
		for ( m = 1; m <= l; ++m ) {
			COMPLEX16TimeSeries *hmode;
			hmode = XLALCreateSimInspiralPNModeCOMPLEX16TimeSeries(v, phi, v0, m1, m2, r, O, l, m);
			if ( ! hmode )
				XLAL_ERROR(XLAL_EFUNC);
			if ( XLALSimAddMode(*hplus, *hcross, hmode, i, 0.0, l, m, 1) < 0 )
				XLAL_ERROR(XLAL_EFUNC);
			XLALDestroyCOMPLEX16TimeSeries(hmode);
		}
	}
	return 0;
}

/**
 * Given time series for a binary's orbital dynamical variables,
 * construct the waveform polarizations h+ and hx directly.
 * NB: Valid only for non-precessing binaries!
 *
 * Implements Equations (8.8) - (8.10) of:
 * Luc Blanchet, Guillaume Faye, Bala R. Iyer and Siddhartha Sinha,
 * \"The third post-Newtonian gravitational wave polarisations
 * and associated spherical harmonic modes for inspiralling compact binaries
 * in quasi-circular orbits\", Class. Quant. Grav. 25 165003 (2008);
 * arXiv:0802.1249
 *
 * Note however, that we do not include the constant \"memory\" terms
 */
int XLALSimInspiralPNPolarizationWaveforms(
	REAL8TimeSeries **hplus,  /**< +-polarization waveform [returned] */
	REAL8TimeSeries **hcross, /**< x-polarization waveform [returned] */
	REAL8TimeSeries *V,       /**< post-Newtonian (PN) parameter */
	REAL8TimeSeries *Phi,     /**< orbital phase */
	REAL8 v0,                 /**< tail-term gauge choice (default = 1) */
	REAL8 m1,                 /**< mass of companion 1 (kg) */
	REAL8 m2,                 /**< mass of companion 2 (kg) */
	REAL8 r,                  /**< distance of source (m) */
	REAL8 i,                  /**< inclination of source (rad) */
	int ampO                  /**< twice PN order of the amplitude */
	)
{
  REAL8 M, eta, eta2, eta3, dm, dist, ampfac, phi, phiShift, v, v2, v3;
    REAL8 hp0, hp05, hp1, hp15, hp2, hp25, hp3;
    REAL8 hc0, hc05, hc1, hc15, hc2, hc25, hc3;
    REAL8 ci, si, ci2, ci4, ci6, ci8, si2, si3, si4, si5, si6;
    INT4 idx, len;

    /* Sanity check input time series */
    LAL_CHECK_VALID_SERIES(V, XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(Phi, XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, Phi, XLAL_FAILURE);

    /* Allocate polarization vectors and set to 0 */
    *hplus = XLALCreateREAL8TimeSeries( "H_PLUS", &V->epoch, 0.0, 
            V->deltaT, &lalStrainUnit, V->data->length );
    *hcross = XLALCreateREAL8TimeSeries( "H_CROSS", &V->epoch, 0.0, 
            V->deltaT, &lalStrainUnit, V->data->length );
    if ( ! hplus || ! hcross )
        XLAL_ERROR(XLAL_EFUNC);
    memset((*hplus)->data->data, 0, (*hplus)->data->length 
            * sizeof(*(*hplus)->data->data));
    memset((*hcross)->data->data, 0, (*hcross)->data->length 
            * sizeof(*(*hcross)->data->data));

    M = m1 + m2;
    eta = m1 * m2 / M / M; // symmetric mass ratio - '\nu' in the paper
    eta2 = eta*eta;	eta3 = eta2*eta;
    dm = (m1 - m2) / M; // frac. mass difference - \delta m/m in the paper
    dist = r / LAL_C_SI;   // r (m) / c (m/s) --> dist in units of seconds
    /* convert mass from kg to s, so ampfac ~ M/dist is dimensionless */
    ampfac = 2. * M * LAL_G_SI * pow(LAL_C_SI, -3) * eta / dist;
    
    /* 
     * cosines and sines of inclination between 
     * line of sight (N) and binary orbital angular momentum (L_N)
     */
    ci = cos(i);  	si = sin(i);
    ci2 = ci*ci;  ci4 = ci2*ci2;  ci6 = ci2*ci4;  ci8 = ci6*ci2;
    si2 = si*si;  si3 = si2*si;  si4 = si2*si2;  si5 = si*si4; si6 = si4*si2;

    /* loop over time steps and compute polarizations h+ and hx */
    len = V->data->length;
    for(idx = 0; idx < len; idx++)
    {
        /* Abbreviated names in lower case for time series at this sample */
        phi = Phi->data->data[idx]; 	v = V->data->data[idx];   
        v2 = v * v; 	v3 = v * v2;

        /* 
         * As explained in Blanchet et al, a phase shift can be applied 
         * to make log terms vanish which would appear in the amplitude 
         * at 1.5PN and 2.5PN orders. This shift is given in Eq. (8.8)
         * We apply the shift only for the PN orders which need it.
         */
        if( (ampO == -1) || ampO >= 5 )
            phiShift = 3.*v3*(1. - v2*eta/2.)*log( v2 / v0 / v0  );
        else if( ampO >= 3 )
            phiShift = 3.*v3*log( v2 / v0 / v0 );
        else
            phiShift = 0.;

        phi = phi - phiShift;

        /*
         * First set all h+/x coefficients to 0. Then use a switch to
         * set proper non-zero values up to order ampO. Note we
         * fall through the PN orders and break only after Newt. order
         */
        hp0 = hp05 = hp1 = hp15 = hp2 = hp25 = hp3 = 0.;
        hc0 = hc05 = hc1 = hc15 = hc2 = hc25 = hc3 = 0.;

        switch( ampO )
        {
            /* case LAL_PNORDER_THREE_POINT_FIVE: */
            case 7:
                XLALPrintError("XLAL Error - %s: Amp. corrections not known "
                        "to PN order %s\n", __func__, ampO );
                XLAL_ERROR(XLAL_EINVAL);
                break;
            case -1: // Highest known PN order - move if higher terms added!
            /* case LAL_PNORDER_THREE: */
            case 6:
		/* FIXME: These 3PN terms are known to be incorrect and the
		 * authors are producing an errata to fix them. */
                hp3 = LAL_PI*dm*si*cos(phi)*(19./64. + ci2*5./16. - ci4/192.
                        + eta*(-19./96. + ci2*3./16. + ci4/96.)) + cos(2.*phi)
                        * (-465497./11025. + (LAL_GAMMA*856./105. 
                        - 2.*LAL_PI*LAL_PI/3. + log(16.*v2)*428./105.)
                        * (1. + ci2) - ci2*3561541./88200. - ci4*943./720.
                        + ci6*169./720. - ci8/360. + eta*(2209./360.
                        - LAL_PI*LAL_PI*41./96.*(1. + ci2) + ci2*2039./180.
                        + ci4*3311./720. - ci6*853./720. + ci8*7./360.)
                        + eta2*(12871./540. - ci2*1583./60. - ci4*145./108.
                        + ci6*56./45. - ci8*7./180.) + eta3*(-3277./810.
                        + ci2*19661./3240. - ci4*281./144. - ci6*73./720.
                        + ci8*7./360.)) + LAL_PI*dm*si*cos(3.*phi)*(-1971./128.
                        - ci2*135./16. + ci4*243./128. + eta*(567./64.
                        - ci2*81./16. - ci4*243./64.)) + si2*cos(4.*phi)
                        * (-2189./210. + ci2*1123./210. + ci4*56./9. 
                        - ci6*16./45. + eta*(6271./90. - ci2*1969./90.
                        - ci4*1432./45. + ci6*112./45.) + eta2*(-3007./27.
                        + ci2*3493./135. + ci4*1568./45. - ci6*224./45.)
                        + eta3*(161./6. - ci2*1921./90. - ci4*184./45.
                        + ci6*112./45.)) + dm*cos(5.*phi)*(LAL_PI*3125./384.
                        * si3*(1. + ci2)*(1. - 2.*eta)) + si4*cos(6.*phi)
                        * (1377./80. + ci2*891./80. - ci4*729./280. 
                        + eta*(-7857./80. - ci2*891./16. + ci4*729./40.)
                        + eta2*(567./4. + ci2*567./10. - ci4*729./20.)
                        + eta3*(-729./16. - ci2*243./80. + ci4*729./40.)) 
                        + cos(8.*phi)*(-1024./315.*si6*(1. + ci2)*(1. - 7.*eta 
                        + 14.*eta2 - 7.*eta3)) + dm*si*sin(phi)*(-2159./40320.
                        - log(2.)*19./32. + (-95./224. - log(2.)*5./8.)*ci2
                        + (181./13440. + log(2.)/96.)*ci4 + eta*(81127./10080.
                        + log(2.)*19./48. + (-41./48. - log(2.)*3./8.)*ci2
                        + (-313./480. - log(2.)/48.)*ci4)) + sin(2.*phi)
                        * (-428.*LAL_PI/105.*(1. + ci2)) + dm*si*sin(3.*phi)
                        * (205119./8960. - log(3./2.)*1971./64. 
                        + (1917./224. - log(3./2.)*135./8.)*ci2
                        + (-43983./8960. + log(3./2.)*243./64.)*ci4 + eta
                        * (-54869./960. + log(3./2.)*567./32. 
                        + (-923./80. - log(3./2.)*81./8.)*ci2 
                        + (41851./2880. - log(3./2.)*243./32.)*ci4)) 
                        + dm*si3*(1. + ci2)*sin(5.*phi)*(-113125./5376. 
                        + log(5./2.)*3125./192. 
                        + eta*(17639./320. - log(5./2.)*3125./96.));
                hc3 = dm*si*ci*cos(phi)*(11617./20160. + log(2.)*21./16.
                        + (-251./2240. - log(2.)*5./48.)*ci2 
                        + eta*(-48239./5040. - log(2.)*5./24. 
                        + (727./240. + log(2.)*5./24.)*ci2)) + ci*cos(2.*phi)
                        * (LAL_PI*856./105.) + dm*si*ci*cos(3.*phi)
                        * (-36801./896. + log(3./2.)*1809./32.
                        + (65097./4480. - log(3./2.)*405./32.)*ci2 
                        + eta*(28445./288. - log(3./2.)*405./16. 
                        + (-7137./160. + log(3./2.)*405./16.)*ci2)) 
                        + dm*si3*ci*cos(5.*phi)*(113125./2688. 
                        - log(5./2.)*3125./96. + eta*(-17639./160. 
                        + log(5./2.)*3125./48.)) + LAL_PI*dm*si*ci*sin(phi)
                        * (21./32. - ci2*5./96. + eta*(-5./48. + ci2*5./48.))
                        + ci*sin(2.*phi)*(-3620761./44100. 
                        + LAL_GAMMA*1712./105. - 4.*LAL_PI*LAL_PI/3.
                        + log(16.*v2)*856./105. - ci2*3413./1260. 
                        + ci4*2909./2520. - ci6/45. + eta*(743./90. 
                        - 41.*LAL_PI*LAL_PI/48. + ci2*3391./180. 
                        - ci4*2287./360. + ci6*7./45.) + eta2*(7919./270.
                        - ci2*5426./135. + ci4*382./45. - ci6*14./45.) 
                        + eta3*(-6457./1620. + ci2*1109./180. - ci4*281./120.
                        + ci6*7./45.)) + LAL_PI*dm*si*ci*sin(3.*phi)
                        * (-1809./64. + ci2*405./64. + eta*(405./32. 
                        - ci2*405./32.)) + si2*ci*sin(4.*phi)*(-1781./105.
                        + ci2*1208./63. - ci4*64./45. + eta*(5207./45. 
                        - ci2*536./5. + ci4*448./45.) + eta2*(-24838./135.
                        + ci2*2224./15. - ci4*896./45.) + eta3*(1703./45.
                        - ci2*1976./45. + ci4*448./45.)) + dm*sin(5.*phi)
                        * (3125.*LAL_PI/192.*si3*ci*(1. - 2.*eta)) 
                        + si4*ci*sin(6.*phi)*(9153./280. - ci2*243./35. 
                        + eta*(-7371./40. + ci2*243./5.) + eta2*(1296./5. 
                        - ci2*486./5.) + eta3*(-3159./40. + ci2*243./5.))
                        + sin(8.*phi)*(-2048./315.*si6*ci*(1. - 7.*eta 
                        + 14.*eta2 - 7.*eta3));
            /* case LAL_PNORDER_TWO_POINT_FIVE: */
            case 5:
                hp25 = cos(phi)*si*dm*(1771./5120. - ci2*1667./5120. 
                        + ci4*217./9216. - ci6/9126. + eta*(681./256. 
                        + ci2*13./768. - ci4*35./768. + ci6/2304.)
                        + eta2*(-3451./9216. + ci2*673./3072. - ci4*5./9216.
                        - ci6/3072.)) + cos(2.*phi)*LAL_PI*(19./3. + 3.*ci2 
                        - ci4*2./3. + eta*(-16./3. + ci2*14./3. + 2.*ci4))
                        + cos(3.*phi)*si*dm*(3537./1024. - ci2*22977./5120. 
                        - ci4*15309./5120. + ci6*729./5120. 
                        + eta*(-23829./1280. + ci2*5529./1280. 
                        + ci4*7749./1280. - ci6*729./1280.) 
                        + eta2*(29127./5120. - ci2*27267./5120. 
                        - ci4*1647./5120. + ci6*2187./5120.)) + cos(4.*phi)
                        * (-16.*LAL_PI/3.*(1. + ci2)*si2*(1. - 3.*eta))
                        + cos(5.*phi)*si*dm*(-108125./9216. + ci2*40625./9216. 
                        + ci4*83125./9216. - ci6*15625./9216. 
                        + eta*(8125./256. - ci2*40625./2304. - ci4*48125./2304.
                        + ci6*15625./2304.) + eta2*(-119375./9216. 
                        + ci2*40625./3072. + ci4*44375./9216. 
                        - ci6*15625./3072.)) + cos(7.*phi)*dm
                        * (117649./46080.*si5*(1. + ci2)*(1. - 4.*eta 
                        + 3.*eta2)) + sin(2.*phi)*(-9./5. + ci2*14./5. 
                        + ci4*7./5. + eta*(32. + ci2*56./5. - ci4*28./5.)) 
                        + sin(4.*phi)*si2*(1. + ci2)*(56./5. - 32.*log(2.)/3. 
                        + eta*(-1193./30. + 32.*log(2.)));
                /* below would have a constant memory term of si2*ci*eta*6./5. */
                hc25 = cos(2.*phi)*ci*(2. - ci2*22./5. + eta*(-282./5. 
                        + ci2*94./5.)) + cos(4.*phi)*ci*si2*(-112./5. 
                        + 64.*log(2.)/3. + eta*(1193./15. - 64.*log(2.)))
                        + sin(phi)*si*ci*dm*(-913./7680. + ci2*1891./11520. 
                        - ci4*7./4608. + eta*(1165./384. - ci2*235./576. 
                        + ci4*7./1152.) + eta2*(-1301./4608. + ci2*301./2304.
                        - ci4*7./1536.)) + sin(2.*phi)*LAL_PI*ci*(34./3. 
                        - ci2*8./3. + eta*(-20./3. + 8.*ci2)) 
                        + sin(3.*phi)*si*ci*dm*(12501./2560. - ci2*12069./1280.
                        + ci4*1701./2560. + eta*(-19581./640. + ci2*7821./320.
                        - ci4*1701./640.) + eta2*(18903./2560. 
                        - ci2*11403./1280. + ci4*5103./2560.)) 
                        + sin(4.*phi)*si2*ci*(-32.*LAL_PI/3.*(1. - 3.*eta))
                        + sin(5.*phi)*si*ci*dm*(-101875./4608. + ci2*6875./256.
                        - ci4*21875./4608. + eta*(66875./1152. 
                        - ci2*44375./576. + ci4*21875./1152.) 
                        + eta2*(-100625./4608. + ci2*83125./2304. 
                        - ci4*21875./1536.)) + sin(7.*phi)*si5*ci*dm
                        * (117649./23040.*(1. - 4.*eta + 3.*eta2));
            /* case LAL_PNORDER_TWO: */
            case 4:
                hp2 = cos(phi)*LAL_PI*si*dm*(-5./8. - ci2/8.) 
                        + cos(2.*phi)*(11./60. + ci2*33./10. + ci4*29./24. 
                        - ci6/24. + eta*(353./36. - 3.*ci2 - ci4*251./72. 
                        + ci6*5./24.) + eta2*(-49./12. + ci2*9./2. 
                        - ci4*7./24. - ci6*5./24.)) + cos(3.*phi)*LAL_PI*si*dm
                        * (27./8.*(1 + ci2)) + cos(4.*phi)*si2*2./15.*(59. 
                        + ci2*35. - ci4*8. 
                        - eta*5./3.*(131. + 59.*ci2 + 24.*ci4)
                        + eta2*5.*(21. - 3.*ci2 - 8.*ci4))
                        + cos(6.*phi)*(-81./40.*si4*(1. + ci2)
                        * (1. - 5.*eta + 5.*eta2)) + sin(phi)*si*dm
                        * (11./40. + 5.*log(2)/4. + ci2*(7./40. + log(2)/4.))
                        + sin(3.*phi)*si*dm*((-189./40. + 27./4.*log(3./2.))
                        * (1. + ci2));
                hc2 = cos(phi)*si*ci*dm*(-9./20. - 3./2.*log(2.)) 
                        + cos(3.*phi)*si*ci*dm*(189./20. - 27./2.*log(3./2.))
                        - sin(phi)*si*ci*dm*3.*LAL_PI/4. 
                        + sin(2.*phi)*ci*(17./15. + ci2*113./30. - ci4/4.
                        + eta*(143./9. - ci2*245./18. + ci4*5./4.)
                        + eta2*(-14./3. + ci2*35./6. - ci4*5./4.))
                        + sin(3.*phi)*si*ci*dm*27.*LAL_PI/4.
                        + sin(4.*phi)*ci*si2*4./15.*(55. - 12.*ci2 
                        - eta*5./3.*(119. - 36.*ci2)
                        + eta2*5.*(17. - 12.*ci2))
                        + sin(6.*phi)*ci*(-81./20.*si4
                        * (1. - 5.*eta + 5.*eta2));
            /* case LAL_PNORDER_ONE_POINT_FIVE: */
            case 3:
                hp15 = cos(phi)*si*dm*(19./64. + ci2*5./16. - ci4/192. 
                        + eta*(-49./96. + ci2/8. + ci4/96.))
                        + cos(2.*phi)*(-2.*LAL_PI*(1. + ci2))
                        + cos(3.*phi)*si*dm*(-657./128. - ci2*45./16. 
                        + ci4*81./128. + eta*(225./64. - ci2*9./8. 
                        - ci4*81./64.)) + cos(5.*phi)*si*dm*(625./384.*si2
                        * (1. + ci2)*(1. - 2.*eta));
                hc15 = sin(phi)*si*ci*dm*(21./32. - ci2*5./96. 
                        + eta*(-23./48. + ci2*5./48.)) 
                        - 4.*LAL_PI*ci*sin(2.*phi) + sin(3.*phi)*si*ci*dm
                        * (-603./64. + ci2*135./64. 
                        + eta*(171./32. - ci2*135./32.)) 
                        + sin(5.*phi)*si*ci*dm*(625./192.*si2*(1. - 2.*eta));
            /* case LAL_PNORDER_ONE: */
            case 2:
                hp1 = cos(2.*phi)*(19./6. + 3./2.*ci2 - ci4/3. 
                        + eta*(-19./6. + ci2*11./6. + ci4)) 
                        - cos(4.*phi) * (4./3.*si2*(1. + ci2)*(1. - 3.*eta));
                hc1 = sin(2.*phi)*ci*(17./3. - ci2*4./3. 
                        + eta*(-13./3. + 4.*ci2)) 
                        + sin(4.*phi)*ci*si2*(-8./3.*(1. - 3.*eta));
            /*case LAL_PNORDER_HALF:*/
            case 1:
                hp05 = - si*dm*(cos(phi)*(5./8. + ci2/8.) 
                        - cos(3.*phi)*(9./8. + 9.*ci2/8.));
                hc05 = si*ci*dm*(-sin(phi)*3./4. + sin(3.*phi)*9./4.);
            case 0:
                /* below would have a constant memory term of -si2/96.*(17. + ci2) */
                hp0 = -(1. + ci2)*cos(2.*phi);
                hc0 = -2.*ci*sin(2.*phi);
                break;
            /*case LAL_PNORDER_NEWTONIAN:*/
            default:
                XLALPrintError("XLAL Error - %s: Invalid amp. PN order %s\n",
                        __func__, ampO );
                XLAL_ERROR(XLAL_EINVAL);
                break;
        } /* End switch on ampO */

        /* Fill the output polarization arrays */
        (*hplus)->data->data[idx] = ampfac * v2 * ( hp0 + v * ( hp05 
                + v * ( hp1 + v * ( hp15 + v * ( hp2 + v * ( hp25 + v * hp3 
                ) ) ) ) ) );
        (*hcross)->data->data[idx] = ampfac * v2 * ( hc0 + v * ( hc05 
                + v * ( hc1 + v * ( hc15 + v * ( hc2 + v * ( hc25 + v * hc3 
                ) ) ) ) ) );

    } /* end loop over time series samples idx */
    return XLAL_SUCCESS;
}

/**
 * Computes polarizations h+ and hx for a spinning, precessing binary
 * when provided time series of all the dynamical quantities.
 * Amplitude can be chosen between 1.5PN and Newtonian orders (inclusive).
 *
 * Based on K.G. Arun, Alesssandra Buonanno, Guillaume Faye and Evan Ochsner
 * \"Higher-order spin effects in the amplitude and phase of gravitational
 * waveforms emitted by inspiraling compact binaries: Ready-to-use
 * gravitational waveforms\", Phys Rev. D 79, 104023 (2009), arXiv:0810.5336
 *
 * HOWEVER, the formulae have been adapted to use the output of the so-called
 * \"Frameless\" convention for evolving precessing binary dynamics,
 * which is not susceptible to hitting coordinate singularities.
 *
 * FIXME: Clean up and commit Mathematica NB Showing correctness. Cite here.
 *
 * NOTE: The vectors MUST be given in the so-called radiation frame where
 * Z is the direction of propagation, X is the principal '+' axis and Y = Z x X
 */
int XLALSimInspiralPrecessingPolarizationWaveforms(
	REAL8TimeSeries **hplus,  /**< +-polarization waveform [returned] */
	REAL8TimeSeries **hcross, /**< x-polarization waveform [returned] */
	REAL8TimeSeries *V,       /**< post-Newtonian parameter */
	REAL8TimeSeries *Phi,     /**< orbital phase */
	REAL8TimeSeries *S1x,	  /**< Spin1 vector x component */
	REAL8TimeSeries *S1y,	  /**< Spin1 vector y component */
	REAL8TimeSeries *S1z,	  /**< Spin1 vector z component */
	REAL8TimeSeries *S2x,	  /**< Spin2 vector x component */
	REAL8TimeSeries *S2y,	  /**< Spin2 vector y component */
	REAL8TimeSeries *S2z,	  /**< Spin2 vector z component */
	REAL8TimeSeries *LNhatx,  /**< unit orbital ang. mom. x comp. */
	REAL8TimeSeries *LNhaty,  /**< unit orbital ang. mom. y comp. */
	REAL8TimeSeries *LNhatz,  /**< unit orbital ang. mom. z comp. */
	REAL8TimeSeries *E1x,	  /**< orbital plane basis vector x comp. */
	REAL8TimeSeries *E1y,	  /**< orbital plane basis vector y comp. */
	REAL8TimeSeries *E1z,	  /**< orbital plane basis vector z comp. */
	REAL8 m1,                 /**< mass of companion 1 (kg) */
	REAL8 m2,                 /**< mass of companion 2 (kg) */
	REAL8 r,                  /**< distance of source (m) */
	REAL8 v0,                 /**< tail-term gauge choice (default = 1) */
	INT4 ampO	 	  /**< twice amp. post-Newtonian order */
	)
{
    REAL8 s1x, s1y, s1z, s2x, s2y, s2z, lnhx, lnhy, lnhz;
    REAL8 e1x, e1y, e1z, e2x, e2y, e2z, nx, ny, nz, lx, ly, lz;
    REAL8 nx2, ny2, nz2, nz3, lx2, ly2, lz2, lz3;
    REAL8 hplus0, hcross0, hplus05, hcross05, hplus1, hcross1;
    REAL8 hplus15, hcross15, hplusSpin1, hcrossSpin1;
    REAL8 hplusSpin15, hcrossSpin15, hplusTail15, hcrossTail15; 
    REAL8 M, eta, dm, phi, v, v2, dist, ampfac, logfac = 0.;
    INT4 idx, len;

    /* Macros to check time series vectors */
    LAL_CHECK_VALID_SERIES(V, 			XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(Phi, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S1x, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S1y, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S1z, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S2x, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S2y, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S2z, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(LNhatx, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(LNhaty, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(LNhatz, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(E1x, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(E1y, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(E1z, 		XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, Phi, 	XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S1x, 	XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S1y, 	XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S1z, 	XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S2x, 	XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S2y, 	XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S2z, 	XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, LNhatx, XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, LNhaty, XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, LNhatz, XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, E1x, 	XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, E1y, 	XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, E1z, 	XLAL_FAILURE);

    /* Allocate polarization vectors and set to 0 */
    *hplus = XLALCreateREAL8TimeSeries( "H_PLUS", &V->epoch, 
            0.0, V->deltaT, &lalStrainUnit, V->data->length );
    *hcross = XLALCreateREAL8TimeSeries( "H_CROSS", &V->epoch, 
            0.0, V->deltaT, &lalStrainUnit, V->data->length );
    if ( ! hplus || ! hcross )
        XLAL_ERROR(XLAL_EFUNC);
    memset((*hplus)->data->data, 0, 
            (*hplus)->data->length*sizeof(*(*hplus)->data->data));
    memset((*hcross)->data->data, 0, 
            (*hcross)->data->length*sizeof(*(*hcross)->data->data));

    M = m1 + m2;
    eta = m1 * m2 / M / M; // symmetric mass ratio - '\nu' in the paper
    dm = (m1 - m2) / M;    // frac. mass difference - \delta m/m in the paper
    dist = r / LAL_C_SI;   // r (m) / c (m/s) --> dist in units of seconds
    /* convert mass from kg to s, so ampfac ~ M/dist is dimensionless */
    ampfac = 2. * M * LAL_G_SI * pow(LAL_C_SI, -3) * eta / dist;
    
    /* loop over time steps and compute polarizations h+ and hx */
    len = V->data->length;
    for(idx = 0; idx < len; idx++)
    {
        /* Abbreviated names in lower case for time series at this sample */
        phi  = Phi->data->data[idx]; 	v = V->data->data[idx];     v2 = v * v;
        lnhx = LNhatx->data->data[idx]; e1x = E1x->data->data[idx];
        lnhy = LNhaty->data->data[idx];	e1y = E1y->data->data[idx];
        lnhz = LNhatz->data->data[idx];	e1z = E1z->data->data[idx];
        s1x  = S1x->data->data[idx];	s2x = S2x->data->data[idx];
        s1y  = S1y->data->data[idx];	s2y = S2y->data->data[idx];
        s1z  = S1z->data->data[idx];	s2z = S2z->data->data[idx];

        /* E2 = LNhat x E1 */
        e2x = lnhy*e1z - lnhz*e1y;
        e2y = lnhz*e1x - lnhx*e1z;
        e2z = lnhx*e1y - lnhy*e1x;

        /* Unit orbital separation vector */
        nx = e1x*cos(phi) + e2x*sin(phi);
        ny = e1y*cos(phi) + e2y*sin(phi);
        nz = e1z*cos(phi) + e2z*sin(phi);

        /* Unit inst. orbital velocity vector */
        lx = e2x*cos(phi) - e1x*sin(phi);
        ly = e2y*cos(phi) - e1y*sin(phi);
        lz = e2z*cos(phi) - e1z*sin(phi);

        /* Powers of vector components */
        nx2 = nx*nx;	ny2 = ny*ny;	nz2 = nz*nz;	nz3 = nz*nz2;
        lx2 = lx*lx;	ly2 = ly*ly;	lz2 = lz*lz;	lz3 = lz*lz2;

        /* 
         * First set all h+/x coefficients to 0. Then use a switch to
         * set proper non-zero values up to order ampO. Note we
         * fall through the PN orders and break only after Newt. order
         */
        hplus0 = hplus05 = hplus1 = hplus15 = hplusTail15 = 0.;
        hcross0 = hcross05 = hcross1 = hcross15 = hcrossTail15 = 0.;
        hplusSpin1 = hplusSpin15 = hcrossSpin1 = hcrossSpin15 = 0.;

        switch( ampO )
        {
            /*
             * case LAL_PNORDER_THREE_POINT_FIVE:
             * case LAL_PNORDER_THREE:
             * case LAL_PNORDER_TWO_POINT_FIVE:
             * case LAL_PNORDER_TWO:
             */
            case 7:
            case 6:
            case 5:
            case 4:
                XLALPrintError("XLAL Error - %s: Amp. corrections not known "
                        "to PN order %d, highest is %d\n", __func__, ampO,
                        MAX_PRECESSING_AMP_PN_ORDER );
                XLAL_ERROR(XLAL_EINVAL);
                break;
            case -1: /* Use highest known PN order - move if new orders added */
            /*case LAL_PNORDER_ONE_POINT_FIVE:*/
            case 3:
                /* 1.5PN non-spinning amp. corrections */
                hplus15 = (dm*(2*lx*nx*nz*(-95 + 90*lz2 - 65*nz2 
                        - 2*eta*(-9 + 90*lz2 - 65*nz2)) - 2*ly*ny*nz
                        * (-95 + 90*lz2 - 65*nz2 - 2*eta*(-9 + 90*lz2 - 65*nz2))
                        + 6*lx2*lz*(13 - 4*lz2 + 29*nz2 + eta*(-2 + 8*lz2 
                        - 58*nz2)) - 6*ly2*lz*(13 - 4*lz2 + 29*nz2 + eta
                        * (-2 + 8*lz2 - 58*nz2)) - lz*(nx2 - ny2)*(83 - 6*lz2 
                        + 111*nz2 + 6*eta*(-1 + 2*lz2 - 37*nz2))))/24.;
                hcross15 = (dm*(lz*(6*(19 - 4*eta)*lx*ly + (-101 + 12*eta)
                        * nx*ny) + (-149 + 36*eta) * (ly*nx + lx*ny)*nz 
                        + 6*(-3 + eta) * (2*lx*ly*lz - lz*nx*ny - 3*ly*nx*nz 
                        - 3*lx*ny*nz) + (1 - 2*eta) * (6*lz3*(-4*lx*ly + nx*ny) 
                        + 90*lz2*(ly*nx + lx*ny)*nz + 3*lz*(58*lx*ly 
                        - 37*nx*ny)*nz2 - 65*(ly*nx + lx*ny)*nz3)))/12.;
                /* 1.5PN spinning amp. corrections */
                hplusSpin15 = (6*lz*ny*s1x + 6*dm*lz*ny*s1x - 3*eta*lz*ny*s1x 
                        + 2*ly2*lnhy*s1y + 2*dm*ly2*lnhy*s1y 
                        + 2*eta*ly2*lnhy*s1y + 6*lz*nx*s1y + 6*dm*lz*nx*s1y 
                        - 3*eta*lz*nx*s1y + 8*lnhy*nx2*s1y + 8*dm*lnhy*nx2*s1y 
                        - eta*lnhy*nx2*s1y - 8*lnhy*ny2*s1y - 8*dm*lnhy*ny2*s1y
                        + eta*lnhy*ny2*s1y + 2*ly2*lnhz*s1z + 2*dm*ly2*lnhz*s1z
                        + 2*eta*ly2*lnhz*s1z - 6*ly*nx*s1z - 6*dm*ly*nx*s1z 
                        - 9*eta*ly*nx*s1z + 8*lnhz*nx2*s1z + 8*dm*lnhz*nx2*s1z 
                        - eta*lnhz*nx2*s1z - 8*lnhz*ny2*s1z - 8*dm*lnhz*ny2*s1z
                        + eta*lnhz*ny2*s1z + 6*lz*ny*s2x - 6*dm*lz*ny*s2x 
                        - 3*eta*lz*ny*s2x + lnhx*(2*ly2*((1 + dm + eta)*s1x 
                        + (1 - dm + eta)*s2x) + (nx2 - ny2)*((8 + 8*dm - eta)
                        * s1x - (-8 + 8*dm + eta)*s2x)) + 2*ly2*lnhy*s2y 
                        - 2*dm*ly2*lnhy*s2y + 2*eta*ly2*lnhy*s2y + 6*lz*nx*s2y 
                        - 6*dm*lz*nx*s2y - 3*eta*lz*nx*s2y + 8*lnhy*nx2*s2y 
                        - 8*dm*lnhy*nx2*s2y - eta*lnhy*nx2*s2y - 8*lnhy*ny2*s2y 
                        + 8*dm*lnhy*ny2*s2y + eta*lnhy*ny2*s2y + 2*ly2*lnhz*s2z 
                        - 2*dm*ly2*lnhz*s2z + 2*eta*ly2*lnhz*s2z - 6*ly*nx*s2z 
                        + 6*dm*ly*nx*s2z - 9*eta*ly*nx*s2z + 8*lnhz*nx2*s2z 
                        - 8*dm*lnhz*nx2*s2z - eta*lnhz*nx2*s2z - 8*lnhz*ny2*s2z 
                        + 8*dm*lnhz*ny2*s2z + eta*lnhz*ny2*s2z - 3*lx*ny 
                        * ((2 + 2*dm + 3*eta)*s1z + (2 - 2*dm + 3*eta)*s2z)
                        - 2*lx2*(lnhx*((1 + dm + eta)*s1x + (1 - dm + eta)*s2x) 
                        + lnhy*((1 + dm + eta)*s1y + (1 - dm + eta)*s2y) 
                        + lnhz*((1 + dm + eta)*s1z + (1 - dm + eta)*s2z)))/3.;
                hcrossSpin15 = (-3*lz*(nx*((2 + 2*dm - eta)*s1x 
                        - (-2 + 2*dm + eta)*s2x) + ny*((-2 - 2*dm + eta)*s1y 
                        + (-2 + 2*dm + eta)*s2y)) + ny*(-6*ly*s1z - 6*dm*ly*s1z 
                        - 9*eta*ly*s1z + 16*lnhz*nx*s1z + 16*dm*lnhz*nx*s1z 
                        - 2*eta*lnhz*nx*s1z + 2*lnhx*nx*((8 + 8*dm - eta)*s1x 
                        - (-8 + 8*dm + eta)*s2x) + 2*lnhy*nx*((8 + 8*dm - eta)
                        * s1y - (-8 + 8*dm + eta)*s2y) - 6*ly*s2z + 6*dm*ly*s2z 
                        - 9*eta*ly*s2z + 16*lnhz*nx*s2z - 16*dm*lnhz*nx*s2z 
                        - 2*eta*lnhz*nx*s2z) - lx*(4*lnhx*ly*((1 + dm + eta)*s1x
                        + (1 - dm + eta)*s2x) - 3*nx*((2 + 2*dm + 3*eta)*s1z 
                        + (2 - 2*dm + 3*eta)*s2z) + 4*ly*(lnhy*((1 + dm + eta)
                        * s1y + (1 - dm + eta)*s2y) + lnhz*((1 + dm + eta)*s1z 
                        + (1 - dm + eta)*s2z))))/3.;
                /* 1.5PN tail amp. corrections */
                logfac = log(v/v0);
                hplusTail15 = 2*((lx2 - ly2 - nx2 + ny2)*LAL_PI 
                        + 12*(lx*nx - ly*ny)*logfac);
                hcrossTail15 = 4*((lx*ly - nx*ny)*LAL_PI 
                        + 6*(ly*nx + lx*ny)*logfac);

            /*case LAL_PNORDER_ONE:*/
            case 2:
                /* 1PN non-spinning amp. corrections */
                hplus1 = (-13*lx2 + 13*ly2 + 6*lx2*lz2 - 6*ly2*lz2 
                        + 13*(nx2 - ny2) - 2*lz2*(nx2 - ny2) - 32*lx*lz*nx*nz 
                        + 32*ly*lz*ny*nz - 14*lx2*nz2 + 14*ly2*nz2 
                        + 10*(nx2 - ny2)*nz2)/6. + (eta*(lx2 - 18*lx2*lz2 
                        + 96*lx*lz*nx*nz - 96*ly*lz*ny*nz + 42*lx2*nz2 
                        + ly2*(-1 + 18*lz2 - 42*nz2) + (nx2 - ny2) 
                        * (-1 + 6*lz2 - 30*nz2)))/6.;
                hcross1 = (eta*(lx*ly - nx*ny - 6*(lz2*(3*lx*ly - nx*ny) 
                        - 8*lz*(ly*nx + lx*ny)*nz + (-7*lx*ly 
                        + 5*nx*ny)*nz2)))/3. + (-13*(lx*ly - nx*ny) 
                        + 2*(lz2*(3*lx*ly - nx*ny) - 8*lz*(ly*nx + lx*ny)*nz 
                        + (-7*lx*ly + 5*nx*ny)*nz2))/3.;
                /* 1PN spinning amp. corrections */
                hplusSpin1 = (-(ny*((1 + dm)*s1x + (-1 + dm)*s2x)) 
                        - nx*((1 + dm)*s1y + (-1 + dm)*s2y))/2.;
                hcrossSpin1 = (nx*((1 + dm)*s1x + (-1 + dm)*s2x) 
                        - ny*((1 + dm)*s1y + (-1 + dm)*s2y))/2.;

            /*case LAL_PNORDER_HALF:*/
            case 1:
                /* 0.5PN non-spinning amp. corrections */
                hplus05 = (dm*(-2*lx2*lz + 2*ly2*lz + lz*(nx2 - ny2) 
                        + 6*lx*nx*nz - 6*ly*ny*nz))/2.;
                hcross05 = dm*(-2*lx*ly*lz + lz*nx*ny 
					+ 3*ly*nx*nz + 3*lx*ny*nz);

            /*case LAL_PNORDER_NEWTONIAN:*/
            case 0:
                /* Newtonian order polarizations */
                hplus0 = lx2 - ly2 - nx2 + ny2;
                hcross0 = 2*lx*ly - 2*nx*ny;
                break;
            default: 
                XLALPrintError("XLAL Error - %s: Invalid amp. PN order %s\n", 
                        __func__, ampO );
                XLAL_ERROR(XLAL_EINVAL);
                break;
        } /* End switch on ampO */

        /* Fill the output polarization arrays */
        (*hplus)->data->data[idx] = ampfac * v2 * ( hplus0 
                + v * ( hplus05 + v * ( hplus1 + hplusSpin1 
                + v * ( hplus15 + hplusSpin15 + hplusTail15 ) ) ) );
        (*hcross)->data->data[idx] = ampfac * v2 * ( hcross0 
                + v * ( hcross05 + v * ( hcross1 + hcrossSpin1 
                + v * ( hcross15 + hcrossSpin15 + hcrossTail15 ) ) ) );
    } /* end of loop over time samples, idx */
    return XLAL_SUCCESS;
}

/**
 * Compute the physical template family "Q" vectors for a spinning, precessing
 * binary when provided time series of all the dynamical quantities.
 * These vectors always supplied to dominant order.
 *
 * Based on Pan, Buonanno, Chan and Vallisneri PRD69 104017, (see also theses
 * of Diego Fazi and Ian Harry)
 *
 * NOTE: The vectors MUST be given in the so-called radiation frame where
 * Z is the direction of propagation, X is the principal '+' axis and Y = Z x X
 */


int XLALSimInspiralPrecessingPTFQWaveforms(
        REAL8TimeSeries **Q1,     /**< PTF-Q1 waveform [returned] */
        REAL8TimeSeries **Q2,     /**< PTF-Q2 waveform [returned] */
        REAL8TimeSeries **Q3,     /**< PTF-Q2 waveform [returned] */
        REAL8TimeSeries **Q4,     /**< PTF-Q2 waveform [returned] */
        REAL8TimeSeries **Q5,     /**< PTF-Q2 waveform [returned] */
        REAL8TimeSeries *V,       /**< post-Newtonian parameter */
        REAL8TimeSeries *Phi,     /**< orbital phase */
        REAL8TimeSeries *S1x,     /**< Spin1 vector x component */
        REAL8TimeSeries *S1y,     /**< Spin1 vector y component */
        REAL8TimeSeries *S1z,     /**< Spin1 vector z component */
        REAL8TimeSeries *S2x,     /**< Spin2 vector x component */
        REAL8TimeSeries *S2y,     /**< Spin2 vector y component */
        REAL8TimeSeries *S2z,     /**< Spin2 vector z component */
        REAL8TimeSeries *LNhatx,  /**< unit orbital ang. mom. x comp. */
        REAL8TimeSeries *LNhaty,  /**< unit orbital ang. mom. y comp. */
        REAL8TimeSeries *LNhatz,  /**< unit orbital ang. mom. z comp. */
        REAL8TimeSeries *E1x,     /**< orbital plane basis vector x comp. */
        REAL8TimeSeries *E1y,     /**< orbital plane basis vector y comp. */
        REAL8TimeSeries *E1z,     /**< orbital plane basis vector z comp. */
        REAL8 m1,                 /**< mass of companion 1 (kg) */
        REAL8 m2,                 /**< mass of companion 2 (kg) */
        REAL8 r                  /**< distance of source (m) */
        )
{
    REAL8 lnhx, lnhy, lnhz;
    REAL8 e1x, e1y, e1z, e2x, e2y, e2z, nx, ny, nz, lx, ly, lz;
    REAL8 nx2, ny2, nz2, lx2, ly2, lz2;
    REAL8 q1tmp, q2tmp, q3tmp, q4tmp, q5tmp;
    REAL8 M, eta, phi, v, v2, dist, ampfac;
    INT4 idx, len;
    REAL8 sqrt_three = pow(3,0.5);

    /* Macros to check time series vectors */
    LAL_CHECK_VALID_SERIES(V,                   XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(Phi,                 XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S1x,                 XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S1y,                 XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S1z,                 XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S2x,                 XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S2y,                 XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S2z,                 XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(LNhatx,              XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(LNhaty,              XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(LNhatz,              XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(E1x,                 XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(E1y,                 XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(E1z,                 XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, Phi,    XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S1x,    XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S1y,    XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S1z,    XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S2x,    XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S2y,    XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S2z,    XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, LNhatx, XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, LNhaty, XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, LNhatz, XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, E1x,    XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, E1y,    XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, E1z,    XLAL_FAILURE);


    /* Allocate polarization vectors and set to 0 */
    *Q1 = XLALCreateREAL8TimeSeries( "PTF_Q_1", &V->epoch,
            0.0, V->deltaT, &lalStrainUnit, V->data->length );
    *Q2 = XLALCreateREAL8TimeSeries( "PTF_Q_2", &V->epoch,
            0.0, V->deltaT, &lalStrainUnit, V->data->length );
    *Q3 = XLALCreateREAL8TimeSeries( "PTF_Q_3", &V->epoch,
            0.0, V->deltaT, &lalStrainUnit, V->data->length );
    *Q4 = XLALCreateREAL8TimeSeries( "PTF_Q_4", &V->epoch,
            0.0, V->deltaT, &lalStrainUnit, V->data->length );
    *Q5 = XLALCreateREAL8TimeSeries( "PTF_Q_5", &V->epoch,
            0.0, V->deltaT, &lalStrainUnit, V->data->length );

    if ( ! Q1 || ! Q2 || !Q3 || !Q4 || !Q5 )
        XLAL_ERROR(XLAL_EFUNC);
    memset((*Q1)->data->data, 0,
            (*Q1)->data->length*sizeof(*(*Q1)->data->data));
    memset((*Q2)->data->data, 0,
            (*Q2)->data->length*sizeof(*(*Q2)->data->data));
    memset((*Q3)->data->data, 0,
            (*Q3)->data->length*sizeof(*(*Q3)->data->data));
    memset((*Q4)->data->data, 0,
            (*Q4)->data->length*sizeof(*(*Q4)->data->data));
    memset((*Q5)->data->data, 0,
            (*Q5)->data->length*sizeof(*(*Q5)->data->data));

    M = m1 + m2;
    eta = m1 * m2 / M / M; // symmetric mass ratio - '\nu' in the paper
    dist = r / LAL_C_SI;   // r (m) / c (m/s) --> dist in units of seconds
    /* convert mass from kg to s, so ampfac ~ M/dist is dimensionless */
    ampfac = 2. * M * LAL_G_SI * pow(LAL_C_SI, -3) * eta / dist;

    /* loop over time steps and compute the Qi */
    len = V->data->length;
    for(idx = 0; idx < len; idx++)
    {
        /* Abbreviated names in lower case for time series at this sample */
        phi  = Phi->data->data[idx];    v = V->data->data[idx];     v2 = v * v;
        lnhx = LNhatx->data->data[idx]; e1x = E1x->data->data[idx];
        lnhy = LNhaty->data->data[idx]; e1y = E1y->data->data[idx];
        lnhz = LNhatz->data->data[idx]; e1z = E1z->data->data[idx];

        /* E2 = LNhat x E1 */
        e2x = lnhy*e1z - lnhz*e1y;
        e2y = lnhz*e1x - lnhx*e1z;
        e2z = lnhx*e1y - lnhy*e1x;

        /* Unit orbital separation vector */
        nx = e1x*cos(phi) + e2x*sin(phi);
        ny = e1y*cos(phi) + e2y*sin(phi);
        nz = e1z*cos(phi) + e2z*sin(phi);

        /* Unit inst. orbital velocity vector */
        lx = e2x*cos(phi) - e1x*sin(phi);
        ly = e2y*cos(phi) - e1y*sin(phi);
        lz = e2z*cos(phi) - e1z*sin(phi);

        /* Powers of vector components */
        nx2 = nx*nx;    ny2 = ny*ny;    nz2 = nz*nz;
        lx2 = lx*lx;    ly2 = ly*ly;    lz2 = lz*lz;

        /* 
         * NOTE: For PTF waveforms, we must use only the dominant amplitude
         * The Q values are computed from equations 13,14,17, 46 + 47 in PBCV or
         * more simply from equations (3.10) in Diego Fazi's thesis.
         * Note that Q5 is simplified from that shown in Fazi's thsis
         * by using traceless condition. As demonstrated in (6.29)
         * in Ian Harry's thesis.
         */

        q1tmp = lx2 - ly2 - nx2 + ny2;
        q2tmp = 2*lx*ly - 2*nx*ny;
        q3tmp = 2*lx*lz - 2*nx*nz;
        q4tmp = 2*ly*lz - 2*ny*nz;
        q5tmp = sqrt_three * (nz2 - lz2);

        /* Fill the output vectors */
        (*Q1)->data->data[idx] = ampfac * v2 * q1tmp;
        (*Q2)->data->data[idx] = ampfac * v2 * q2tmp;
        (*Q3)->data->data[idx] = ampfac * v2 * q3tmp;
        (*Q4)->data->data[idx] = ampfac * v2 * q4tmp;
        (*Q5)->data->data[idx] = ampfac * v2 * q5tmp;
    }
    return XLAL_SUCCESS;
}

/**
 * Function to specify the desired orientation of a precessing binary in terms
 * of several angles and then compute the vector components in the so-called
 * \"radiation frame\" (with the z-axis along the direction of propagation) as
 * needed for initial conditions for the SpinTaylorT4 waveform routines.
 *
 * Input:
 *     thetaJN, phiJN are angles describing the desired orientation of the
 * total angular momentum (J) relative to direction of propagation (N)
 *     theta1, phi1, theta2, phi2 are angles describing the desired orientation
 * of spin 1 and 2 relative to the Newtonian orbital angular momentum (L_N)
 *     m1, m2, f0 are the component masses and initial GW frequency,
 * they are needed to compute the magnitude of L_N, and thus J
 *     chi1, chi2 are the dimensionless spin magnitudes ( \f$0 \le chi1,2 \le 1\f$),
 * they are needed to compute the magnitude of S1 and S2, and thus J
 *
 * Output:
 *     x, y, z components of LNhat (unit vector along orbital angular momentum),
 *     x, y, z components of E1 (unit vector in the initial orbital plane)
 *     x, y, z components S1 and S2 (unit spin vectors times their
 * dimensionless spin magnitudes - i.e. they have unit magnitude for
 * extremal BHs and smaller magnitude for slower spins)
 *
 * NOTE: Here the \"total\" angular momentum is computed as
 * J = L_N + S1 + S2
 * where L_N is the Newtonian orbital angular momentum. In fact, there are
 * PN corrections to L which contribute to J that are NOT ACCOUNTED FOR
 * in this function. This is done so the function does not need to know about
 * the PN order of the system and to avoid subtleties with spin-orbit
 * contributions to L. Also, it is believed that the difference in Jhat
 * with or without these PN corrections to L is quite small.
 */
int XLALSimInspiralTransformPrecessingInitialConditions(
		REAL8 *LNhatx,	/**< LNhat x component (returned) */
		REAL8 *LNhaty,	/**< LNhat y component (returned) */
		REAL8 *LNhatz,	/**< LNhat z component (returned) */
		REAL8 *E1x,	/**< E1 x component (returned) */
		REAL8 *E1y,	/**< E1 y component (returned) */
		REAL8 *E1z,	/**< E1 z component (returned) */
		REAL8 *S1x,	/**< S1 x component (returned) */
		REAL8 *S1y,	/**< S1 y component (returned) */
		REAL8 *S1z,	/**< S1 z component (returned) */
		REAL8 *S2x,	/**< S2 x component (returned) */
		REAL8 *S2y,	/**< S2 y component (returned) */
		REAL8 *S2z,	/**< S2 z component (returned) */
		REAL8 thetaJN, 	/**< zenith angle between J and N */
		REAL8 phiJN,  	/**< azimuth angle between J and N */
		REAL8 theta1,  	/**< zenith angle between S1 and LNhat */
		REAL8 phi1,  	/**< azimuth angle between S1 and LNhat */
		REAL8 theta2,  	/**< zenith angle between S2 and LNhat */
		REAL8 phi2,  	/**< azimuth angle between S2 and LNhat */
		REAL8 m1,	/**< mass of body 1 (kg) */
		REAL8 m2,	/**< mass of body 2 (kg) */
		REAL8 f0,	/**< initial GW frequency (Hz) */
		REAL8 chi1,	/**< dimensionless spin of body 1 */
		REAL8 chi2	/**< dimensionless spin of body 2 */
		)
{
	REAL8 omega0, M, eta, theta0, phi0, thetaLN, phiLN, Jnorm, tmp1, tmp2;
	REAL8 Jhatx, Jhaty, Jhatz, LNhx, LNhy, LNhz, Jx, Jy, Jz, LNx, LNy, LNz;
	REAL8 s1hatx, s1haty, s1hatz, s2hatx, s2haty, s2hatz;
	REAL8 e1x, e1y, e1z, s1x, s1y, s1z, s2x, s2y, s2z;

	/* Starting frame: LNhat is along the z-axis and the unit 
	   spin vectors are defined from the angles relative to LNhat */
	LNhx = 0.;
	LNhy = 0.;
	LNhz = 1.;
	s1hatx = sin(theta1) * cos(phi1);
	s1haty = sin(theta1) * sin(phi1);
	s1hatz = cos(theta1);
	s2hatx = sin(theta2) * cos(phi2);
	s2haty = sin(theta2) * sin(phi2);
	s2hatz = cos(theta2);

	/* Define several internal variables needed for magnitudes */
	omega0 = LAL_PI * f0; // initial orbital angular frequency
	m1 *= LAL_G_SI / LAL_C_SI / LAL_C_SI / LAL_C_SI;
	m2 *= LAL_G_SI / LAL_C_SI / LAL_C_SI / LAL_C_SI;
	M = m1 + m2;
	eta = m1 * m2 / M / M;
	
	/* Define LN, S1, S2, J with proper magnitudes */
	LNx = LNy = 0.;
	LNz = pow(M, 5./3.) * eta * pow(omega0, -1./3.) * LNhz;
	s1x = m1 * m1 * chi1 * s1hatx;
	s1y = m1 * m1 * chi1 * s1haty;
	s1z = m1 * m1 * chi1 * s1hatz;
	s2x = m2 * m2 * chi2 * s2hatx;
	s2y = m2 * m2 * chi2 * s2haty;
	s2z = m2 * m2 * chi2 * s2hatz;
	Jx = LNx + s1x + s2x;
	Jy = LNy + s1y + s2y;
	Jz = LNz + s1z + s2z;

	/* Normalize J to Jhat, find it's angles in starting frame */
	Jnorm = sqrt( Jx*Jx + Jy*Jy + Jz*Jz);
	Jhatx = Jx / Jnorm;
	Jhaty = Jy / Jnorm;
	Jhatz = Jz / Jnorm;
	theta0 = acos(Jhatz);
	phi0 = atan2(Jhaty, Jhatx);

	/* Rotate about z-axis by -phi0 to put Jhat in x-z plane */
	ROTATEZ(-phi0, LNhx, LNhy, LNhz);
	ROTATEZ(-phi0, s1hatx, s1haty, s1hatz);
	ROTATEZ(-phi0, s2hatx, s2haty, s2hatz);
	ROTATEZ(-phi0, Jhatx, Jhaty, Jhatz);

	/* Rotate about new y-axis by theta0 - thetaJN to put Jhat at
	   desired inclination relative to direction of propagation */
	ROTATEY(theta0 - thetaJN, LNhx, LNhy, LNhz);
	ROTATEY(theta0 - thetaJN, s1hatx, s1haty, s1hatz);
	ROTATEY(theta0 - thetaJN, s2hatx, s2haty, s2hatz);
	ROTATEY(theta0 - thetaJN, Jhatx, Jhaty, Jhatz);

	/* Rotate about new z-axis by phiJN to put J at desired azimuth */
	ROTATEZ(phiJN, LNhx, LNhy, LNhz);
	ROTATEZ(phiJN, s1hatx, s1haty, s1hatz);
	ROTATEZ(phiJN, s2hatx, s2haty, s2hatz);
	ROTATEZ(phiJN, Jhatx, Jhaty, Jhatz);

	/* E1 is at same azimuth as LNhat, but its zenith is an extra pi/2.
	   Same as sin(theta) -> cos(theta), cos(theta) -> -sin(theta) */
	thetaLN = acos(LNhz);
	phiLN = atan2(LNhy, LNhx);
	e1x = cos(thetaLN)*cos(phiLN);
	e1y = cos(thetaLN)*sin(phiLN);
	e1z = -sin(thetaLN);

	/* Multiply spin unit vectors by chi magnitude (but NOT m_i^2) */
	s1hatx *= chi1;
	s1haty *= chi1;
	s1hatz *= chi1;
	s2hatx *= chi2;
	s2haty *= chi2;
	s2hatz *= chi2;

	/* Set pointers to rotated vector components */
	*LNhatx = LNhx;
	*LNhaty = LNhy;
	*LNhatz = LNhz;
	*E1x = e1x;
	*E1y = e1y;
	*E1z = e1z;
	*S1x = s1hatx;
	*S1y = s1haty;
	*S1z = s1hatz;
	*S2x = s2hatx;
	*S2y = s2haty;
	*S2z = s2hatz;

	return XLAL_SUCCESS;
}

/**
 * DEPRECATED: USE XLALSimInspiralChooseTDWaveform() INSTEAD
 *
 * Chooses between different approximants when requesting a waveform to be generated
 * For spinning waveforms, all known spin effects up to given PN order are included
 *
 * The parameters passed must be in SI units.
 */
int XLALSimInspiralChooseWaveform(
    REAL8TimeSeries **hplus,                    /**< +-polarization waveform */
    REAL8TimeSeries **hcross,                   /**< x-polarization waveform */
    REAL8 phi0,                                 /**< peak phase */
    REAL8 deltaT,                               /**< sampling interval */
    REAL8 m1,                                   /**< mass of companion 1 */
    REAL8 m2,                                   /**< mass of companion 2 */
    REAL8 S1x,                                  /**< x-component of the dimensionless spin of object 1 */
    REAL8 S1y,                                  /**< y-component of the dimensionless spin of object 1 */
    REAL8 S1z,                                  /**< z-component of the dimensionless spin of object 1 */
    REAL8 S2x,                                  /**< x-component of the dimensionless spin of object 2 */
    REAL8 S2y,                                  /**< y-component of the dimensionless spin of object 2 */
    REAL8 S2z,                                  /**< z-component of the dimensionless spin of object 2 */
    REAL8 f_min,                                /**< start frequency */
    REAL8 r,                                    /**< distance of source */
    REAL8 i,                                    /**< inclination of source (rad) */
    REAL8 lambda1,                              /**< (tidal deformability of mass 1) / (total mass)^5 (dimensionless) */
    REAL8 lambda2,                              /**< (tidal deformability of mass 2) / (total mass)^5 (dimensionless) */
    LALSimInspiralInteraction interactionFlags, /**< flag to control spin and tidal effects */
    int amplitudeO,                             /**< twice post-Newtonian amplitude order */
    int phaseO,                                 /**< twice post-Newtonian order */
    Approximant approximant                     /**< post-Newtonian approximant to use for waveform production */
    )
{
    XLALPrintDeprecationWarning("XLALSimInspiralChooseWaveform", "XLALSimInspiralChooseTDWaveform");

    return XLALSimInspiralChooseTDWaveform(hplus, hcross, phi0, deltaT, m1, m2, S1x, S1y, S1z, S2x, S2y, S2z, f_min, r, i, lambda1, lambda2, interactionFlags, amplitudeO, phaseO, approximant);
}

/**
 * Chooses between different approximants when requesting a waveform to be generated
 * For spinning waveforms, all known spin effects up to given PN order are included
 * Returns the waveform in the time domain.
 *
 * The parameters passed must be in SI units.
 */
int XLALSimInspiralChooseTDWaveform(
    REAL8TimeSeries **hplus,                    /**< +-polarization waveform */
    REAL8TimeSeries **hcross,                   /**< x-polarization waveform */
    REAL8 phi0,                                 /**< peak phase */
    REAL8 deltaT,                               /**< sampling interval */
    REAL8 m1,                                   /**< mass of companion 1 */
    REAL8 m2,                                   /**< mass of companion 2 */
    REAL8 S1x,                                  /**< x-component of the dimensionless spin of object 1 */
    REAL8 S1y,                                  /**< y-component of the dimensionless spin of object 1 */
    REAL8 S1z,                                  /**< z-component of the dimensionless spin of object 1 */
    REAL8 S2x,                                  /**< x-component of the dimensionless spin of object 2 */
    REAL8 S2y,                                  /**< y-component of the dimensionless spin of object 2 */
    REAL8 S2z,                                  /**< z-component of the dimensionless spin of object 2 */
    REAL8 f_min,                                /**< start frequency */
    REAL8 r,                                    /**< distance of source */
    REAL8 i,                                    /**< inclination of source (rad) */
    REAL8 lambda1,                              /**< (tidal deformability of mass 1) / (total mass)^5 (dimensionless) */
    REAL8 lambda2,                              /**< (tidal deformability of mass 2) / (total mass)^5 (dimensionless) */
    LALSimInspiralInteraction interactionFlags, /**< flag to control spin and tidal effects */
    int amplitudeO,                             /**< twice post-Newtonian amplitude order */
    int phaseO,                                 /**< twice post-Newtonian order */
    Approximant approximant                     /**< post-Newtonian approximant to use for waveform production */
    )
{
    REAL8 LNhatx, LNhaty, LNhatz, E1x, E1y, E1z;
    int ret;
    REAL8 v0 = 1.;

    /* Sanity check the input parameters */
    if( deltaT > 1. )
        XLALPrintWarning("XLAL Warning - %s: Large value of deltaT = %e requested.\nPerhaps sample rate and time step size were swapped?\n", __func__, deltaT);
    if( deltaT < 1./16385. )
        XLALPrintWarning("XLAL Warning - %s: Small value of deltaT = %e requested.\nCheck for errors, this could create very large time series.\n", __func__, deltaT);
    if( m1 < 0.09 * LAL_MSUN_SI )
        XLALPrintWarning("XLAL Warning - %s: Small value of m1 = %e (kg) = %e (Msun) requested.\nPerhaps you have a unit conversion error?\n", __func__, m1, m1/LAL_MSUN_SI);
    if( m2 < 0.09 * LAL_MSUN_SI )
        XLALPrintWarning("XLAL Warning - %s: Small value of m2 = %e (kg) = %e (Msun) requested.\nPerhaps you have a unit conversion error?\n", __func__, m2, m2/LAL_MSUN_SI);
    if( m1 + m2 > 1000. * LAL_MSUN_SI )
        XLALPrintWarning("XLAL Warning - %s: Large value of total mass m1+m2 = %e (kg) = %e (Msun) requested.\nSignal not likely to be in band of ground-based detectors.\n", __func__, m1+m2, (m1+m2)/LAL_MSUN_SI);
    if( S1x*S1x + S1y*S1y + S1z*S1z > 1.000001 )
        XLALPrintWarning("XLAL Warning - %s: S1 = (%e,%e,%e) with norm > 1 requested.\nAre you sure you want to violate the Kerr bound?\n", __func__, S1x, S1y, S1z);
    if( S2x*S2x + S2y*S2y + S2z*S2z > 1.000001 )
        XLALPrintWarning("XLAL Warning - %s: S2 = (%e,%e,%e) with norm > 1 requested.\nAre you sure you want to violate the Kerr bound?\n", __func__, S2x, S2y, S2z);
    if( f_min < 1. )
        XLALPrintWarning("XLAL Warning - %s: Small value of fmin = %e requested.\nCheck for errors, this could create a very long waveform.\n", __func__, f_min);
    if( f_min > 40.000001 )
        XLALPrintWarning("XLAL Warning - %s: Large value of fmin = %e requested.\nCheck for errors, the signal will start in band.\n", __func__, f_min);

    switch (approximant)
    {
        /* non-spinning inspiral-only models */
        case TaylorEt:
            ret = XLALSimInspiralTaylorEtPNGenerator(hplus, hcross, phi0, v0, deltaT, m1, m2, f_min, r, i, amplitudeO, phaseO);
            break;
        case TaylorT1:
            ret = XLALSimInspiralTaylorT1PNGenerator(hplus, hcross, phi0, v0, deltaT, m1, m2, f_min, r, i, amplitudeO, phaseO);
            break;
        case TaylorT2:
            ret = XLALSimInspiralTaylorT2PNGenerator(hplus, hcross, phi0, v0, deltaT, m1, m2, f_min, r, i, amplitudeO, phaseO);
            break;
        case TaylorT3:
            ret = XLALSimInspiralTaylorT3PNGenerator(hplus, hcross, phi0, v0, deltaT, m1, m2, f_min, r, i, amplitudeO, phaseO);
            break;
        case TaylorT4:
            ret = XLALSimInspiralTaylorT4PNGenerator(hplus, hcross, phi0, v0, deltaT, m1, m2, f_min, r, i, amplitudeO, phaseO);
            break;

        /* non-spinning inspiral-merger-ringdown models */
        case IMRPhenomA:
            // FIXME: decide proper f_max to pass here
            ret = XLALSimIMRPhenomAGenerateTD(hplus, hcross, phi0, deltaT, m1, m2, f_min, .5/deltaT, r, i);
            break;
        case EOBNRv2HM:
            // FIXME: need to create a function to take in different modes or produce an error if all modes not given
            ret = XLALSimIMREOBNRv2AllModes(hplus, hcross, phi0, deltaT, m1, m2, f_min, r, i);
            break;
        case EOBNRv2:
            ret = XLALSimIMREOBNRv2DominantMode(hplus, hcross, phi0, deltaT, m1, m2, f_min, r, i);
            break;

        /* spinning inspiral-only models */

        // need to make a consistent choice for SpinTaylorT4 and PSpinInspiralRD waveform inputs
        // proposal: TotalJ frame of PSpinInspiralRD
        // inclination denotes the angle between the view directoin 
        // and J (J is constant during the evolution, J//z, both N and initial 
		// L are in the x-z plane) and the spin coordinates are given wrt 
		// initial ** L **.
        case SpinTaylorT4:
            LNhatx = sin(i);
            LNhaty = 0.;
            LNhatz = cos(i);
            E1x = cos(i);
            E1y = 0.;
            E1z = - sin(i);
            /* Maximum PN amplitude order for precessing waveforms is MAX_PRECESSING_AMP_PN_ORDER */
            amplitudeO = amplitudeO <= MAX_PRECESSING_AMP_PN_ORDER ? amplitudeO : MAX_PRECESSING_AMP_PN_ORDER;
            ret = XLALSimInspiralSpinTaylorT4(hplus, hcross, phi0, v0, deltaT, m1, m2, f_min, r, S1x, S1y, S1z, S2x, S2y, S2z, LNhatx, LNhaty, LNhatz, E1x, E1y, E1z, lambda1, lambda2, interactionFlags, phaseO, amplitudeO);
            break;

        /* spinning inspiral-merger-ringdown models */
        case IMRPhenomB:
            ret = XLALSimIMRPhenomBGenerateTD(hplus, hcross, phi0, deltaT, m1, m2, XLALSimIMRPhenomBComputeChi(m1, m2, S1z, S2z), f_min, .5/deltaT, r, i);
            break;
        case PhenSpinTaylorRD:
            // FIXME: need to create a function to take in different modes or produce an error if all modes not given
	  ret = XLALSimIMRPSpinInspiralRDGenerator(hplus, hcross, phi0, deltaT, m1, m2, f_min, r, i, S1x, S1y, S1z, S2x, S2y, S2z, phaseO, TotalJ, 0);
            break;
        case SEOBNRv1:
            ret = XLALSimIMRSpinAlignedEOBWaveform(hplus, hcross, phi0, deltaT, m1, m2, f_min, r, i, S1z, S2z);
            break;

        default:
            XLALPrintError("TD version of approximant not implemented in lalsimulation\n");
            XLAL_ERROR(XLAL_EINVAL);
    }

    if (ret == XLAL_FAILURE)
        XLAL_ERROR(XLAL_EFUNC);

    return ret;
}

/**
 * Chooses between different approximants when requesting a waveform to be generated
 * For spinning waveforms, all known spin effects up to given PN order are included
 * Returns the waveform in the frequency domain.
 */
int XLALSimInspiralChooseFDWaveform(
    COMPLEX16FrequencySeries **htilde,          /**< FD waveform */
    REAL8 phi0,                                 /**< peak phase */
    REAL8 deltaF,                               /**< sampling interval */
    REAL8 m1,                                   /**< mass of companion 1 */
    REAL8 m2,                                   /**< mass of companion 2 */
    UNUSED REAL8 S1x,                           /**< x-component of the dimensionless spin of object 1 */
    UNUSED REAL8 S1y,                           /**< y-component of the dimensionless spin of object 1 */
    REAL8 S1z,                                  /**< z-component of the dimensionless spin of object 1 */
    UNUSED REAL8 S2x,                           /**< x-component of the dimensionless spin of object 2 */
    UNUSED REAL8 S2y,                           /**< y-component of the dimensionless spin of object 2 */
    REAL8 S2z,                                  /**< z-component of the dimensionless spin of object 2 */
    REAL8 f_min,                                /**< start frequency */
    REAL8 f_max,                                /**< end frequency */
    REAL8 r,                                    /**< distance of source */
    UNUSED REAL8 i,                             /**< inclination of source (rad) */
    REAL8 lambda1,                              /**< (tidal deformability of mass 1) / (total mass)^5 (dimensionless) */
    REAL8 lambda2,                              /**< (tidal deformability of mass 2) / (total mass)^5 (dimensionless) */
    UNUSED LALSimInspiralInteraction interactionFlags, /**< flag to control spin and tidal effects */
    int amplitudeO,                             /**< twice post-Newtonian amplitude order */
    int phaseO,                                 /**< twice post-Newtonian order */
    Approximant approximant                     /**< post-Newtonian approximant to use for waveform production */
    )
{
    int ret;

    /* Sanity check the input parameters */
    if( deltaF > 1. )
        XLALPrintWarning("XLAL Warning - %s: Large value of deltaF = %e requested.\nThis corresponds to a very short TD signal (with padding). Consider a smaller value.\n", __func__, deltaF);
    if( deltaF < 1./4096. )
        XLALPrintWarning("XLAL Warning - %s: Small value of deltaF = %e requested.\nThis corresponds to a very long TD signal. Consider a larger value.\n", __func__, deltaF);
    if( m1 < 0.09 * LAL_MSUN_SI )
        XLALPrintWarning("XLAL Warning - %s: Small value of m1 = %e (kg) = %e (Msun) requested.\nPerhaps you have a unit conversion error?\n", __func__, m1, m1/LAL_MSUN_SI);
    if( m2 < 0.09 * LAL_MSUN_SI )
        XLALPrintWarning("XLAL Warning - %s: Small value of m2 = %e (kg) = %e (Msun) requested.\nPerhaps you have a unit conversion error?\n", __func__, m2, m2/LAL_MSUN_SI);
    if( m1 + m2 > 1000. * LAL_MSUN_SI )
        XLALPrintWarning("XLAL Warning - %s: Large value of total mass m1+m2 = %e (kg) = %e (Msun) requested.\nSignal not likely to be in band of ground-based detectors.\n", __func__, m1+m2, (m1+m2)/LAL_MSUN_SI);
    if( S1x*S1x + S1y*S1y + S1z*S1z > 1.000001 )
        XLALPrintWarning("XLAL Warning - %s: S1 = (%e,%e,%e) with norm > 1 requested.\nAre you sure you want to violate the Kerr bound?\n", __func__, S1x, S1y, S1z);
    if( S2x*S2x + S2y*S2y + S2z*S2z > 1.000001 )
        XLALPrintWarning("XLAL Warning - %s: S2 = (%e,%e,%e) with norm > 1 requested.\nAre you sure you want to violate the Kerr bound?\n", __func__, S2x, S2y, S2z);
    if( f_min < 1. )
        XLALPrintWarning("XLAL Warning - %s: Small value of fmin = %e requested.\nCheck for errors, this could create a very long waveform.\n", __func__, f_min);
    if( f_min > 40.000001 )
        XLALPrintWarning("XLAL Warning - %s: Large value of fmin = %e requested.\nCheck for errors, the signal will start in band.\n", __func__, f_min);

    switch (approximant)
    {
        /* non-spinning inspiral-only models */
        case TaylorF2:
            ret = XLALSimInspiralTaylorF2(htilde, phi0, deltaF, m1, m2, f_min, r, phaseO);
            break;

        /* non-spinning inspiral-merger-ringdown models */
        case IMRPhenomA:
            // FIXME: decide proper f_max to pass here
            ret = XLALSimIMRPhenomAGenerateFD(htilde, phi0, deltaF, m1, m2, f_min, f_max, r);
            break;

        /* spinning inspiral-only models */
        case TaylorF2RedSpin:
            ret = XLALSimInspiralTaylorF2ReducedSpin(htilde, phi0, deltaF, m1, m2, XLALSimIMRPhenomBComputeChi(m1, m2, S1z, S2z), f_min, r, phaseO, amplitudeO);
            break;
        case TaylorF2RedSpinTidal:
            ret = XLALSimInspiralTaylorF2ReducedSpinTidal(htilde, phi0, deltaF, m1, m2, XLALSimIMRPhenomBComputeChi(m1, m2, S1z, S2z), lambda1, lambda2, f_min, r, phaseO, amplitudeO);
            break;

        /* spinning inspiral-merger-ringdown models */
        case IMRPhenomB:
            ret = XLALSimIMRPhenomBGenerateFD(htilde, phi0, deltaF, m1, m2, XLALSimIMRPhenomBComputeChi(m1, m2, S1z, S2z), f_min, f_max, r);
            break;

        default:
            XLALPrintError("FD version of approximant not implemented in lalsimulation\n");
            XLAL_ERROR(XLAL_EINVAL);
    }

    if (ret == XLAL_FAILURE)
        XLAL_ERROR(XLAL_EFUNC);

    return ret;
}


/**
 * Checks whether the given approximant is implemented in lalsimulation's XLALSimInspiralChooseTDWaveform().
 *
 * returns 1 if the approximant is implemented, 0 otherwise.
 */
int XLALSimInspiralImplementedTDApproximants(
    Approximant approximant /**< post-Newtonian approximant for use in waveform production */
    )
{
    switch (approximant)
    {
        case TaylorEt:
        case TaylorT1:
        case TaylorT2:
        case TaylorT3:
        case TaylorT4:
        case EOBNRv2:
        case IMRPhenomA:
        case EOBNRv2HM:
        case SpinTaylorT4:
        case IMRPhenomB:
        case PhenSpinTaylorRD:
        case SEOBNRv1:
            return 1;

        default:
            return 0;
    }
}

/**
 * Checks whether the given approximant is implemented in lalsimulation's XLALSimInspiralChooseFDWaveform().
 *
 * returns 1 if the approximant is implemented, 0 otherwise.
 */
int XLALSimInspiralImplementedFDApproximants(
    Approximant approximant /**< post-Newtonian approximant for use in waveform production */
    )
{
    switch (approximant)
    {
        case IMRPhenomA:
        case IMRPhenomB:
        case TaylorF2:
        case TaylorF2RedSpin:
        case TaylorF2RedSpinTidal:
            return 1;

        default:
            return 0;
    }
}

/** 
 * XLAL function to determine approximant from a string.  The string need not 
 * match exactly, only contain a member of the Approximant enum.
 */
int XLALGetApproximantFromString(const CHAR *inString)
{
#ifndef LAL_NDEBUG
  if ( !inString )
    XLAL_ERROR( XLAL_EFAULT );
#endif

  if ( strstr(inString, "TaylorF2RedSpinTidal" ) )
  {
    return TaylorF2RedSpinTidal;
  }
  else if ( strstr(inString, "TaylorF2RedSpin" ) )
  {
    return TaylorF2RedSpin;
  }
  else if ( strstr(inString, "TaylorF2" ) )
  {
    return TaylorF2;
  }
  else if ( strstr(inString, "PhenSpinTaylorRDF" ) )
  {
    return PhenSpinTaylorRDF;
  }
  else if ( strstr(inString, "PhenSpinTaylorRD" ) )
  {
    return PhenSpinTaylorRD;
  }
  else if ( strstr(inString, "SpinTaylorT4" ) )
  {
    return SpinTaylorT4;
  }
  else if ( strstr(inString, "SpinTaylorFrameless" ) )
  {
    return SpinTaylorFrameless;
  }
  else if ( strstr(inString, "SpinTaylorT3" ) )
  {
    return SpinTaylorT3;
  }
  else if ( strstr(inString, "SpinTaylor" ) )
  {
    return SpinTaylor;
  }
  else if ( strstr(inString, "SpinQuadTaylor" ) )
  {
    return SpinQuadTaylor;
  }
  else if ( strstr(inString, "TaylorT1" ) )
  {
    return TaylorT1;
  }
  else if ( strstr(inString, "TaylorT2" ) )
  {
    return TaylorT2;
  }
  else if ( strstr(inString, "TaylorT3" ) )
  {
    return TaylorT3;
  }
  else if ( strstr(inString, "TaylorT4" ) )
  {
    return TaylorT4;
  }
  else if ( strstr(inString, "IMRPhenomA" ) )
  {
    return IMRPhenomA;
  }
  else if ( strstr(inString, "IMRPhenomB" ) )
  {
    return IMRPhenomB;
  }
  else if ( strstr(inString, "IMRPhenomFA" ) )
  {
    return IMRPhenomFA;
  }
  else if ( strstr(inString, "IMRPhenomFB" ) )
  {
    return IMRPhenomFB;
  }
  else if ( strstr(inString, "SEOBNRv1" ) )
  {
    return SEOBNRv1;
  }
  else if ( strstr(inString, "EOBNRv2HM" ) )
  {
    return EOBNRv2HM;
  }
  else if ( strstr(inString, "EOBNRv2" ) )
  {
    return EOBNRv2;
  }
  else if ( strstr(inString, "EOBNR" ) )
  {
    return EOBNR;
  }
  else if ( strstr(inString, "EOB" ) )
  {
    return EOB;
  }
  else if ( strstr(inString, "AmpCorPPN" ) )
  {
    return AmpCorPPN;
  }
  else if ( strstr(inString, "GeneratePPN" ) )
  {
    return GeneratePPN;
  }
  else if ( strstr(inString, "NumRelNinja2" ) )
  {
    return NumRelNinja2;
  }
  else if ( strstr(inString, "NumRel" ) )
  {
    return NumRel;
  }
  else if ( strstr(inString, "Ninja2" ) )
  {
    return NumRelNinja2;
  }
  else if ( strstr(inString, "FindChirpSP" ) )
  {
    return FindChirpSP;
  }
  else if ( strstr(inString, "FindChirpPTF" ) )
  {
    return FindChirpPTF;
  }
  else if ( strstr(inString, "TaylorEt" ) )
  {
    return TaylorEt;
  }
  else if ( strstr(inString, "TaylorN" ) )
  {
    return TaylorN;
  }
  else if ( strstr(inString, "TaylorF1" ) )
  {
    return TaylorF1;
  }
  else if ( strstr(inString, "PadeT1" ) )
  {
    return PadeT1;
  }
  else if ( strstr(inString, "PadeF1" ) )
  {
    return PadeF1;
  }
  else if ( strstr(inString, "BCVSpin" ) )
  {
    return BCVSpin;
  }
  else if ( strstr(inString, "BCVC" ) )
  {
    return BCVC;
  }
  else if ( strstr(inString, "BCV" ) )
  {
    return BCV;
  }
  else if ( strstr(inString, "FrameFile" ) )
  {
    return FrameFile;
  }
  else if ( strstr(inString, "Eccentricity" ) )
  {
    return Eccentricity;
  }
  else
  {
    XLALPrintError( "Cannot parse approximant from string: %s \n", inString );
    XLAL_ERROR( XLAL_EINVAL );
  }
}

/** 
 * XLAL function to determine PN order from a string.  The string need not 
 * match exactly, only contain a member of the LALPNOrder enum.
 */
int XLALGetOrderFromString(const CHAR *inString)
{

#ifndef LAL_NDEBUG
  if ( !inString )
    XLAL_ERROR( XLAL_EFAULT );
#endif

  if ( strstr(inString, "newtonian") )
  {
    return LAL_PNORDER_NEWTONIAN;
  }
  else if ( strstr(inString, "oneHalfPN") )
  {
    return LAL_PNORDER_HALF;
  }
  else if ( strstr(inString, "onePN") )
  {
    return LAL_PNORDER_ONE;
  }
  else if ( strstr(inString, "onePointFivePN") )
  {
    return LAL_PNORDER_ONE_POINT_FIVE;
  }
  else if ( strstr(inString, "twoPN") )
  {
    return LAL_PNORDER_TWO;
  }
  else if ( strstr(inString, "twoPointFivePN") )
  {
    return LAL_PNORDER_TWO_POINT_FIVE;
  }
  else if (strstr(inString, "threePN") )
  {
    return LAL_PNORDER_THREE;
  }
  else if ( strstr(inString, "threePointFivePN") )
  {
    return LAL_PNORDER_THREE_POINT_FIVE;
  }
  else if ( strstr(inString, "pseudoFourPN") )
  {
    return LAL_PNORDER_PSEUDO_FOUR;
  }
  else
  {
    XLALPrintError( "Cannot parse order from string: %s\n", inString );
    XLAL_ERROR( XLAL_EINVAL );
  }
}

/** 
 * XLAL function to determine tapering flag from a string.  The string must 
 * match exactly with a member of the LALSimInspiralApplyTaper enum. 
 */
int XLALGetTaperFromString(const CHAR *inString)
{
  if ( ! strcmp( "TAPER_NONE", inString ) )
  {
    return LAL_SIM_INSPIRAL_TAPER_NONE;
  }
  else if ( ! strcmp( "TAPER_START", inString ) )
  {
    return LAL_SIM_INSPIRAL_TAPER_START;
  }
  else if ( ! strcmp( "TAPER_END", inString ) )
  {
    return LAL_SIM_INSPIRAL_TAPER_END;
  }
  else if ( ! strcmp( "TAPER_STARTEND", inString ) )
  {
    return LAL_SIM_INSPIRAL_TAPER_STARTEND;
  }
  else
  {
    XLALPrintError( "Invalid injection tapering option specified: %s\n", inString );
    XLAL_ERROR( XLAL_EINVAL );
  }
}

/** 
 * XLAL function to determine LALSimInspiralInteraction from a string.
 *
 * TODO: return the bit sum if the string is a concatenation of several 
 * interaction terms. Also make names match cases of enum.
 */
int XLALGetInteractionFromString(const CHAR *inString) 
{
  if (strstr(inString, "NO")) {
    return LAL_SIM_INSPIRAL_INTERACTION_NONE;
  } else if (strstr(inString, "SO15")) {
    return LAL_SIM_INSPIRAL_INTERACTION_SPIN_ORBIT_15PN;
  } else if (strstr(inString,"SS")) {
    return LAL_SIM_INSPIRAL_INTERACTION_SPIN_SPIN_2PN;
  } else if (strstr(inString,"SELF")) {
    return LAL_SIM_INSPIRAL_INTERACTION_SPIN_SPIN_SELF_2PN;
  } else if (strstr(inString, "QM")) {
    return LAL_SIM_INSPIRAL_INTERACTION_QUAD_MONO_2PN;
  } else if (strstr(inString, "SO25")) {
    return LAL_SIM_INSPIRAL_INTERACTION_SPIN_ORBIT_25PN;
  } else if (strstr(inString, "SO")) {
    return LAL_SIM_INSPIRAL_INTERACTION_SPIN_ORBIT_3PN;
  } else if (strstr(inString, "ALL_SPIN")) {
    return LAL_SIM_INSPIRAL_INTERACTION_ALL_SPIN;
  } else if (strstr(inString, "TIDAL5PN")) {
    return LAL_SIM_INSPIRAL_INTERACTION_TIDAL_5PN;
  } else if (strstr(inString, "TIDAL")) {
    return LAL_SIM_INSPIRAL_INTERACTION_TIDAL_6PN;
  } else if (strstr(inString, "ALL")){
    return LAL_SIM_INSPIRAL_INTERACTION_ALL;
  } else {
    XLALPrintError( "Cannot parse LALSimInspiralInteraction from string: %s\n Please add 'ALL' to the above string for including all spin interactions\n", inString );
    XLAL_ERROR( XLAL_EINVAL );
  }
}

/** 
 * XLAL function to determine axis choice flag from a string.  The string need 
 * not match exactly, only contain a member of the FrameAxis enum.  Will return
 * default case 'View' (line of sight) if the string contains no match.
 */
int XLALGetFrameAxisFromString(const CHAR *inString) 
{
  if (strstr(inString, "TotalJ"))
    return TotalJ;
  else if (strstr(inString, "OrbitalL"))
    return OrbitalL;
  else
    return View;
}

/** 
 * XLAL function to determine adaptive integration flag from a string.  Returns
 * 1 if string contains 'fixedStep', otherwise returns 0 to signal 
 * adaptive integration should be used.
 */
int XLALGetAdaptiveIntFromString(const CHAR *inString) 
{
  if (strstr(inString, "fixedStep"))
    return 1;
  else 
    return 0;
}

/** 
 * XLAL function to determine inspiral-only flag from a string.  Returns
 * 1 if string contains 'inspiralOnly', otherwise returns 0 to signal 
 * full inspiral-merger-ringdown waveform should be generated.
 */
int XLALGetInspiralOnlyFromString(const CHAR *inString)
{
  if (strstr(inString, "inspiralOnly"))
    return 1;
  else
    return 0;
}

