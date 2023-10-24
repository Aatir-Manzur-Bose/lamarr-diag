# !<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
# !<<<< Makefile
# !<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

# !<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
# !<<<< Globals 
# !<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
PATH := $(PATH):$(HOME)/Lamarr-HSP/src/output/bose_a5_av400_a6432_release/host/bin
CROSS_COMPILE := aarch64-none-linux-gnu-
CXX := $(CROSS_COMPILE)g++
CC := $(CROSS_COMPILE)gcc

.DEFAULT_GOAL	:= all

# !<<<< auth flags
auth_cflags	:= 
ifeq ($(GDB),ON)
auth_cflags	+= -ggdb
else
endif
ifeq ($(OPTIMIZE),OFF)
auth_cflags	+= -O0
else
auth_cflags	+= -O2
endif
auth_cflags	+= -Wall -pthread
auth_cflags	+= -ftrack-macro-expansion=0 -fno-diagnostics-show-caret

# !<<<< make command line options:
# !<<<< build definitions
# !<<<< if make invoked like this: make OPTIMIZE=ON...
auth_defines	:=
auth_defines	+= -DMODULE=CORONADDD

ifeq ($(OPTIMIZE),OFF)
else
auth_defines	+= -DOPTIMIZE
endif
ifeq ($(GDB),ON)
auth_defines	+= -DGDB
else
endif
ifeq ($(AUTH_CLI_OFF),ON)
auth_defines	+= -DAUTH_CLI_OFF
else
endif

# !<<<< leave I2C_READ/WRITE_RETRY ON (default)
# !<<<< otherwise it doesn't work (currently)
ifeq ($(I2C_READ_RETRY),OFF)
else
auth_defines	+= -DI2C_READ_RETRY
endif
ifeq ($(I2C_WRITE_RETRY),OFF)
else
auth_defines	+= -DI2C_WRITE_RETRY
endif
ifeq ($(I2C_RETRY_DISPLAY),OFF)
else
auth_defines	+= -DI2C_RETRY_DISPLAY
endif
ifeq ($(NCURSES_ON),ON)
auth_defines	+= -DNCURSES_ON
else
endif
ifeq ($(MOF_INT),OFF)
else
auth_defines	+= -DMOF_INT
endif
# !<<<< update CFLAGS
CFLAGS 		+= $(auth_cflags) $(auth_defines)

# !<<<< update LIBFLAGS
LIBFLAGS	+= -lpthread
LIBFLAGS	+= -lc
LIBFLAGS	+= -lrt
ifeq ($(NCURSES_ON),ON)
LIBFLAGS	+= -lncurses
endif

# !<<<< auth distro base, etc, relative to .
auth_base       = .
auth_source	= $(auth_base)
auth_make	= $(auth_base)
auth_build	= $(auth_base)
diag_common	= $(auth_base)/../diag_common/

# !<<<< auth includes
auth_includes	:=
auth_includes	+= -I$(auth_base)
auth_includes	+= -I$(diag_common)
INCLUDES	+= $(auth_includes)

c_srcs		:= 
c_srcs		+= auth.c
c_srcs		+= $(auth_base)/I2cDevLinuxWrapper.c
c_srcs		+= $(diag_common)/diag_common.c
c_srcs		+= $(diag_common)/jWrite.c 

h_srcs		:=
h_srcs		+= $(auth_base)/auth.h
h_srcs		+= $(auth_base)/I2cDevLinuxWrapper.h
h_srcs		+= $(diag_common)/diag_common.h
h_srcs		+= $(auth_base)/AuthChipDefines.h
h_srcs		+= $(auth_base)/AuthChipDevice.h
h_srcs		+= $(auth_base)/TimeConvert.h
h_srcs		+= $(auth_base)/GpioDevice.h
h_srcs		+= $(diag_common)/jWrite.h

a_srcs		:=
a_srcs		+= Makefile

# !<<<< source
auth.o:		$(a_srcs) $(c_srcs) $(h_srcs) 

# !<<<< objs
a_objs		:= 
a_objs		+= auth.o 
a_objs		+= diag_common.o 
a_objs		+= I2cDevLinuxWrapper.o
a_objs		+= jWrite.o 

# !<<<< for generating emacs __TAGS		
a_tags		:= 
a_tags		+= $(c_srcs) $(h_srcs)

a_exe		= dt_auth
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
ifeq ($(DOETAGS),ON)
	etags -o __TAGS $(a_tags)	
	@echo $(CROSS_COMPILE)
endif

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
		@echo "make auth  	make auth utility"
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
	$(RM) $(auth_build)/core 
	$(RM) $(auth_build)/*.o 
	$(RM) $(auth_build)/*.a
	$(RM) $(auth_build)/*.out 
	$(RM) $(auth_build)/*.exe 
	$(RM) $(auth_build)/__TAGS 
	$(RM) $(auth_build)/*~
	$(RM) $(auth_build)/$(a_exe)


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
	adb push auth.c /opt/Bose/bin
	adb push $(diag_common)/jWrite.c /opt/Bose/bin
	adb push $(diag_common)/diag_common.c /opt/Bose/bin
	adb push $(OECORE_NATIVE_SYSROOT)/usr/bin/arm-oemllib32-linux-gnueabi/$(TARGET_PREFIX)gdb /opt/Bose/bin
endif

# !<=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
# !< test (requires target connected
# !<=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
.PHONY:	test
test:
	adb shell /opt/Bose/bin/dt_auth -l5
	adb shell /opt/Bose/bin/dt_auth -M -l5



