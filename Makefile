# Makefile for ESC32 firmware
#
# ! Use of this makefile requires setup of a compatible development environment.
# ! For latest development recommendations, check here: http://autoquad.org/wiki/wiki/development/
# ! This file is ignored when building with CrossWorks Studio.
#
# All paths are relative to Makefile location.  Possible make targets:
#  all         build firmware .elf and .hex binaries
#  flash       attempt to build ../ground/loader and flash firmware to board (linux only)
#  pack        create .zip archive of generated .hex file (requires GNU zip)
#  clean       delete all built objects (not binaries or archives)
#  clean-bin   delete all binaries created in build folder (*.elf, *.bin, *.hex)
#  clean-pack  delete all archives in build folder (*.zip)
#  clean-all   run all the above clean* steps.
#
# Read comments below under "External libraries required by ESC32" for dependency details.
#
# Build from inside an svn repo folder because 'svnversion' command is used to retrieve the latest revision number.
#
# Usage examples:
#  make all                                  # default Release type builds .hex and .elf binaries
#  make all BUILD_TYPE=Debug                 # build with compiler debugging flags/options enabled
#  make all BUILD_TYPE=test INCR_BUILDNUM=0  # build a release version in a folder named "test", don't increment the buildnumber
#
# Windows needs some core GNU tools in your %PATH% (probably same place your "make" is). 
#    Required: gawk, mv, echo, rm
#    Optional: mkdir (auto-create build folders),  zip (to compress hex files using make pack)
#   Also see EXE_MKDIR variable below -- due to a naming conflict with the Windows "mkdir", you may need to specify a full path for it.
#   Recommend GnuWin32 CoreUtils http://gnuwin32.sourceforge.net/packages/coreutils.htm
#

# Include user-specific settings file, if any, in regular Makefile format.
# This file can set any default variable values you wish to override (all defaults are listed below).
# The .user file is not included with the source code distribution, so it will not be overwritten.
-include Makefile.user

# Defaults - modify here, on command line, or in Makefile.user
#
# Output folder name; Use 'Debug' to set debug compiler options;
BUILD_TYPE ?= Debug
# Path to source files - no trailing slash
SRC_PATH ?= .
# Increment build number? (0|1)  This is automatically disabled for debug builds.
INCR_BUILDNUM ?= 1
# Produced binaries file name prefix (version/revision/build/hardware info will be automatically appended)
BIN_NAME ?= esc32
# Build debug version? (0|1; true by default if build_type contains the word "debug")
ifeq ($(findstring Debug, $(BUILD_TYPE)), Debug)
	DEBUG_BUILD ?= 1
else 
	DEBUG_BUILD ?= 0
endif

# You may also use BIN_SUFFIX to append text 
# to generated bin file name after version string;
# BIN_SUFFIX = 
STMLIB_PATH = ../STM32F10x_StdPeriph_Lib_V3.5.0/Libraries/STM32F10x_StdPeriph_Driver/
CMSIS_PATH = ../STM32F10x_StdPeriph_Lib_V3.5.0/Libraries/CMSIS/CM3/


# System-specific folder paths
#
# compiler base path
CC_PATH ?= /usr/arm-none-eabi/
#CC_PATH ?= C:/devel/gcc/crossworks_for_arm_2.3

# shell commands
EXE_AWK ?= gawk 
EXE_MKDIR ?= mkdir
#EXE_MKDIR = C:/cygwin/bin/mkdir
EXE_ZIP ?= zip
# file extention for compressed files (gz for gzip, etc)
ZIP_EXT ?= zip

# Where to put the built objects and binaries.
# A sub-folder is created along this path, named as the BUILD_TYPE.
#BUILD_PATH ?= .
BUILD_PATH ?= ./build

# Add preprocessor definitions here
CC_VARS ?=


# defaults end

#
## probably don't need to change anything below here ##
#

# build/object directory
OBJ_PATH = $(BUILD_PATH)/$(BUILD_TYPE)/obj
# bin file(s) output path
BIN_PATH = $(BUILD_PATH)/$(BUILD_TYPE)

# command to execute (later, if necessary) for increasing build number in buildnum.h
CMD_BUILDNUMBER = $(shell $(EXE_AWK) '$$2 ~ /BUILDNUMBER/{ $$NF=$$NF+1 } 1' $(SRC_PATH)/buildnum.h > $(SRC_PATH)/tmp_buildnum.h && mv $(SRC_PATH)/tmp_buildnum.h $(SRC_PATH)/buildnum.h)

