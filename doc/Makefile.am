# API HTML is Doxygen only.
#
#   doxygen doc/Doxyfile
#   # or: make html
#
# Output: doc/html/index.html (shipped in git; distclean must not delete it).

AUTOMAKE_OPTIONS = 1.10 foreign

EXTRA_DIST = \
	Doxyfile \
	api-groups.h \
	mainpage.md \
	examples.dox \
	thanks.md \
	copyright.md \
	header.html \
	doxygen-footer.html \
	doxygen-include/rsvg-version.h \
	doxygen-include/rsvg-features.h

html-local:
	@if command -v doxygen >/dev/null 2>&1; then \
	  echo "  DOXYGEN doc/Doxyfile"; \
	  (cd $(top_srcdir) && doxygen doc/Doxyfile && \
	   cp -f LICENCE LICENCE-LIB doc/html/); \
	else \
	  echo "doxygen not found; skipping API HTML (install doxygen, then: doxygen doc/Doxyfile)"; \
	fi

docs:
	$(MAKE) $(AM_MAKEFLAGS) html
