# !<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
# !<<<< Makefile
# !<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
# !<<<< Globals 
# !<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
PATH := $(PATH):$(HOME)/Lamarr-HSP/src/output/bose_a5_av400_a6432_release/host/bin
CROSS_COMPILE := aarch64-none-linux-gnu-
CXX := $(CROSS_COMPILE)g++
CC := $(CROSS_COMPILE)gcc
.DEFAULT_GOAL	:= all

# !<<<< so flags
so_cflags	:= 
ifeq ($(GDB),ON)
so_cflags	+= -ggdb
else
endif
ifeq ($(OPTIMIZE),OFF)
so_cflags	+= -O0
else
so_cflags	+= -O2
endif
so_cflags	+= -Wall -pthread
so_cflags	+= -ftrack-macro-expansion=0 -fno-diagnostics-show-caret

# !<<<< make command line options:
# !<<<< build definitions
# !<<<< if make invoked like this: make OPTIMIZE=ON...
so_defines	:=
ifeq ($(OPTIMIZE),OFF)
else
so_defines	+= -DOPTIMIZE
endif
ifeq ($(GDB),ON)
so_defines	+= -DGDB
else
endif
ifeq ($(SO_CLI_OFF),ON)
so_defines	+= -DSO_CLI_OFF
else
endif

ifeq ($(NCURSES_ON),ON)
so_defines	+= -DNCURSES_ON
else
endif
ifeq ($(MOF_INT),OFF)
else
so_defines	+= -DMOF_INT
endif
# !<<<< update CFLAGS
CFLAGS 		+= $(so_cflags) $(so_defines)

# !<<<< update LIBFLAGS
LIBFLAGS	+= -lpthread
LIBFLAGS	+= -lc
LIBFLAGS	+= -lrt
ifeq ($(NCURSES_ON),ON)
LIBFLAGS	+= -lncurses
endif

# !<<<< so distro base, etc, relative to .
so_base       	= .
so_source	= $(so_base)
so_make		= $(so_base)
so_build	= $(so_base)
diag_common	= $(so_base)/../diag_common/

# !<<<< so includes
so_includes	:=
so_includes	+= -I$(so_base)
so_includes	+= -I$(diag_common)
INCLUDES	+= $(so_includes)

c_srcs		:= 
c_srcs		+= so.c
c_srcs		+= $(diag_common)/diag_common.c
c_srcs		+= $(diag_common)/jWrite.c 

h_srcs		:=
h_srcs		+= $(so_base)/so.h
h_srcs		+= $(diag_common)/diag_common.h
h_srcs		+= $(diag_common)/jWrite.h

a_srcs		:=
a_srcs		+= Makefile

# !<<<< source
so.o:		$(a_srcs) $(c_srcs) $(h_srcs) 

# !<<<< objs
a_objs		:= 
a_objs		+= so.o 
a_objs		+= diag_common.o 
a_objs		+= jWrite.o 

# !<<<< for generating emacs __TAGS		
a_tags		:= 
a_tags		+= $(c_srcs) $(h_srcs)

a_exe		= dt_so
$(a_exe):	$(a_objs)

# !<=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
# !< copy
# !<=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
.PHONY: copy
copy:
	cp $(a_exe) $(DEST)

# !<=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
# !< all
# !<=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
.PHONY: install
install:	all

.PHONY: all
all:
	$(CXX) -c $(INCLUDES) $(CFLAGS) $(c_srcs)
	$(CXX) $(a_objs) $(LIBFLAGS) -o $(a_exe)

# !<=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
# !< tags
# !<=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
.PHONY: tags
tags:
		etags -o __TAGS $(a_tags)	


# !<=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
# !< help
# !<=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
.PHONY: help
help:	
		@echo "help"
		@echo "make so  	make so utility"
		@echo "make tags:       make __TAGS file"
		@echo "make all:       	do everything"
		@echo "make install:    do everything"
		@echo "make clean:      clean"
		@echo "make deploy:     deploy executable to device (/opt/Bose)"
		@echo "make readelf:    show ELF info for .exe"
		@echo "make test:       run regression, requires device connected"
		@echo "make help:       show this help"
		@echo "Environment setup:"
		@echo "# . /scratch/Riviera-SDK-2.0/environment-setup-cortexa53-vfp-neon-oemllib32-linux-gnueabi"

# !<=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
# !< clean
# !<=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
.PHONY: clean 
clean:
	$(RM) $(so_build)/core 
	$(RM) $(so_build)/*.o 
	$(RM) $(so_build)/*.a
	$(RM) $(so_build)/*.out 
	$(RM) $(so_build)/*.exe 
	$(RM) $(so_build)/__TAGS 
	$(RM) $(so_build)/*~
	$(RM) $(so_build)/$(a_exe)


# !<=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
# !< readelf (info)
# !<=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
.PHONY:	readelf
readelf:
	$(TARGET_PREFIX)readelf -h -A $(a_exe) 

# !<=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
# !< deploy
# !<=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
.PHONY:	deploy
deploy:
	@echo "Make sure you're connected to DUT"
	adb push $(a_exe) /opt/Bose/bin/$(a_exe)
	adb shell chmod +x /opt/Bose/bin/$(a_exe)	
ifeq ($(GDB),ON)
	@echo "pushing gdb and sources for debugging"
	adb push so.c /opt/Bose/bin
	adb push $(diag_common)/jWrite.c /opt/Bose/bin
	adb push $(diag_common)/diag_common.c /opt/Bose/bin
	adb push $(OECORE_NATIVE_SYSROOT)/usr/bin/arm-oemllib32-linux-gnueabi/$(TARGET_PREFIX)gdb /opt/Bose/bin
endif

# !<=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
# !< test (requires target connected
# !<=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
.PHONY:	test
test:
	adb shell /opt/Bose/bin/dt_so -d10 -cALO -f1000 -a-54.0 -cALI -f1000 -a-54.0 -cARO -f1000 -a-54.0 -cARI -f1000 -a-54.0 -cCTR -f1000 -a-54.0 -cBB -f1000 -a-54.0



