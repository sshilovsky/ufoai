TARGET             := ufoslicer

# if the linking should be static
$(TARGET)_STATIC   ?= $(STATIC)
ifeq ($($(TARGET)_STATIC),1)
$(TARGET)_LDFLAGS  += -static
endif

$(TARGET)_LINKER   := $(CXX)
$(TARGET)_FILE     := $(TARGET)$(EXE_EXT)
$(TARGET)_CFLAGS   += -DCOMPILE_MAP $(SDL_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS)
$(TARGET)_LDFLAGS  += $(PNG_LIBS) $(JPEG_LIBS) -lm -lz $(SDL_LIBS)

$(TARGET)_SRCS      = \
	tools/ufoslicer.cpp \
	\
	common/bspslicer.cpp \
	common/files.cpp \
	common/list.cpp \
	common/mem.cpp \
	common/unzip.cpp \
	common/ioapi.cpp \
	\
	tools/ufo2map/common/bspfile.cpp \
	tools/ufo2map/common/scriplib.cpp \
	\
	shared/mathlib.cpp \
	shared/aabb.cpp \
	shared/byte.cpp \
	shared/images.cpp \
	shared/parse.cpp \
	shared/shared.cpp \
	shared/utf8.cpp \
	\
	$(PNG_SRCS) \
	\
	$(JPEG_SRCS)

ifneq ($(findstring $(TARGET_OS), mingw32 mingw64),)
	$(TARGET)_SRCS+=\
		ports/windows/win_shared.cpp
else
	$(TARGET)_SRCS+= \
		ports/unix/unix_files.cpp \
		ports/unix/unix_shared.cpp \
		ports/unix/unix_main.cpp
endif

$(TARGET)_OBJS     := $(call ASSEMBLE_OBJECTS,$(TARGET))
$(TARGET)_CXXFLAGS := $($(TARGET)_CFLAGS)
$(TARGET)_CCFLAGS  := $($(TARGET)_CFLAGS)
