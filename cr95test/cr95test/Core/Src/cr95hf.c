/*
 * cr95hf.c
 *
 *  Created on: Apr 13, 2021
 *      Author: MichalObs97
 */

#include "main.h"
#include "ssd1306.h"
#include "fonts.h"
#include "stm32f0xx_hal.h"
#include "stm32f0xx_hal_uart.h"
#include "stm32f0xx_hal_dma.h"
#include "stm32f0xx_hal_gpio.h"
#include "stdlib.h"
#include "stdio.h"
#include "stdbool.h"
#include "string.h"
#include "cr95hf.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart2_rx;

#define RX_BUFFER_LEN  64
#define CMD_BUFFER_LEN 64
#define NFC_TIMEOUT	1000

uint8_t uart_rx_buf[RX_BUFFER_LEN];
volatile uint16_t uart_rx_read_ptr = 0;
#define uart_rx_write_ptr (RX_BUFFER_LEN - hdma_usart2_rx.Instance->CNDTR)

uint8_t nfc_rx_buf[RX_BUFFER_LEN];
volatile uint16_t nfc_rx_read_ptr = 0;
#define nfc_rx_write_ptr (RX_BUFFER_LEN - hdma_usart1_rx.Instance->CNDTR)

bool nfc_ready = false;
bool printf_en = true;
uint8_t disp_len;
static uint8_t DacDataRef;

void uart_init(void)
{
	HAL_UART_Receive_DMA(&huart2, uart_rx_buf, RX_BUFFER_LEN);
}

int _write(int file, char const *buf, int n)
{
    if (printf_en) HAL_UART_Transmit(&huart2, (uint8_t*)(buf), n, HAL_MAX_DELAY);
    return n;
}

static void cr95write(const uint8_t *data, uint8_t length)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)(data), length, HAL_MAX_DELAY);
}

static uint8_t cr95read(uint8_t *data, uint8_t *length)
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


static void cr95_init14(void)
{
	const uint8_t cmd_init1[] = { 0x02, 0x02, 0x02, 0x00 };
	const uint8_t cmd_init2[] = { 0x09, 0x04, 0x68, 0x01, 0x01, 0xD1 };
	const uint8_t cmd_init3[] = { 0x09, 0x04, 0x3A, 0x00, 0x58, 0x04 };


	cr95write(cmd_init1, sizeof(cmd_init1));
	printf("Initiation of 14 %s", (cr95read(NULL, NULL) == 0x00) ? "yes" : "no");
	cr95write(cmd_init2, sizeof(cmd_init2));
	printf(" %s", (cr95read(NULL, NULL) == 0x00) ? "yes" : "no");
	cr95write(cmd_init3, sizeof(cmd_init3));
	printf(" %s\n", (cr95read(NULL, NULL) == 0x00) ? "yes" : "no");
}

static void cr95_init15(void)
{
	const uint8_t cmd_init1_15[] = { 0x02, 0x02, 0x01, 0x05 };
	const uint8_t cmd_init2_15[] = { 0x09, 0x04, 0x68, 0x01, 0x01, 0x50 };

	cr95write(cmd_init1_15, sizeof(cmd_init1_15));
	printf("Initiation of 15 %s", (cr95read(NULL, NULL) == 0x00) ? "yes" : "no");
	cr95write(cmd_init2_15, sizeof(cmd_init2_15));
	printf(" %s\n", (cr95read(NULL, NULL) == 0x00) ? "yes" : "no");
}

