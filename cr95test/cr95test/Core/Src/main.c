/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

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
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart2_rx;

/* USER CODE BEGIN PV */

#define RX_BUFFER_LEN 64
#define CMD_BUFFER_LEN 64

#define NFC_TIMEOUT	1000

static uint8_t uart_rx_buf[RX_BUFFER_LEN];
static volatile uint16_t uart_rx_read_ptr = 0;
#define uart_rx_write_ptr (RX_BUFFER_LEN - hdma_usart2_rx.Instance->CNDTR)

static uint8_t nfc_rx_buf[RX_BUFFER_LEN];
static volatile uint16_t nfc_rx_read_ptr = 0;
#define nfc_rx_write_ptr (RX_BUFFER_LEN - hdma_usart1_rx.Instance->CNDTR)

static uint8_t DacDataRef;
static bool nfc_ready = false;
static bool printf_en = true;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

int _write(int file, char const *buf, int n)
{
    if (printf_en) HAL_UART_Transmit(&huart2, (uint8_t*)(buf), n, HAL_MAX_DELAY);
    return n;
}

void cr95write(const uint8_t *data, uint8_t length)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)(data), length, HAL_MAX_DELAY);
}

uint8_t cr95read(uint8_t *data, uint8_t *length)
{
	uint32_t timeout = HAL_GetTick();

	do {
		if (HAL_GetTick() - timeout > NFC_TIMEOUT) return 0xFF; // timeout
	} while (nfc_rx_read_ptr == nfc_rx_write_ptr);
	uint8_t resp = nfc_rx_buf[nfc_rx_read_ptr];
    if (++nfc_rx_read_ptr >= RX_BUFFER_LEN) nfc_rx_read_ptr = 0; // increase read pointer

    if (resp == 0x55) return resp;

	do {
		if (HAL_GetTick() - timeout > NFC_TIMEOUT) return 0xFF; // timeout
	} while (nfc_rx_read_ptr == nfc_rx_write_ptr);
	uint8_t len = nfc_rx_buf[nfc_rx_read_ptr];
    if (++nfc_rx_read_ptr >= RX_BUFFER_LEN) nfc_rx_read_ptr = 0; // increase read pointer

    if (length) *length = len;
    while (len--) {
    	do {
    		if (HAL_GetTick() - timeout > NFC_TIMEOUT) return 0xFF; // timeout
    	} while (nfc_rx_read_ptr == nfc_rx_write_ptr);
    	if (data) *data++ = nfc_rx_buf[nfc_rx_read_ptr];
        if (++nfc_rx_read_ptr >= RX_BUFFER_LEN) nfc_rx_read_ptr = 0; // increase read pointer
    }

    return resp;
}

static void cr95_wakeup(void)
{
	const uint8_t wakeup = 0;
	cr95write(&wakeup, 1);
	printf("WAKEUP sent\n");
}

static void cr95_init14(void)
{
	const uint8_t cmd_init1[] = { 0x02, 0x02, 0x02, 0x00 };
	const uint8_t cmd_init2[] = { 0x09, 0x04, 0x3A, 0x00, 0x58, 0x04 };
	const uint8_t cmd_init3[] = { 0x09, 0x04, 0x68, 0x01, 0x01, 0xD1 };

	cr95write(cmd_init1, sizeof(cmd_init1));
	printf("Initiation of 14 %s", (cr95read(NULL, NULL) == 0x00) ? "yes" : "no");
	cr95write(cmd_init2, sizeof(cmd_init2));
	printf(" %s", (cr95read(NULL, NULL) == 0x00) ? "yes" : "no");
	cr95write(cmd_init3, sizeof(cmd_init3));
	printf(" %s\n", (cr95read(NULL, NULL) == 0x00) ? "yes" : "no");
}

static void cr95_init14B(void)
{
	const uint8_t cmd_init1B[] = { 0x02, 0x02, 0x03, 0x01 };
	const uint8_t cmd_init2B[] = { 0x09, 0x04, 0x3A, 0x00, 0x58, 0x04 };
	const uint8_t cmd_init3B[] = { 0x09, 0x04, 0x68, 0x01, 0x01, 0x20 };

	cr95write(cmd_init1B, sizeof(cmd_init1B));
	printf("Initiation of 14B %s", (cr95read(NULL, NULL) == 0x00) ? "yes" : "no");
	cr95write(cmd_init2B, sizeof(cmd_init2B));
	printf(" %s", (cr95read(NULL, NULL) == 0x00) ? "yes" : "no");
	cr95write(cmd_init3B, sizeof(cmd_init3B));
	printf(" %s\n", (cr95read(NULL, NULL) == 0x00) ? "yes" : "no");
}

