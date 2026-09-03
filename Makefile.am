SUBDIRS = . gdk-pixbuf-loader tests tools doc build

# Packaging and extra checks. `make` with no goal is still `all`.
# VERSION is AC_INIT / configure.ac (2.62.3). Source tarball: ../pkg/.
# Debian packages: ../debpkg/.
help:
	@echo "librsvg-c $(VERSION) targets:"
	@echo "  help          this list"
	@echo "  clean         objects and .libs only (not shipped docs or configure)"
	@echo "  all           library + tools  (default \`make\`)"
	@echo "  check         test suite"
	@echo "  check-parity  rust-oracle reftests"
	@echo "  fuzz-css      optional CSS/SVG harness (not make check)"
	@echo "  fuzz-afl      optional AFL++ binary (skips if AFL++ missing)"
	@echo "  html          API HTML (Doxygen)"
	@echo "  pkg           source-only ../pkg/librsvg-c-$(VERSION).tar.xz"
	@echo "  deb           Debian packages into ../debpkg/ (DESTDIR; not /usr)"
	@echo "  crux          ../pkg/Pkgfile + source tarball (does not run pkgmk)"
	@echo "  dist-release  alias of pkg"
	@echo "Never: Autotools dist/distcheck, pkgmk, or install onto the live host."

check-parity:
	$(MAKE) -C tests check-parity
check-parity-engine:
	$(MAKE) -C tests check-parity-engine
check-parity-croco:
	@echo "check-parity-croco: libcroco is discontinued; css/ is the only engine" >&2
	@false

fuzz-css: librsvg-@RSVG_API_MAJOR_VERSION@.la
	$(MAKE) -C tests fuzz-css

fuzz-afl: librsvg-@RSVG_API_MAJOR_VERSION@.la
	$(MAKE) -C tests fuzz-afl

pkg:
	$(SHELL) $(srcdir)/dist/make-dist-release.sh
dist-release: pkg

deb:
	$(SHELL) $(srcdir)/dist/debian/build-deb.sh

crux: pkg
	$(SHELL) $(srcdir)/dist/make-crux.sh

.PHONY: help check-parity check-parity-engine check-parity-croco \
	fuzz-css fuzz-afl pkg dist-release deb crux

NULL =
BUILT_SOURCES =

lib_LTLIBRARIES = librsvg-@RSVG_API_MAJOR_VERSION@.la

bin_PROGRAMS = rsvg-convert
if HAVE_GTK_3
bin_PROGRAMS += rsvg-view-3
endif

headers = \
	rsvg.h	\
	rsvg-cairo.h \
	rsvg-pixbuf.h

enum_sources = \
	librsvg-enum-types.h	\
	librsvg-enum-types.c
BUILT_SOURCES += $(enum_sources)

librsvg_@RSVG_API_MAJOR_VERSION@_la_SOURCES = \
	librsvg-features.c 	\
	rsvg-css.c 		\
	rsvg-css.h 		\
	rsvg-compat.h		\
	rsvg-defs.c 		\
	rsvg-defs.h 		\
	rsvg-image.c		\
	rsvg-image.h		\
	rsvg-io.c		\
	rsvg-io.h		\
	rsvg-paint-server.c 	\
	rsvg-paint-server.h 	\
	rsvg-path.c 		\
	rsvg-path.h 		\
	rsvg-private.h 		\
	rsvg-base-file-util.c 	\
	rsvg-filter.c		\
	rsvg-filter.h		\
	rsvg-marker.c		\
	rsvg-marker.h		\
	rsvg-mask.c		\
	rsvg-mask.h		\
	rsvg-shapes.c		\
	rsvg-shapes.h		\
	rsvg-structure.c	\
	rsvg-structure.h	\
	rsvg-styles.c		\
	rsvg-styles.h		\
	css/rsvg-css-engine.c	\
	css/rsvg-css-engine.h	\
	rsvg-text.c		\
	rsvg-text.h		\
	rsvg-cond.c		\
	rsvg-base.c		\
	librsvg-enum-types.c	\
	rsvg-cairo-draw.c	\
	rsvg-cairo-draw.h	\
	rsvg-cairo-render.c	\
	rsvg-cairo-render.h	\
	rsvg-cairo-clip.h	\
	rsvg-cairo-clip.c	\
	rsvg.c			\
	rsvg-gobject.c		\
	rsvg-file-util.c	\
	rsvg-size-callback.c	\
	rsvg-size-callback.h	\
	rsvg-xml.c		\
	rsvg-xml.h		\
	rsvg.h			\
	rsvg-cairo.h		\
	rsvg-pixbuf.h		\
	rsvg-features.h 	\
	rsvg-version.h		\
	librsvg-enum-types.h	\
	$(NULL)

