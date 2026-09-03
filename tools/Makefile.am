if BUILD_MISC_TOOLS
noinst_PROGRAMS = 			\
	rsvg-dimensions			\
	test-performance

noinst_LTLIBRARIES = 			\
	librsvg_tools_main.la
endif

INCLUDES =				\
	-I$(top_srcdir) 		\
	-I$(top_builddir) 		\
	$(LIBRSVG_CFLAGS) 		\
	$(GTK_CFLAGS)			\
	$(GDK_X11_CFLAGS)		\
	-DG_LOG_DOMAIN=\"librsvg\" 	\
	-DLIBRSVG_DATADIR="\"$(datadir)\""

# Tools must not call deprecated public APIs. The library still
# implements those entry points and keeps the deprecated attributes.
AM_CFLAGS = @AM_CFLAGS@ -Werror=deprecated-declarations

DEPS = $(top_builddir)/librsvg-@RSVG_API_MAJOR_VERSION@.la	\
	librsvg_tools_main.la
LDADDS =				\
	$(GLIB_LIBS)			\
	$(LIBRSVG_LIBS) 		\
	$(top_builddir)/librsvg-@RSVG_API_MAJOR_VERSION@.la

librsvg_tools_main_la_SOURCES = 		\
	rsvg-tools-main.c		\
	rsvg-tools-main.h
librsvg_tools_main_la_LIBADD =

test_performance_SOURCES = test-performance.c
test_performance_LDFLAGS =
test_performance_DEPENDENCIES = $(DEPS)
test_performance_LDADD = librsvg_tools_main.la $(LDADDS) $(LIBM)

rsvg_dimensions_SOURCES = rsvg-dimensions.c
rsvg_dimensions_LDFLAGS =
rsvg_dimensions_DEPENDENCIES = $(DEPS)
rsvg_dimensions_LDADD = $(LDADDS) $(LIBM)
