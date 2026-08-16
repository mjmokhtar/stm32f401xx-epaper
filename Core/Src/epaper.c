/*
 * epaper.c
 *
 *  Created on: Aug 16, 2026
 *      Author: MJ
 */


#include "epaper.h"
#include <string.h>
#include "cmsis_os2.h"   // osDelay — FreeRTOS CMSIS-RTOS v2 (di-generate CubeMX)

// ─── Optional debug log ──────────────────────────────────────────────────────
// ESP32 versi asli pakai ESP_LOGI(TAG, ...). Tidak ada padanan langsung di STM32
// HAL, jadi diganti macro ringan. Aktifkan dengan #define EPD_ENABLE_LOG di atas
// include ini (atau lewat compiler flag), kalau tidak — tidak melakukan apa-apa.
// printf perlu di-retarget ke USART1 (docs/PIN_MAPPING.md) supaya log ini muncul.
#ifdef EPD_ENABLE_LOG
    #include <stdio.h>
    #define EPD_LOG(...) printf(__VA_ARGS__)
#else
    #define EPD_LOG(...)
#endif

// ─── SSD1680 Command Bytes — identik dengan versi ESP32 ──────────────────────
#define CMD_DRIVER_OUTPUT_CTRL      0x01
#define CMD_DEEP_SLEEP              0x10
#define CMD_DATA_ENTRY_MODE         0x11
#define CMD_SW_RESET                0x12
#define CMD_DISPLAY_UPDATE_CTRL1    0x21
#define CMD_DISPLAY_UPDATE_CTRL2    0x22
#define CMD_WRITE_RAM_BW            0x24
#define CMD_WRITE_RAM_RED           0x26
#define CMD_BORDER_WAVEFORM_CTRL    0x3C
#define CMD_SET_RAM_X_ADDR          0x44
#define CMD_SET_RAM_Y_ADDR          0x45
#define CMD_SET_RAM_X_COUNTER       0x4E
#define CMD_SET_RAM_Y_COUNTER       0x4F
#define CMD_MASTER_ACTIVATION       0x20

#define EPD_SPI_TIMEOUT_MS   200  // timeout HAL_SPI_Transmit per transaksi

// ─── State ───────────────────────────────────────────────────────────────────
static uint8_t s_buf[EPD_BUF_SIZE];

// ─── Low-Level SPI ───────────────────────────────────────────────────────────
// CS software (NSS_SOFT) — beda dengan versi ESP32 yang CS-nya dihandle
// otomatis oleh spi_device_transmit(). Di sini CS di-assert/deassert manual
// membungkus setiap transaksi SPI.

static void _dc_cmd(void)  { HAL_GPIO_WritePin(EPD_DC_GPIO_Port, EPD_DC_Pin, GPIO_PIN_RESET); }
static void _dc_data(void) { HAL_GPIO_WritePin(EPD_DC_GPIO_Port, EPD_DC_Pin, GPIO_PIN_SET); }