static void cr95_init15(void)
{
	const uint8_t cmd_init1_15[] = { 0x02, 0x02, 0x01, 0x03 };
	const uint8_t cmd_init2_15[] = { 0x09, 0x04, 0x3A, 0x00, 0x58, 0x04 };
	const uint8_t cmd_init3_15[] = { 0x09, 0x04, 0x68, 0x01, 0x01, 0xD0 };

	cr95write(cmd_init1_15, sizeof(cmd_init1_15));
	printf("Initiation of 15 %s", (cr95read(NULL, NULL) == 0x00) ? "yes" : "no");
	cr95write(cmd_init2_15, sizeof(cmd_init2_15));
	printf(" %s", (cr95read(NULL, NULL) == 0x00) ? "yes" : "no");
	cr95write(cmd_init3_15, sizeof(cmd_init3_15));
	printf(" %s\n", (cr95read(NULL, NULL) == 0x00) ? "yes" : "no");
}

static void cr95_init18(void)
{
	const uint8_t cmd_init1_18[] = { 0x02, 0x02, 0x04, 0x51 };
	const uint8_t cmd_init2_18[] = { 0x09, 0x04, 0x3A, 0x00, 0x58, 0x04 };
	const uint8_t cmd_init3_18[] = { 0x09, 0x04, 0x68, 0x01, 0x01, 0x50 };
	const uint8_t cmd_init4_18[] = { 0x09, 0x04, 0x0A, 0x01, 0x02, 0xA1 };


	cr95write(cmd_init1_18, sizeof(cmd_init1_18));
	printf("Initiation of 18 %s", (cr95read(NULL, NULL) == 0x00) ? "yes" : "no");
	cr95write(cmd_init2_18, sizeof(cmd_init2_18));
	printf(" %s", (cr95read(NULL, NULL) == 0x00) ? "yes" : "no");
	cr95write(cmd_init3_18, sizeof(cmd_init3_18));
	printf(" %s\n", (cr95read(NULL, NULL) == 0x00) ? "yes" : "no");
	cr95write(cmd_init4_18, sizeof(cmd_init4_18));
	printf(" %s\n", (cr95read(NULL, NULL) == 0x00) ? "yes" : "no");
}


static void cr95_read(void)
{
	const uint8_t cmd_reqa[] =  { 0x04, 0x02, 0x26, 0x07 };
	const uint8_t cmd_acl1[] =  { 0x04, 0x03, 0x93, 0x20, 0x08 };
	const uint8_t cmd_acl2[] =  { 0x04, 0x03, 0x95, 0x20, 0x08 };

	uint8_t data[8];
	char uid[32];
	uint8_t len;

	cr95write(cmd_reqa, sizeof(cmd_reqa));
	if (cr95read(data, &len) == 0x80) {
		printf("ATQA =");
		for (uint8_t i = 0; i < len; i++) printf(" %02X", data[i]);
		printf("\n");

    	sprintf(uid, "UID =");

    	cr95write(cmd_acl1, sizeof(cmd_acl1));
    	if (cr95read(data, &len) == 0x80 && len == 8 && (data[0]^data[1]^data[2]^data[3]) == data[4]) {
    		printf("UID CL1 =");

    		for (uint8_t i = 0; i < len; i++) printf(" %02X", data[i]);
    		printf("\n");


    		if (data[5] & 0x80) printf("Collision detected!\n");
    		if (data[0] == 0x88) {
    			sprintf(uid, "%s %2X %2X %2X", uid, data[1], data[2], data[3]);

				cr95write(cmd_acl2, sizeof(cmd_acl2));
				if (cr95read(data, &len) == 0x80 && len == 9 && (data[0]^data[1]^data[2]^data[3]) == data[4]) {
					printf("UID CL2 =");
					for (uint8_t i = 0; i < len; i++) printf(" %02X", data[i]);
					printf("\n");

		    		if (data[5] & 0x80) printf("Collision detected!\n");
		    		if (data[0] == 0x88) {
		    			sprintf(uid, "%s %2X %2X %2X", uid, data[1], data[2], data[3]);
						/*cr95write(cmd_acl3, sizeof(cmd_acl3));
						if (cr95read(data, &len) == 0x80 && len == 8 && (data[0]^data[1]^data[2]^data[3]) == data[4]) {
							printf("UID CL3 =");
							for (uint8_t i = 0; i < len; i++) printf(" %02X", data[i]);
							printf("\n");

				    		if (data[5] & 0x80) printf("Collision detected!\n");
			    			sprintf(uid, "%s %2X %2X %2X %2X", uid, data[0], data[1], data[2], data[3]);
						} else {
							printf("UID CL3 error\n");
						} */
		    		} else {
		    			sprintf(uid, "%s %2X %2X %2X %2X", uid, data[0], data[1], data[2], data[3]);
		    		}
				} else {
					printf("UID CL2 error\n");
				}

    		} else {
    			sprintf(uid, "%s %2X %2X %2X %2X", uid, data[1], data[2], data[3], data[4]);
    		}


    		HAL_UART_Transmit(&huart2, (uint8_t*)(uid), strlen(uid), HAL_MAX_DELAY);
    	} else {
    		printf("UID CL1 error\n");
    	}

	} else {
		printf("REQA error\n");
	}
}

