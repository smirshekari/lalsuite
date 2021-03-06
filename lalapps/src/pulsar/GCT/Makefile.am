## Process this file with automake to produce Makefile.in

CFSsrc = $(top_srcdir)/src/pulsar/FDS_isolated
EAHsrc = $(top_srcdir)/src/pulsar/hough/src2/EinsteinAtHome

# for using lalapps-defines + functions
AM_CPPFLAGS = -I$(top_srcdir)/src/lalapps -I$(CFSsrc) -I$(EAHsrc) -DPKG_DATA_DIR='"$(pkgdatadir)/"' @AM_CPPFLAGS@
LDADD = $(top_builddir)/src/lalapps/liblalapps.la

bin_PROGRAMS = lalapps_HierarchSearchGCT
check_PROGRAMS =
EXTRA_PROGRAMS = eah_HierarchSearchGCT \
	         lalapps_HierarchSearchGCT_SSE2 lalapps_HierarchSearchGCT_SSE2_NONC

EAHSources = $(EAHsrc)/hs_boinc_extras.c $(EAHsrc)/hs_boinc_options.cpp $(EAHsrc)/win_lib.cpp
HSGCTSources = HierarchSearchGCT.c HierarchSearchGCT.h \
                          GCTtoplist.c GCTtoplist.h \
                          $(CFSsrc)/HeapToplist.c $(CFSsrc)/HeapToplist.h \
                          $(CFSsrc)/ComputeFStatistic.h \
                          RecalcToplistStats.c RecalcToplistStats.h

lalapps_HierarchSearchGCT_SOURCES = $(HSGCTSources)

lalapps_HierarchSearchGCT_SSE2_SOURCES = $(HSGCTSources)
lalapps_HierarchSearchGCT_SSE2_CPPFLAGS = $(AM_CPPFLAGS) -DHS_OPTIMIZATION -DHIERARCHSEARCHGCT -DGC_SSE2_OPT
lalapps_HierarchSearchGCT_SSE2_CFLAGS = $(AM_CFLAGS) -msse -msse2 -mfpmath=sse

lalapps_HierarchSearchGCT_SSE2_NONC_SOURCES = $(HSGCTSources)
lalapps_HierarchSearchGCT_SSE2_NONC_CPPFLAGS = $(AM_CPPFLAGS) -DHS_OPTIMIZATION -DHIERARCHSEARCHGCT -DGC_SSE2_OPT -DEXP_NO_NUM_COUNT
lalapps_HierarchSearchGCT_SSE2_NONC_CFLAGS = $(AM_CFLAGS) -msse -msse2 -mfpmath=sse


eah_HierarchSearchGCT_SOURCES = $(HSGCTSources) $(EAHSources)
eah_HierarchSearchGCT_CPPFLAGS = $(AM_CPPFLAGS) -DEAH_BOINC -DHS_OPTIMIZATION -DHIERARCHSEARCHGCT
# this shouldn't harm - it just prefers libraries in the build directory over e.g. system ones
eah_HierarchSearchGCT_LDFLAGS = -L. $(AM_LDFLAGS)
# RELEASE_DEPS contains additional dependencies of the E@H build
# items in there should have rules listed below to create them
# the idea is to increase compatibility of E@H applications by linking these libs statically
eah_HierarchSearchGCT_DEPENDENCIES = $(RELEASE_DEPS)
eah_HierarchSearchGCT_LDADD = $(LDADD) $(RELEASE_LDADD)

libstdc++.a:
	cp -f `$(CC) -print-file-name=$@` $@

exchndl.o: $(EAHsrc)/exchndl.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

erp_execinfo_plus.o: $(EAHsrc)/erp_execinfo_plus.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

## make sure lalapps.la is rebuilt when appropriate
BUILT_SOURCES =
include $(top_srcdir)/gnuscripts/vcsID.common

# how to compile CUDA files (.cu)
NVCC ?= nvcc
.cu.o:
	$(NVCC) $(CPPFLAGS) -c $< -o $@

## pretty dirty hack, particulary for compiling an E@H Mac OS 10.3 App on 10.4
eah_HierarchSearchGCT_manual: eah_HierarchSearchGCT-HierarchSearchGCT.o \
    eah_HierarchSearchGCT-GCTtoplist.o \
    eah_HierarchSearchGCT-HeapToplist.o \
    eah_HierarchSearchGCT-ComputeFstat_RS.o \
    eah_HierarchSearchGCT-RecalcToplistStats.o \
    eah_HierarchSearchGCT-hs_boinc_extras.o \
    eah_HierarchSearchGCT-hs_boinc_options.o \
    eah_HierarchSearchGCT-win_lib.o \
    libstdc++.a
	$(CXX) $(CPPFLAGS) $(AC_CPPFLAGS) -o $@ $^ $(LDFLAGS) $(AC_LDFLAGS) $(LDADD) $(AC_LDADD) $(LIBS) $(AC_LIBS) libstdc++.a


EXTRA_DIST = testHS.sh

TESTS = testHS.sh

TESTS_ENVIRONMENT = \
	LAL_DEBUG_LEVEL=msglvl1,memdbg; export LAL_DEBUG_LEVEL; \
	LAL_DATA_PATH="$(LAL_DATA_PATH)"; export LAL_DATA_PATH;

CLEANFILES =

clean-local:
	-rm -rf ./testHS_dir