librsvg_@RSVG_API_MAJOR_VERSION@_la_CPPFLAGS = \
	-I$(top_srcdir) 			\
	-I$(top_builddir) 			\
	-DG_LOG_DOMAIN=\"librsvg\" 		\
	-DLIBRSVG_DATADIR="\"$(datadir)\""	\
	-DSRCDIR=\""${abs_top_srcdir}"\"	\
	-DRSVG_DISABLE_DEPRECATION_WARNINGS	\
	$(AM_CPPFLAGS)

librsvg_@RSVG_API_MAJOR_VERSION@_la_CFLAGS = \
	-DRSVG_COMPILATION	\
	$(LIBRSVG_CFLAGS) 	\
	$(AM_CFLAGS)

librsvg_@RSVG_API_MAJOR_VERSION@_la_LDFLAGS = \
	$(BSYMBOLIC_LDFLAG) \
	-version-info @RSVG_LT_VERSION_INFO@ \
	-export-dynamic \
	-no-undefined \
	-export-symbols-regex "^(rsvg_|librsvg_)" \
	$(AM_LDFLAGS)

librsvg_@RSVG_API_MAJOR_VERSION@_la_LIBADD = \
	$(LIBRSVG_LIBS) 	\
	$(LIBM)

librsvgincdir = $(includedir)/librsvg-$(RSVG_API_VERSION)/librsvg
librsvginc_HEADERS = 	\
	$(headers)	\
	rsvg-features.h \
	rsvg-version.h \
	librsvg-features.h

dist_man_MANS = rsvg-convert.1

rsvg_convert_SOURCES = \
	rsvg-convert.c \
	rsvg-size-callback.c \
	rsvg-size-callback.h \
	$(NULL)

rsvg_convert_CPPFLAGS = \
	-I$(top_srcdir) 			\
	-I$(top_builddir) 			\
	-DLIBRSVG_DATADIR="\"$(datadir)\""	\
	$(AM_CPPFLAGS)

rsvg_convert_CFLAGS =\
	$(RSVG_CONVERT_CFLAGS) \
	-Werror=deprecated-declarations \
	$(AM_CFLAGS)

rsvg_convert_LDFLAGS = $(AM_LDFLAGS)

rsvg_convert_LDADD = \
	$(top_builddir)/librsvg-@RSVG_API_MAJOR_VERSION@.la	\
	$(RSVG_CONVERT_LIBS) \
	$(LIBM)

rsvg_view_3_SOURCES = \
	test-display.c \
	rsvg-size-callback.h

rsvg_view_3_CPPFLAGS = \
	-I$(top_srcdir) 			\
	-I$(top_builddir) 			\
	-DLIBRSVG_DATADIR="\"$(datadir)\""	\
	$(AM_CPPFLAGS)

rsvg_view_3_CFLAGS =\
	$(GTK3_CFLAGS)		\
	$(LIBRSVG_CFLAGS) 	\
	$(AM_CFLAGS)

rsvg_view_3_LDFLAGS =

rsvg_view_3_LDADD = \
	$(top_builddir)/librsvg-@RSVG_API_MAJOR_VERSION@.la \
	$(GTK3_LIBS)		\
	$(LIBRSVG_LIBS)		\
	$(GTHREAD_LIBS)		\
	$(LIBM)

if OS_WIN32
rsvg_view_3_LDFLAGS += -mwindows
endif # OS_WIN32

EXTRA_DIST =				\
	librsvg.doap			\
	AUTHORS				\
	README.md			\
	NEWS				\
	LICENCE				\
	LICENCE-LIB			\
	librsvg-zip.in 			\
	Rsvg-2.0-custom.vala		\
	Rsvg-2.0.metadata		\
	config.h.win32.in		\
	config.h.win32			\
	rsvg.symbols			\
	glib-tap.mk			\
	tap-driver.sh			\
	tap-test			\
	css/README			\
	examples/README			\
	examples/icon.svg		\
	examples/01-render-document.c	\
	examples/02-render-element.c	\
	examples/03-stylesheet.c	\
	examples/04-geometry.c		\
	examples/05-from-memory.c	\
	examples/06-pixbuf.c		\
	examples/deprecated/render-cairo.c \
	dist/debian/rsvg-convert.bash-completion

