#ifndef USB_DEVICE_H
#define USB_DEVICE_H

#include "usbd_core.h"

#include <stdbool.h>

extern USBD_HandleTypeDef hUsbDeviceFS;

void MX_USB_DEVICE_Init(void);
void USB_DEVICE_DeInit(void);
bool USB_DEVICE_IsConfigured(void);

#endif /* USB_DEVICE_H */
