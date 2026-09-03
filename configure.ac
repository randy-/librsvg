m4_define([rsvg_major_version],[2])
m4_define([rsvg_minor_version],[62])
m4_define([rsvg_micro_version],[3])
m4_define([rsvg_extra_version],[])
m4_define([rsvg_version],[rsvg_major_version.rsvg_minor_version.rsvg_micro_version()rsvg_extra_version])
m4_define([rsvg_lt_version_info],m4_eval(rsvg_major_version + rsvg_minor_version):rsvg_micro_version:rsvg_minor_version)

AC_INIT([RSVG],[rsvg_version],[https://bugzilla.gnome.org/enter_bug.cgi?product=librsvg],[librsvg])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([rsvg.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.9 foreign no-dist-gzip dist-xz])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl Default off: the shipped Makefile.in/configure are for automake 1.16.
dnl Enabling maintainer mode on a newer automake tries to regenerate
dnl them and fails without aclocal-1.16.
AM_MAINTAINER_MODE

dnl ===========================================================================
dnl API versioning
dnl ===========================================================================

RSVG_API_VERSION=2.0
RSVG_API_MAJOR_VERSION=2
RSVG_API_MINOR_VERSION=0

AC_SUBST([RSVG_API_VERSION])
AC_SUBST([RSVG_API_MAJOR_VERSION])
AC_SUBST([RSVG_API_MINOR_VERSION])
AC_SUBST([RSVG_API_VERSION_U],[AS_TR_SH([$RSVG_API_VERSION])])

dnl ===========================================================================

GLIB_REQUIRED=2.12.0
GIO_REQUIRED=2.24.0
LIBXML_REQUIRED=2.9.0
CAIRO_REQUIRED=1.2.0
PANGO_REQUIRED=1.38.0
GDK_PIXBUF_REQUIRED=2.20
GTK3_REQUIRED=3.10.0
CROCO_REQUIRED=0.6.1

# ===========================================================================

AC_SUBST([RSVG_LT_VERSION_INFO],[rsvg_lt_version_info])
AC_SUBST([LIBRSVG_MAJOR_VERSION],[rsvg_major_version])
AC_SUBST([LIBRSVG_MINOR_VERSION],[rsvg_minor_version])
AC_SUBST([LIBRSVG_MICRO_VERSION],[rsvg_micro_version])

dnl ===========================================================================

AC_PROG_LN_S
AC_PROG_CC
AC_PROG_CPP
AC_PROG_AWK
AM_PROG_CC_C_O

AC_C_BIGENDIAN

PKG_PROG_PKG_CONFIG

LT_INIT([win32-dll])
LT_LIB_M

dnl ===========================================================================

#AC_CANONICAL_HOST

AC_MSG_CHECKING([for some Win32 platform])
case "$host" in
  *-*-mingw*|*-*-cygwin*)
    platform_win32=yes
    ;;
  *)
    platform_win32=no
    ;;
esac
AC_MSG_RESULT([$platform_win32])
AM_CONDITIONAL(PLATFORM_WIN32, test "$platform_win32" = "yes")

AC_MSG_CHECKING([for native Win32])
case "$host" in
  *-*-mingw*)
    native_win32=yes
    ;;
  *)
    native_win32=no
    ;;
esac
AC_MSG_RESULT([$native_win32])
AM_CONDITIONAL(OS_WIN32, test "$native_win32" = "yes")

dnl ===========================================================================

GLIB_MKENUMS=`$PKG_CONFIG --variable=glib_mkenums glib-2.0`
AC_SUBST(GLIB_MKENUMS)

PKG_CHECK_MODULES(LIBRSVG, \
	gdk-pixbuf-2.0 >= $GDK_PIXBUF_REQUIRED \
	glib-2.0 >= $GLIB_REQUIRED \
	gio-2.0 >= $GIO_REQUIRED \
	libxml-2.0 >= $LIBXML_REQUIRED \
	pangocairo >= $PANGO_REQUIRED \
	pangoft2 >= $PANGO_REQUIRED \
	cairo >= $CAIRO_REQUIRED \
	cairo-png >= $CAIRO_REQUIRED)

dnl WebP in SVG <image>: decode with libwebp (not gdk-pixbuf's optional
dnl webp loader). Default is auto: enable when libwebp is present.
AC_ARG_ENABLE([webp],
  [AS_HELP_STRING([--disable-webp],[Disable WebP in SVG <image> (default=auto)])],
  [],[enable_webp=auto])
