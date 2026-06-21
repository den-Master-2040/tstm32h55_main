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
#include "string.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "freedv_api.h"
#include "codec2_fifo.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* DUAL_CORE_BOOT_SYNC_SEQUENCE: Define for dual core boot synchronization    */
/*                             demonstration code based on hardware semaphore */
/* This define is present in both CM7/CM4 projects                            */
/* To comment when developping/debugging on a single core                     */
#define DUAL_CORE_BOOT_SYNC_SEQUENCE

#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
#ifndef HSEM_ID_0
#define HSEM_ID_0 (0U) /* HW semaphore 0*/
#endif
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

ADC_HandleTypeDef hadc3;
DMA_HandleTypeDef hdma_adc3;

DAC_HandleTypeDef hdac1;

TIM_HandleTypeDef htim6;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void MX_GPIO_Init(void);
static void MX_BDMA_Init(void);
static void MX_ADC3_Init(void);
static void MX_TIM6_Init(void);
static void MX_DAC1_Init(void);
/* USER CODE BEGIN PFP */
void ipc_m4_init(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ---- config --------------------------------------------------------------- */
#define MODEM_MODE       FREEDV_MODE_DATAC3
#define RX_FIFO_SAMPLES  2048
#define ADC_DMA_LEN      256        /* circular; half-callback every 128 samples */
#define DAC_BURST_MAX    12000      /* one full burst must fit here              */
#define SILENCE_GAP      1600       /* 0.2 s @ 8 kHz between bursts -> re-sync    */
#define DAC_SHIFT        4          /* int16 -> 12-bit scale; lower = louder      */

/* Some Nucleo-H755 boards: LD1 (green) = PB0. Adjust to your CubeMX label. */
#ifndef LED_GPIO_Port
#define LED_GPIO_Port    GPIOB
#define LED_Pin          GPIO_PIN_0
#endif

/* ---- modem state ---------------------------------------------------------- */
static struct freedv *g_fdv;
static int g_frame_len;             /* bytes incl. 2 CRC */
static int g_payload_len;           /* g_frame_len - 2   */
static int g_ntx;                   /* samples per modulated data frame */
static int g_nmax;                  /* max samples rawdatarx may request */

/* ---- buffers (D2 SRAM by default on CM4 = DMA-reachable) ------------------- */
static uint16_t g_dac_burst[DAC_BURST_MAX];
static int      g_dac_burst_len;
static uint16_t g_adc_buf[ADC_DMA_LEN];
static struct FIFO *g_rx_fifo;
static int16_t  g_demod_in[6000];   /* must be >= g_nmax */
static uint8_t  g_frame_out[600];   /* must be >= g_frame_len */

/* ---- debugger-visible counters -------------------------------------------- */
volatile uint32_t g_rx_frames   = 0;   /* CRC-valid frames decoded */
volatile uint32_t g_rx_pattern_ok = 0; /* of those, payload matched test pattern */
volatile float    g_snr_est     = 0.0f;

/* ===========================================================================
 *  sample conversions
 * =========================================================================== */
static inline uint16_t s16_to_dac12(int16_t s) {
    int v = (s >> DAC_SHIFT) + 2048;
    if (v < 0)    v = 0;
    if (v > 4095) v = 4095;
    return (uint16_t)v;
}
static inline int16_t adc12_to_s16(uint16_t a) {
    return (int16_t)(((int)a - 2048) << DAC_SHIFT);
}

/* ===========================================================================
 *  ADC DMA -> FIFO (ISR context; codec2_fifo is lock-free single-producer)
 * =========================================================================== */
static void push_adc(const uint16_t *p, int n) {
    int16_t tmp[ADC_DMA_LEN / 2];
    for (int i = 0; i < n; i++) tmp[i] = adc12_to_s16(p[i]);
    codec2_fifo_write(g_rx_fifo, tmp, n);
}
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *h) {
    if (h->Instance == ADC3) push_adc(&g_adc_buf[0], ADC_DMA_LEN / 2);
}
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *h) {
    if (h->Instance == ADC3) push_adc(&g_adc_buf[ADC_DMA_LEN / 2], ADC_DMA_LEN / 2);
}

