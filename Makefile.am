bin_PROGRAMS = omvp
omvp_SOURCES = \
	omvp_main.c \
	omvp_gst.h \
	omvp_gst.c \
	omvp_gst_plugin.h \
	omvp_gst_plugin.c \
	omvp_gst_plugin_rtpsrc.h \
	omvp_gst_plugin_rtpsrc.c \
	omvp_vids.h \
	omvp_vids.c
omvp_CFLAGS = $(CLUTTER_CFLAGS) $(CLUTTER_GST_CFLAGS)
omvp_CFLAGS += -std=c99 -W -Wall -Wno-unused-result -pedantic
omvp_LDADD = $(CLUTTER_LIBS) $(CLUTTER_GST_LIBS)
ACLOCAL_AMFLAGS = -I m4

.PHONY: run

run: omvp
	ulimit -c unlimited ; G_MESSAGES_DEBUG=all G_DEBUG=fatal-criticals ./omvp
