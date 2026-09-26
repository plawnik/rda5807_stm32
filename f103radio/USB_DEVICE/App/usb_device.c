#include "usb_device.h"

#include "usbd_cdc.h"
#include "usbd_cdc_if.h"
#include "usbd_desc.h"

USBD_HandleTypeDef hUsbDeviceFS;

static bool usb_initialized;

void MX_USB_DEVICE_Init(void) {
  if (usb_initialized) return;
  if (USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS) != USBD_OK) return;
  if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC) != USBD_OK) {
    (void)USBD_DeInit(&hUsbDeviceFS);
    return;
  }
  if (USBD_CDC_RegisterInterface(&hUsbDeviceFS,
                                 &USBD_Interface_fops_FS) != USBD_OK) {
    (void)USBD_DeInit(&hUsbDeviceFS);
    return;
  }
  if (USBD_Start(&hUsbDeviceFS) != USBD_OK) {
    (void)USBD_DeInit(&hUsbDeviceFS);
    return;
  }
  usb_initialized = true;
}

void USB_DEVICE_DeInit(void) {
  if (!usb_initialized) return;
  (void)USBD_Stop(&hUsbDeviceFS);
  (void)USBD_DeInit(&hUsbDeviceFS);
  usb_initialized = false;
}

bool USB_DEVICE_IsConfigured(void) {
  return usb_initialized &&
         hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED;
}