# Tab completion for rsvg-convert. Not a --completion CLI flag.
install-data-local:
	$(MKDIR_P) "$(DESTDIR)$(datadir)/bash-completion/completions"
	$(INSTALL_DATA) "$(srcdir)/dist/debian/rsvg-convert.bash-completion" \
	  "$(DESTDIR)$(datadir)/bash-completion/completions/rsvg-convert"

uninstall-local:
	rm -f "$(DESTDIR)$(datadir)/bash-completion/completions/rsvg-convert"

# AGENTS.md, AGENTS_history.md, and AGENTS_deprecated-abi.md are
# developer-only. Do not add them to EXTRA_DIST or pkg/ tarballs.
# debug/ and make-check logs are never part of a release tarball.
dist-hook:
	rm -f $(distdir)/AGENTS.md $(distdir)/AGENTS_history.md \
	      $(distdir)/AGENTS_deprecated-abi.md $(distdir)/PLAN.md
	rm -rf $(distdir)/debug
	-find $(distdir) \( -name '*.log' -o -name '*.trs' \) -delete
	rm -f $(distdir)/README
	test -f $(distdir)/README.md

CLEANFILES = \
	$(enum_sources) \
	s-enum-types-h s-enum-types-c

## Put `exec' in the name because this should be installed by
## `install-exec', not `install-data'.

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = librsvg-$(RSVG_API_VERSION).pc

librsvg-enum-types.h: s-enum-types-h
	@true

s-enum-types-h: $(headers) Makefile
	$(AM_V_GEN) ( cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#if !defined (__RSVG_RSVG_H_INSIDE__) && !defined (RSVG_COMPILATION)\n#warning \"Including <librsvg/librsvg-enum-types.h> directly is deprecated.\"\n#endif\n\n#ifndef __LIBRSVG_ENUM_TYPES_H__\n#define __LIBRSVG_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void);\n#define RSVG_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" 	\
			--ftail "G_END_DECLS\n\n#endif /* __LIBRSVG_ENUM_TYPES_H__ */" \
		$(headers) ) > tmp-librsvg-enum-types.h \
	&& (cmp -s tmp-librsvg-enum-types.h librsvg-enum-types.h || cp tmp-librsvg-enum-types.h librsvg-enum-types.h ) \
	&& rm -f tmp-librsvg-enum-types.h	\
	&& echo timestamp > $(@F)

librsvg-enum-types.c: s-enum-types-c librsvg-enum-types.h
	@true

s-enum-types-c: $(headers) Makefile
	$(AM_V_GEN) ( cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#include \"rsvg.h\"" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" 	\
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		$(headers) ) > tmp-librsvg-enum-types.c \
	&& (cmp -s tmp-librsvg-enum-types.c librsvg-enum-types.c || cp tmp-librsvg-enum-types.c librsvg-enum-types.c ) \
	&& rm -f tmp-librsvg-enum-types.c \
	&& echo timestamp > $(@F)

DISTCHECK_CONFIGURE_FLAGS = --disable-introspection --disable-vala

if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS = Rsvg-@RSVG_API_VERSION@.gir
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --symbol-prefix=rsvg --symbol-prefix=librsvg
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

Rsvg-@RSVG_API_VERSION@.gir: librsvg-@RSVG_API_MAJOR_VERSION@.la

Rsvg_@RSVG_API_VERSION_U@_gir_NAMESPACE = Rsvg
Rsvg_@RSVG_API_VERSION_U@_gir_EXPORT_PACKAGES = librsvg-$(RSVG_API_VERSION)
Rsvg_@RSVG_API_VERSION_U@_gir_FILES = \
	$(librsvginc_HEADERS) \
	$(filter-out %.h,$(librsvg_@RSVG_API_MAJOR_VERSION@_la_SOURCES)) \
	$(NULL)
Rsvg_@RSVG_API_VERSION_U@_gir_INCLUDES = \
	GLib-2.0 \
	GObject-2.0 \
	Gio-2.0 \
	cairo-1.0 \
	GdkPixbuf-2.0 \
	$(NULL)
Rsvg_@RSVG_API_VERSION_U@_gir_PACKAGES = \
	pango \
	libxml-2.0 \
	$(NULL)
Rsvg_@RSVG_API_VERSION_U@_gir_CFLAGS = -I$(top_builddir) -DRSVG_COMPILATION
Rsvg_@RSVG_API_VERSION_U@_gir_LIBS = librsvg-@RSVG_API_MAJOR_VERSION@.la
Rsvg_@RSVG_API_VERSION_U@_gir_SCANNERFLAGS = --c-include="librsvg/rsvg.h"