static void cr95_readtopaz(void)
{
	const uint8_t cmd_reqtopaz[] =  { 0x04, 0x02, 0x26, 0x07 };
	const uint8_t cmd_rid[]      =  { 0x04, 0x08, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA8 };

	uint8_t data[16];
	char uid[32];
	uint8_t len;

	cr95write(cmd_reqtopaz, sizeof(cmd_reqtopaz));
	if (cr95read(data, &len) == 0x80) {
		printf("ATQA =");
		for (uint8_t i = 0; i < len; i++) printf(" %02X", data[i]);
		printf("\n");

		sprintf(uid, "UID =");

		cr95write(cmd_rid, sizeof(cmd_rid));
		if (cr95read(data, &len) == 0x80 ) {
			printf("RID =");
			for (uint8_t i = 0; i < len; i++) printf(" %02X", data[i]);
			printf("\n");
			printf("Header 1 = %2X", data[0]);
			printf("Header 2 = %2X", data[1]);
			sprintf(uid, "%s %2X %2X %2X %2X", uid, data[2], data[3], data[4], data[5]);
		}
	}
}

static void cr95_read14B(void)
{
	const uint8_t cmd_reqb[] =  { 0x04, 0x03, 0x05, 0x00, 0x00 };

	uint8_t data[16];
	uint8_t len;

	cr95write(cmd_reqb, sizeof(cmd_reqb));
	if (cr95read(data, &len) == 0x80 && data[0] == 0x50) {
		printf("ATQB =");
		for (uint8_t i = 0; i <= len; i++) printf(" %02X", data[i]);
		printf("\n");
	}
}

static void cr95_read18(void)
{
	const uint8_t cmd_reqc[] =  { 0x04, 0x05, 0x00, 0xFF, 0xFF, 0x00, 0x00 };

	uint8_t data[16];
	uint8_t len;

	cr95write(cmd_reqc, sizeof(cmd_reqc));
	if (cr95read(data, &len) == 0x80) {
		printf("ISO/IEC 18092 DATA =");
		for (uint8_t i = 0; i <= len; i++) printf(" %02X", data[i]);
		printf("\n");
	}
}

