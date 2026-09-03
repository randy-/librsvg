if ENABLE_PIXBUF_LOADER

NULL =

gdk_pixbuf_module_LTLIBRARIES = libpixbufloader-svg.la

libpixbufloader_svg_la_SOURCES = \
	io-svg.c

libpixbufloader_svg_la_CPPFLAGS = \
	-I$(top_srcdir) 			\
	-I$(top_builddir)			\
	-DGDK_PIXBUF_ENABLE_BACKEND		\
	-DG_LOG_DOMAIN=\"libpixbufloader-svg\" 	\
	$(AM_CPPFLAGS)

libpixbufloader_svg_la_CFLAGS = \
	$(GDK_PIXBUF_CFLAGS)	\
	$(LIBRSVG_CFLAGS)	\
	$(AM_CFLAGS)

libpixbufloader_svg_la_LDFLAGS = -avoid-version -module $(AM_LDFLAGS)

libpixbufloader_svg_la_LIBADD = \
	$(top_builddir)/librsvg-@RSVG_API_MAJOR_VERSION@.la	\
	$(GDK_PIXBUF_LIBS)	\
	$(LIBRSVG_LIBS)

if PLATFORM_WIN32
libpixbufloader_svg_la_LDFLAGS += -no-undefined
endif

noinst_PROGRAMS = rsvg-loader

rsvg_loader_SOURCES = \
	test.c \
	$(NULL)
rsvg_loader_CPPFLAGS = \
	-I$(top_srcdir) 			\
	-I$(top_builddir)			\
	$(AM_CPPFLAGS)
rsvg_loader_CFLAGS = \
	$(GDK_PIXBUF_CFLAGS) \
	$(AM_CFLAGS)
rsvg_loader_LDFLAGS = \
	$(AM_LDFLAGS)
rsvg_loader_LDADD = \
	$(GDK_PIXBUF_LIBS)
EXTRA_rsvg_loader_DEPENDENCIES = libpixbufloader-svg.la gdk-pixbuf-loaders

gdk-pixbuf-loaders: Makefile
	$(AM_V_GEN) ( $(GDK_PIXBUF_QUERYLOADERS) ./libpixbufloader-svg.la && $(GDK_PIXBUF_QUERYLOADERS)) > gdk-pixbuf.loaders 2>/dev/null

thumbnailerdir = $(datadir)/thumbnailers/
thumbnailer_DATA = librsvg.thumbnailer
librsvg.thumbnailer: librsvg.thumbnailer.in
	$(AM_V_GEN) $(SED) -e "s|\@bindir\@|$(bindir)|" \
	$< > $@

EXTRA_DIST = librsvg.thumbnailer.in

# Never update the live system's loaders.cache. A DESTDIR install writes a
# cache that names only the staged SVG loader so the tree can be tested with
# GDK_PIXBUF_MODULE_FILE without touching /usr.
install-data-hook:
	@if test -z "$(DESTDIR)" ; then \
	  echo "***" ; \
	  echo "*** Skipping gdk-pixbuf cache update (no DESTDIR)." ; \
	  echo "*** Refusing to write the live loaders.cache." ; \
	  echo "***" ; \
	elif test -x "$(GDK_PIXBUF_QUERYLOADERS)" ; then \
	  $(mkinstalldirs) "$(DESTDIR)$(gdk_pixbuf_binarydir)" ; \
	  LD_LIBRARY_PATH="$(DESTDIR)$(libdir)$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH}" \
	    $(GDK_PIXBUF_QUERYLOADERS) \
	      "$(DESTDIR)$(gdk_pixbuf_moduledir)/libpixbufloader-svg.so" \
	      > "$(DESTDIR)$(gdk_pixbuf_cache_file)" ; \
	  echo "*** Wrote staged cache: $(DESTDIR)$(gdk_pixbuf_cache_file)" ; \
	else \
	  echo "***" ; \
	  echo "*** Warning: gdk-pixbuf-query-loaders not executable" ; \
	  echo "***" ; \
	fi

CLEANFILES = \
	gdk-pixbuf.loaders \
	$(thumbnailer_DATA) \
	$(NULL)

DISTCLEANFILES = \
	gdk-pixbuf.loaders \
	$(NULL)

endif # ENABLE_PIXBUF_LOADER
