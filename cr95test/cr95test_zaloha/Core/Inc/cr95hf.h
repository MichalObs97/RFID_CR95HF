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

//int _write(int file, char const *buf, int n);

//void cr95write(const uint8_t *data, uint8_t length);

//uint8_t cr95read(uint8_t *data, uint8_t *length);

void cr95_wakeup(void);

void cr95_idle(uint8_t mode);

void cr95_init14(void);

void cr95_init15(void);

void cr95_read(void);

void cr95_readtopaz(void);

void cr95_calibrate(void);

//void uart_process_command(char *cmd);

//void uart_byte_available(uint8_t c);

#endif /* INC_CR95HF_H_ */