# get current revision and build numbers
FW_VER := $(shell $(EXE_AWK) 'BEGIN { FS = "[ \"]+" }$$2 ~ /VERSION/{print $$3}' $(SRC_PATH)/main.h)
REV_NUM := $(shell svnversion)
BUILD_NUM := $(shell $(EXE_AWK) '$$2 ~ /BUILDNUMBER/{print $$NF}' $(SRC_PATH)/buildnum.h)
ifeq ($(INCR_BUILDNUM), 1)
	BUILD_NUM := $(shell echo $$[$(BUILD_NUM)+1])
endif

# Resulting bin file names before extension
ifeq ($(DEBUG_BUILD), 1)
	# debug build gets a consistent name to simplify dev setup
	BIN_NAME := $(BIN_NAME)-debug
	INCR_BUILDNUM = 0
else
	BIN_NAME := $(BIN_NAME)v$(FW_VER).r$(REV_NUM).b$(BUILD_NUM)
	ifdef BIN_SUFFIX
		BIN_NAME := $(BIN_NAME)-$(BIN_SUFFIX)
	endif
endif

# Compiler-specific paths
CC_BIN_PATH = /usr/bin/arm-none-eabi-
CC_LIB_PATH = $(CC_PATH)/lib
CC_INC_PATH = $(CC_PATH)/include
CC = $(CC_BIN_PATH)gcc
AS = $(CC_BIN_PATH)as
LD = $(CC_BIN_PATH)ld
OBJCP = $(CC_BIN_PATH)objcopy
OBJDMP = $(CC_BIN_PATH)objdump

#
## External libraries required by ESC32
#

# all include flags for the compiler
CC_INCLUDES :=  -I$(SRC_PATH) -I$(STMLIB_PATH)/inc -I$(CMSIS_PATH)/CoreSupport/ -I$(CMSIS_PATH)/DeviceSupport/ST/STM32F10x/ -I$(CC_INC_PATH)

# compiler flags
CC_OPTS = -mcpu=cortex-m3 -mthumb -mlittle-endian -Wall -std=c99 \
-ffunction-sections -fdata-sections

# macro definitions to pass via compiler command line
#
CC_VARS += -D__TARGET_PROCESSOR=STM32F103CB -D__TARGET_MD= -DSTM32F10X_MD= -D__THUMB -DUSE_STDPERIPH_DRIVER 


# Additional target(s) to build based on conditionals
#
EXTRA_TARGETS =
ifeq ($(INCR_BUILDNUM), 1)
	EXTRA_TARGETS = BUILDNUMBER
endif

# build type flags/defs (debug vs. release)
# (exclude STARTUP_FROM_RESET in debug builds if using Rowley debugger)
ifeq ($(DEBUG_BUILD), 1)
	BT_CFLAGS = -DDEBUG -DSTARTUP_FROM_RESET -DUSE_FULL_ASSERT -ggdb -g2
else
	BT_CFLAGS = -DNDEBUG -DSTARTUP_FROM_RESET -g1 -Os
endif


# all compiler options
CFLAGS = $(CC_OPTS) $(CC_INCLUDES) $(CC_VARS) $(BT_CFLAGS)

# linker (ld) options
LINKER_OPTS = -nostartfiles -lm -lnosys --specs=nano.specs -Wl,--gc-sections -T stm32_flash.ld -L /usr/arm-none-eabi-gcc/lib/ -Wl,-Map,$(BIN_PATH)/$(BIN_NAME).map

# ESC32 code objects to create (correspond to .c source to compile)
ESC32_OBJS := main.o res.o fet.o

# STM32 related including preprocessor and startup 
STM32_SYS_OBJ_FILES =  misc.o stm32f10x_gpio.o stm32f10x_rcc.o stm32f10x_tim.o stm32f10x_dbgmcu.o \
	stm32f10x_adc.o stm32f10x_dma.o stm32f10x_usart.o stm32f10x_exti.o stm32f10x_pwr.o stm32f10x_flash.o stm32f10x_iwdg.o stm32f10x_can.o