/* ===========================================================================
 *  build the burst the DAC will replay forever
 * =========================================================================== */
static void build_tx_burst(void) {
    int16_t mod[2400];               /* >= g_ntx and >= preamble length */
    uint8_t frame[600];
    int idx = 0, n;

    n = freedv_rawdatapreambletx(g_fdv, mod);
    for (int i = 0; i < n; i++) g_dac_burst[idx++] = s16_to_dac12(mod[i]);

    /* one data frame: known test pattern + CRC in the last two bytes */
    for (int i = 0; i < g_payload_len; i++) frame[i] = (uint8_t)(i & 0xff);
    uint16_t crc = freedv_gen_crc16(frame, g_payload_len);
    frame[g_frame_len - 2] = crc >> 8;
    frame[g_frame_len - 1] = crc & 0xff;
    freedv_rawdatatx(g_fdv, mod, frame);
    for (int i = 0; i < g_ntx; i++) g_dac_burst[idx++] = s16_to_dac12(mod[i]);

    n = freedv_rawdatapostambletx(g_fdv, mod);
    for (int i = 0; i < n; i++) g_dac_burst[idx++] = s16_to_dac12(mod[i]);

    for (int i = 0; i < SILENCE_GAP; i++) g_dac_burst[idx++] = 2048;  /* mid-rail */

    g_dac_burst_len = idx;
    /* If this trips, raise DAC_BURST_MAX. */
    while (idx > DAC_BURST_MAX) { /* hang: buffer too small */ }
}

/* ===========================================================================
 *  public entry — call from CM4 main() AFTER MX_*_Init()
 * =========================================================================== */
void loopback_selftest_run(void) {
    g_fdv = freedv_open(MODEM_MODE);
    while (!g_fdv) { /* open failed: check heap size in linker script */ }

    g_frame_len   = freedv_get_bits_per_modem_frame(g_fdv) / 8;
    g_payload_len = g_frame_len - 2;
    g_ntx         = freedv_get_n_tx_modem_samples(g_fdv);
    g_nmax        = freedv_get_n_max_modem_samples(g_fdv);

    /* sanity: scratch big enough, sample rate is the 8 kHz we clocked TIM6 to */
    while (g_nmax  > (int)(sizeof g_demod_in / sizeof g_demod_in[0])) { }
    while (g_frame_len > (int)sizeof g_frame_out) { }
    while (freedv_get_modem_sample_rate(g_fdv) != 8000) { }   /* must match TIM6 */

    g_rx_fifo = codec2_fifo_create(RX_FIFO_SAMPLES);
    while (!g_rx_fifo) { }

    build_tx_burst();

    /* H7 ADC wants an offset calibration before first use */
    HAL_ADCEx_Calibration_Start(&hadc3, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED);

    /* Start consumers first, producer (TIM6) last so nothing is missed */
    HAL_ADC_Start_DMA(&hadc3, (uint32_t *)g_adc_buf, ADC_DMA_LEN);
    HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1,
                      (uint32_t *)g_dac_burst, g_dac_burst_len, DAC_ALIGN_12B_R);
    HAL_TIM_Base_Start(&htim6);      /* 8 kHz TRGO now drives both */

    for (;;) {
        int nin = freedv_nin(g_fdv);                 /* varies — never hard-code */
        if (codec2_fifo_used(g_rx_fifo) >= nin) {
            codec2_fifo_read(g_rx_fifo, g_demod_in, nin);
            int nbytes = freedv_rawdatarx(g_fdv, g_frame_out, g_demod_in);
            if (nbytes) {
                int sync; freedv_get_modem_stats(g_fdv, &sync, &g_snr_est);
                g_rx_frames++;
                /* verify the recovered payload matches what we sent */
                int ok = 1;
                for (int i = 0; i < g_payload_len; i++)
                    if (g_frame_out[i] != (uint8_t)(i & 0xff)) { ok = 0; break; }
                if (ok) g_rx_pattern_ok++;
                HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
            }
        }
        /* nothing to do until more audio arrives; optional: __WFI(); */
    }
}

