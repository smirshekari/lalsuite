/**** <lalVerbatim file="FrameCalibrationHV">
 * Author: Brown, D. A.
 * $Id$
 **** </lalVerbatim> */

/**** <lalLaTeX>
 *
 * \section{Header \texttt{FrameCalibration.h}}
 *
 * High-level routines for exctracting calibration data from frames
 *
 * \subsection*{Synopsis}
 * \begin{verbatim}
 * #include <lal/FrameCalibration.h>
 * \end{verbatim}
 *
 * Provides a high level interface for building a transfer or response 
 * functions from raw calibration data provided by the calibration team.
 *
 **** </lalLaTeX> */

#include <lal/LALDatatypes.h>

#ifndef _FRAMECALIBRATION_H
#define _FRAMECALIBRATION_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif

NRCSID( FRAMECALIBRATIONH, "$Id$" );

/**** <lalLaTeX>
 *
 * \subsection*{Error conditions}
 *
 * These error conditions are generated by the function
 * \verb+ExtractFrameCalibration()+ if it encounters an error.
 * If codes 1 through 7 are returned, the no calibration has
 * been generated since there was an error reading the reference
 * calibration. If an error occours once the reference calibration
 * has been read in, then the reference calibration is returned
 * without being updated. This allows the user the option to 
 * trap the error and fall back on the reference calibration or 
 * to give up completely. This can be done in the case of error
 * code 8, or error code $-1$, which indicates an error in one
 * of the functions used to update the reference calibration.
 *
 **** </lalLaTeX> */
/**** <lalErrTable> */
#define FRAMECALIBRATIONH_ENULL 1
#define FRAMECALIBRATIONH_ENNUL 2
#define FRAMECALIBRATIONH_EMCHE 3
#define FRAMECALIBRATIONH_ECREF 4
#define FRAMECALIBRATIONH_EOREF 5
#define FRAMECALIBRATIONH_EDCHE 6
#define FRAMECALIBRATIONH_EREFR 7
#define FRAMECALIBRATIONH_ECFAC 8
#define FRAMECALIBRATIONH_EDTMM 9

#define FRAMECALIBRATIONH_MSGENULL "No calibration generated: Null pointer"
#define FRAMECALIBRATIONH_MSGENNUL "No calibration generated: Non-null pointer"
#define FRAMECALIBRATIONH_MSGEMCHE "No calibration generated: unable to open calibration cache file"
#define FRAMECALIBRATIONH_MSGECREF "No calibration generated: no reference calibration in cache"
#define FRAMECALIBRATIONH_MSGEOREF "No calibration generated: unable to open reference frame"
#define FRAMECALIBRATIONH_MSGEDCHE "No calibration generated: error reference calibration cache"
#define FRAMECALIBRATIONH_MSGEREFR "No calibration generated: error reading refernce calibration"
#define FRAMECALIBRATIONH_MSGECFAC "Calibration not updated: no update factor frames in cache"
#define FRAMECALIBRATIONH_MSGEDTMM "Calibration not updated: mismatch between sample rate of alpha and alpha*beta"
/**** </lalErrTable> */

/**** <lalLaTeX>
 *
 * \subsection*{Structures}
 *
 **** </lalLaTeX> */

/**** <lalLaTeX>
 * 
 * \vfill{\footnotesize\input{FrameCalibrationHV}}
 * \newpage\input{FrameCalibrationC}
 * \newpage\input{FrameCalibrationTestC}
 *
 **** </lalLaTeX> */

void
LALExtractFrameResponse(
    LALStatus               *status,
    COMPLEX8FrequencySeries *output,
    const CHAR              *catalog,
    const CHAR              *ifo,
    LIGOTimeGPS		    *duration
    );

#ifdef __cplusplus
#pragma {
}
#endif

#endif /* _FRAMECALIBRATION_H */
