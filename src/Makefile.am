# plugindir is set in configure

##############################################################################
# change libgstdmtx.la to something more suitable, e.g. libmysomething.la  #
##############################################################################
plugin_LTLIBRARIES = libgstdmtx.la

# sources used to compile this plug-in
libgstdmtx_la_SOURCES = gstdmtx.c

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgstdmtx_la_CFLAGS = $(GST_CFLAGS)
libgstdmtx_la_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS) $(GSTCTRL_LIBS)
libgstdmtx_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstdmtx_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstdmtx.h
