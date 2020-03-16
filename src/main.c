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

#define LINUX_FILENAME	"/linux/zImage"
#define DTB_FILENAME	"/linux/vita.dtb"

#define DTB_ADDR	0x4A000000
#define LINUX_ADDR	0x44000000

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

int main(struct sysroot_buffer *sysroot)
{
	FATFS fs;
	FRESULT res;
	UINT nread;
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
	uart_print(0, "Vita baremetal Linux loader UART initialized\n");

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
		goto end;
	}

	msif_init();
	syscon_msif_set_power(1);
	msif_setup(msif_key);

	LOG("Memory card authenticated!\n");

	res = f_mount(&fs, "/", 0);
	if (res != FR_OK) {
		LOG("Error mounting Memory card: %d\n", res);
		goto end;
	}

	LOG("Memory card mounted!\n");

	LOG("Loading " LINUX_FILENAME " ...\n");
	res = file_load(LINUX_FILENAME, LINUX_ADDR, &nread);
	if (res != FR_OK) {
		LOG("Error loading " LINUX_FILENAME ": %d\n", res);
		goto end;
	}

	LOG(LINUX_FILENAME " loaded at 0x%08X, size: %d\n", LINUX_ADDR, nread);

	LOG("Loading " DTB_FILENAME " ...\n");
	res = file_load(DTB_FILENAME, DTB_ADDR, &nread);
	if (res != FR_OK) {
		LOG("Error loading " DTB_FILENAME ": %d\n", res);
		goto end;
	}

	LOG(DTB_FILENAME " loaded at 0x%08X, size: %d\n", DTB_ADDR, nread);

	LOG("Jumping to the kernel entrypoint...\n");

	((void (*)(int, int, uintptr_t))LINUX_ADDR)(0, 0, DTB_ADDR);

end:
	syscon_reset_device(SYSCON_RESET_TYPE_POWEROFF, 2);

	return 0;
}
