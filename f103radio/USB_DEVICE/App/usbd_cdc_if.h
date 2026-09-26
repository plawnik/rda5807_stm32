#ifndef USBD_CDC_IF_H
#define USBD_CDC_IF_H

#include "usbd_cdc.h"

#include <stdbool.h>
#include <stddef.h>

extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;

bool usb_console_write_n(const char *data, size_t length);

#endif /* USBD_CDC_IF_H */
