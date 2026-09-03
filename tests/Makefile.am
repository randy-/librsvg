include $(top_srcdir)/glib-tap.mk

# Keep "errors" at the end; they are the slowest to run
test_programs =		\
	api		\
	loading		\
	rsvg-test	\
	crash		\
	styles		\
	render-crash	\
	dimensions	\
	errors

test_utils_common_sources = \
	test-utils.c	\
	test-utils.h

rsvg_test_SOURCES = \
	rsvg-test.c	\
	$(test_utils_common_sources)

errors_SOURCES = 	\
	errors.c	\
	$(test_utils_common_sources)

crash_SOURCES = \
	crash.c		\
	$(test_utils_common_sources)

render_crash_SOURCES = \
	render-crash.c	\
	$(test_utils_common_sources)

styles_SOURCES = \
	styles.c	\
	$(test_utils_common_sources)

dimensions_SOURCES = \
	dimensions.c	\
	$(test_utils_common_sources)

api_SOURCES = \
	api.c \
	$(test_utils_common_sources)

loading_SOURCES =		 	\
	loading.c			\
	$(test_utils_common_sources)

LDADD = $(top_builddir)/librsvg-@RSVG_API_MAJOR_VERSION@.la		\
	$(LIBRSVG_LIBS)							\
	$(LIBM)

INCLUDES = -I$(srcdir) 							\
	   -I$(top_srcdir)						\
	   -DTEST_DATA_DIR="\"$(srcdir)\""				\
	   -DTEST_SRC_DIR="\"$(PWD)\""					\
	   -DTOP_SRC_DIR="\"$(top_srcdir)\""				\
	   -DCONVERT_PROGRAM="\"$(abs_top_builddir)/rsvg-convert$(EXEEXT)\"" \
	  $(LIBRSVG_CFLAGS)

