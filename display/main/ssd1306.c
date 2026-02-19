#include "ssd1306.h"
#include "FreeMonoBold18pt7b.h"

#include <string.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define COLS    DISPLAY_WIDTH
#define PAGES   (DISPLAY_HEIGHT / 8)    //8 pages * 8 rows = 64 rows


static const char *TAG = "display";

static const uint8_t BRIGHTNESS[3][3] = {
    {0x00, 0x11, 0x00},  // low  
    {0x8F, 0x81, 0x20},  //med 
    {0xFF, 0xF1, 0x40},  //high
};


//frame buffer: page 0-7 column 0-127
// bit N of s_buf[page][col] = pixel at row page*8 + N
static uint8_t s_buf[PAGES][COLS];
static volatile display_brightness_t s_level  = DISPLAY_BRIGHTNESS_HIGH;
static volatile bool s_dither = false;  // set for low
static SemaphoreHandle_t s_mutex  = NULL;


static esp_err_t send_cmd(uint8_t cmd)
{
    i2c_cmd_handle_t h = i2c_cmd_link_create();
    i2c_master_start(h);
    i2c_master_write_byte(h, (DISPLAY_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(h, 0x00, true); 
    i2c_master_write_byte(h, cmd,  true);
    i2c_master_stop(h);
    esp_err_t ret = i2c_master_cmd_begin(DISPLAY_I2C_PORT, h, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(h);

    return ret;
}



static esp_err_t send_data(const uint8_t *data, size_t len)
{
    i2c_cmd_handle_t h = i2c_cmd_link_create();

    i2c_master_start(h);
    i2c_master_write_byte(h, (DISPLAY_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(h, 0x40, true); 
    i2c_master_write(h, data, len, true);
    i2c_master_stop(h);
    esp_err_t ret = i2c_master_cmd_begin(DISPLAY_I2C_PORT, h, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(h);

    return ret;
}


// Push whole frame buffer to dislay
static void flush(void)
{
    send_cmd(0x21); send_cmd(0);       send_cmd(COLS  - 1);  
    send_cmd(0x22); send_cmd(0);       send_cmd(PAGES - 1);

    for (int p = 0; p < PAGES; p++) {
        send_data(s_buf[p], COLS);
    }
}




//one character  into the frame buffer
//  no row padding.
static void draw_char(const GFXfont *font, char c, int cursor_x, int cursor_y)
{
    uint8_t uc = (uint8_t)c;
    if (uc < font->first || uc > font->last) return;

    const GFXglyph *g = &font->glyph[uc - font->first];
    const uint8_t  *bmp = &font->bitmap[g->bitmapOffset];

    int x0 = cursor_x + g->xOffset;
    int y0 = cursor_y + g->yOffset;

    //rows then columns, consume bits MSB first
    uint8_t byte_val = 0;
    uint8_t bit = 0;   // current bit mask: 0 means fetch next byte

    for (int row = 0; row < g->height; row++) {
        for (int col = 0; col < g->width; col++) {
            if (bit == 0) {
                byte_val = *bmp++;
                bit = 0x80;
            }

            if (byte_val & bit) {
                int px = x0 + col;
                int py = y0 + row;

                if (s_dither && ((px + py) & 1)) { 
                    bit >>= 1; continue; 
                }

                if (px >= 0 && px < COLS && py >= 0 && py < DISPLAY_HEIGHT) {
                    s_buf[py >> 3][px] |= (uint8_t)(1u << (py & 7));
                }
            }
            bit >>= 1;
        }
    }
}



esp_err_t display_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) return ESP_ERR_NO_MEM;

    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = DISPLAY_SDA_PIN,
        .scl_io_num = DISPLAY_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = DISPLAY_I2C_FREQ,
    };
    ESP_ERROR_CHECK(i2c_param_config(DISPLAY_I2C_PORT, &cfg));
    ESP_ERROR_CHECK(i2c_driver_install(DISPLAY_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0));

    vTaskDelay(pdMS_TO_TICKS(100));  // wait for display power-up

    // SSD1306 initialisation sequence
    static const uint8_t cmds[] = {
        0xAE,        // display off
        0xD5, 0x80,  // clock divide ratio / oscillator frequency
        0xA8, 0x3F,  // multiplex ratio = 63 (64-row panel)
        0xD3, 0x00,  // display vertical offset = 0
        0x40,        // display start line = 0
        0x8D, 0x14,  // charge pump ON 
        0x20, 0x00,  // memory addressing mode: horizontal
        0xA1,        // segment remap: column 127 → SEG0
        0xC8,        // COM scan: remapped direction
        0xDA, 0x12,  // COM pins hardware config (alternate, 128×64)
        0x81, 0xFF,  // contrast = 255 at startup
        0xD9, 0xF1,  // pre-charge period
        0xDB, 0x40,  // VCOM deselect level
        0xA4,        // output follows RAM content
        0xA6,        // normal (non-inverted) display
        0xAF,        // display on
    };

    for (size_t i = 0; i < sizeof(cmds); i++) {
        esp_err_t ret = send_cmd(cmds[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "init cmd 0x%02X failed: %s", cmds[i], esp_err_to_name(ret));
            return ret;
        }
    }

    display_clear();

    return ESP_OK;
}





void display_set_brightness(display_brightness_t level)
{
    if ((unsigned)level >= 3) return;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_level  = level;
    s_dither = (level == DISPLAY_BRIGHTNESS_LOW);
    send_cmd(0x81); send_cmd(BRIGHTNESS[level][0]);  
    send_cmd(0xD9); send_cmd(BRIGHTNESS[level][1]); 
    send_cmd(0xDB); send_cmd(BRIGHTNESS[level][2]);

    xSemaphoreGive(s_mutex);
}

void display_cycle_brightness(void)
{
    display_set_brightness((display_brightness_t)((s_level + 1) % 3));
}

void display_show_time(uint32_t total_seconds)
{
    if (total_seconds > 5999) total_seconds = 5999;  //limit 99:59

    uint8_t mins = (uint8_t)(total_seconds / 60);
    uint8_t secs = (uint8_t)(total_seconds % 60);

    char buf[6];
    buf[0] = '0' + mins / 10;
    buf[1] = '0' + mins % 10;
    buf[2] = ':';
    buf[3] = '0' + secs / 10;
    buf[4] = '0' + secs % 10;
    buf[5] = '\0';

    int cursor_x = (COLS - 5 * 21) / 2;
    int cursor_y = 42;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memset(s_buf, 0, sizeof(s_buf));

    for (int i = 0; buf[i]; i++) {
        draw_char(&FreeMonoBold18pt7b, buf[i], cursor_x, cursor_y);
        cursor_x += 21;  
    }

    flush();
    xSemaphoreGive(s_mutex);
}


void display_clear(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memset(s_buf, 0, sizeof(s_buf));
    flush();
    xSemaphoreGive(s_mutex);
}
