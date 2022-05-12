#include <stdio.h>

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sh1106.h"
#include "font8x8_basic.h"

void i2c_master_init(int _SDA, int _SCL)
{
	i2c_config_t i2c_config = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = _SDA,
		.scl_io_num = _SCL,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = 1000000
	};
	i2c_param_config(I2C_NUM_0, &i2c_config);
	i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

void sh1106_init(int _SDA, int _SCL) {

    i2c_master_init(_SDA, _SCL);

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);

	i2c_master_write_byte(cmd, OLED_CMD_SET_CHARGE_PUMP_CTRL, true);
	i2c_master_write_byte(cmd, OLED_CMD_SET_CHARGE_PUMP_ON, true);

	i2c_master_write_byte(cmd, OLED_CMD_SET_SEGMENT_REMAP_INVERSE, true); // reverse left-right mapping
	i2c_master_write_byte(cmd, OLED_CMD_SET_COM_SCAN_MODE_REVERSE, true); // reverse up-bottom mapping

	i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_ON, true);

	i2c_master_write_byte(cmd, 0x00, true); // reset column low bits
	i2c_master_write_byte(cmd, 0x10, true); // reset column high bits
	i2c_master_write_byte(cmd, 0xB0, true); // reset page
    i2c_master_write_byte(cmd, 0x40, true); // set start line
    i2c_master_write_byte(cmd, OLED_CMD_SET_DISPLAY_OFFSET, true);
    i2c_master_write_byte(cmd, 0x00, true);

	i2c_master_stop(cmd);

	esp_err_t espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	if (espRc == ESP_OK) {
		ESP_LOGI(tag, "OLED configured successfully");
	} else {
		ESP_LOGE(tag, "OLED configuration failed. code: 0x%.2X", espRc);
	}
	i2c_cmd_link_delete(cmd);
}

void sh1106_set_display_start_line(i2c_cmd_handle_t cmd, uint_fast8_t start_line) {
    // REQUIRES:
    //   0 <= start_line <= 63
    if (start_line >= 0 && start_line <= 63) {
        i2c_master_write_byte(cmd, OLED_CMD_SET_DISPLAY_START_LINE | start_line, true);
    }
}


void task_sh1106_display_pattern(void *ignore) {
	i2c_cmd_handle_t cmd;

	for (uint8_t i = 0; i < 8; i++) {
		cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
		i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_SINGLE, true);
		i2c_master_write_byte(cmd, 0xB0 | i, true);
		i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);
		for (uint8_t j = 0; j < 132; j++) {
			i2c_master_write_byte(cmd, 0xFF >> (j % 8), true);
		}
		i2c_master_stop(cmd);
		i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
		i2c_cmd_link_delete(cmd);
	}
}

void task_sh1106_display_clear(void *ignore) {
	i2c_cmd_handle_t cmd;

	uint8_t zero[132];
    memset(zero, 0, 132);
	for (uint8_t i = 0; i < 8; i++) {
		cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
		i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_SINGLE, true);
		i2c_master_write_byte(cmd, 0xB0 | i, true);

		i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);
		i2c_master_write(cmd, zero, 132, true);
		i2c_master_stop(cmd);
		i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
		i2c_cmd_link_delete(cmd);
	}

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
    i2c_master_write_byte(cmd, 0x00, true); // reset column
    i2c_master_write_byte(cmd, 0x10, true);
    i2c_master_write_byte(cmd, 0xB0, true); // reset page
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}

void task_sh1106_contrast(void *ignore) {
	i2c_cmd_handle_t cmd;

	uint8_t contrast = 0;
	uint8_t direction = 1;
	while (true) {
		cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
		i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
		i2c_master_write_byte(cmd, OLED_CMD_SET_CONTRAST, true);
		i2c_master_write_byte(cmd, contrast, true);
		i2c_master_stop(cmd);
		i2c_master_cmd_begin(I2C_NUM_0, cmd, 2/portTICK_PERIOD_MS);
		i2c_cmd_link_delete(cmd);
		vTaskDelay(1/portTICK_PERIOD_MS);

		contrast += direction;
		if (contrast == 0xFF) { direction = -1; }
		if (contrast == 0x0) { direction = 1; }
	}
	vTaskDelete(NULL);
}

void task_sh1106_display_text(const void *arg_text) {

	char *text = (char*)arg_text;
	uint8_t text_len = strlen(text);

	i2c_cmd_handle_t cmd;

	uint8_t cur_page = 0;

	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

	i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
	i2c_master_write_byte(cmd, 0x08, true); // reset column
	i2c_master_write_byte(cmd, 0x10, true);
	i2c_master_write_byte(cmd, 0xB0 | cur_page, true); // reset page

	// i2c_master_write_byte(cmd, OLED_CMD_SET_DISPLAY_START_LINE | 0, true);

	i2c_master_stop(cmd);
	i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);

	for (uint8_t i = 0; i < text_len; i++) {
		if (text[i] == '\n') {
			cmd = i2c_cmd_link_create();
			i2c_master_start(cmd);
			i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

			i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
			i2c_master_write_byte(cmd, 0x08, true); // reset column
			i2c_master_write_byte(cmd, 0x10, true);
			i2c_master_write_byte(cmd, 0xB0 | ++cur_page, true); // increment page

			i2c_master_stop(cmd);
			i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
			i2c_cmd_link_delete(cmd);
		} else {
			cmd = i2c_cmd_link_create();
			i2c_master_start(cmd);
			i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

			i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);
			i2c_master_write(cmd, font8x8_basic_new[(uint8_t)text[i]], 8, true);

			i2c_master_stop(cmd);
			i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
			i2c_cmd_link_delete(cmd);
		}
	}
}

void sh1106_invert(uint8_t *buf, size_t blen)
{
	uint8_t wk;
	for(int i=0; i<blen; i++){
		wk = buf[i];
		buf[i] = ~wk;
	}
}

void sh1106_display_text(int page, char * text, int text_len, bool invert)
{
	if (page >= 8) return;
	int _text_len = text_len;
	if (_text_len > 16) _text_len = 16;

	uint8_t seg = 0;
	uint8_t image[8];
	for (uint8_t i = 0; i < _text_len; i++) {
		memcpy(image, font8x8_basic_new[(uint8_t)text[i]], 8);
		if (invert) sh1106_invert(image, 8);
		i2c_display_image(page, seg, image, 8);
		seg = seg + 8;
	}
}

void i2c_display_image(int page, int seg, uint8_t * images, int width) 
{
	i2c_cmd_handle_t cmd;

	if (page >= 8) return;
	if (seg >= 132) return;

	int _seg = seg;
	uint8_t columLow = _seg & 0x0F;
	uint8_t columHigh = (_seg >> 4) & 0x0F;

	int _page = page;

	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

	i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
	// Set Lower Column Start Address for Page Addressing Mode
	i2c_master_write_byte(cmd, (0x00 + columLow + 2), true);
	// Set Higher Column Start Address for Page Addressing Mode
	i2c_master_write_byte(cmd, (0x10 + columHigh), true);
	// Set Page Start Address for Page Addressing Mode
	i2c_master_write_byte(cmd, 0xB0 | _page, true);

	i2c_master_stop(cmd);
	i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);

	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

	i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);
	i2c_master_write(cmd, images, width, true);

	i2c_master_stop(cmd);
	i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);
}