AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/src/lalapps -I$(top_builddir)/src/lalapps -DPKG_DATA_DIR='"$(pkgdatadir)/"' @AM_CPPFLAGS@

if LALFRAME
if LALMETAIO
if LALBURST
bin_PROGRAMS = lalapps_StringSearch lalapps_StringAddFrame 
lalapps_StringSearch_SOURCES = StringSearch.c
lalapps_StringAddFrame_SOURCES = AddFrame.c
endif
endif
endif

if HAVE_PYTHON
dist_bin_SCRIPTS = \
	lalapps_cosmicstring_pipe \
	lalapps_string_cs_gamma
pkgpython_PYTHON = cosmicstring.py
endif

#pkgdata_DATA = S3_H1_StringDag.ini S3_StringDag.ini S4_StringDag.ini

EXTRA_DIST = cosmicstring.py $(DATA)

LDADD = $(top_builddir)/src/lalapps/liblalapps.la

BUILT_SOURCES =
include $(top_srcdir)/gnuscripts/vcsID.common
