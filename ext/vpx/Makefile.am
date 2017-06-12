sources = \
	gstvpxdec.c \
	gstvpxenc.c \
	gstvp8dec.c \
	gstvp8enc.c \
	gstvp8utils.c \
	gstvp9dec.c \
	gstvp9enc.c \
	plugin.c

cflags = \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) \
	$(GST_CFLAGS) \
	-DGST_USE_UNSTABLE_API

libadd = \
	$(GST_PLUGINS_BASE_LIBS) -lgsttag-@GST_API_VERSION@ -lgstvideo-@GST_API_VERSION@ \
	$(GST_BASE_LIBS) $(GST_LIBS)

if USE_VPX
plugin_LTLIBRARIES = libgstvpx.la
libgstvpx_la_SOURCES = $(sources)
libgstvpx_la_CFLAGS = $(cflags) $(VPX_CFLAGS)
libgstvpx_la_LIBADD = $(libadd) $(VPX_LIBS)
libgstvpx_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstvpx_la_LIBTOOLFLAGS =  $(GST_PLUGIN_LIBTOOLFLAGS)
endif

if USE_VPX_SSE
pluginsse_LTLIBRARIES = libgstvpx-sse.la
EXTRA_libgstvpx_sse_la_DEPENDENCIES = \
	$(prefix)/lib/libvpx-sse.a
libgstvpx_sse_la_SOURCES = $(sources)
libgstvpx_sse_la_CFLAGS = $(cflags) $(VPX_SSE_CFLAGS)
libgstvpx_sse_la_LIBADD = $(libadd) $(VPX_SSE_LIBS)
libgstvpx_sse_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstvpx_sse_la_LIBTOOLFLAGS =  $(GST_PLUGIN_LIBTOOLFLAGS)
endif

if USE_VPX_AVX
pluginavx_LTLIBRARIES = libgstvpx-avx.la
EXTRA_libgstvpx_avx_la_DEPENDENCIES = \
	$(prefix)/lib/libvpx-avx.a
libgstvpx_avx_la_SOURCES = $(sources)
libgstvpx_avx_la_CFLAGS = $(cflags) $(VPX_AVX_CFLAGS)
libgstvpx_avx_la_LIBADD = $(libadd) $(VPX_AVX_LIBS)
libgstvpx_avx_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstvpx_avx_la_LIBTOOLFLAGS =  $(GST_PLUGIN_LIBTOOLFLAGS)
endif

if USE_VPX_AVX2
pluginavx2_LTLIBRARIES = libgstvpx-avx2.la
EXTRA_libgstvpx_avx2_la_DEPENDENCIES = \
	$(prefix)/lib/libvpx-avx2.a
libgstvpx_avx2_la_SOURCES = $(sources)
libgstvpx_avx2_la_CFLAGS = $(cflags) $(VPX_AVX2_CFLAGS)
libgstvpx_avx2_la_LIBADD = $(libadd) $(VPX_AVX2_LIBS)
libgstvpx_avx2_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstvpx_avx2_la_LIBTOOLFLAGS =  $(GST_PLUGIN_LIBTOOLFLAGS)
endif

if USE_VPX_AVX512
pluginavx512_LTLIBRARIES = libgstvpx-avx512.la
EXTRA_libgstvpx_avx512_la_DEPENDENCIES = \
	$(prefix)/lib/libvpx-avx512.a
libgstvpx_avx512_la_SOURCES = $(sources)
libgstvpx_avx512_la_CFLAGS = $(cflags) $(VPX_AVX512_CFLAGS)
libgstvpx_avx512_la_LIBADD = $(libadd) $(VPX_AVX512_LIBS)
libgstvpx_avx512_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstvpx_avx512_la_LIBTOOLFLAGS =  $(GST_PLUGIN_LIBTOOLFLAGS)
endif

noinst_HEADERS = \
	gstvpxdec.h \
	gstvpxenc.h \
	gstvp8dec.h \
	gstvp8enc.h \
	gstvp9dec.h \
	gstvp9enc.h \
	gstvp8utils.h

presetdir = $(datadir)/gstreamer-$(GST_API_VERSION)/presets
preset_DATA = GstVP8Enc.prs

EXTRA_DIST = $(preset_DATA)
