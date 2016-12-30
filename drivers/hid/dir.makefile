
$(call standard_dirs)

$c_CXX = x86_64-managarm-g++

$c_PKGCONF := PKG_CONFIG_SYSROOT_DIR=$(SYSROOT_PATH) \
	PKG_CONFIG_LIBDIR=$(SYSROOT_PATH)/usr/lib/pkgconfig pkg-config

$c_INCLUDES := -I$($c_GENDIR) -I$(TREE_PATH)/frigg/include \
	$(shell $($c_PKGCONF) --cflags protobuf-lite)

$c_CXXFLAGS := $(CXXFLAGS) $($c_INCLUDES)
$c_CXXFLAGS += -std=c++14 -Wall
$c_CXXFLAGS += -DFRIGG_HAVE_LIBC

$c_LIBS := -lhelix -lcofiber -lmbus_protocol -lusb_protocol \
	$(shell $($c_PKGCONF) --libs protobuf-lite)

$(call make_exec,hid, main.o)
$(call compile_cxx,$($c_SRCDIR),$($c_OBJDIR))

