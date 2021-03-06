/*
 * Copyright (C) 2011 David Keitel
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

/**
 * \defgroup RecalcToplistStats_h Header RecalcToplistStats.h
 * \ingroup pkg_pulsarCommon
 * \author David Keitel
 *
 * \brief Functions to recompute statistics for GCT/hough toplist entries
 */
/*@{*/

#ifndef _RECALCTOPLISTSTATS_H  /* Double-include protection. */
#define _RECALCTOPLISTSTATS_H

/* C++ protection. */
#ifdef  __cplusplus
extern "C" {
#endif

/*---------- exported INCLUDES ----------*/

/* lal includes */
#include <lal/ExtrapolatePulsarSpins.h>
#include <lal/ComputeFstat.h>
#include <lal/StringVector.h>
#include <lal/LineRobustStats.h>

/* additional includes */
#include "GCTtoplist.h"
#include "../hough/src2/HoughFStatToplist.h"

/*---------- exported DEFINES ----------*/

/*---------- exported types ----------*/

/*---------- exported Global variables ----------*/
/* empty init-structs for the types defined in here */

/*---------- exported prototypes [API] ----------*/
int
XLALComputeExtraStatsForToplist ( toplist_t *list,
				  const char *listEntryTypeName,
				  const FstatInputDataVector *Fstat_in_vec,
				  const LALStringVector *detectorIDs,
				  const LIGOTimeGPSVector *startTstack,
				  const LIGOTimeGPS refTimeGPS,
				  const char* outputSingleSegStats );

int
XLALComputeExtraStatsSemiCoherent ( LVcomponents *lineVeto,
				    const PulsarDopplerParams *dopplerParams,
				    const FstatInputDataVector *Fstat_in_vec,
				    const LALStringVector *detectorIDs,
				    const LIGOTimeGPSVector *startTstack,
				    FILE *singleSegStatsFile );

// @}

#ifdef  __cplusplus
}
#endif
/* C++ protection. */

#endif  /* Double-include protection. */