have_libwebp=no
if test "x$enable_webp" != "xno"; then
  PKG_CHECK_MODULES([LIBWEBP],[libwebp],[have_libwebp=yes],[
    if test "x$enable_webp" = "xyes"; then
      AC_MSG_ERROR([--enable-webp was given, but libwebp was not found])
    fi
    have_libwebp=no
  ])
fi
if test "x$have_libwebp" = "xyes"; then
  AC_DEFINE([HAVE_LIBWEBP],[1],[Decode WebP in SVG <image> with libwebp])
  LIBRSVG_CFLAGS="$LIBRSVG_CFLAGS $LIBWEBP_CFLAGS"
  LIBRSVG_LIBS="$LIBRSVG_LIBS $LIBWEBP_LIBS"
fi
AM_CONDITIONAL([HAVE_LIBWEBP],[test "x$have_libwebp" = "xyes"])

dnl PNG in SVG <image>: decode with libpng (not gdk-pixbuf's PNG loader).
AC_ARG_ENABLE([png],
  [AS_HELP_STRING([--disable-png],[Disable standalone PNG in SVG <image> (default=auto)])],
  [],[enable_png=auto])
have_libpng=no
if test "x$enable_png" != "xno"; then
  PKG_CHECK_MODULES([LIBPNG],[libpng],[have_libpng=yes],[
    if test "x$enable_png" = "xyes"; then
      AC_MSG_ERROR([--enable-png was given, but libpng was not found])
    fi
    have_libpng=no
  ])
fi
if test "x$have_libpng" = "xyes"; then
  AC_DEFINE([HAVE_LIBPNG],[1],[Decode PNG in SVG <image> with libpng])
  LIBRSVG_CFLAGS="$LIBRSVG_CFLAGS $LIBPNG_CFLAGS"
  LIBRSVG_LIBS="$LIBRSVG_LIBS $LIBPNG_LIBS"
fi
AM_CONDITIONAL([HAVE_LIBPNG],[test "x$have_libpng" = "xyes"])

dnl JPEG in SVG <image>: libjpeg-turbo (libjpeg.pc) or libjpeg.
AC_ARG_ENABLE([jpeg],
  [AS_HELP_STRING([--disable-jpeg],[Disable standalone JPEG in SVG <image> (default=auto)])],
  [],[enable_jpeg=auto])
have_libjpeg=no
libjpeg_pc=
if test "x$enable_jpeg" != "xno"; then
  PKG_CHECK_MODULES([LIBJPEG],[libjpeg],[have_libjpeg=yes libjpeg_pc=libjpeg],[
    PKG_CHECK_MODULES([LIBJPEG],[libturbojpeg],[have_libjpeg=yes libjpeg_pc=libturbojpeg],[
      if test "x$enable_jpeg" = "xyes"; then
        AC_MSG_ERROR([--enable-jpeg was given, but libjpeg was not found])
      fi
      have_libjpeg=no
    ])
  ])
fi
if test "x$have_libjpeg" = "xyes"; then
  AC_DEFINE([HAVE_LIBJPEG],[1],[Decode JPEG in SVG <image> with libjpeg])
  LIBRSVG_CFLAGS="$LIBRSVG_CFLAGS $LIBJPEG_CFLAGS"
  LIBRSVG_LIBS="$LIBRSVG_LIBS $LIBJPEG_LIBS"
fi
AM_CONDITIONAL([HAVE_LIBJPEG],[test "x$have_libjpeg" = "xyes"])

dnl GIF in SVG <image>: decode with giflib (not gdk-pixbuf's GIF loader).
AC_ARG_ENABLE([gif],
  [AS_HELP_STRING([--disable-gif],[Disable standalone GIF in SVG <image> (default=auto)])],
  [],[enable_gif=auto])
have_libgif=no
have_libgif_pc=no
if test "x$enable_gif" != "xno"; then
  PKG_CHECK_MODULES([LIBGIF],[libgif],[have_libgif=yes have_libgif_pc=yes],[
    AC_CHECK_HEADER([gif_lib.h],[
      AC_CHECK_LIB([gif],[DGifOpen],[
        have_libgif=yes
        LIBGIF_LIBS="-lgif"
        LIBGIF_CFLAGS=""
      ],[have_libgif=no])
    ],[have_libgif=no])
    if test "x$have_libgif" != "xyes" && test "x$enable_gif" = "xyes"; then
      AC_MSG_ERROR([--enable-gif was given, but giflib was not found])
    fi
  ])
fi
if test "x$have_libgif" = "xyes"; then
  AC_DEFINE([HAVE_LIBGIF],[1],[Decode GIF in SVG <image> with giflib])
  LIBRSVG_CFLAGS="$LIBRSVG_CFLAGS $LIBGIF_CFLAGS"
  LIBRSVG_LIBS="$LIBRSVG_LIBS $LIBGIF_LIBS"
fi
AM_CONDITIONAL([HAVE_LIBGIF],[test "x$have_libgif" = "xyes"])

dnl AVIF in SVG <image>: decode with libavif only. Do not invent an AV1
dnl decoder. Default is auto: enable when pkg-config libavif succeeds.
dnl A runtime libavif.so without headers / .pc stays off.
AC_ARG_ENABLE([avif],
  [AS_HELP_STRING([--disable-avif],[Disable standalone AVIF in SVG <image> (default=auto)])],
  [],[enable_avif=auto])
have_libavif=no
if test "x$enable_avif" != "xno"; then
  PKG_CHECK_MODULES([LIBAVIF],[libavif],[have_libavif=yes],[
    if test "x$enable_avif" = "xyes"; then
      AC_MSG_ERROR([--enable-avif was given, but libavif was not found (pkg-config libavif)])
    fi
    have_libavif=no
  ])
fi
if test "x$have_libavif" = "xyes"; then
  AC_DEFINE([HAVE_LIBAVIF],[1],[Decode AVIF in SVG <image> with libavif])
  LIBRSVG_CFLAGS="$LIBRSVG_CFLAGS $LIBAVIF_CFLAGS"
  LIBRSVG_LIBS="$LIBRSVG_LIBS $LIBAVIF_LIBS"
fi
AM_CONDITIONAL([HAVE_LIBAVIF],[test "x$have_libavif" = "xyes"])

dnl BMP in SVG <image>: in-tree decoder (no libbmp). Default on.
dnl --disable-bmp omits it; --enable-bmp is always valid.
AC_ARG_ENABLE([bmp],
  [AS_HELP_STRING([--disable-bmp],[Disable standalone BMP in SVG <image> (default=auto/on)])],
  [],[enable_bmp=auto])
have_bmp=yes
if test "x$enable_bmp" = "xno"; then
  have_bmp=no
fi
if test "x$have_bmp" = "xyes"; then
  AC_DEFINE([HAVE_BMP],[1],[Decode BMP in SVG <image> with an in-tree decoder])
fi
AM_CONDITIONAL([HAVE_BMP],[test "x$have_bmp" = "xyes"])

dnl pkg-config Requires.private: always-linked modules, plus rasters
dnl that were found via pkg-config. giflib without a .pc goes in
dnl Libs.private. Do not list an optional module when it is off.
RSVG_PC_REQUIRES_PRIVATE="pangocairo pangoft2 libxml-2.0 cairo-png"
RSVG_PC_LIBS_PRIVATE=""
if test "x$have_libpng" = "xyes"; then
  RSVG_PC_REQUIRES_PRIVATE="$RSVG_PC_REQUIRES_PRIVATE libpng"
fi
if test "x$have_libjpeg" = "xyes" && test -n "$libjpeg_pc"; then
  RSVG_PC_REQUIRES_PRIVATE="$RSVG_PC_REQUIRES_PRIVATE $libjpeg_pc"
fi
if test "x$have_libwebp" = "xyes"; then
  RSVG_PC_REQUIRES_PRIVATE="$RSVG_PC_REQUIRES_PRIVATE libwebp"
fi
if test "x$have_libgif" = "xyes"; then
  if test "x$have_libgif_pc" = "xyes"; then
    RSVG_PC_REQUIRES_PRIVATE="$RSVG_PC_REQUIRES_PRIVATE libgif"
  else
    RSVG_PC_LIBS_PRIVATE="$RSVG_PC_LIBS_PRIVATE -lgif"
  fi
fi
if test "x$have_libavif" = "xyes"; then
  RSVG_PC_REQUIRES_PRIVATE="$RSVG_PC_REQUIRES_PRIVATE libavif"
fi
AC_SUBST([RSVG_PC_REQUIRES_PRIVATE])
AC_SUBST([RSVG_PC_LIBS_PRIVATE])

dnl libcroco is discontinued. css/ is the only engine. libcroco/ stays
dnl in the tree as an unbuilt archive. Never PKG_CHECK_MODULES for
dnl libcroco-0.6. Never AC_CONFIG_SUBDIRS([libcroco]).
case " $ac_configure_args " in
  *enable-croco*|*disable-croco*)
    AC_MSG_ERROR([libcroco is discontinued; css/ is the only CSS engine])
    ;;
esac
if test -n "$PKG_CONFIG" && "$PKG_CONFIG" --exists libcroco-0.6 2>/dev/null; then
  AC_MSG_NOTICE([ignoring host libcroco-0.6 from pkg-config; css/ is the only engine])
fi
dnl automake 1.16 is required only for maintainer-mode rebuilds of
dnl the shipped Makefile.in files.
case " $ac_configure_args " in
  *" --enable-maintainer-mode "*) ;;
  *) ac_configure_args="$ac_configure_args --disable-maintainer-mode" ;;
