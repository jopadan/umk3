AC_INIT([umk3], [0.0.0], [jopadan@ctemplar.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects serial-tests])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/])
AC_CONFIG_HEADERS([config.h])

PKG_PROG_PKG_CONFIG
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX

AC_PROG_INSTALL
AM_PROG_AR

AC_CHECK_LIB([m], [floor])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_SYS_LARGEFILE

# Checks for header files.
AC_CHECK_HEADERS([limits.h \
                  stddef.h \
                  stdint.h \
                  stdlib.h \
                  string.h ])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

AC_FUNC_ERROR_AT_LINE

AC_CONFIG_FILES([Makefile])

AC_OUTPUT