static void cr95_read(void)
{
	const uint8_t cmd_reqa[] =  { 0x04, 0x02, 0x26, 0x07 };
	const uint8_t cmd_acl1[] =  { 0x04, 0x03, 0x93, 0x20, 0x08 };
	const uint8_t cmd_acl2[] =  { 0x04, 0x03, 0x95, 0x20, 0x08 };

	uint8_t data[8];
	uint8_t saved_data[10] =  { 0x04, 0x08, 0x93, 0x70, 0x00, 0x00, 0x00, 0x00,  0x00, 0x28};
	char uid[32];
	uint8_t len;


	cr95write(cmd_reqa, sizeof(cmd_reqa));
	if (cr95read(data, &len) == 0x80) {
		printf("ATQA =");
		for (uint8_t i = 0; i < len; i++) printf(" %02X", data[i]);
		printf("\n");

    	sprintf(uid, "UID=");

    	cr95write(cmd_acl1, sizeof(cmd_acl1));
    	if (cr95read(data, &len) == 0x80 && len == 8 && (data[0]^data[1]^data[2]^data[3]) == data[4]) {
    		printf("UID CL1 =");
    		for (uint8_t i = 0; i < len; i++) printf(" %02X", data[i]);
    		printf("\n");
    		saved_data[4] = data[0]; saved_data[5] = data[1]; saved_data[6] = data[2]; saved_data[7] = data[3]; saved_data[8] = data[4];

			if (data[0] == 0x88) {
				printf("Cascade bit detected, longer UID!\n");
				cr95write(saved_data, sizeof(saved_data));
				if (cr95read(data, &len) == 0x80) {
					printf("SEL1 Response =");
					for (uint8_t i = 0; i < len; i++) printf(" %02X", data[i]);
					printf("\n");
				}

				if (data[0] != 0x08) {
				   cr95write(cmd_acl2, sizeof(cmd_acl2));
				   if (cr95read(data, &len) == 0x80 && (data[0]^data[1]^data[2]^data[3]) == data[4]) {
				    	printf("UID CL2 =");
				    	for (uint8_t i = 0; i < len; i++) printf(" %02X", data[i]);
				    	printf("\n");
				    	sprintf(uid, "%s%X%X%X%X%X%X%X", uid, saved_data[5], saved_data[6], saved_data[7], data[0], data[1], data[2], data[3]);
				    	saved_data[2] = 0x95; saved_data[4] = data[0]; saved_data[5] = data[1]; saved_data[6] = data[2]; saved_data[7] = data[3]; saved_data[8] = data[4];

				    	cr95write(saved_data, sizeof(saved_data));
				    	if (cr95read(data, &len) == 0x80) {
				    		printf("SEL2 Response =");
				    		for (uint8_t i = 0; i < len; i++) printf(" %02X", data[i]);
				    		printf("\n");
				    		disp_len=2;
				    	}
				    } else {
				    	printf("UID CL2 error\n");
				    	disp_len=3;
				    }
				} else {
					printf("SEL CL1 error\n");
					disp_len=3;
				}
    		} else {
    			cr95write(saved_data, sizeof(saved_data));
    			if (cr95read(data, &len) == 0x80) {
    				printf("SEL1 Response =");
    				for (uint8_t i = 0; i < len; i++) printf(" %02X", data[i]);
    				printf("\n");
    			}
    			sprintf(uid, "%s%X%X%X%X", uid, saved_data[4], saved_data[5], saved_data[6], saved_data[7]);
    			disp_len=1;
    		}
    		HAL_UART_Transmit(&huart2, (uint8_t*)(uid), strlen(uid), HAL_MAX_DELAY);
    		printf("\n");
    	} else {
    		printf("UID CL1 error\n");
    		disp_len=3;
    	}
	} else {
		printf("REQA error\n");
		disp_len=3;
	}
	SSD1306_Clear();
	SSD1306_GotoXY (50, 10);
	SSD1306_Puts ("UID:", &Font_11x18, 1);
	SSD1306_GotoXY (5, 30);
	SSD1306_Puts (uid, &Font_7x10, 1);
	if (disp_len == 1) {
		SSD1306_GotoXY (5, 45);
		SSD1306_Puts ("UID length: 4B", &Font_7x10, 1);
			}
	else if (disp_len == 2) {
		SSD1306_GotoXY (5, 45);
		SSD1306_Puts ("UID length: 7B", &Font_7x10, 1);
	}
	else if (disp_len == 3) {
		SSD1306_Clear();
		SSD1306_GotoXY (25, 19);
		SSD1306_Puts ("ERROR", &Font_16x26, 1);
	}
	SSD1306_UpdateScreen();
}

