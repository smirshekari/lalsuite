pkginclude_HEADERS = \
	BandPassTimeSeries.h \
	IIRFilter.h \
	ZPGFilter.h

all-local: include-link
@INCLUDEMKFILE@

swig_headers = $(pkginclude_HEADERS)
include $(top_srcdir)/swig/swiglal_header.am
