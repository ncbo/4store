AM_CFLAGS = -Wall -g -std=gnu99 -O2 @GLIB_CFLAGS@ @MDNS_CFLAGS@
LIBS = -lz @UUID_LIBS@ @GLIB_LIBS@ @MDNS_LIBS@ @RAPTOR_LIBS@ @CHECK_LIBS@

TESTS = check_backend_main

check_PROGRAMS = check_backend_main
check_backend_main_SOURCES = check_backend.h \
							check_ptree.c \
							check_ptable.c \
							check_mhash.c \
							check_tbchain.c \
							check_rhash.c \
							check_backend_main.c \
							$(top_builddir)/src/backend/backend.c \
							$(top_builddir)/src/backend/ptree.h \
							$(top_builddir)/src/backend/ptable.h \
							$(top_builddir)/src/backend/mhash.h \
							$(top_builddir)/src/backend/mchain.h \
							$(top_builddir)/src/backend/rhash.h \
							$(top_builddir)/src/backend/tbchain.h

check_backend_main_LDADD = $(top_builddir)/src/backend/lib4storage.a \
						   $(top_builddir)/src/common/lib4sintl.a \
						   @MDNS_LIBS@ \
						   @UUID_LIBS@ \
						   @GLIB_LIBS@ \
						   @CHECK_LIBS@