static void cr95_calibrate(void)
{
	uint8_t cmd_cal[] =  	    { 0x07, 0x0E, 0x03, 0xA1, 0x00, 0xF8, 0x01, 0x18, 0x00, 0x20, 0x60, 0x60, 0x00, 0x00, 0x3F, 0x01 };

	uint8_t data[16];
	uint8_t len;

	cmd_cal[13] = 0x00;
	cr95write(cmd_cal, sizeof(cmd_cal));
	printf("CAL #0 0x%02x %c, result 0x%02x\n", cmd_cal[13], (cr95read(data, &len) == 0x00) ? 'y' : 'n', data[0]);

	cmd_cal[13] = 0xFC;
	cr95write(cmd_cal, sizeof(cmd_cal));
	printf("CAL #1 0x%02x %c, result 0x%02x\n", cmd_cal[13], (cr95read(data, &len) == 0x00) ? 'y' : 'n', data[0]);

	cmd_cal[13] -= 0x80;
	cr95write(cmd_cal, sizeof(cmd_cal));
	printf("CAL #2 0x%02x %c, result 0x%02x\n", cmd_cal[13], (cr95read(data, &len) == 0x00) ? 'y' : 'n', data[0]);

	if (data[0] == 0x01) cmd_cal[13] -= 0x40; else cmd_cal[13] += 0x40;
	cr95write(cmd_cal, sizeof(cmd_cal));
	printf("CAL #3 0x%02x %c, result 0x%02x\n", cmd_cal[13], (cr95read(data, &len) == 0x00) ? 'y' : 'n', data[0]);

	if (data[0] == 0x01) cmd_cal[13] -= 0x20; else cmd_cal[13] += 0x20;
	cr95write(cmd_cal, sizeof(cmd_cal));
	printf("CAL #4 0x%02x %c, result 0x%02x\n", cmd_cal[13], (cr95read(data, &len) == 0x00) ? 'y' : 'n', data[0]);

	if (data[0] == 0x01) cmd_cal[13] -= 0x10; else cmd_cal[13] += 0x10;
	cr95write(cmd_cal, sizeof(cmd_cal));
	printf("CAL #5 0x%02x %c, result 0x%02x\n", cmd_cal[13], (cr95read(data, &len) == 0x00) ? 'y' : 'n', data[0]);

	if (data[0] == 0x01) cmd_cal[13] -= 0x08; else cmd_cal[13] += 0x08;
	cr95write(cmd_cal, sizeof(cmd_cal));
	printf("CAL #6 0x%02x %c, result 0x%02x\n", cmd_cal[13], (cr95read(data, &len) == 0x00) ? 'y' : 'n', data[0]);

	if (data[0] == 0x01) cmd_cal[13] -= 0x04; else cmd_cal[13] += 0x04;
	cr95write(cmd_cal, sizeof(cmd_cal));
	printf("CAL #7 0x%02x %c, result 0x%02x\n", cmd_cal[13], (cr95read(data, &len) == 0x00) ? 'y' : 'n', data[0]);

	if (data[0] == 0x01) cmd_cal[13] -= 0x04;
	DacDataRef = cmd_cal[13];
	printf("CAL finished, DacDataRef=0x%02x\n", DacDataRef);
}

static void cr95_idle(uint8_t mode)
{
	uint8_t cmd_idle[] =  		{ 0x07, 0x0E, 0x0A, 0x21, 0x00, 0x79, 0x01, 0x18, 0x00, 0x20, 0x60, 0x60, 0x00, 0x00, 0x3F, 0x08 };

	if (mode == 1) cmd_idle[2] = 0x08;   // Hibernate
	else cmd_idle[2] = 0x0A;             // TagDetect

	cmd_idle[12] = DacDataRef - 8;
	cmd_idle[13] = DacDataRef + 8;
	cr95write(cmd_idle, sizeof(cmd_idle));
	printf("IDLE sent\n");
}

