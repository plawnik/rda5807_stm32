PROJECT := rda5807_stm32
TARGET := rda5807_stm32
FIRMWARE_DIR := build/firmware
TEST_DIR := build/tests

CROSS_COMPILE ?= arm-none-eabi-
CC := $(CROSS_COMPILE)gcc
AS := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
SIZE := $(CROSS_COMPILE)size
HOST_CC ?= gcc

MCU_FLAGS := -mcpu=cortex-m3 -mthumb
DEFINES := -DUSE_HAL_DRIVER -DSTM32F103xB
INCLUDES := \
	-If103radio/Core/Inc \
	-If103radio/USB_DEVICE/App \
	-If103radio/USB_DEVICE/Target \
	-If103radio/Middlewares/ST/STM32_USB_Device_Library/Core/Inc \
	-If103radio/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc \
	-If103radio/Drivers/STM32F1xx_HAL_Driver/Inc \
	-If103radio/Drivers/STM32F1xx_HAL_Driver/Inc/Legacy \
	-If103radio/Drivers/CMSIS/Device/ST/STM32F1xx/Include \
	-If103radio/Drivers/CMSIS/Include

CFLAGS := $(MCU_FLAGS) $(DEFINES) $(INCLUDES) -std=gnu11 -Os -g3 \
	-ffunction-sections -fdata-sections -fstack-usage \
	-Wall -Wextra -Wshadow -Wformat=2 -Wundef -Wno-unused-parameter \
	-MMD -MP
ASFLAGS := $(MCU_FLAGS) $(DEFINES) $(INCLUDES) -x assembler-with-cpp -MMD -MP
LDFLAGS := $(MCU_FLAGS) --specs=nano.specs --specs=nosys.specs \
	-Tf103radio/STM32F103C8TX_FLASH.ld -Wl,--gc-sections -static \
	-Wl,-Map=$(FIRMWARE_DIR)/$(TARGET).map,--cref \
	-Wl,--start-group -lc -lm -Wl,--end-group

CORE_SOURCES := $(wildcard f103radio/Core/Src/*.c)
HAL_SOURCES := $(wildcard f103radio/Drivers/STM32F1xx_HAL_Driver/Src/*.c)
USB_APP_SOURCES := $(wildcard f103radio/USB_DEVICE/App/*.c) \
	$(wildcard f103radio/USB_DEVICE/Target/*.c)
USB_MIDDLEWARE_SOURCES := \
	$(wildcard f103radio/Middlewares/ST/STM32_USB_Device_Library/Core/Src/*.c) \
	$(wildcard f103radio/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/*.c)
C_SOURCES := $(CORE_SOURCES) $(HAL_SOURCES) $(USB_APP_SOURCES) \
	$(USB_MIDDLEWARE_SOURCES)
ASM_SOURCES := f103radio/Core/Startup/startup_stm32f103c8tx.s
C_OBJECTS := $(patsubst %.c,$(FIRMWARE_DIR)/%.o,$(C_SOURCES))
ASM_OBJECTS := $(patsubst %.s,$(FIRMWARE_DIR)/%.o,$(ASM_SOURCES))
OBJECTS := $(C_OBJECTS) $(ASM_OBJECTS)
DEPS := $(OBJECTS:.o=.d)

HOST_TEST_SOURCES := tests/test_main.c \
	f103radio/Core/Src/app_config.c \
	f103radio/Core/Src/rds_decoder.c
HOST_TEST := $(TEST_DIR)/test_runner
HOST_TERMINAL_TEST_SOURCES := tests/test_terminal_ui.c \
	f103radio/Core/Src/terminal_ui.c \
	f103radio/Core/Src/uart_debug.c \
	f103radio/Core/Src/app_config.c \
	f103radio/Core/Src/rds_decoder.c
HOST_TERMINAL_TEST := $(TEST_DIR)/test_terminal_ui

.PHONY: all firmware test ci clean

all: firmware

firmware: $(FIRMWARE_DIR)/$(TARGET).elf $(FIRMWARE_DIR)/$(TARGET).hex $(FIRMWARE_DIR)/$(TARGET).bin
	@$(SIZE) $(FIRMWARE_DIR)/$(TARGET).elf

$(FIRMWARE_DIR)/$(TARGET).elf: $(OBJECTS)
	@mkdir -p $(@D)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(FIRMWARE_DIR)/$(TARGET).hex: $(FIRMWARE_DIR)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(FIRMWARE_DIR)/$(TARGET).bin: $(FIRMWARE_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary -S $< $@

$(FIRMWARE_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(FIRMWARE_DIR)/%.o: %.s
	@mkdir -p $(@D)
	$(AS) $(ASFLAGS) -c $< -o $@

test: $(HOST_TEST) $(HOST_TERMINAL_TEST)
	$(HOST_TEST)
	$(HOST_TERMINAL_TEST)

$(HOST_TEST): $(HOST_TEST_SOURCES)
	@mkdir -p $(@D)
	$(HOST_CC) -std=gnu11 -O2 -g -Wall -Wextra -Werror \
		-If103radio/Core/Inc $(HOST_TEST_SOURCES) -o $@

$(HOST_TERMINAL_TEST): $(HOST_TERMINAL_TEST_SOURCES)
	@mkdir -p $(@D)
	$(HOST_CC) -std=gnu11 -O2 -g -Wall -Wextra -Werror \
		-Wno-int-to-pointer-cast $(DEFINES) $(INCLUDES) \
		$(HOST_TERMINAL_TEST_SOURCES) -o $@

ci: test firmware

clean:
	rm -rf build

-include $(DEPS)
