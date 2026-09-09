# ============================================================
# Project
# ============================================================

PROJECT      := startup_stm32h755_cm7
BUILD        := build
BUILD_TYPE   ?= release

# Makefile находится:
# CM7/Core/startrc/Makefile
#
# Поэтому:
# ../../ -> CM7
PROJECT_ROOT := ../..


# ============================================================
# Toolchain
# ============================================================

CC      := arm-none-eabi-gcc
AS      := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE    := arm-none-eabi-size


# ============================================================
# MCU
# ============================================================

CPU     := cortex-m7
FPU     := fpv5-d16
FLOAT   := hard

MCUFLAGS := \
	-mcpu=$(CPU) \
	-mthumb \
	-mfpu=$(FPU) \
	-mfloat-abi=$(FLOAT)


# ============================================================
# Linker
# ============================================================

LDSCRIPT := $(PROJECT_ROOT)/Core/startrc/linker.ld

LDFLAGS := $(MCUFLAGS)
LDFLAGS += -T$(LDSCRIPT)
LDFLAGS += -Wl,-Map=$(BUILD)/$(PROJECT).map
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -specs=nosys.specs


# ============================================================
# Compiler flags
# ============================================================

CFLAGS := $(MCUFLAGS)

CFLAGS += -ffunction-sections
CFLAGS += -fdata-sections
CFLAGS += -std=gnu11

CFLAGS += -DUSE_HAL_DRIVER
CFLAGS += -DSTM32H755xx
CFLAGS += -DCORE_CM7

ASFLAGS := $(MCUFLAGS)


# ============================================================
# Build type
# ============================================================

OPT_CFLAGS_debug   := -Og -g3 -DDEBUG
OPT_CFLAGS_release := -O2

OPT_LDFLAGS_debug   := -g3
OPT_LDFLAGS_release :=

CFLAGS  += $(OPT_CFLAGS_$(BUILD_TYPE))
LDFLAGS += $(OPT_LDFLAGS_$(BUILD_TYPE))

CFLAGS += -Wall -Wextra


# ============================================================
# Host utilities
# ============================================================

ifeq ($(OS),Windows_NT)

MKDIR_P = powershell -NoProfile -Command \
	"New-Item -ItemType Directory -Force -Path '$(subst /,\,$1)' | Out-Null"

RM_RF = powershell -NoProfile -Command \
	"if (Test-Path '$(subst /,\,$1)') { Remove-Item -Recurse -Force '$(subst /,\,$1)' }"

else

MKDIR_P = mkdir -p $1
RM_RF   = rm -rf $1

endif


# ============================================================
# Include directories
# ============================================================

INCLUDES := \
	-I$(PROJECT_ROOT)/Core/startrc \
	-I$(PROJECT_ROOT)/Drivers/STM32H7xx_HAL_Driver/Inc \
	-I$(PROJECT_ROOT)/Drivers/STM32H7xx_HAL_Driver/Inc/Legacy \
	-I$(PROJECT_ROOT)/Drivers/CMSIS/Include \
	-I$(PROJECT_ROOT)/Drivers/CMSIS/Device/ST/STM32H7xx/Include


# ============================================================
# HAL configuration
# ============================================================

HAL_CONF := \
	$(PROJECT_ROOT)/Core/startrc/stm32h7xx_hal_conf.h

HAL_DRIVER_DIR := \
	$(PROJECT_ROOT)/Drivers/STM32H7xx_HAL_Driver/Src


# ============================================================
# Automatically detect enabled HAL modules
# ============================================================

ENABLED_MODULES := $(shell \
	grep "^#define HAL_.*_MODULE_ENABLED" "$(HAL_CONF)" | \
	sed 's/#define HAL_\(.*\)_MODULE_ENABLED.*/\1/' | \
	tr '[:upper:]' '[:lower:]' \
)


# ============================================================
# HAL source files
# ============================================================

HAL_FILES := \
	$(foreach mod,$(ENABLED_MODULES), \
		$(HAL_DRIVER_DIR)/stm32h7xx_hal_$(mod).c)


