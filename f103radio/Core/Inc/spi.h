#ifndef SPI_H
#define SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

extern SPI_HandleTypeDef hspi1;
extern DMA_HandleTypeDef hdma_spi1_tx;

void MX_SPI1_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* SPI_H */