static void cr95_read15(void)
{
	const uint8_t cmd_req15[] =  { 0x04, 0x03, 0x26, 0x01, 0x00 };

	uint8_t data[16];
	uint8_t len;
	uint8_t saved_data[8];

	cr95write(cmd_req15, sizeof(cmd_req15));
	if (cr95read(data, &len) == 0x80) {
		saved_data[0]=data[9]; saved_data[1]=data[8]; saved_data[2]=data[7];; saved_data[3]=data[6];
		saved_data[4]=data[5]; saved_data[5]=data[4]; saved_data[6]=data[3]; saved_data[7]=data[2];
		printf("UID = ");
		for (uint8_t i = 0; i < sizeof(saved_data); i++) printf(" %02X", saved_data[i]);
		printf("\n");

	} else {
		printf("Reading error\n");
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

static void uart_process_command(char *cmd)
{
    char *token;
    token = strtok(cmd, " ");
	uint8_t data[16];
	uint8_t len;

	const uint8_t cmd_echo[] =  { 0x55 };
	const uint8_t cmd_idn[] =   { 0x01, 0x00 };

    if (strcasecmp(token, "HELLO") == 0) {
        printf("Communication is working\n");
    }
    else if (strcasecmp(token, "ON") == 0) {
    	nfc_init();
    	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
        HAL_UART_Receive_DMA(&huart1, nfc_rx_buf, RX_BUFFER_LEN);
    	HAL_Delay(5);
    	printf("RFID ON\n");
        nfc_rx_read_ptr = nfc_rx_write_ptr;
    	cr95_wakeup();
    	nfc_ready = true;
    	SSD1306_GotoXY (45,10);
    	SSD1306_Puts ("RFID", &Font_11x18, 1);
    	SSD1306_GotoXY (30, 30);
    	SSD1306_Puts ("SCANNER", &Font_11x18, 1);
    	SSD1306_UpdateScreen(); // update screen
    }
    else if (strcasecmp(token, "OFF") == 0) {
    	nfc_ready = false;
        HAL_UART_AbortReceive(&huart1);
    	HAL_UART_DeInit(&huart1);
    	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
    	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
    	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);
    	SSD1306_Clear();
    	printf("RFID OFF\n");
    }
    else if (strcasecmp(token, "ECHO") == 0) {
    	cr95write(cmd_echo, sizeof(cmd_echo));
    	uint8_t resp = cr95read(NULL, NULL);
    	printf("ECHO %s %02X\n", (resp == 0x55) ? "yes" : "no", resp);
    	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
    	SSD1306_Clear();
    	SSD1306_GotoXY (40,10);
    	SSD1306_Puts ("ECHO", &Font_11x18, 1);
    	SSD1306_GotoXY (25, 30);
    	SSD1306_Puts ("COMMAND", &Font_11x18, 1);
    	SSD1306_UpdateScreen();
    	HAL_Delay(1000);
    	SSD1306_Clear();
    	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
    }
    else if (strcasecmp(token, "IDN") == 0) {
    	char idn[28];
    	cr95write(cmd_idn, sizeof(cmd_idn));
    	if (cr95read(data, &len) == 0x00) {
    		printf("IDN =");
    		for (uint8_t i = 0; i < len; i++)
    			{
    			idn[i]=data[i];
    			printf(" %02X", data[i]);
    			}
    		SSD1306_Clear();
    		SSD1306_GotoXY (50, 10);
    		SSD1306_Puts ("IDN:", &Font_11x18, 1);
    		SSD1306_GotoXY (20, 30);
    		SSD1306_Puts (idn, &Font_7x10, 1);
    		SSD1306_UpdateScreen();
    		printf("\n");
    	} else {
    		printf("IDN error\n");
    	}
    }
    else if (strcasecmp(token, "INIT14") == 0) {
    	cr95_init14();
    }
    else if (strcasecmp(token, "INIT15") == 0) {
        cr95_init15();
    }
    else if (strcasecmp(token, "READ") == 0) {
    	cr95_read();
    	if (disp_len == 1) {
    		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
    		HAL_Delay(2000);
    		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);
    		}
    	else {
    		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
    		HAL_Delay(2000);
    		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
    	}
    }
    else if (strcasecmp(token, "READ15") == 0) {
        cr95_read15();
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
    	SSD1306_Clear();
    	SSD1306_GotoXY (1,10);
    	SSD1306_Puts ("Calibration", &Font_11x18, 1);
    	SSD1306_GotoXY (15,30);
    	SSD1306_Puts ("sequence", &Font_11x18, 1);
    	SSD1306_UpdateScreen();
    	cr95_calibrate();
    	SSD1306_Clear();
    	SSD1306_GotoXY (45,10);
    	SSD1306_Puts ("RFID", &Font_11x18, 1);
    	SSD1306_GotoXY (30, 30);
    	SSD1306_Puts ("SCANNER", &Font_11x18, 1);
    	SSD1306_UpdateScreen();
    	do {
        	cr95_idle(0);

			do {} while (nfc_rx_read_ptr == nfc_rx_write_ptr);
			uint8_t resp = cr95read(data, &len);

			if (resp == 0x00 && data[0] == 0x02) printf("WAKEUP by tag detect\n");
			else printf("Error\n");
			printf("Code of wakeup is:%02X\n", data[0]);

			cr95_init14();
        	cr95_read();

        	if (disp_len == 1) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
        	if (disp_len == 2) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);

        	HAL_Delay(3000);

        	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);
        	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);

        	SSD1306_Clear();
        	SSD1306_GotoXY (45,10);
        	SSD1306_Puts ("RFID", &Font_11x18, 1);
        	SSD1306_GotoXY (30, 30);
        	SSD1306_Puts ("SCANNER", &Font_11x18, 1);
        	SSD1306_UpdateScreen();
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

void manual_operation(void)
{
	while (uart_rx_read_ptr != uart_rx_write_ptr) {
		      uint8_t b = uart_rx_buf[uart_rx_read_ptr];
		      if (++uart_rx_read_ptr >= RX_BUFFER_LEN) uart_rx_read_ptr = 0;
		      uart_byte_available(b);
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
}

void automatic_operation(void)
{
	printf_en = true;
	uart_process_command("on");
	HAL_Delay(5000);
	uart_process_command("echo");
	uart_process_command("auto");
}