esac

PKG_CHECK_MODULES([GTHREAD],[gthread-2.0 >= $GLIB_REQUIRED])

PKG_CHECK_MODULES([GMODULE],[gmodule-2.0])

case "$host" in
  *-*-mingw*)
    PKG_CHECK_MODULES([RSVG_CONVERT],[gio-2.0 gio-windows-2.0 gdk-pixbuf-2.0 cairo pangocairo])
    ;;
  *)
    PKG_CHECK_MODULES([RSVG_CONVERT],[gio-2.0 gio-unix-2.0 gdk-pixbuf-2.0 cairo pangocairo])
    ;;
esac

GLIB_TESTS

dnl ===========================================================================

AC_CHECK_FUNCS(strtok_r)

# ===========================================================================
# GTK
# ===========================================================================

PKG_CHECK_MODULES([GTK3],[gtk+-3.0 >= $GTK3_REQUIRED],[have_gtk_3=yes],[have_gtk_3=no])

GTK3_BINARY_VERSION=

if test "$have_gtk_3" = "yes"; then
    GTK3_BINARY_VERSION="`$PKG_CONFIG --variable=gtk_binary_version gtk+-3.0`"
fi

AC_SUBST([GTK3_BINARY_VERSION])

AM_CONDITIONAL([HAVE_GTK_3],[test "$have_gtk_3" = "yes"])

