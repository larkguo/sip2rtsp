#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([sip2rtsp],[1.0.0],[larkguo@gmail.com])
AM_INIT_AUTOMAKE(sip2rtsp, 1.0.0)
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
# FIXME: Replace `main' with a function in `-leXosip2':
AC_CHECK_LIB([eXosip2], [main])
# FIXME: Replace `main' with a function in `-losip2':
AC_CHECK_LIB([osip2], [main])
# FIXME: Replace `main' with a function in `-losipparser2':
AC_CHECK_LIB([osipparser2], [main])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stdlib.h string.h sys/socket.h sys/time.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([gethostbyname gettimeofday inet_ntoa memmove memset select socket strcasecmp strchr strdup strerror strncasecmp strrchr strstr strtoul])

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
