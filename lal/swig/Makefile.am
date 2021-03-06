# Top-level SWIG build makefile.
# Author: Karl Wette, 2011, 2012

# regenerate dependency files when Makefile is regenerated
am__depfiles_maybe = depfiles

# make subdirectories
SUBDIRS = . octave python

# SWIG interface file
swig_iface = @PACKAGE@swig.i

# SWIG interface header list
swig_iface_headers = $(top_builddir)/swig/@PACKAGE@swig_headers.list

# SWIG preprocessing interface
swig_iface_preproc = @PACKAGE@swig_preproc.i
swig_iface_preproc_xml = @PACKAGE@swig_preproc.xml

# SWIG preprocessing interface dependencies file
swig_iface_preproc_deps = ./$(DEPDIR)/@PACKAGE@swig_preproc.deps

# SWIG preprocessing interface flags
swig_preprocflags = -xml -xmllite -includeall -ignoremissing -w201,302

# script which builds SWIG interface header
swig_generate_iface = $(srcdir)/generate_swiglal_iface.py

# installation location for SWIG interfaces
swigincludedir = $(includedir)/lal

# extra distribution files
EXTRA_DIST = $(swig_generate_iface)

# clean files
CLEANFILES = \
	$(builddir)/swig_config.h \
	$(swig_iface_headers) \
	$(swig_iface_preproc) \
	$(swig_iface_preproc_xml) \
	$(swig_iface)
clean-local:
	-rm -rf $(builddir)/lal
DISTCLEANFILES = \
	$(swig_iface_preproc_deps)

if SWIG_BUILD

# generated SWIG interfaces to install
nodist_swiginclude_HEADERS = $(swig_iface)

# delete failed targets
rm_targets = { rm -f $@; exit 1; }

# build SWIG preprocessing interface file
$(swig_iface_preproc) : $(swig_iface_headers) Makefile
	$(AM_V_GEN)echo '// $@: generated by Makefile' > $@; \
	echo "%module $(SWIG_IFACE_NAME)_preproc;" >> $@; \
	echo "%warnfilter(SWIGWARN_PARSE_REDUNDANT) __swiglal__;" >> $@; \
	echo "%warnfilter(SWIGWARN_PARSE_REDUNDANT) __swiglal_clear__;" >> $@; \
	echo "#define _LAL_INLINE_" >> $@; \
	echo "#define _LAL_RESTRICT_" >> $@; \
	echo "#define SWIGLAL(...) const char __swiglal__[] = #__VA_ARGS__" >> $@; \
	echo "#define SWIGLAL_CLEAR(...) const char __swiglal_clear__[] = #__VA_ARGS__" >> $@; \
	headers=`cat $(swig_iface_headers) | $(SED) 's|^.*:||'`; \
	for header in $$headers; do \
		header=`echo $$header | $(SED) '\|LibVCSInfo.h$$|p;\|VCSInfo.h$$|d'`; \
		if test "x$$header" != x; then \
			echo "%include <lal/$${header}>" >> $@; \
		fi; \
	done

# generate SWIG configuration header
$(builddir)/swig_config.h : $(CONFIG_HEADER)
	$(AM_V_GEN)echo '// $@: generated from $< by $(subdir)/Makefile' > $@; \
	$(SED) -n 's/^#define  */#define SWIGLAL_/p' $< >> $@

# include SWIG preprocessing interface dependencies file
@am__include@ @am__quote@./$(DEPDIR)/@PACKAGE@swig_preproc.deps@am__quote@

# generate SWIG parse tree of preprocessing interface file
$(swig_iface_preproc_xml) : $(swig_iface_preproc) $(builddir)/swig_config.h
	$(AM_V_GEN)$(SWIG) $(SWIG_SWIGFLAGS) $(swig_preprocflags) -MD -MF $(swig_iface_preproc_deps) -o $@ $< || $(rm_targets)

# generate SWIG interface file
$(swig_iface) : $(swig_iface_preproc_xml) $(swig_generate_iface)
	$(AM_V_GEN)$(PYTHON) $(swig_generate_iface) -n "$(PACKAGE)" -d "$(SWIG_MODULE_DEPENDS)" -s "$(SWIG_SYMBOL_PREFIXES)" -p "$(swig_iface_preproc)" -x "$(swig_iface_preproc_xml)" -i "$(swig_iface)" || $(rm_targets); \
	if test -f "$(top_srcdir)/include/Makefile.am"; then \
		$(MKDIR_P) "$(top_builddir)/include/lal"; \
		rm -f "$(top_builddir)/include/lal/$@"; \
		$(LN_S) "$(abs_builddir)/$@" "$(top_builddir)/include/lal/$@"; \
	else \
		$(MKDIR_P) "$(top_builddir)/src/lal"; \
		rm -f "$(top_builddir)/src/lal/$@"; \
		$(LN_S) "$(abs_builddir)/$@" "$(top_builddir)/src/lal/$@"; \
	fi

endif
