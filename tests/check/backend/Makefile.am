AM_CFLAGS = -Wall -g -std=gnu99 -O2 @GLIB_CFLAGS@ @MDNS_CFLAGS@
LIBS = -lz @UUID_LIBS@ @GLIB_LIBS@ @MDNS_LIBS@ @RAPTOR_LIBS@ @CHECK_LIBS@

TESTS = check_backend_main

check_PROGRAMS = check_backend_main
check_backend_main_SOURCES = check_backend.h \
							check_ptree.c \
							check_ptable.c \
							check_backend_main.c \
							$(top_builddir)/src/backend/ptree.h \
							$(top_builddir)/src/backend/ptable.h

check_backend_main_CFLAGS = @CFLAGS@
check_backend_main_LDADD = $(top_builddir)/src/backend/lib4storage.a \
						   $(top_builddir)/src/common/lib4sintl.a \
						   @MDNS_LIBS@ \
						   @UUID_LIBS@ \
						   @GLIB_LIBS@ \
						   @CHECK_LIBS@
