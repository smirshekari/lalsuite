pkginclude_HEADERS = \
	BitField.h \
	CoarseGrainFrequencySeries.h \
	Dirichlet.h \
	Factorial.h \
	FindRoot.h \
	GSLSupport.h \
	Integrate.h \
	Interpolate.h \
	LALAdaptiveRungeKutta4.h \
	LALRunningMedian.h \
	MatrixUtils.h \
	ODE.h \
	Random.h \
	RngMedBias.h \
	Sort.h \
	SphericalHarmonics.h

all-local: include-link
@INCLUDEMKFILE@

swig_headers = $(pkginclude_HEADERS)
include $(top_srcdir)/swig/swiglal_header.am