dnl ===========================================================================
dnl GDK-Pixbuf SVG loader
dnl ===========================================================================

AC_MSG_CHECKING([whether to build the GDK-Pixbuf SVG loader])
AC_ARG_ENABLE([pixbuf-loader],
  [AS_HELP_STRING([--disable-pixbuf-loader],[Disable a RSVG based GdkPixbuf loader (default=yes)])],
  [],[enable_pixbuf_loader=yes])
AC_MSG_RESULT([$enable_pixbuf_loader])

gdk_pixbuf_moduledir=
if test "$enable_pixbuf_loader" = "yes"; then
  PKG_CHECK_MODULES([GDK_PIXBUF],[gdk-pixbuf-2.0 >= $GDK_PIXBUF_REQUIRED])

  AC_PATH_PROGS([GDK_PIXBUF_QUERYLOADERS],[gdk-pixbuf-query-loaders gdk-pixbuf-query-loaders-64 gdk-pixbuf-query-loaders-32],[none])

  if test "x$GDK_PIXBUF_QUERYLOADERS" = "xnone"; then
    GDK_PIXBUF_QUERYLOADERS="`$PKG_CONFIG --variable=gdk_pixbuf_query_loaders gdk-pixbuf-2.0 2>/dev/null`"
  fi

  if test -z "$GDK_PIXBUF_QUERYLOADERS" || test "x$GDK_PIXBUF_QUERYLOADERS" = "xnone" || test ! -x "$GDK_PIXBUF_QUERYLOADERS"; then
    AC_MSG_ERROR([gdk-pixbuf-query-loaders not found in PATH or via pkg-config])
  fi

  # First try the standalone gdk-pixbuf
  gdk_pixbuf_binary_version="`$PKG_CONFIG --variable=gdk_pixbuf_binary_version gdk-pixbuf-2.0`"
  gdk_pixbuf_binarydir="`$PKG_CONFIG --variable=gdk_pixbuf_binarydir gdk-pixbuf-2.0`"
  gdk_pixbuf_moduledir=`$PKG_CONFIG --variable gdk_pixbuf_moduledir gdk-pixbuf-2.0`
  gdk_pixbuf_cache_file=`$PKG_CONFIG --variable gdk_pixbuf_cache_file gdk-pixbuf-2.0`
