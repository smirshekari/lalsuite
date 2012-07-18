# Copyright (C) 2012  Matthew West
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
# Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.


#
# =============================================================================
#
#                                   Preamble
#
# =============================================================================
#

"""
A collection of functions needed for ligolw_cbc_compute_durations
"""

import sys
import sqlite3
from operator import itemgetter

from glue import iterutils
from glue import segments
from glue.ligolw import lsctables
from glue.ligolw import dbtables
from glue.ligolw.utils import segments as ligolw_segments

#
# =============================================================================
#
#                                  Functions
#
# =============================================================================
#

def get_single_ifo_segments(connection, program_name = "inspiral", usertag = None):
	"""
	Return a glue.segments.segmentlistdict object containing coalesced single-ifo
	segments obtained from the search_summary table.

	@param connection: sqlite connection for the input database
	@param usertag: the usertag for the desired filter jobs e.g. ("FULL_DATA","PLAYGROUND")
	"""

	xmldoc = dbtables.get_xml(connection)
	seglist_dict = segments.segmentlistdict()
	# extract segments indexed by available instrument
	for row in map( 
		dbtables.table.get_table(xmldoc, lsctables.SearchSummaryTable.tableName).row_from_cols, 
		connection.cursor().execute("""
			SELECT search_summary.*
			FROM search_summary
				JOIN process_params ON (
					process_params.process_id == search_summary.process_id)
			WHERE
				process_params.value == :1
				AND process_params.program == :2
		""", (usertag, program_name) )):

		instrument = row.get_ifos().pop()
		if usertag == "FULL_DATA" or usertag == "PLAYGROUND":
			try:
				seglist_dict[instrument].append(row.get_out())
			except KeyError:
				seglist_dict[instrument] = [row.get_out()]
		else:
			buffer_time = 72
			filtered_segment = segments.segment(row.get_in()[0]+buffer_time, row.get_in()[1]-buffer_time)
			try:
				seglist_dict[instrument].append(filtered_segment)
			except KeyError:
				seglist_dict[instrument] = [filtered_segment]
			
	xmldoc.unlink()

	seglist_dict = segments.segmentlistdict((key, segments.segmentlist(sorted(set(value)))) for key, value in seglist_dict.items())
	return seglist_dict.coalesce()

def get_allifo_combos(ifo_keys, min_num_ifos):
	"""
	Returns two dictionaries, one of the on instruments and the other of the excluded 
	instruments in a given coincident time type e.g. (H1,L1; H1,L1,V1). The dictionary
	keys are in the same format as the instruments column of the experiment table.

	@param ifo_keys: an sorted list of single ifos as strings
	@param min_num_ifos: the minimum number of ifos in a combination
	"""
	on_ifos = {}
	excluded_ifos = {}
	ifo_set = set(ifo_keys)
	for num_ifos in range(min_num_ifos, len(ifo_set)+1):
		for sub_combo in iterutils.choices( list(ifo_set), num_ifos):
			sorted_on_list = sorted(sub_combo)
			on_ifos[','.join(sorted_on_list)] = sorted_on_list
			excluded_ifos[','.join(sorted_on_list)] = list(ifo_set - set(sorted_on_list))

	return on_ifos, excluded_ifos


def get_veto_segments(connection, name):
	"""
	Return a coalesced glue.segments.segmentlistdict object containing the
	segments of the given name extracted from the database at the given
	connection.
	"""
	xmldoc = dbtables.get_xml(connection)
	seglists = ligolw_segments.segmenttable_get_by_name(xmldoc, name).coalesce()
	xmldoc.unlink()
	return seglists

def get_vetosegs_allcats(connection, verbose):
	"""
	Return a dictionary of glue.segments.segmentlistdict objects containing 
	veto segments and the keys being the associated veto-definer name.
	"""

	sqlquery = """
	SELECT DISTINCT veto_def_name
	FROM experiment_summary
	"""
	veto_segments = {}
	for veto_def_name in connection.cursor().execute(sqlquery).fetchall():
		veto_def_name = veto_def_name[0]
		if verbose:
			print >>sys.stderr, "Retrieving veto segments for %s..." % veto_def_name
		try:
			veto_segments[veto_def_name] = get_veto_segments(connection, veto_def_name)
		except AttributeError:
			# will get an AttributeError if using newer format veto segment file because
			# the new format does not include _ns; if so, remove the _ns columns from the
			# segment table and reset the definitions of lsctables.Segment.get and lsctables.Segment.set
			from glue.lal import LIGOTimeGPS
		
			del lsctables.SegmentTable.validcolumns['start_time_ns']
			del lsctables.SegmentTable.validcolumns['end_time_ns']
		
			def get_segment(self):
				"""
				Return the segment described by this row.
				"""
				return segments.segment(LIGOTimeGPS(self.start_time, 0), LIGOTimeGPS(self.end_time, 0))
		
			def set_segment(self, segment):
				"""
				Set the segment described by this row.
				"""
				self.start_time = segment[0].seconds
				self.end_time = segment[1].seconds
		
			lsctables.Segment.get = get_segment
			lsctables.Segment.set = set_segment
		
			veto_segments[veto_def_name] = db_thinca_rings.get_veto_segments(connection, veto_def_name)

	return veto_segments

def get_coinc_segments(segments_dict, time_slide_dict):
	"""
	Return the exclusive coinc segments for each (time-slide,on instruments) pair for
	time-slides done along a line (as opposed to rings).

	@param segments_dict: the glue.segments.segmentlistdict object which contains the 
		single-ifo segments used to compute experiment durations
	@param time_slide_dict: dictionary object where the time_slide_id is the key for
		the corresponding offsetvector
	"""
	segments_dict.coalesce()
	on_ifos_dict, excluded_ifos_dict = get_allifo_combos(segments_dict, 2)
	# the keys for the coinc-segs dictionaries are the TS-ids & ifo-combos
	coinc_segs = {}

	for time_slide_id, offset_vec in time_slide_dict.items():
		# shift the segment times according to the values in the offset vector
		tsid = str(time_slide_id)
		for ifo, shift in offset_vec.items():
			segments_dict.offsets[ifo] = shift

		for on_ifos_key, combo in on_ifos_dict.items():
			# determine inclusive coincident segments for each time_slide
			coinc_segs[tsid, on_ifos_key] = segments_dict.intersection( combo )
			
			# get lists of excluded ifos and associated keys for this coinc-time type
			excluded_ifos = excluded_ifos_dict[on_ifos_key]
			if len(excluded_ifos) > 0:
				# Make exclusive segments
				coinc_segs[tsid, on_ifos_key] -= segments_dict.union( excluded_ifos )

			coinc_segs[tsid, on_ifos_key].coalesce()

	return coinc_segs

def get_livetimes(segments_dict, time_slide_dict, verbose = False):
	"""
	Obtain the live-times for each set of coincident segments grouped by
	time_slide_id and on-ifos.
	"""
	livetimes = {}
	# determine coincident segments by (time-slide, on-ifos) pair
	coinc_segs = get_coinc_segments(segments_dict, time_slide_dict)

	# calculate the livetime for each pair
	for key, segments_list in coinc_segs.items():
		livetimes[key] = float( abs(segments_list) )

	return livetimes