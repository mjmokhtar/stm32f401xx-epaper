/*
 * sensor.h
 *
 *  Created on: Aug 16, 2026
 *      Author: WELCOME
 */

/*
 * sensor.h
 *
 * XY-MD02 Temperature & Humidity Sensor Driver
 * STM32F401CCU6 (Blackpill) — USART2 (PA2=TX, PA3=RX), RS485, Modbus RTU
 *
 * (Komentar target chip dikoreksi dari draft asli yang menyebut STM32F407VET6
 * — proyek ini target-nya STM32F401CCU6. Kode Modbus di sensor.c tidak berubah
 * karena sama-sama pakai stm32f4xx_hal, tapi clock max F401 84MHz vs F407 168MHz,
 * jadi jangan asumsikan timing/clock config dari referensi F407 manapun.)
 */
#ifndef SENSOR_H
#define SENSOR_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ── USART Handle ────────────────────────────────────────────── */
extern UART_HandleTypeDef huart2;

/* ── Config ──────────────────────────────────────────────────── */
#define SENSOR_UART         huart2
#define SENSOR_TIMEOUT_MS   500
#define SENSOR_DEVICE_ADDR  0x01    /* default Modbus address XY-MD02 */

/* ── Error codes ─────────────────────────────────────────────── */
#define SENSOR_OK           0
#define SENSOR_TIMEOUT     -1
#define SENSOR_CRC_ERROR   -2

/* ── Data struct ─────────────────────────────────────────────── */
typedef struct {
    float temperature;  /* derajat Celsius */
    float humidity;     /* persen RH */
    bool  valid;        /* true jika data terakhir berhasil dibaca */
} sensor_data_t;

/* ── Public API ──────────────────────────────────────────────── */
void sensor_init(void);
int  sensor_read(sensor_data_t *out);
void sensor_get_last(sensor_data_t *out);

#endif /* SENSOR_H */