fi

AC_SUBST([gdk_pixbuf_binary_version])
AC_SUBST([gdk_pixbuf_binarydir])
AC_SUBST([gdk_pixbuf_moduledir])
AC_SUBST([gdk_pixbuf_cache_file])

AM_CONDITIONAL([ENABLE_PIXBUF_LOADER],[test "$enable_pixbuf_loader" = "yes"])

AM_CONDITIONAL(CROSS_COMPILING, test $cross_compiling = yes)

##########################################################
# Check for -Bsymbolic-functions linker flag used to avoid
# intra-library PLT jumps, if available.
##########################################################

AC_ARG_ENABLE([Bsymbolic],
  [AS_HELP_STRING([--disable-Bsymbolic],
                  [disable linking with -Bsymbolic])],
  [],[enable_Bsymbolic=yes])

BSYMBOLIC_LDFLAG=
if test "$enable_Bsymbolic" != "no"; then
  CC_CHECK_LDFLAGS(["-Wl,-Bsymbolic-functions"],
    [BSYMBOLIC_LDFLAG="-Wl,-Bsymbolic-functions"],
    [if test "$enable_Bsymbolic" = "auto"; then
       AC_MSG_WARN([-Bsymbolic not supported by ld; disabling])
       enable_Bsymbolic=no
     else
       AC_MSG_ERROR([-Bsymbolic requested but not supported by ld. Use --disable-Bsymbolic to disable])
     fi])
fi

AC_SUBST([BSYMBOLIC_LDFLAG])

dnl API HTML is Doxygen (doc/Doxyfile).
dnl Shipped Makefile.in still substitute these unused names.
GTKDOC_CHECK=
GTKDOC_CHECK_PATH=
GTKDOC_DEPS_CFLAGS=
GTKDOC_DEPS_LIBS=
GTKDOC_MKPDF=
GTKDOC_REBASE=
HTML_DIR=
AC_SUBST([GTKDOC_CHECK])
AC_SUBST([GTKDOC_CHECK_PATH])
AC_SUBST([GTKDOC_DEPS_CFLAGS])
AC_SUBST([GTKDOC_DEPS_LIBS])
AC_SUBST([GTKDOC_MKPDF])
AC_SUBST([GTKDOC_REBASE])
AC_SUBST([HTML_DIR])
AM_CONDITIONAL([ENABLE_GTK_DOC], [false])
AM_CONDITIONAL([GTK_DOC_BUILD_HTML], [false])
AM_CONDITIONAL([GTK_DOC_BUILD_PDF], [false])
AM_CONDITIONAL([GTK_DOC_USE_LIBTOOL], [false])
AM_CONDITIONAL([HAVE_GTK_DOC], [false])
AC_SUBST([GLIB_PREFIX],[$($PKG_CONFIG --variable=prefix glib-2.0)])
AC_SUBST([GDK_PIXBUF_PREFIX],[$($PKG_CONFIG --variable=prefix gdk-pixbuf-2.0)])
AC_SUBST([GTK_PREFIX],[$($PKG_CONFIG --variable=prefix gdk-pixbuf-2.0)])
AC_SUBST([CAIRO_PREFIX],[$($PKG_CONFIG --variable=prefix cairo)])

dnl ===========================================================================
dnl Do we build the miscellaneous tools provided with librsvg ?

AC_ARG_ENABLE(tools,
              AS_HELP_STRING([--disable-tools],
                             [do not build miscellaneous tools [default=no]]),
              [build_misc_tools=$enableval],
              [build_misc_tools=yes])

AC_MSG_CHECKING(whether to build miscellaneous tools)
if test "x$build_misc_tools" = "xyes" ; then
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi
AM_CONDITIONAL([BUILD_MISC_TOOLS], [test "x$build_misc_tools" = "xyes"])

dnl ===========================================================================
dnl Build introspectable bindings

