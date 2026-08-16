/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

#include "epaper.h"
#include "epaper_gfx.h"
#include "sensor.h"

#include <stdio.h>


/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
#define SENSOR_INTERVAL_MS    10000U   // baca sensor tiap 10 detik — sudah disepakati
#define DISPLAY_INTERVAL_MS   60000U   // refresh e-paper tiap 60 detik — sudah disepakati

/* Definitions for defaultTask */
static osThreadId_t   sensorTaskHandle;
static osThreadId_t   displayTaskHandle;
static osMutexId_t    dataMutexHandle;

static const osThreadAttr_t sensorTask_attrs = {
    .name       = "SensorTask",
    .stack_size = 512 * 4,     // 512 word
    .priority   = osPriorityNormal,
};

static const osThreadAttr_t displayTask_attrs = {
    .name       = "DisplayTask",
    .stack_size = 1024 * 4,    // 1024 word — perlu ruang utk framebuffer 4000 byte + font render
    .priority   = osPriorityLow,
};

static sensor_data_t g_latest_data = {0};

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */
static void StartSensorTask(void *argument);
static void StartDisplayTask(void *argument);
static void RenderSensorData(const sensor_data_t *data);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  {
      const char test_msg[] = "\r\n=== UART1 TEST OK ===\r\n";
      HAL_StatusTypeDef test_status = HAL_UART_Transmit(
          &huart1, (uint8_t *)test_msg, sizeof(test_msg) - 1, 1000);
      // Kalau test_status != HAL_OK, LED/breakpoint di sini bisa membantu
      // debug lebih lanjut — untuk sekarang cukup lihat apa test_msg muncul
      // di terminal atau tidak.
      (void)test_status;
  }

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  dataMutexHandle = osMutexNew(NULL);
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */


  /* USER CODE BEGIN RTOS_THREADS */
  sensorTaskHandle  = osThreadNew(StartSensorTask,  NULL, &sensorTask_attrs);
  displayTaskHandle = osThreadNew(StartDisplayTask, NULL, &displayTask_attrs);

  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, EPD_DC_Pin|EPD_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(EPD_CS_GPIO_Port, EPD_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : EPD_DC_Pin EPD_RST_Pin EPD_CS_Pin */
  GPIO_InitStruct.Pin = EPD_DC_Pin|EPD_RST_Pin|EPD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : EPD_BUSY_Pin */
  GPIO_InitStruct.Pin = EPD_BUSY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(EPD_BUSY_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN 4 */

/**
  * @brief Retarget printf() supaya keluar ke USART1 (debug).
  *        Pola standar STM32CubeIDE — syscalls.c (auto-generated) sudah punya
  *        _write() yang manggil __io_putchar() per karakter; tanpa fungsi ini
  *        diimplementasikan, printf() akan diam saja (atau linker error
  *        "undefined reference to __io_putchar" tergantung versi toolchain).
  *        Ini KEMUNGKINAN BESAR penyebab "serial debug tidak bisa" kamu.
  */
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

/**
  * @brief SensorTask — baca XY-MD02 tiap SENSOR_INTERVAL_MS, tulis ke
  *        g_latest_data (dilindungi mutex). Tidak pernah menyentuh epaper.
  */
static void StartSensorTask(void *argument)
{
    (void)argument;

    sensor_init();   // dipindah dari main() — aman di sini, sudah dalam task context
    printf("[Sensor] task started\r\n");

    uint32_t tick = osKernelGetTickCount();

    for (;;) {
        sensor_data_t tmp;
        int ret = sensor_read(&tmp);

        if (ret == SENSOR_OK) {
            osMutexAcquire(dataMutexHandle, osWaitForever);
            g_latest_data = tmp;
            osMutexRelease(dataMutexHandle);
            printf("[Sensor] OK  temp=%d.%02d humi=%d.%02d\r\n",
                   (int)tmp.temperature,
                   (int)((tmp.temperature - (int)tmp.temperature) * 100),
                   (int)tmp.humidity,
                   (int)((tmp.humidity - (int)tmp.humidity) * 100));
        } else if (ret == SENSOR_TIMEOUT) {
            printf("[Sensor] TIMEOUT - cek wiring RS485 / baudrate / device address\r\n");
        } else if (ret == SENSOR_CRC_ERROR) {
            printf("[Sensor] CRC_ERROR - data korup, cek wiring/noise RS485\r\n");
        }
        // ret == SENSOR_TIMEOUT / SENSOR_CRC_ERROR: g_latest_data TIDAK diupdate,
        // DisplayTask akan tetap tampilkan data valid terakhir (bukan data rusak).

        tick += SENSOR_INTERVAL_MS;
        osDelayUntil(tick);
    }
}

/**
  * @brief DisplayTask — tiap DISPLAY_INTERVAL_MS, ambil salinan data terbaru,
  *        render ke framebuffer, lalu epd_display() (blocking 2-8 detik).
  *        Task terpisah dari SensorTask -> blocking ini tidak menunda sensor.
  *
  *        epd_wake()/epd_sleep() membungkus tiap siklus render (pola diambil
  *        dari referensi ESP32 app_main kamu) — panel tidur di antara refresh
  *        untuk hemat daya, bukan cuma sekali di awal boot.
  *
  *        CATATAN: berbeda dari referensi ESP32 (yang pakai FreeRTOS software
  *        timer + flag `s_needs_refresh` dipolling di while(1) supaya render
  *        tidak jalan langsung dari timer callback/ISR context), di sini pola
  *        itu TIDAK dipakai — DisplayTask ini sudah berjalan di task context-nya
  *        sendiri (bukan callback timer/ISR), jadi osDelayUntil + render
  *        langsung di loop task sudah aman, tidak perlu flag perantara.
  */
static void StartDisplayTask(void *argument)
{
    (void)argument;

    // epd_init() dipindah dari main() ke sini — panggilan ini memanggil osDelay()
    // di dalamnya (lewat _panel_init()), yang HANYA valid setelah osKernelStart().
    // Kalau dipanggil dari main() sebelum kernel start, board akan hang total.
    epd_init();

    uint32_t tick = osKernelGetTickCount();

    for (;;) {
        sensor_data_t local;

        osMutexAcquire(dataMutexHandle, osWaitForever);
        local = g_latest_data;
        osMutexRelease(dataMutexHandle);

        epd_wake();              // bangunkan panel dari deep sleep (reset + init ulang)
        RenderSensorData(&local);
        printf("[Display] rendering: valid=%d temp=%d.%02d humi=%d.%02d\r\n",
               local.valid,
               (int)local.temperature,
               (int)((local.temperature - (int)local.temperature) * 100),
               (int)local.humidity,
               (int)((local.humidity - (int)local.humidity) * 100));
        epd_display();           // blocking ~2-8 detik
        epd_sleep();              // tidurkan lagi setelah tampil, hemat daya
        printf("[Display] refresh done\r\n");

        tick += DISPLAY_INTERVAL_MS;
        osDelayUntil(tick);
    }
}

/**
  * @brief Format 1 desimal manual (mis. 27.3) TANPA pakai "%f" di printf sama
  *        sekali — supaya tidak bergantung ke -u _printf_float linker flag
  *        (yang gampang lupa di-set ulang tiap generate/clean project).
  *        Cuma pakai %d/%s biasa, itu selalu didukung newlib-nano default.
  */
static void format_1dp(char *buf, size_t len, float value, const char *suffix)
{
    int is_neg = (value < 0.0f);
    float av   = is_neg ? -value : value;

    int whole = (int)av;
    int frac  = (int)((av - (float)whole) * 10.0f + 0.5f);   // rounding ke 1 desimal
    if (frac >= 10) { frac = 0; whole += 1; }                  // handle carry, mis. 27.96 -> 28.0

    snprintf(buf, len, "%s%d.%d%s", is_neg ? "-" : "", whole, frac, suffix);
}

/**
  * @brief Render nilai sensor ke framebuffer epaper (belum tampil — epd_display()
  *        yang memicu refresh fisik ke panel). Layout mengikuti pola referensi
  *        ESP32 kamu: header bar solid + judul putih, garis pemisah, konten,
  *        footer dengan garis + teks — bagian WiFi/RSSI di referensi diganti
  *        baris suhu/kelembapan, bagian uptime dipertahankan (pakai HAL_GetTick()
  *        sebagai pengganti esp_timer_get_time()).
  *
  * CATATAN: sudah TIDAK pakai snprintf("%.1f", ...) lagi — diganti format_1dp()
  * manual di atas, supaya tidak bergantung ke printf-float linker flag sama sekali.
  */
static void RenderSensorData(const sensor_data_t *data)
{
    char buf[48];
    char numbuf[16];
    int  y = 0;

    epd_clear_buffer();

    // ── Header ───────────────────────────────────────────────────────────────
    epd_fill_rect(0, y, EPD_WIDTH, 16, EPD_BLACK);
    epd_draw_string(4, y + 4, "STM32 Env Monitor", FONT_SMALL, EPD_WHITE);
    y += 17;
    epd_draw_hline(0, y, EPD_WIDTH, EPD_BLACK);
    y += 6;

    // ── Data sensor ──────────────────────────────────────────────────────────
    if (data->valid) {
        format_1dp(numbuf, sizeof(numbuf), data->temperature, " C");
        snprintf(buf, sizeof(buf), "Temp : %s", numbuf);
        epd_draw_string(4, y, buf, FONT_MEDIUM, EPD_BLACK);
        y += 18;

        format_1dp(numbuf, sizeof(numbuf), data->humidity, " %");
        snprintf(buf, sizeof(buf), "Humi : %s", numbuf);
        epd_draw_string(4, y, buf, FONT_MEDIUM, EPD_BLACK);
        y += 18;
    } else {
        epd_draw_string(4, y, "Sensor error",  FONT_MEDIUM, EPD_BLACK); y += 18;
        epd_draw_string(4, y, "no data yet",   FONT_MEDIUM, EPD_BLACK); y += 18;
    }

    y += 4;
    epd_draw_hline(0, y, EPD_WIDTH, EPD_BLACK);
    y += 6;

    // ── Uptime ───────────────────────────────────────────────────────────────
    // HAL_GetTick() akurat asal CubeMX pakai timer terpisah (mis. TIM10/TIM11)
    // sebagai HAL tick source saat FreeRTOS di-enable, bukan SysTick — SysTick
    // dipakai FreeRTOS sendiri. Ini setting default CubeMX kalau FreeRTOS
    // di-enable via .ioc, tapi tolong dicek waktu generate project.
    uint32_t sec_total = HAL_GetTick() / 1000U;
    uint32_t hours = sec_total / 3600U;
    uint32_t mins  = (sec_total % 3600U) / 60U;
    uint32_t secs  = sec_total % 60U;
    snprintf(buf, sizeof(buf), "Uptime: %02lu:%02lu:%02lu",
             (unsigned long)hours, (unsigned long)mins, (unsigned long)secs);
    epd_draw_string(4, y, buf, FONT_SMALL, EPD_BLACK);

    // ── Footer ───────────────────────────────────────────────────────────────
    epd_draw_hline(0, EPD_HEIGHT - 12, EPD_WIDTH, EPD_BLACK);
    epd_draw_string(4, EPD_HEIGHT - 10, "MJ Mokhtar - Toaster Co", FONT_SMALL, EPD_BLACK);
}


/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM10 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM10)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