static void uart_process_command(char *cmd)
{
    char *token;
    token = strtok(cmd, " ");
	uint8_t data[16];
	uint8_t len;

	const uint8_t cmd_echo[] =  { 0x55 };
	const uint8_t cmd_idn[] =   { 0x01, 0x00 };

    if (strcasecmp(token, "HELLO") == 0) {
        printf("Komunikace OK\n");
    }
    else if (strcasecmp(token, "ON") == 0) {
    	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
    	MX_USART1_UART_Init();
        HAL_UART_Receive_DMA(&huart1, nfc_rx_buf, RX_BUFFER_LEN);
    	HAL_Delay(5);
    	printf("RFID ON\n");
        nfc_rx_read_ptr = nfc_rx_write_ptr;
    	cr95_wakeup();
    	nfc_ready = true;
    }
    else if (strcasecmp(token, "OFF") == 0) {
    	nfc_ready = false;
        HAL_UART_AbortReceive(&huart1);
    	HAL_UART_DeInit(&huart1);
    	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
    	printf("RFID OFF\n");
    }
    else if (strcasecmp(token, "ECHO") == 0) {
    	cr95write(cmd_echo, sizeof(cmd_echo));
    	uint8_t resp = cr95read(NULL, NULL);
    	printf("ECHO %s %02X\n", (resp == 0x55) ? "yes" : "no", resp);
    }
    else if (strcasecmp(token, "IDN") == 0) {
    	cr95write(cmd_idn, sizeof(cmd_idn));
    	if (cr95read(data, &len) == 0x00) {
    		printf("IDN =");
    		for (uint8_t i = 0; i < len; i++) printf(" %02X", data[i]);
    		printf("\n");
    	} else {
    		printf("IDN error\n");
    	}
    }
    else if (strcasecmp(token, "INIT14") == 0) {
    	cr95_init14();
    }
    else if (strcasecmp(token, "INIT14B") == 0) {
        cr95_init14B();
    }
    else if (strcasecmp(token, "INIT15") == 0) {
        cr95_init15();
    }
    else if (strcasecmp(token, "INIT18") == 0) {
        cr95_init18();
    }
    else if (strcasecmp(token, "READ") == 0) {
    	cr95_read();
    }
    else if (strcasecmp(token, "READTOPAZ") == 0) {
        cr95_readtopaz();
       }
    else if (strcasecmp(token, "READ14B") == 0) {
        cr95_read14B();
    }
    else if (strcasecmp(token, "READ18") == 0) {
        cr95_read18();
    }
    else if (strcasecmp(token, "CALIBRATE") == 0) {
    	cr95_calibrate();
    }
    else if (strcasecmp(token, "IDLE") == 0) {
    	cr95_idle(1);
    }
    else if (strcasecmp(token, "WAKEUP") == 0) {
    	cr95_wakeup();
    	uint8_t resp = cr95read(data, &len);
    	printf("Code of wakeup is: %02X with response: %02X\n", data[0],resp);
    }
    else if (strcasecmp(token, "AUTO") == 0) {
    	cr95_calibrate();
    	do {
        	cr95_idle(0);

			do {} while (nfc_rx_read_ptr == nfc_rx_write_ptr);
			uint8_t resp = cr95read(data, &len);
			if (resp == 0x00 && data[0] == 0x02) printf("WAKEUP by tag detect\n");
			else printf("Error\n");
			printf("Code of wakeup is:%02X\n", data[0]);
        	cr95_init14();
        	cr95_read();
        	HAL_Delay(2000);
    	} while (uart_rx_read_ptr == uart_rx_write_ptr);
    }
    else {
        printf("Unknown command\n");
    }
}


static void uart_byte_available(uint8_t c)
{
    static uint16_t cnt;
    static char data[CMD_BUFFER_LEN];

    if (cnt < CMD_BUFFER_LEN) data[cnt++] = c;
    if (c == '\n' || c == '\r') {
        data[cnt - 1] = '\0';
        uart_process_command(data);
        cnt = 0;
    }
}


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
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  HAL_UART_DeInit(&huart1);
  HAL_UART_Receive_DMA(&huart2, uart_rx_buf, RX_BUFFER_LEN);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
#if 1
	  while (uart_rx_read_ptr != uart_rx_write_ptr) {
	      uint8_t b = uart_rx_buf[uart_rx_read_ptr];
	      if (++uart_rx_read_ptr >= RX_BUFFER_LEN) uart_rx_read_ptr = 0; // increase read pointer

	      uart_byte_available(b); // process every received byte with the RX state machine
	  }

	  if (nfc_ready && nfc_rx_read_ptr != nfc_rx_write_ptr) {
		  uint8_t data[16];
		  uint8_t len;
		  uint8_t resp = cr95read(data, &len);

		  if (resp != 0xFF) {
			  printf("Async response, code = 0x%02x, len = %d, data =", resp, len);
			  for (uint8_t i = 0; i < len; i++) printf(" %02X", data[i]);
			  printf("\n");
		  } else {
			  printf("Async reponse, invalid (timeout)\n");
		  }

	  }
#else
	  printf_en = true;
	  uart_process_command("on");
	  uart_process_command("echo");
	  HAL_Delay(2000);


	/*  do {
		  cr95_init14443();
		  cr95_read();

		  cr95_idle(1);
		  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
		  HAL_Delay(500);
		  cr95_wakeup(); cr95read(NULL, NULL);
		  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
	  } while (1); */


#endif

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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the CPU, AHB and APB busses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL12;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
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
  huart1.Init.BaudRate = 57600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_2;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
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
  huart2.Init.BaudRate = 38400;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel2_3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
  /* DMA1_Channel4_5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel4_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_5_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LD2_Pin|GPIO_PIN_8, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD2_Pin PA8 */
  GPIO_InitStruct.Pin = LD2_Pin|GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PC7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

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

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