volatile const char *g_assert_file;
volatile int          g_assert_line;
volatile const char *g_assert_expr;

void __assert_func(const char *file, int line, const char *func, const char *expr)
{
    g_assert_file = file;
    g_assert_line = line;
    g_assert_expr = expr;
    __asm volatile ("bkpt 0");   /* остановка прямо здесь */
    while (1) {}
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
	  SCB->CPACR |= ((3UL << 20)|(3UL << 22));   /* CP10/CP11 full access */

  /* USER CODE END 1 */

/* USER CODE BEGIN Boot_Mode_Sequence_1 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
  /*HW semaphore Clock enable*/
  __HAL_RCC_HSEM_CLK_ENABLE();
  /* Activate HSEM notification for Cortex-M4*/
  HAL_HSEM_ActivateNotification(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_0));
  /*
  Domain D2 goes to STOP mode (Cortex-M4 in deep-sleep) waiting for Cortex-M7 to
  perform system initialization (system clock config, external memory configuration.. )
  */
  HAL_PWREx_ClearPendingEvent();
  HAL_PWREx_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFE, PWR_D2_DOMAIN);
  /* Clear HSEM flag */
  __HAL_HSEM_CLEAR_FLAG(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_0));

#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
/* USER CODE END Boot_Mode_Sequence_1 */
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  //MX_BDMA_Init();
  //MX_ADC3_Init();
  //MX_TIM6_Init();
  //MX_DAC1_Init();
  /* USER CODE BEGIN 2 */
  //ipc_m4_init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  loopback_selftest_run();
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief ADC3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC3_Init(void)
{

  /* USER CODE BEGIN ADC3_Init 0 */

  /* USER CODE END ADC3_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC3_Init 1 */

  /* USER CODE END ADC3_Init 1 */

  /** Common config
  */
  hadc3.Instance = ADC3;
  hadc3.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc3.Init.Resolution = ADC_RESOLUTION_16B;
  hadc3.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc3.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc3.Init.LowPowerAutoWait = DISABLE;
  hadc3.Init.ContinuousConvMode = DISABLE;
  hadc3.Init.NbrOfConversion = 1;
  hadc3.Init.DiscontinuousConvMode = DISABLE;
  hadc3.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc3.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
  hadc3.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc3.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc3.Init.OversamplingMode = DISABLE;
  hadc3.Init.Oversampling.Ratio = 1;
  if (HAL_ADC_Init(&hadc3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  sConfig.OffsetSignedSaturation = DISABLE;
  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC3_Init 2 */

  /* USER CODE END ADC3_Init 2 */

}

/**
  * @brief DAC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC1_Init(void)
{

  /* USER CODE BEGIN DAC1_Init 0 */

  /* USER CODE END DAC1_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC1_Init 1 */

  /* USER CODE END DAC1_Init 1 */

  /** DAC Initialization
  */
  hdac1.Instance = DAC1;
  if (HAL_DAC_Init(&hdac1) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT1 config
  */
  sConfig.DAC_SampleAndHold = DAC_SAMPLEANDHOLD_DISABLE;
  sConfig.DAC_Trigger = DAC_TRIGGER_NONE;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  sConfig.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_DISABLE;
  sConfig.DAC_UserTrimming = DAC_TRIMMING_FACTORY;
  if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC1_Init 2 */

  /* USER CODE END DAC1_Init 2 */

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 0;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 65535;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_BDMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_BDMA_CLK_ENABLE();

  /* DMA interrupt init */
  /* BDMA_Channel0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(BDMA_Channel0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(BDMA_Channel0_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
