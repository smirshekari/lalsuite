## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/src/lalapps -DPKG_DATA_DIR='"$(pkgdatadir)/"' @AM_CPPFLAGS@
LDADD = $(top_builddir)/src/lalapps/liblalapps.la

bin_PROGRAMS =  lalapps_skynet \
		lalapps_getMetric \
		lalapps_getMesh \
		lalapps_FstatMetric_v2

lalapps_skynet_SOURCES = skynet.c

lalapps_getMetric_SOURCES = getMetric.c

lalapps_getMesh_SOURCES = getMesh.c

lalapps_FstatMetric_v2_SOURCES = FstatMetric_v2.c


EXTRA_DIST =

TESTS =

TESTS_ENVIRONMENT = \
	LAL_DEBUG_LEVEL=msglvl1,memdbg; export LAL_DEBUG_LEVEL; \
	LAL_DATA_PATH="$(LAL_DATA_PATH)"; export LAL_DATA_PATH;

CLEANFILES =

## make sure lalapps.la is rebuilt when appropriate
BUILT_SOURCES =
include $(top_srcdir)/gnuscripts/vcsID.common