## assemble object lists
STM32_SYS_OBJS := $(STM32_SYS_OBJ_FILES)
STM32_OBJ_TARGET := $(OBJ_PATH)

# all objects
C_OBJECTS := $(addprefix $(OBJ_PATH)/, $(ESC32_OBJS) $(STM32_SYS_OBJS) startup_stm32f10x_md.o system_stm32f10x.o core_cm3.o)

# dependency files generated by previous make runs
DEPS := $(C_OBJECTS:.o=.d)


#
## Target definitions
#

.PHONY: all clean-all clean clean-bin clean-pack pack CREATE_BUILD_FOLDER BUILDNUMBER

all: CREATE_BUILD_FOLDER $(BIN_PATH)/$(BIN_NAME).hex

clean-all: clean clean-bin clean-pack

clean:
	rm -fr $(OBJ_PATH)
	
clean-bin:
	-rm -f $(BIN_PATH)/*.elf
	-rm -f $(BIN_PATH)/*.bin
	-rm -f $(BIN_PATH)/*.hex

clean-pack:
	-rm -f $(BIN_PATH)/*.$(ZIP_EXT)
	
pack:
	@echo "Compressing binaries... "
	$(EXE_ZIP) $(BIN_PATH)/$(BIN_NAME).hex.$(ZIP_EXT) $(BIN_PATH)/$(BIN_NAME).hex

# include auto-generated depenency targets
-include $(DEPS)

$(OBJ_PATH)/%.o: $(SRC_PATH)/%.c
	@echo "## Compiling $< -> $@ ##"
	@$(CC) -c $(CFLAGS) $< -o $(basename $@).o

$(STM32_OBJ_TARGET)/%.o: $(STMLIB_PATH)/src/%.c
	@echo "## Compiling $< -> $@ ##"
	@$(CC) -c $(CFLAGS) $< -o $(basename $@).o

$(STM32_OBJ_TARGET)/startup_stm32f10x_md.o: $(CMSIS_PATH)/DeviceSupport/ST/STM32F10x/startup/gcc_ride7/startup_stm32f10x_md.s
	@echo "## Compiling $< -> $@ ##"
	@$(CC) -c $(CFLAGS) $< -o $(basename $@).o

$(STM32_OBJ_TARGET)/system_stm32f10x.o: $(CMSIS_PATH)/DeviceSupport/ST/STM32F10x/system_stm32f10x.c
	@echo "## Compiling $< -> $@ ##"
	@$(CC) -c $(CFLAGS) $< -o $(basename $@).o

$(STM32_OBJ_TARGET)/core_cm3.o: $(CMSIS_PATH)/CoreSupport/core_cm3.c
	@echo "## Compiling $< -> $@ ##"
	@$(CC) -c $(CFLAGS) $< -o $(basename $@).o

$(BIN_PATH)/$(BIN_NAME).elf: $(C_OBJECTS)
	@echo "## Linking --> $@ ##"
	@$(CC) $(LINKER_OPTS) $(CC_OPTS) -o $@ $(C_OBJECTS) $(EXTRA_OBJECTS)

$(BIN_PATH)/$(BIN_NAME).bin: $(BIN_PATH)/$(BIN_NAME).elf
	@echo "## Objcopy $< --> $@ ##"
	@$(OBJCP) -O binary $< $@

$(BIN_PATH)/$(BIN_NAME).hex: $(BIN_PATH)/$(BIN_NAME).elf
	@echo "## Objcopy $< --> $@ ##"
	@$(OBJCP) -O ihex $< $@
	@echo "## Objdump $< --> $@ ##"
	@$(OBJDMP) -S $< > $<.lst

CREATE_BUILD_FOLDER :
	@echo "Attempting to create build folders..."
	@$(EXE_MKDIR) -p $(OBJ_PATH)

BUILDNUMBER :
	@echo "Incrementing Build Number"
	$(CMD_BUILDNUMBER)

## Flash-Loader (Linux only) 			##
## Requires AQ ground tools sources	##
$(SRC_PATH)/../ground/loader: $(SRC_PATH)/../ground/loader.c
	(cd $(SRC_PATH)/../ground/ && make loader)

flash: $(SRC_PATH)/../ground/loader
	$(SRC_PATH)/../ground/loader -p $(USB_DEVICE) -b 115200 -f $(BIN_PATH)/$(BIN_NAME).hex
