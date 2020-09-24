#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <baremetal/pervasive.h>
#include <baremetal/cdram.h>
#include <baremetal/gpio.h>
#include <baremetal/i2c.h>
#include <baremetal/syscon.h>
#include <baremetal/sysroot.h>
#include <baremetal/display.h>
#include <baremetal/ctrl.h>
#include <baremetal/msif.h>
#include <baremetal/font.h>
#include <baremetal/uart.h>
#include <baremetal/utils.h>
#include "ff.h"

#define LINUX_DIR	"/linux/"

#define ZIMAGE 		LINUX_DIR "zImage"
#define VITA1000_DTB 	LINUX_DIR "vita1000.dtb"
#define VITA2000_DTB 	LINUX_DIR "vita2000.dtb"
#define PSTV_DTB 	LINUX_DIR "pstv.dtb"
#define FALLBACK_DTB	LINUX_DIR "vita.dtb"

#define DTB_LOAD_ADDR	0x4A000000
#define LINUX_LOAD_ADDR	0x44000000

#define CPU123_WAIT_BASE 0x1F007F00

extern unsigned int _bss_start;
extern unsigned int _bss_end;

static const unsigned char msif_key[32] = {
	0xD4, 0x19, 0xA2, 0xEB, 0x9D, 0x61, 0xA5, 0x2F,
	0x4F, 0xA2, 0x8B, 0x27, 0xE3, 0x2F, 0xCD, 0xD7,
	0xE0, 0x04, 0x8D, 0x44, 0x3D, 0x63, 0xC9, 0x2C,
	0x0B, 0x27, 0x13, 0x55, 0x41, 0xD9, 0x2E, 0xC4
};

static void LOG(const char *str, ...)
{
	static int console_y = 200;
	char buf[256];
	va_list argptr;

	va_start(argptr, str);
	vsnprintf(buf, sizeof(buf), str, argptr);
	va_end(argptr);

	uart_print(0, buf);
	font_draw_string(10, console_y+=20, WHITE, buf);
}

static void cpu123_wait(unsigned int cpu_id)
{
	volatile unsigned int val, *base = (unsigned int *)CPU123_WAIT_BASE;
	while (1) {
		wfe();
		val = base[cpu_id];
		if (val)
			((void (*)())val)();
	}
}

static FRESULT file_load(const char *path, uintptr_t addr, UINT *nread)
{
	FRESULT res;
	FIL file;
	FSIZE_t size;

	res = f_open(&file, path, FA_READ);
	if (res != FR_OK)
		return res;

	size = f_size(&file);

	res = f_read(&file, (void *)addr, size, nread);
	if (res != FR_OK)
		return res;

	f_close(&file);

	return FR_OK;
}

static FRESULT file_load_log(const char *path, uintptr_t addr)
{
	FRESULT res;
	UINT nread;

	LOG("Loading '%s'...\n", path);

	res = file_load(path, addr, &nread);
	if (res != FR_OK) {
		LOG("Error loading '%s': %d\n", path, res);
		return res;
	}

	LOG("Loaded '%s' at 0x%08X, size: %dKiB\n", path, addr, nread / 1024);

	return FR_OK;
}

int main(struct sysroot_buffer *sysroot)
{
	FATFS fs;
	FRESULT res;
	unsigned int *bss;
	unsigned int cpu_id = get_cpu_id();

	/* CPUs 1-3 wait for Linux secondary startup */
	if (cpu_id != 0)
		cpu123_wait(cpu_id);

	/* Clear BSS */
	for (bss = &_bss_start; bss < &_bss_end; bss++)
		*bss = 0;

	sysroot_init(sysroot);

	pervasive_clock_enable_gpio();
	pervasive_reset_exit_gpio();
	pervasive_clock_enable_uart(0);
	pervasive_reset_exit_uart(0);
	pervasive_clock_enable_i2c(1);
	pervasive_reset_exit_i2c(1);

	uart_init(0, 115200);

	cdram_enable();
	i2c_init_bus(1);
	syscon_init();

	if (sysroot_model_is_dolce())
		display_init(DISPLAY_TYPE_HDMI);
	else if (sysroot_model_is_vita2k())
		display_init(DISPLAY_TYPE_LCD);
	else
		display_init(DISPLAY_TYPE_OLED);

	LOG("Vita baremetal Linux loader started!\n");

	if (!pervasive_msif_get_card_insert_state()) {
		LOG("Memory card not inserted.\n");
		goto fatal_error;
	}

	msif_init();
	syscon_msif_set_power(1);
	msif_setup(msif_key);

	LOG("Memory card authenticated!\n");

	res = f_mount(&fs, "/", 0);
	if (res != FR_OK) {
		LOG("Error mounting Memory card: %d\n", res);
		goto fatal_error;
	}

	LOG("Memory card mounted!\n");

	res = file_load_log(ZIMAGE, LINUX_LOAD_ADDR);
	if (res != FR_OK)
		goto fatal_error;

	if (sysroot_model_is_vita()) {
		res = file_load_log(VITA1000_DTB, DTB_LOAD_ADDR);
	} else if (sysroot_model_is_vita2k()) {
		res = file_load_log(VITA2000_DTB, DTB_LOAD_ADDR);
	} else if (sysroot_model_is_dolce()) {
		res = file_load_log(PSTV_DTB, DTB_LOAD_ADDR);
	} else {
		LOG("Unsupported Vita model.\n");
		goto fatal_error;
	}

	if (res != FR_OK) {
		LOG("Trying with the fallback DTB...\n");
		res = file_load_log(FALLBACK_DTB, DTB_LOAD_ADDR);
		if (res != FR_OK)
			goto fatal_error;
	}

	((void (*)(int, int, uintptr_t))LINUX_LOAD_ADDR)(0, 0, DTB_LOAD_ADDR);

fatal_error:
	LOG("\nPress X to restart.\n");

	while (1) {
		struct ctrl_data ctrl;
		ctrl_read(&ctrl);
		if (CTRL_BUTTON_HELD(ctrl.buttons, CTRL_CROSS))
			break;
	}

	syscon_reset_device(SYSCON_RESET_TYPE_COLD_RESET, 0);

	return 0;
}