girdir = $(datadir)/gir-1.0
nodist_gir_DATA = $(INTROSPECTION_GIRS)

typelibsdir = $(libdir)/girepository-1.0
nodist_typelibs_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(nodist_gir_DATA) $(nodist_typelibs_DATA)

if ENABLE_VAPIGEN
include $(VAPIGEN_MAKEFILE)

librsvg-$(RSVG_API_VERSION).vapi: Rsvg-$(RSVG_API_VERSION).gir

VAPIGEN_VAPIS = librsvg-$(RSVG_API_VERSION).vapi

librsvg_@RSVG_API_VERSION_U@_vapi_DEPS = gio-2.0 cairo
librsvg_@RSVG_API_VERSION_U@_vapi_METADATADIRS = $(top_srcdir)
librsvg_@RSVG_API_VERSION_U@_vapi_FILES = $(top_builddir)/Rsvg-$(RSVG_API_VERSION).gir $(top_srcdir)/Rsvg-$(RSVG_API_VERSION)-custom.vala

vapidir = $(datadir)/vala/vapi
vapi_DATA = $(VAPIGEN_VAPIS)

CLEANFILES += $(VAPIGEN_VAPIS)

endif # ENABLE_VAPIGEN

endif # HAVE_INTROSPECTION

# ------------------- MSVC Build Items ----------------

MSVCPROJS = rsvg rsvg-convert

rsvg_FILES = $(librsvg_@RSVG_API_MAJOR_VERSION@_la_SOURCES)
rsvg_EXCLUDES = dummy
rsvg_HEADERS_DIR = $(librsvgincdir)
rsvg_HEADERS_INST = $(librsvginc_HEADERS)

rsvg_HEADERS_EXCLUDES = dummy

rsvg_convert_FILES = $(rsvg_convert_SOURCES)
rsvg_convert_EXCLUDES = dummy

include $(top_srcdir)/build/Makefile.msvcproj

# autotools: Why must we do this instead of dist-hook?
MSVCPROJ_GENERATED = \
	$(top_builddir)/build/win32/vs9/rsvg.vcproj		\
	$(top_builddir)/build/win32/vs9/rsvg-convert.vcproj

if HAVE_INTROSPECTION

MSVC_INTROSPECT_GIRS = $(INTROSPECTION_GIRS)

INTROSPECTION_INTERMEDIATE_ITEMS = \
	$(top_builddir)/build/win32/Rsvg-@RSVG_API_VERSION@.gir.msvc.introspect		\
	$(top_builddir)/build/win32/Rsvg_@RSVG_API_VERSION_U@_gir_list

Rsvg_@RSVG_API_VERSION_U@_gir_MSVC_FILES = $(Rsvg_@RSVG_API_VERSION_U@_gir_FILES)
Rsvg_@RSVG_API_VERSION_U@_gir_MSVC_PACKAGES = $(Rsvg_@RSVG_API_VERSION_U@_gir_PACKAGES)
Rsvg_@RSVG_API_VERSION_U@_gir_MSVC_EXPORT_PACKAGES = $(Rsvg_@RSVG_API_VERSION_U@_gir_EXPORT_PACKAGES)
Rsvg_@RSVG_API_VERSION_U@_gir_MSVC_INCLUDE_GIRS = $(Rsvg_@RSVG_API_VERSION_U@_gir_INCLUDES)
Rsvg_@RSVG_API_VERSION_U@_gir_MSVC_LIBS = rsvg-@RSVG_API_VERSION@
Rsvg_@RSVG_API_VERSION_U@_gir_MSVC_CFLAGS = -I../.. -DRSVG_COMPILATION
Rsvg_@RSVG_API_VERSION_U@_gir_MSVC_SCANNERFLAGS = $(Rsvg_@RSVG_API_VERSION_U@_gir_SCANNERFLAGS)

MSVCPROJ_GENERATED += $(INTROSPECTION_INTERMEDIATE_ITEMS)

include $(top_srcdir)/build/Makefile.msvc-introspection
endif # HAVE_INTROSPECTION

$(MSVCPROJ_GENERATED): $(top_builddir)/build/win32/vs9/rsvg.headers

EXTRA_DIST += $(MSVCPROJ_GENERATED)

# ChangeLog generation

ChangeLog:
	$(AM_V_GEN) if test -d $(top_srcdir)/.git; then \
		GIT_DIR="$(top_srcdir)/.git" git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog
