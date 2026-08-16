/*
 * epaper.h
 *
 *  Created on: Aug 16, 2026
 *      Author: WELCOME
 */

#ifndef INC_EPAPER_H_
#define INC_EPAPER_H_


#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

// ─── Panel Specs ──────────────────────────────────────────────────────────────
// WeAct Studio 2.13" SSD1680 — sama persis dengan versi ESP32
#define EPD_PHYSICAL_WIDTH   122
#define EPD_PHYSICAL_HEIGHT  250
#define EPD_WIDTH            250   // logical width = landscape
#define EPD_HEIGHT           122   // logical height = landscape
#define EPD_BUF_WIDTH        ((EPD_PHYSICAL_WIDTH + 7) / 8)  // 16
#define EPD_BUF_SIZE         (EPD_BUF_WIDTH * EPD_PHYSICAL_HEIGHT)  // 4000

// ─── RAM Address — sesuai WeAct reference (sama dengan versi ESP32) ──────────
#define EPD_X_ADDR_START    0x01
#define EPD_X_ADDR_END      0x10
#define EPD_Y_ADDR_START_L  0xF9
#define EPD_Y_ADDR_START_H  0x00
#define EPD_Y_ADDR_END_L    0x00
#define EPD_Y_ADDR_END_H    0x00

// ─── SPI Peripheral ───────────────────────────────────────────────────────────
// SPI1 — lihat docs/PIN_MAPPING.md. hspi1 didefinisikan di main.c (CubeMX-generated).
extern SPI_HandleTypeDef hspi1;
#define EPD_SPI_HANDLE   (&hspi1)

// ─── GPIO Pin Assignment ──────────────────────────────────────────────────────
// PA5/PA6/PA7 (SCK/MISO/MOSI) dihandle otomatis oleh HAL SPI1 — tidak perlu
// digerakkan manual di sini, itu sebabnya tidak ada define MOSI/CLK seperti versi ESP32.
#define EPD_CS_GPIO_Port     GPIOA
#define EPD_CS_Pin           GPIO_PIN_4

#define EPD_DC_GPIO_Port     GPIOA
#define EPD_DC_Pin           GPIO_PIN_0

#define EPD_RST_GPIO_Port    GPIOA
#define EPD_RST_Pin          GPIO_PIN_1

#define EPD_BUSY_GPIO_Port   GPIOA
#define EPD_BUSY_Pin         GPIO_PIN_8

// ─── Color ───────────────────────────────────────────────────────────────────
// SSD1680: bit 1 = putih, bit 0 = hitam (identik dengan versi ESP32)
#define EPD_WHITE   0xFF
#define EPD_BLACK   0x00

// ─── Public API — SAMA PERSIS dengan versi ESP32, epaper_gfx.c tidak perlu diubah ──

/**
 * @brief Inisialisasi GPIO dan panel SSD1680.
 *        SPI1 (hspi1) HARUS sudah di-init lebih dulu (MX_SPI1_Init() dari CubeMX)
 *        sebelum epd_init() dipanggil — beda dengan versi ESP32 yang init SPI
 *        di dalam epd_init() sendiri, di sini SPI bus setup jadi tanggung jawab
 *        CubeMX-generated code, epd_init() cuma pakai handle-nya.
 */
void epd_init(void);

/**
 * @brief Isi seluruh framebuffer dengan putih. Belum tampil ke layar — perlu epd_display().
 */
void epd_clear_buffer(void);

/**
 * @brief Kirim framebuffer ke panel dan trigger full refresh.
 *        Blocking ~2–8 detik. Jangan panggil dari ISR.
 *        Di firmware ini dipanggil dari DisplayTask (FreeRTOS) — task terpisah
 *        dari SensorTask supaya blocking ini tidak menunda baca sensor berikutnya.
 */
void epd_display(void);

/**
 * @brief Full clear + display sekaligus. Layar jadi putih bersih.
 */
void epd_clear_screen(void);

/**
 * @brief Masukkan panel ke deep sleep (<1uA). Butuh epd_wake() sebelum bisa display lagi.
 */
void epd_sleep(void);

/**
 * @brief Bangunkan panel dari deep sleep. Hardware reset + init ulang penuh.
 *        Framebuffer di-reset ke putih setelah wake.
 */
void epd_wake(void);

/**
 * @brief Set satu piksel di framebuffer (belum ke layar).
 * @param x     kolom, 0 = kiri, max EPD_WIDTH-1
 * @param y     baris, 0 = atas, max EPD_HEIGHT-1
 * @param color EPD_BLACK atau EPD_WHITE
 */
void epd_draw_pixel(int x, int y, uint8_t color);

/**
 * @brief Akses langsung ke framebuffer. Dipakai oleh epaper_gfx layer.
 * @return Pointer ke array uint8_t[EPD_BUF_SIZE]
 */
uint8_t *epd_get_buffer(void);
#endif /* INC_EPAPER_H_ */
