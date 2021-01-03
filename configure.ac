AC_PREREQ([2.69])
AC_INIT([Oh! Multi Video Player], [0.9], [ohhara@postech.edu], [omvp],
  [http://ohhara.sarang.net/omvp])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall])
AM_MAINTAINER_MODE([disable])
LT_INIT
AC_CONFIG_SRCDIR([omvp_main.c])
AC_CONFIG_HEADERS([config.h])
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
PKG_CHECK_MODULES([CLUTTER_GST], [clutter-gst-3.0], [clutter_gst_3_exists=yes],
  [clutter_gst_3_exists=no])
AS_IF([test x"$clutter_gst_3_exists" = x"no"],
  [PKG_CHECK_MODULES([CLUTTER_GST], [clutter-gst-2.0])])
PKG_CHECK_MODULES(CLUTTER, [clutter-1.0 > 1.12 gstreamer-1.0 glib-2.0 pango gdk-pixbuf-2.0])
AC_CHECK_HEADERS([string.h])
AC_CHECK_FUNCS([memset strchr strcspn strrchr])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
