
TESTS = check_common_main

check_PROGRAMS = check_common_main
check_common_main_SOURCES = check_common.h \
							check_datatypes.c \
							check_hash.c \
							check_common_main.c \
							$(top_builddir)/src/common/4s-datatypes.h \
							$(top_builddir)/src/common/4s-hash.h

check_common_main_CFLAGS = @CFLAGS@
check_common_main_LDADD = $(top_builddir)/src/common/lib4store.la \
						  @CHECK_LIBS@
