SUBDIRS = \
	std \
	tools \
	factories \
	vectorops \
	utilities \
	stats \
	inject \
	date \
	tdfilter \
	window \
	fft \
	noisemodels

include-links:
	@set fnord $(MAKEFLAGS); amf=$$2; \
		for d in $(SUBDIRS); do \
			subdir=${top_builddir}/packages/$$d/include; \
			echo "Making include-links in $$d/include"; \
			(cd $$subdir && $(MAKE) $(AM_MAKEFLAGS) include-link) \
				|| case "$$amf" in *=*) exit1;; *k*) fail=yes;; *) exit 1;; esac; \
		done; test -z "$$fail"