HAL_EX_FILES := \
	$(wildcard \
		$(foreach mod,$(ENABLED_MODULES), \
			$(HAL_DRIVER_DIR)/stm32h7xx_hal_$(mod)_ex.c))


# ============================================================
# Project source files
# ============================================================

C_SOURCES := \
	$(PROJECT_ROOT)/Core/startrc/stm32h7xx_it.c \
	$(PROJECT_ROOT)/Core/Src/syscalls.c \
	$(PROJECT_ROOT)/Core/Src/sysmem.c \
	$(PROJECT_ROOT)/Common/Src/system_stm32h7xx_dualcore_boot_cm4_cm7.c \
	$(PROJECT_ROOT)/Core/startrc/main.c


# ============================================================
# Add HAL sources
# ============================================================

C_SOURCES += \
	$(HAL_DRIVER_DIR)/stm32h7xx_hal.c

C_SOURCES += \
	$(HAL_DRIVER_DIR)/stm32h7xx_hal_cortex.c

C_SOURCES += \
	$(HAL_FILES)

C_SOURCES += \
	$(HAL_EX_FILES)


# Remove duplicate source files
C_SOURCES := $(sort $(C_SOURCES))


# ============================================================
# Object files
#
# Example:
#
# ../../Common/Src/foo.c
#        ↓
# build/Common/Src/foo.o
#
# ../../Core/Src/bar.c
#        ↓
# build/Core/Src/bar.o
# ============================================================

OBJECTS := \
	$(patsubst $(PROJECT_ROOT)/%.c,$(BUILD)/%.o,$(C_SOURCES))


# ============================================================
# Targets
# ============================================================

ELF := $(BUILD)/$(PROJECT).elf
BIN := $(BUILD)/$(PROJECT).bin
HEX := $(BUILD)/$(PROJECT).hex
MAP := $(BUILD)/$(PROJECT).map


# ============================================================
# Default target
# ============================================================

all: $(ELF) $(BIN) $(HEX) size


# ============================================================
# Compile C
# ============================================================

$(BUILD)/%.o: $(PROJECT_ROOT)/%.c
	@$(call MKDIR_P,$(dir $@))
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@


# ============================================================
# Compile Assembly
# ============================================================

$(BUILD)/%.o: $(PROJECT_ROOT)/%.S
	@$(call MKDIR_P,$(dir $@))
	$(AS) $(ASFLAGS) -c $< -o $@


# ============================================================
# Link
# ============================================================

$(ELF): $(OBJECTS)
	@$(call MKDIR_P,$(dir $@))
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@


# ============================================================
# Binary
# ============================================================

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@


# ============================================================
# Intel HEX
# ============================================================

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@


# ============================================================
# Size
# ============================================================

size: $(ELF)
	$(SIZE) $(ELF)


# ============================================================
# Clean
# ============================================================

clean:
	@$(call RM_RF,$(BUILD))


# ============================================================
# Debug / Release
# ============================================================

debug:
	$(MAKE) BUILD_TYPE=debug all

release:
	$(MAKE) BUILD_TYPE=release all


# ============================================================
# Debug information
# ============================================================

print:
	@echo "PROJECT_ROOT = $(PROJECT_ROOT)"
	@echo "BUILD        = $(BUILD)"
	@echo "BUILD_TYPE   = $(BUILD_TYPE)"
	@echo ""
	@echo "HAL_CONF:"
	@echo "$(HAL_CONF)"
	@echo ""
	@echo "ENABLED_MODULES:"
	@echo "$(ENABLED_MODULES)"
	@echo ""
	@echo "C_SOURCES:"
	@$(foreach src,$(C_SOURCES),echo "$(src)";)
	@echo ""
	@echo "OBJECTS:"
	@$(foreach obj,$(OBJECTS),echo "$(obj)";)


# ============================================================
# Phony targets
# ============================================================

.PHONY: all clean size debug release print