# ===================== Project =====================
PROJECT := stm32h755_cm7
BUILD   := build
BUILD_TYPE ?= release

# ===================== Toolchain ===================
CC      := arm-none-eabi-gcc
AS      := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE    := arm-none-eabi-size

# ===================== MCU =========================
CPU     := cortex-m7
FPU     := fpv5-d16
FLOAT   := hard

MCUFLAGS := -mcpu=$(CPU) -mthumb -mfpu=$(FPU) -mfloat-abi=$(FLOAT)

# ===================== Linker ======================
LDSCRIPT := STM32H755ZITX_FLASH.ld

# ===================== Flags =======================
CFLAGS := $(MCUFLAGS)
CFLAGS += -ffunction-sections -fdata-sections -MMD -MP
CFLAGS += -std=gnu11
CFLAGS += -DUSE_HAL_DRIVER
CFLAGS += -DSTM32H755xx
CFLAGS += -DCORE_CM7

ASFLAGS := $(MCUFLAGS)

LDFLAGS := $(MCUFLAGS)
LDFLAGS += -T$(LDSCRIPT)
LDFLAGS += -Wl,-Map=$(BUILD)/$(PROJECT).map
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -specs=nosys.specs

OPT_CFLAGS_debug := -Og -g3 -DDEBUG
OPT_CFLAGS_release := -O2
OPT_LDFLAGS_debug := -g3
OPT_LDFLAGS_release :=

CFLAGS += $(OPT_CFLAGS_$(BUILD_TYPE))
LDFLAGS += $(OPT_LDFLAGS_$(BUILD_TYPE))

CFLAGS += -Wall -Wextra

# ===================== Host Utils ==================
ifeq ($(OS),Windows_NT)
MKDIR_P = powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(subst /,\,$1)' | Out-Null"
RM_RF   = powershell -NoProfile -Command "if (Test-Path '$(subst /,\,$1)') { Remove-Item -Recurse -Force '$(subst /,\,$1)' }"
else
MKDIR_P = mkdir -p $1
RM_RF   = rm -rf $1
endif

# ===================== Includes ====================
INCLUDES := \
-I Core/Inc \
-I Drivers/STM32H7xx_HAL_Driver/Inc \
-I Drivers/STM32H7xx_HAL_Driver/Inc/Legacy \
-I Drivers/CMSIS/Include \
-I Drivers/CMSIS/Device/ST/STM32H7xx/Include \
-I Drivers/CMSIS/Include \
-I Core/phy \
# -I Drivers/CMSIS/Include \

# -I Core/lwip/include/lwip \
# -I Core/lwip/include/lwip/apps \
# -I Core/lwip/include/lwip/apps/priv \
# -I Core/lwip/include/lwip/apps/priv \
# -I Core/lwip/include/lwip/apps/priv \
# -I Core/lwip/include/netif \
# -I Core/lwip/include/netif/ppp \
# -I Core/lwip/include/netif/ppp/polarssl 

# Native GNU make traversal: Windows find.exe is not the POSIX find tool.
rwildcard = $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))
LIB_DIRS := Core/ssd1306 Core/lwip Core/embedcli Core/FreeRtos
LIB_SOURCES := $(foreach d,$(LIB_DIRS),$(call rwildcard,$d/,*.c))
LIB_HEADERS := $(foreach d,$(LIB_DIRS),$(call rwildcard,$d/,*.h))
INCLUDES += $(addprefix -I,$(sort $(dir $(LIB_HEADERS))))
INCLUDES += -ICore/lwip/include -ICore/lwip/system
# ===================== Sources =====================
# 1. Список базовых файлов
C_SOURCES := \
Core/Src/main.c \
Core/Src/stm32h7xx_hal_msp.c \
Core/Src/stm32h7xx_it.c \
Core/Src/syscalls.c \
Core/Src/sysmem.c \
Common/Src/system_stm32h7xx_dualcore_boot_cm4_cm7.c \
Core/phy/lan8742.c 
# -I Core/ssd1306
# Core/ssd1306/ssd1306.c \
# Core/ssd1306/ssd1306_fonts.c \
# Core/ssd1306/ssd1306_tests.c	
C_SOURCES += $(LIB_SOURCES)
C_SOURCES += Core/Src/camera_stream.c Core/Src/app_cli.c Core/Src/settings.c Core/Src/settings_flash.c Core/Src/flash_access.c

# Keep in sync with HAL_*_MODULE_ENABLED in Core/Inc/stm32h7xx_hal_conf.h.
ENABLED_MODULES := dcmi eth iwdg rtc tim uart gpio dma mdma rcc flash exti pwr i2c cortex hsem

# 3. Формируем список драйверов (фильтруем дубликаты через sort)
HAL_DRIVER_DIR := Drivers/STM32H7xx_HAL_Driver/Src
HAL_FILES := $(foreach mod,$(ENABLED_MODULES),$(HAL_DRIVER_DIR)/stm32h7xx_hal_$(mod).c)

# 4. Добавляем файлы расширений (_ex), если основной файл модуля существует
HAL_EX_FILES := $(wildcard $(foreach mod,$(ENABLED_MODULES),$(HAL_DRIVER_DIR)/stm32h7xx_hal_$(mod)_ex.c))

# 5. Собираем всё вместе и удаляем дубликаты с помощью функции sort
# Также ВСЕГДА добавляем базовый hal.c и hal_cortex.c
C_SOURCES += $(HAL_DRIVER_DIR)/stm32h7xx_hal.c
C_SOURCES += $(HAL_DRIVER_DIR)/stm32h7xx_hal_cortex.c
C_SOURCES += $(HAL_FILES)
C_SOURCES += $(HAL_EX_FILES)

# Уникализируем список, чтобы линковщик не ругался на duplicate definition
C_SOURCES := $(sort $(C_SOURCES))

ASM_SOURCES := \
Core/Startup/startup_stm32h755zitx.S

# ===================== Objects =====================
OBJECTS := \
$(C_SOURCES:%.c=$(BUILD)/%.o) \
$(ASM_SOURCES:%.S=$(BUILD)/%.o)

# ===================== Rules =======================
all: $(BUILD)/$(PROJECT).elf $(BUILD)/$(PROJECT).bin $(BUILD)/$(PROJECT).hex size

$(BUILD)/%.o: %.c
	@$(call MKDIR_P,$(dir $@))
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/%.o: %.S
	@$(call MKDIR_P,$(dir $@))
	$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD)/$(PROJECT).elf: $(OBJECTS) $(LDSCRIPT)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILD)/%.bin: $(BUILD)/%.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/%.hex: $(BUILD)/%.elf
	$(OBJCOPY) -O ihex $< $@

size: $(BUILD)/$(PROJECT).elf
	$(SIZE) $(BUILD)/$(PROJECT).elf

clean:
	@$(call RM_RF,$(BUILD))

debug:
	$(MAKE) BUILD_TYPE=debug all

release:
	$(MAKE) BUILD_TYPE=release all

.PHONY: all clean size debug release

-include $(OBJECTS:.o=.d)
