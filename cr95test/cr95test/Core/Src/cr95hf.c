/*
 * cr95hf.c
 *
 *  Created on: Apr 13, 2021
 *      Author: MichalObs97
 */

#include "cr95hf.h"
#include "ssd1306.h"
#include "fonts.h"
#include "stm32f0xx_hal.h"
#include "stm32f0xx_hal_uart.h"
#include <stdio.h>

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

