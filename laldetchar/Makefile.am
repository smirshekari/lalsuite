ACLOCAL_AMFLAGS = -I gnuscripts

# Override automake's default rule for rebuilding aclocal.m4, to ensure that
# ./configure is successfully recreated after certain build system changes which
# require other autotools (e.g. autoheader) to be re-run. See Redmine issue #728.
$(ACLOCAL_M4): $(am__aclocal_m4_deps)
	$(am__cd) $(srcdir) && $(SHELL) ./00boot

EXTRA_DIST = \
	COPYING \
	laldetchar.spec \
	$(DATA)

SUBDIRS = \
	gnuscripts \
	debian \
	include \
	src \
	python \
	swig \
	bin \
	test

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = laldetchar.pc
noinst_DATA = laldetchar-uninstalled.pc
BUILT_SOURCES = laldetchar-uninstalled.pc

DISTCLEANFILES = config.lt

if HAVE_GIT_REPO
vcs-clean: maintainer-clean
	-cd $(top_srcdir) && $(GIT) clean -Xdf
endif

all-local:
	@echo "";\
	echo "==================================================";\
	echo "LALDetChar has now been successfully built.";\
	echo "Now run 'make install' to install LALDetChar.";\
	echo "==================================================";\
	echo ""

install-exec-local:
	@echo "";\
	echo "==================================================";\
	echo "LALDetChar has now been successfully installed.";\
	echo "Now you may want to setup your environment:";\
	echo "";\
	echo "* Bourne shell [bash] users: please add the line";\
	echo "  . $(sysconfdir)/laldetchar-user-env.sh";\
	echo "  to your .profile file.";\
	echo "";\
	echo "* C-shell [tcsh] users: please add the line";\
	echo "  source $(sysconfdir)/laldetchar-user-env.csh";\
	echo "  to your .login file.";\
	echo "==================================================";\
	echo ""