# No automagic please!
if test -z "$enable_introspection"; then
  enable_introspection=yes
fi

GOBJECT_INTROSPECTION_CHECK([0.10.8])

# Vala bindings
VAPIGEN_CHECK([0.17.1.26],,,[no])

dnl ===========================================================================

m4_copy([AC_DEFUN],[glib_DEFUN])
glib_DEFUN([GLIB_LC_MESSAGES],
  [AC_CHECK_HEADERS([locale.h])
    if test $ac_cv_header_locale_h = yes; then
    AC_CACHE_CHECK([for LC_MESSAGES], am_cv_val_LC_MESSAGES,
      [AC_TRY_LINK([#include <locale.h>], [return LC_MESSAGES],
       am_cv_val_LC_MESSAGES=yes, am_cv_val_LC_MESSAGES=no)])
    if test $am_cv_val_LC_MESSAGES = yes; then
      AC_DEFINE(HAVE_LC_MESSAGES, 1,
        [Define if your <locale.h> file defines LC_MESSAGES.])
    fi
  fi])

GLIB_LC_MESSAGES

dnl ===========================================================================

# Check for pangoft2 and fontconfig, which is optional on Windows

PKG_CHECK_MODULES([PANGOFT2],[pangoft2 >= $PANGO_REQUIRED],[have_pangoft2=yes],[have_pangoft2=no])
PKG_CHECK_MODULES([FONTCONFIG],[fontconfig],[have_fontconfig=yes],[have_fontconfig=no])

if test "x$native_win32" != "xyes"; then
  if test "x$have_pangoft2" != "xyes" -o "x$have_fontconfig" != "xyes"; then
    AC_MSG_ERROR([pangoft2 and fontconfig are required for non-Windows platforms.])
  fi
fi

if test "x$have_pangoft2" = "xyes" -a "x$have_fontconfig" = "xyes"; then
  AC_DEFINE(HAVE_PANGOFT2, 1, [Have the pangoft2 library])
fi

dnl ===========================================================================

# Check whether MSVC toolset is explicitly set
AM_CONDITIONAL(MSVC_BASE_NO_TOOLSET_SET, [test x$MSVC_BASE_TOOLSET = x])
AM_CONDITIONAL(MSVC_NO_TOOLSET_SET, [test x$MSVC_TOOLSET = x])

dnl ===========================================================================

AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_LDFLAGS])

AC_CONFIG_FILES([
rsvg-features.h
rsvg-version.h
Makefile
librsvg-zip
gdk-pixbuf-loader/Makefile
tests/Makefile
tools/Makefile
doc/Makefile
config.h.win32
build/Makefile
build/win32/Makefile
build/win32/vs9/Makefile
build/win32/vs9/rsvg-version-paths.vsprops
build/win32/vs10/Makefile
build/win32/vs10/rsvg-version-paths.props
build/win32/vs11/Makefile
build/win32/vs12/Makefile
build/win32/vs14/Makefile
build/win32/vs15/Makefile
])

AC_CONFIG_FILES([librsvg-${RSVG_API_VERSION}.pc:librsvg.pc.in],[],[RSVG_API_VERSION=$RSVG_API_VERSION])
AC_CONFIG_FILES([librsvg-${RSVG_API_VERSION}-uninstalled.pc:librsvg-uninstalled.pc.in],[],[RSVG_API_VERSION=$RSVG_API_VERSION])

AC_OUTPUT

dnl =============================================================================================
echo "
librsvg-$VERSION

	prefix:                         ${prefix}
	compiler:                       ${CC}
	
	Build introspectable bindings:  ${found_introspection}
	Build Vala bindings:            ${enable_vala}
	Build GdkPixbuf loader:         ${enable_pixbuf_loader}
        GTK+ $GTK3_REQUIRED or later:           ${have_gtk_3}
	WebP in SVG <image> (libwebp):  ${have_libwebp}
	PNG in SVG <image> (libpng):    ${have_libpng}
	JPEG in SVG <image> (libjpeg):  ${have_libjpeg}
	GIF in SVG <image> (giflib):    ${have_libgif}
	AVIF in SVG <image> (libavif):  ${have_libavif}
	BMP in SVG <image> (in-tree):   ${have_bmp}
	libcroco:                       discontinued (unbuilt archive in libcroco/; css/ only)
	Build miscellaneous tools:      ${build_misc_tools}
"
