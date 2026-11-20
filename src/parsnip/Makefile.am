## Process this file with automake to produce Makefile.in

noinst_LIBRARIES	= libparsnip.a libparrich.a
noinst_PROGRAMS		= parsnip_test parsnip_test_2 json_format

# For pianod, a "faster" Parsnip without JSON comments or position tracking.
libparsnip_a_CPPFLAGS	= -D_GNU_SOURCE
libparsnip_a_SOURCES	= parsnip.h \
			  parsnip_helpers.h \
			  parsnip_types.cpp \
			  parsnip_iterator.cpp \
			  parsnip_json.cpp \
			  parsnip_argv.h parsnip_argv.cpp \
			  parsnip_command.h parsnip_command.cpp \
			  parsnip_evaluate.h parsnip_evaluate.cpp \
			  parsnip_schema.h parsnip_schema.cpp

rich_flags		= -DPARSNIP_JSON_TRACK_POSITION \
			  -DPARSNIP_JSON_COMMENTS \
			  -DPARSNIP_JSON_HEXADECIMAL_NUMBERS \
			  -DPARSNIP_DICTIONARY_SORTED

# A "richer" version of Parsnip with parse-position tracking and comment support,
# for the JSON reformatter tool, and to unit test behavior with these features in.
libparrich_a_CPPFLAGS	= -D_GNU_SOURCE $(rich_flags)
libparrich_a_SOURCES	= $(libparsnip_a_SOURCES)

json_format_CPPFLAGS    = $(rich_flags)
json_format_LDADD	= libparrich.a
json_format_SOURCES	= json_format.cpp

parsnip_test_CPPFLAGS	= -I$(top_srcdir)/src/common
parsnip_test_LDADD	= libparsnip.a $(ncurses_LIBS)
parsnip_test_SOURCES	= parsnip_test.cpp

parsnip_test_2_CPPFLAGS	= -I$(top_srcdir)/src/common $(rich_flags)
parsnip_test_2_LDADD	= libparrich.a $(ncurses_LIBS)
parsnip_test_2_SOURCES	= $(parsnip_test_SOURCES)

TESTS			= parsnip_test parsnip_test_2

EXTRA_DIST		= schema_tests.json
