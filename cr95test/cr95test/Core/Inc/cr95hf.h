/*
 * cr95hf.h
 *
 *  Created on: Apr 13, 2021
 *      Author: MichalObs97
 */

#ifndef INC_CR95HF_H_
#define INC_CR95HF_H_

#include "ssd1306.h"
#include "fonts.h"
#include "stm32f0xx_hal.h"
#include "stdlib.h"
#include "string.h"
#include "main.h"

void uart_init(void);

int _write(int file, char const *buf, int n);

//static void cr95write(const uint8_t *data, uint8_t length);

//static uint8_t cr95read(uint8_t *data, uint8_t *length);

//static void cr95_wakeup(void);

//static void cr95_idle(uint8_t mode);

//static void cr95_init14(void);

//static void cr95_init15(void);

//static void cr95_read(void);

//static void cr95_readtopaz(void);

//static void cr95_calibrate(void);

//static void uart_process_command(char *cmd);

//static void uart_byte_available(uint8_t c);

void manual_operation(void);

/**
 * @brief  Manual operation of the reader
 */

void automatic_operation(void);

/**
 * @brief  Automatic operation of the reader
 */

#endif /* INC_CR95HF_H_ */
