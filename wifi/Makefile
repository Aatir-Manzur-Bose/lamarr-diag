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
MODULE := CORONADO
.DEFAULT_GOAL	:= all

# !<<<< wifi flags
wifi_cflags	:= 
ifeq ($(GDB),ON)
wifi_cflags	+= -ggdb
else
endif
ifeq ($(OPTIMIZE),OFF)
wifi_cflags	+= -O0
else
wifi_cflags	+= -O2
endif
wifi_cflags	+= -Wall -pthread
wifi_cflags	+= -ftrack-macro-expansion=0 -fno-diagnostics-show-caret

# !<<<< make MODULE=CORONADO
# !<<<< make MODULE=RIVIERA
# !<<<< make default to riviera
wifi_defines	:=
ifeq ($(MODULE),CORONADO)
$(warning ==> setting MODULE to CORONADO MODULE=$(MODULE))
wifi_defines	+= -DMODULE=CORONADO
wifi_defines	+= -DMODULE_CORONADO
else
ifeq ($(MODULE),RIVIERA)
$(warning ==> setting MODULE to RIVIERA MODULE=$(MODULE))
wifi_defines	+= -DMODULE=RIVIERA
wifi_defines	+= -DMODULE_RIVIERA
else
# default if MODULE not specified, RIVIERA
$(warning ==> default MODULE to RIVIERA MODULE=$(MODULE))
wifi_defines	+= -DMODULE=RIVIERA
wifi_defines	+= -DMODULE_RIVIERA
endif
endif

ifeq ($(OPTIMIZE),OFF)
else
wifi_defines	+= -DOPTIMIZE
endif
ifeq ($(GDB),ON)
wifi_defines	+= -DGDB
else
endif
ifeq ($(MOF_INT),OFF)
else
wifi_defines	+= -DMOF_INT
endif

# !<<<< update CFLAGS
CFLAGS 		+= $(wifi_cflags) $(wifi_defines)

# !<<<< update LIBFLAGS
LIBFLAGS	+= -lpthread
LIBFLAGS	+= -lc
LIBFLAGS	+= -lrt
ifeq ($(NCURSES_ON),ON)
LIBFLAGS	+= -lncurses
endif

# !<<<< wifi distro base, etc, relative to .
wifi_base       = .
wifi_source	= $(wifi_base)
wifi_make	= $(wifi_base)
wifi_build	= $(wifi_base)
diag_common	= $(wifi_base)/../diag_common/

# !<<<< wifi includes
wifi_includes	:=
wifi_includes	+= -I$(wifi_base)
wifi_includes	+= -I$(diag_common)
INCLUDES	+= $(wifi_includes)


c_srcs		:=
c_srcs		+= wifi.c
c_srcs		+= $(diag_common)/diag_common.c
c_srcs		+= $(diag_common)/jWrite.c 

h_srcs		:=
h_srcs		+= wifi.h
h_srcs		+= $(diag_common)/diag_common.h
h_srcs		+= $(diag_common)/jWrite.h

a_srcs		:=
a_srcs		+= Makefile

# !<<<< source
wifi.o:		$(a_srcs) $(c_srcs) $(h_srcs) 

# !<<<< objs
a_objs		:= 
a_objs		+= wifi.o 
a_objs		+= diag_common.o
a_objs		+= jWrite.o 

# !<<<< for generating emacs __TAGS		
a_tags		:=
a_tags		+= $(c_srcs) $(h_srcs)

a_exe		= dt_wifi
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

	@echo "Environment setup. using:"
	@echo $(CC)
	@echo $(CFLAGS)
	@echo $(CPPLAGS)
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
		@echo "make wifi  	make wifi utility"
		@echo "make tags:       make __TAGS file"
		@echo "make all:       	do everything"
		@echo "make install:    do everything"
		@echo "make clean:      clean"
		@echo "make deploy:     deploy executable to device (/opt/Bose)"
		@echo "make readelf:    show ELF info for .exe"
		@echo "make help:       show this help"
		@echo "Environment setup:"
		@echo "# . ${CORONADOSDK}/environment-setup-armv7ahf-neon-oe-linux-gnueabi";
		
# !<=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
# !< clean
# !<=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
.PHONY: clean 
clean:
	$(RM) $(wifi_build)/core 
	$(RM) $(wifi_build)/*.o 
	$(RM) $(wifi_build)/*.a
	$(RM) $(wifi_build)/*.out 
	$(RM) $(wifi_build)/*.exe 
	$(RM) $(wifi_build)/__TAGS 
	$(RM) $(wifi_build)/*~
	$(RM) $(wifi_build)/$(a_exe)


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
	adb push wpa_supplicant.conf_open /opt/Bose/etc/wpa_supplicant.conf_open
	adb shell chmod +x /opt/Bose/bin/$(a_exe)	
ifeq ($(GDB),ON)
	@echo "pushing gdb and sources for debugging"
	adb push wifi.c /opt/Bose/bin
	adb push $(diag_common)/diag_common.c /opt/Bose/bin
ifeq ($(MODULE),$(filter $(MODULE),riviera RIVIERA))
	adb push $(OECORE_NATIVE_SYSROOT)/usr/bin/arm-oemllib32-linux-gnueabi/$(TARGET_PREFIX)gdb /opt/Bose/bin
else
	@echo "need gdb source recipe here"
endif
endif


# !<=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
# !< test (requires target connected
# !<=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
.PHONY:	test
test:
	adb shell /opt/Bose/bin/dt_wifi -d5 -iwww.bose.com -Iwlan0 -FTRB -l0
#	adb shell /opt/Bose/bin/dt_wifi -M -d5 -iwww.bose.com -Iwlan0 -FTRB -l0

