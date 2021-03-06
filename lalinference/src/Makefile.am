AM_CPPFLAGS = -I$(top_builddir)/include -DPKG_DATA_DIR='"$(pkgdatadir)/"' @AM_CPPFLAGS@

if LALXML
XMLHDR = LALInferenceXML.h
XMLSRC = LALInferenceXML.c
endif

if HAVE_CHEALPIX
BAYESTARHDR = bayestar_sky_map.h
BAYESTARSRC = bayestar_sky_map.c
endif

lalinferenceincludedir = $(includedir)/lal
lalinferenceinclude_HEADERS = \
	LALInference.h \
	LALInferenceBuildInfo.h \
	LALInferenceConfig.h \
	LALInferenceLikelihood.h \
	LALInferenceNestedSampler.h \
	LALInferencePrior.h \
	LALInferenceReadData.h \
	LALInferenceTemplate.h \
	LALInferenceProposal.h \
	LALInferenceLibVCSInfo.h \
	LALInferenceInit.h \
	LALInferenceVCSInfo.h \
	LALInferenceRemoveLines.h \
	lalinferenceswig_test.i \
	$(BAYESTARHDR) \
	$(XMLHDR)

lib_LTLIBRARIES = liblalinference.la
liblalinference_la_SOURCES = \
	LALInference.c \
	LALInferenceLikelihood.c \
	LALInferenceNestedSampler.c \
	LALInferencePrior.c \
	LALInferenceReadData.c \
	LALInferenceTemplate.c \
	LALInferenceProposal.c \
	LALInferenceInitCBC.c \
	LALInferenceVCSInfo.c \
	LALInferenceRemoveLines.c \
	logaddexp.h \
	$(BAYESTARSRC) \
	$(XMLSRC)
nodist_liblalinference_la_SOURCES = LALInferenceBuildInfo.c

liblalinference_la_LDFLAGS = $(AM_LDFLAGS) -version-info $(LIBVERSION)

MOSTLYCLEANFILES = \
	lalinference-user-env.shell \
	lalinference-user-env.sh \
	lalinference-user-env.csh

sysconf_DATA = \
	lalinference-user-env.sh \
	lalinference-user-env.csh

BUILT_SOURCES = \
	lalinference-user-env.sh \
	lalinference-user-env.csh

lalinference-user-env.shell: Makefile $(srcdir)/generate_user_env.awk
	@echo "# source this file to access $(PACKAGE_NAME)" >$@
	@echo "set LALINFERENCE_PREFIX $(prefix)" >>$@
	@echo "prepend PATH $(bindir)" >>$@
	@echo "prepend MANPATH $(mandir)" >>$@
	@echo "prepend PKG_CONFIG_PATH $(libdir)/pkgconfig" >>$@
	@echo "prepend PYTHONPATH $(pyexecdir)" >>$@
	@echo "prepend LAL_DATA_PATH $(pkgdatadir)" >>$@
	@echo "prepend OCTAVE_PATH $(octexecdir)" >>$@

lalinference-user-env.csh lalinference-user-env.sh: lalinference-user-env.shell $(srcdir)/generate_user_env.awk
	$(AM_V_GEN)$(AWK) -f $(srcdir)/generate_user_env.awk -v SED="$(SED)" -v output="$@" $<

EXTRA_DIST = \
	LALInferenceConfig.h.in \
	LALInferenceXML.c \
	LALInferenceXML.h \
	chapter_inference.dox \
	config.h.in \
	generate_user_env.awk

if HAVE_GIT_REPO
BUILT_SOURCES += LALInferenceVCSInfo.h
MOSTLYCLEANFILES += LALInferenceVCSInfo.h
LALInferenceVCSInfo.h .PHONY : gen-LALInferenceVCSInfo.h
gen-LALInferenceVCSInfo.h :
	$(AM_V_at)$(GENERATE_VCS_INFO) LALInferenceVCSInfo.h $(srcdir)/LALInferenceVCSInfo.h.git
endif

DISTCLEANFILES = config.h

MOSTLYCLEANFILES += LALInferenceBuildInfo.c

LALInferenceBuildInfo.c: Makefile $(liblalinference_la_DEPENDENCIES)
	$(AM_V_GEN)echo '/* $@: generated by Makefile */' >$@; \
	echo 'const char *const lalInferenceConfigureArgs = "'"$(ac_configure_args)"'";' >>$@; \
	echo 'const char *const lalInferenceConfigureDate = "'"$(configure_date)"'";' >>$@; \
	echo 'const char *const lalInferenceBuildDate = "'`date +"%Y-%m-%dT%H:%M:%S%z"`'";' >>$@

@LAL_HEADER_LINK@

swig_headers = $(lalinferenceinclude_HEADERS)
include $(top_srcdir)/swig/swiglal_header.am
