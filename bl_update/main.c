/*
  bootloader update firmware

  this updates the bootloader on an AM32 ESC. It assumes the
  bootloader flash sectors are unlocked
 */
#include <main.h>
#include <stdio.h>

#include <version.h>
#include <stdbool.h>
#include "eeprom.h"

#pragma GCC optimize("O0")

#include <string.h>

#define GPIO_PORT_TYPE typeof(GPIOA)

// dummy pin and port so we can re-use blutil.h
static GPIO_PORT_TYPE input_port;
static uint32_t input_pin;
#define FIRMWARE_RELATIVE_START 0x1000

#define MCU_FLASH_START 0x08000000

/*
  use stringize to construct an include of the right bootloader header
 */
#define bl_header BL_HEADER_FILE
#define xstr(x) #x
#define str(x) xstr(x)
#include str(bl_header)

#include <blutil.h>

static void delayMicroseconds(uint32_t micros)
{
    while (micros > 0) {
        uint32_t us = micros>10000?10000:micros;
        bl_timer_reset();
        while (bl_timer_us() < us) ;
        micros -= us;
    }
}

static void flash_bootloader(void)
{
    uint32_t length = sizeof(bl_image);
    uint32_t address = MCU_FLASH_START;
    const uint8_t *bl = &bl_image[0];

    while (length > 0) {
        uint32_t chunk = 256;
        if (chunk > length) {
            chunk = length;
        }
        // loop until the flash succeeds. We expect it to pass
        // first time, so this is paranoia
        while (!save_flash_nolib(bl, chunk, address)) {
        }
        length -= chunk;
        address += chunk;
        bl += chunk;
    }
}

int main(void)
{
    bl_clock_config();
    bl_timer_init();

    // give 1.5s for debugger to attach
    delayMicroseconds(1500000);

    // don't risk erasing the bootloader if it already matches
    if (memcmp((const void*)MCU_FLASH_START, bl_image, sizeof(bl_image)) != 0) {
        flash_bootloader();
    }

    /*
      wipe eeprom header to ensure we don't boot this firmware
      again. We wipe all 3 possible eeprom addresses for 32k, 64k and
      128k boards

      On boards with 32k of flash the 2nd two will fail. On boards
      with 64k flash the last one will fail
    */
    const uint8_t zeros[8] = {0};
    save_flash_nolib(zeros, sizeof(zeros), MCU_FLASH_START+0x7c00);
    save_flash_nolib(zeros, sizeof(zeros), MCU_FLASH_START+0xf800);
    save_flash_nolib(zeros, sizeof(zeros), MCU_FLASH_START+0x1f800);

    // if we got here then reboot
    NVIC_SystemReset();

    return 0;
}