dist_installed_test_data =						\
	$(wildcard $(srcdir)/resources/*)				\
	$(wildcard $(srcdir)/fixtures/crash/*.svg)			\
	$(wildcard $(srcdir)/fixtures/crash/*.png)			\
	$(wildcard $(srcdir)/fixtures/errors/*)				\
	$(wildcard $(srcdir)/fixtures/loading/*)			\
	$(wildcard $(srcdir)/fixtures/reftests/*.svg)			\
	$(wildcard $(srcdir)/fixtures/reftests/*.png)			\
	$(wildcard $(srcdir)/fixtures/reftests/bugs/*.svg)		\
	$(wildcard $(srcdir)/fixtures/reftests/bugs/*.png)		\
	$(wildcard $(srcdir)/fixtures/reftests/svg1.1/*.svg)		\
	$(wildcard $(srcdir)/fixtures/reftests/svg1.1/*.png)		\
	$(wildcard $(srcdir)/fixtures/reftests/svg1.1/resources/*)	\
	$(wildcard $(srcdir)/fixtures/render-crash/*.svg)		\
	fixtures/dimensions/bug564527.svg				\
	fixtures/dimensions/bug614018.svg				\
	fixtures/dimensions/bug612951.svg				\
	fixtures/dimensions/bug608102.svg				\
	fixtures/dimensions/sub-rect-no-unit.svg			\
	fixtures/styles/bug620693.svg					\
	fixtures/styles/bug614704.svg					\
	fixtures/styles/bug614606.svg					\
	fixtures/styles/important.svg					\
	fixtures/styles/svg-class.svg					\
	fixtures/styles/bug592207.svg					\
	fixtures/styles/bug338160.svg					\
	fixtures/styles/bug379629.svg					\
	fixtures/styles/bug614643.svg					\
	fixtures/styles/bug418823.svg					\
	fixtures/styles/order.svg	\
	fixtures/styles/color-level4.svg	\
	fixtures/styles/filter-funcs.svg	\
	fixtures/styles/text-props.svg	\
	fixtures/styles/radial-fr.svg	\
	fixtures/api/dpi.svg		\
	fixtures/api/example.svg	\
	fixtures/api/document.svg	\
	fixtures/api/geometry.svg	\
	fixtures/api/layers.svg		\
	fixtures/api/geometry-element.svg \
	fixtures/api/size.svg		\
	fixtures/api/no-size.svg	\
	fixtures/api/no-viewbox.svg	\
	fixtures/api/stylesheet.svg	\
	fixtures/api/stylesheet-green.css \
	fixtures/api/accept-language.svg \
	fixtures/api/currentcolor.svg	\
	fixtures/api/context-fill.svg	\
	fixtures/api/context-use.svg	\
	fixtures/api/href-use.svg	\
	fixtures/api/href-prefers-href.svg \
	fixtures/api/xlink-href-use.svg	\
	fixtures/api/paint-order.svg	\
	fixtures/api/mix-blend-mode.svg	\
	fixtures/api/mask-type-alpha.svg \
	fixtures/api/vector-effect.svg	\
	fixtures/api/webp-image.svg	\
	fixtures/api/png-image.svg	\
	fixtures/api/jpeg-image.svg	\
	fixtures/api/gif-image.svg	\
	fixtures/api/bmp-image.svg	\
	fixtures/api/avif-image.svg	\
	fixtures/api/red.webp		\
	fixtures/api/red.png		\
	fixtures/api/red.jpg		\
	fixtures/api/red.gif		\
	fixtures/api/red.bmp		\
	fixtures/api/rectangle.avif	\
	README-parity.md		\
	parity-xfail-categories.md	\
	parity-t-numbers.md		\
	parity-t-numbers.txt		\
	make-check-triage.md		\
	make-check-t-numbers.txt	\
	parity-xfail.txt		\
	parity-xfail-engine.txt		\
	parity-xfail-croco.txt		\
	fixtures/parity/README		\
	$(wildcard $(srcdir)/fixtures/parity/*.svg)			\
	$(wildcard $(srcdir)/fixtures/parity/*.png)			\
	$(wildcard $(srcdir)/fixtures/parity/*.css)			\
	$(wildcard $(srcdir)/fixtures/parity/*.txt)			\
	$(wildcard $(srcdir)/fixtures/parity/*.avif)			\
	$(wildcard $(srcdir)/fixtures/parity/adwaita/*)			\
	$(wildcard $(srcdir)/fixtures/parity/bugs/*)			\
	$(wildcard $(srcdir)/fixtures/parity/bugs-reftests/*)		\
	$(wildcard $(srcdir)/fixtures/parity/svg1.1/*.svg)		\
	$(wildcard $(srcdir)/fixtures/parity/svg1.1/*.png)		\
	$(wildcard $(srcdir)/fixtures/parity/svg1.1/resources/*)	\
	$(wildcard $(srcdir)/fixtures/parity/svg1.1/images/*)		\
	$(wildcard $(srcdir)/fixtures/parity/svg2/*)			\
	$(wildcard $(srcdir)/fixtures/parity/svg2-reftests/*)	\
	generate-parity-local.sh	\
	parity-local.txt		\
	fixtures/parity-local/README	\
	$(wildcard $(srcdir)/fixtures/parity-local/*.png)		\
	$(wildcard $(srcdir)/fixtures/parity-local/bugs/*.png)		\
	$(wildcard $(srcdir)/fixtures/parity-local/svg2/*.png)

# 2.62.3 reftest corpus. In-band tolerance classes are silent passes.
# Out-of-spec residuals FAIL with the TFM/TAA/TCR and numbers.
check-parity: rsvg-test$(EXEEXT)
	G_TEST_SRCDIR="$(abs_srcdir)" \
	G_TEST_BUILDDIR="$(abs_builddir)" \
	RSVG_PARITY=1 \
	RSVG_PARITY_TNORMS="$(abs_srcdir)/parity-t-numbers.txt" \
	$(builddir)/rsvg-test$(EXEEXT) -k

# Alias: default engine is already css/. Kept so existing scripts work.
check-parity-engine: check-parity

check-parity-croco:
	@echo "check-parity-croco: libcroco is discontinued; css/ is the only engine" >&2
	@false

# P20: host-local same-renderer refs. Does not replace rust 2.62.3 PNGs.
generate-parity-local: $(top_builddir)/rsvg-convert$(EXEEXT)
	$(SHELL) $(srcdir)/generate-parity-local.sh \
		$(top_builddir)/rsvg-convert$(EXEEXT) \
		$(abs_srcdir)/fixtures/parity \
		$(abs_srcdir)/fixtures/parity-local \
		$(abs_srcdir)/parity-local.txt

# Same-renderer regression on the class A named subset. No tolerance band
# (strict max_diff ≤ 1). rust check-parity is unchanged.
check-parity-local: rsvg-test$(EXEEXT)
	G_TEST_SRCDIR="$(abs_srcdir)" \
	G_TEST_BUILDDIR="$(abs_builddir)" \
	RSVG_PARITY=1 \
	RSVG_PARITY_LOCAL=1 \
	RSVG_PARITY_LOCAL_DIR="$(abs_srcdir)/fixtures/parity-local" \
	RSVG_PARITY_LOCAL_LIST="$(abs_srcdir)/parity-local.txt" \
	RSVG_PARITY_XFAIL= \
	$(builddir)/rsvg-test$(EXEEXT) -k

.PHONY: check-parity check-parity-engine check-parity-croco generate-parity-local check-parity-local \
	fuzz-bmp fuzz-css fuzz-afl

# Offline BMP / RLE harness. Not run by make check.
#   make -C tests fuzz-bmp
#   ./fuzz-bmp [--mutate N]
fuzz-bmp$(EXEEXT): $(srcdir)/fuzz-bmp.c $(top_builddir)/librsvg-@RSVG_API_MAJOR_VERSION@.la
	$(AM_V_CCLD)$(LIBTOOL) --tag=CC --mode=link $(CC) $(DEFS) \
	  -I$(top_builddir) $(INCLUDES) $(CFLAGS) \
	  $(srcdir)/fuzz-bmp.c $(LDADD) $(LIBS) -o $@

# Optional CSS/SVG harness (default css/ path). Not run by make check.
#   make fuzz-css
#   ./tests/fuzz/rsvg-fuzz-css
#   ./tests/fuzz/rsvg-fuzz-css --seconds 60
fuzz/rsvg-fuzz-css$(EXEEXT): $(srcdir)/fuzz/rsvg-fuzz-css.c $(top_builddir)/librsvg-@RSVG_API_MAJOR_VERSION@.la
	$(AM_V_at)$(MKDIR_P) fuzz
	$(AM_V_CCLD)$(LIBTOOL) --tag=CC --mode=link $(CC) $(DEFS) \
	  -I$(top_builddir) $(INCLUDES) $(CFLAGS) \
	  -DFUZZ_SEED_DIR="\"$(abs_srcdir)/fuzz/seeds\"" \
	  $(srcdir)/fuzz/rsvg-fuzz-css.c $(LDADD) $(LIBS) -o $@

fuzz-css: fuzz/rsvg-fuzz-css$(EXEEXT)

# Optional AFL++ binary. Skips (exit 0) if afl-cc / afl-gcc-fast is missing.
# Not part of make check. Real ELF (not a libtool wrapper) plus in-tree rpath
# so afl-fuzz can exec ./tests/fuzz/rsvg-fuzz-css-afl without a host install.
fuzz-afl: $(top_builddir)/librsvg-@RSVG_API_MAJOR_VERSION@.la
	@afl_cc=`command -v afl-cc 2>/dev/null || command -v afl-gcc-fast 2>/dev/null || true`; \
	if test -z "$$afl_cc"; then \
	  echo "make fuzz-afl: skipped (AFL++ not installed; need afl-cc or afl-gcc-fast)" >&2; \
	else \
	  $(MKDIR_P) fuzz; \
	  echo "make fuzz-afl: $$afl_cc -> fuzz/rsvg-fuzz-css-afl$(EXEEXT)"; \
	  $$afl_cc $(DEFS) \
	    -I$(top_builddir) $(INCLUDES) $(CFLAGS) \
	    -DRSVG_FUZZ_AFL_MAIN \
	    -DFUZZ_SEED_DIR="\"$(abs_srcdir)/fuzz/seeds\"" \
	    $(srcdir)/fuzz/rsvg-fuzz-css.c \
	    -L$(abs_top_builddir)/.libs -lrsvg-@RSVG_API_MAJOR_VERSION@ \
	    $(LIBRSVG_LIBS) $(LIBM) $(LIBS) \
	    -Wl,-rpath,$(abs_top_builddir)/.libs \
	    -o fuzz/rsvg-fuzz-css-afl$(EXEEXT); \
	fi

clean-local:
	-rm -f fuzz-bmp$(EXEEXT) fuzz/rsvg-fuzz-css$(EXEEXT) fuzz/rsvg-fuzz-css-afl$(EXEEXT)

EXTRA_DIST += fuzz-bmp.c fuzz/rsvg-fuzz-css.c fuzz/README README \
	$(wildcard $(srcdir)/fuzz/seeds/*) \
	$(wildcard $(srcdir)/fuzz/regress/*)