static void _spi_write(const uint8_t *data, size_t len)
{
    HAL_GPIO_WritePin(EPD_CS_GPIO_Port, EPD_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(EPD_SPI_HANDLE, (uint8_t *)data, (uint16_t)len, EPD_SPI_TIMEOUT_MS);
    HAL_GPIO_WritePin(EPD_CS_GPIO_Port, EPD_CS_Pin, GPIO_PIN_SET);
}

static void _send_cmd(uint8_t cmd)
{
    _dc_cmd();
    _spi_write(&cmd, 1);
}

static void _send_data(const uint8_t *data, size_t len)
{
    _dc_data();
    _spi_write(data, len);
}

static void _send_byte(uint8_t byte)
{
    _dc_data();
    _spi_write(&byte, 1);
}

// ─── BUSY Wait ───────────────────────────────────────────────────────────────
// osDelay (bukan HAL_Delay) — supaya task ini yield ke scheduler FreeRTOS
// selama polling, bukan busy-block CPU. Konsisten dengan vTaskDelay di versi ESP32.

static void _wait_busy(void)
{
    // SSD1680: BUSY HIGH = panel sedang proses, tunggu sampai LOW
    while (HAL_GPIO_ReadPin(EPD_BUSY_GPIO_Port, EPD_BUSY_Pin) == GPIO_PIN_SET) {
        osDelay(10);
    }
}

// ─── Hardware Reset ───────────────────────────────────────────────────────────

static void _hw_reset(void)
{
    HAL_GPIO_WritePin(EPD_RST_GPIO_Port, EPD_RST_Pin, GPIO_PIN_SET);   osDelay(10);
    HAL_GPIO_WritePin(EPD_RST_GPIO_Port, EPD_RST_Pin, GPIO_PIN_RESET); osDelay(10);
    HAL_GPIO_WritePin(EPD_RST_GPIO_Port, EPD_RST_Pin, GPIO_PIN_SET);   osDelay(10);
}

// ─── Panel Init — identik urutan command dengan versi ESP32 (WeAct reference) ─

static void _panel_init(void)
{
    _hw_reset();

    _send_cmd(CMD_SW_RESET);
    _wait_busy();

    _send_cmd(CMD_DRIVER_OUTPUT_CTRL);
    _send_byte(0xF9);
    _send_byte(0x00);
    _send_byte(0x00);

    _send_cmd(CMD_DATA_ENTRY_MODE);
    _send_byte(0x01);

    _send_cmd(CMD_SET_RAM_X_ADDR);
    _send_byte(0x01);
    _send_byte(0x10);

    _send_cmd(CMD_SET_RAM_Y_ADDR);
    _send_byte(0xF9);
    _send_byte(0x00);
    _send_byte(0x00);
    _send_byte(0x00);

    _send_cmd(CMD_BORDER_WAVEFORM_CTRL);
    _send_byte(0x05);

    _send_cmd(CMD_DISPLAY_UPDATE_CTRL1);
    _send_byte(0x00);
    _send_byte(0x00);

    _send_cmd(CMD_SET_RAM_X_COUNTER);
    _send_byte(0x01);

    _send_cmd(CMD_SET_RAM_Y_COUNTER);
    _send_byte(0xF9);
    _send_byte(0x00);

    // Tidak upload LUT manual — pakai OTP internal panel (sama seperti versi ESP32)

    EPD_LOG("SSD1680 initialized (WeAct reference)\r\n");
}

static void _set_ram_cursor(void)
{
    _send_cmd(CMD_SET_RAM_X_COUNTER);
    _send_byte(0x01);

    _send_cmd(CMD_SET_RAM_Y_COUNTER);
    _send_byte(0xF9);
    _send_byte(0x00);
}

// ─── Public API ──────────────────────────────────────────────────────────────

void epd_init(void)
{
    // GPIO clock — GPIOA sudah pasti di-enable CubeMX untuk SPI1/USART, tapi
    // dipanggil lagi di sini aman (HAL no-op kalau sudah enabled).
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};

    // Output: CS, DC, RST
    gpio.Pin   = EPD_CS_Pin;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(EPD_CS_GPIO_Port, &gpio);
    HAL_GPIO_WritePin(EPD_CS_GPIO_Port, EPD_CS_Pin, GPIO_PIN_SET);  // idle high (unselected)

    gpio.Pin = EPD_DC_Pin;
    HAL_GPIO_Init(EPD_DC_GPIO_Port, &gpio);

    gpio.Pin = EPD_RST_Pin;
    HAL_GPIO_Init(EPD_RST_GPIO_Port, &gpio);

    // Input: BUSY
    gpio.Pin  = EPD_BUSY_Pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(EPD_BUSY_GPIO_Port, &gpio);

    // NOTE: SPI1 (hspi1) TIDAK di-init di sini — beda dengan versi ESP32 yang
    // setup SPI bus di dalam epd_init(). Di firmware ini SPI1 sudah harus
    // di-init lebih dulu oleh MX_SPI1_Init() (CubeMX-generated, dipanggil dari
    // main() sebelum RTOS scheduler start).

    _panel_init();
    memset(s_buf, EPD_WHITE, EPD_BUF_SIZE);
}

void epd_clear_buffer(void)
{
    memset(s_buf, EPD_WHITE, EPD_BUF_SIZE);
}

void epd_display(void)
{
    _set_ram_cursor();

    _send_cmd(CMD_WRITE_RAM_BW);
    _send_data(s_buf, EPD_BUF_SIZE);

    _send_cmd(CMD_DISPLAY_UPDATE_CTRL2);
    _send_byte(0xF7);
    _send_cmd(CMD_MASTER_ACTIVATION);
    _wait_busy();

    EPD_LOG("Display refreshed\r\n");
}

void epd_clear_screen(void)
{
    epd_clear_buffer();
    epd_display();
}

void epd_sleep(void)
{
    _send_cmd(CMD_DEEP_SLEEP);
    _send_byte(0x01);
    EPD_LOG("Panel in deep sleep\r\n");
}

void epd_wake(void)
{
    // SSD1680 deep sleep tidak bisa di-wake dengan command biasa.
    // Satu-satunya cara: hardware reset + full init ulang (sama dengan versi ESP32).
    _panel_init();
    memset(s_buf, EPD_WHITE, EPD_BUF_SIZE);
    EPD_LOG("Panel woke from deep sleep\r\n");
}

void epd_draw_pixel(int x, int y, uint8_t color)
{
    if (x < 0 || x >= EPD_WIDTH || y < 0 || y >= EPD_HEIGHT) return;

    int px = y;
    int py = x;

    int byte_idx = py * EPD_BUF_WIDTH + (px / 8);
    int bit_pos  = 7 - (px % 8);

    if (color == EPD_BLACK)
        s_buf[byte_idx] &= ~(1 << bit_pos);
    else
        s_buf[byte_idx] |=  (1 << bit_pos);
}

uint8_t *epd_get_buffer(void)
{
    return s_buf;
}
