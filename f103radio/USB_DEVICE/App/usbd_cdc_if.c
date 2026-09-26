#include "usbd_cdc_if.h"

#include "terminal_ui.h"
#include "usb_device.h"

#include <string.h>

#define USB_CONSOLE_PACKET_SIZE CDC_DATA_FS_MAX_PACKET_SIZE
#define USB_CONSOLE_TX_TIMEOUT_MS 50U

static uint8_t rx_buffer[USB_CONSOLE_PACKET_SIZE];
static uint8_t tx_buffer[USB_CONSOLE_PACKET_SIZE];
static USBD_CDC_LineCodingTypeDef line_coding = {
    115200U, 0U, 0U, 8U};

static int8_t cdc_init(void) {
  (void)USBD_CDC_SetTxBuffer(&hUsbDeviceFS, tx_buffer, 0U);
  (void)USBD_CDC_SetRxBuffer(&hUsbDeviceFS, rx_buffer);
  return (int8_t)USBD_OK;
}

static int8_t cdc_deinit(void) {
  return (int8_t)USBD_OK;
}

static int8_t cdc_control(uint8_t command, uint8_t *buffer,
                          uint16_t length) {
  (void)length;
  if (buffer == NULL) return (int8_t)USBD_FAIL;
  switch (command) {
    case CDC_SET_LINE_CODING:
      line_coding.bitrate =
          (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8U) |
          ((uint32_t)buffer[2] << 16U) | ((uint32_t)buffer[3] << 24U);
      line_coding.format = buffer[4];
      line_coding.paritytype = buffer[5];
      line_coding.datatype = buffer[6];
      break;
    case CDC_GET_LINE_CODING:
      buffer[0] = (uint8_t)line_coding.bitrate;
      buffer[1] = (uint8_t)(line_coding.bitrate >> 8U);
      buffer[2] = (uint8_t)(line_coding.bitrate >> 16U);
      buffer[3] = (uint8_t)(line_coding.bitrate >> 24U);
      buffer[4] = line_coding.format;
      buffer[5] = line_coding.paritytype;
      buffer[6] = line_coding.datatype;
      break;
    default:
      break;
  }
  return (int8_t)USBD_OK;
}

static int8_t cdc_receive(uint8_t *buffer, uint32_t *length) {
  if (buffer != NULL && length != NULL && *length > 0U) {
    terminal_ui_receive_bytes(buffer, (size_t)*length);
  }
  (void)USBD_CDC_SetRxBuffer(&hUsbDeviceFS, rx_buffer);
  (void)USBD_CDC_ReceivePacket(&hUsbDeviceFS);
  return (int8_t)USBD_OK;
}

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS = {
    cdc_init, cdc_deinit, cdc_control, cdc_receive};

static bool wait_for_tx_idle(void) {
  const uint32_t started_ms = HAL_GetTick();
  USBD_CDC_HandleTypeDef *cdc;
  do {
    cdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
    if (cdc == NULL || cdc->TxState == 0U) return cdc != NULL;
  } while ((uint32_t)(HAL_GetTick() - started_ms) <
           USB_CONSOLE_TX_TIMEOUT_MS);
  return false;
}

bool usb_console_write_n(const char *data, size_t length) {
  if (data == NULL || length == 0U || !USB_DEVICE_IsConfigured()) {
    return false;
  }
  while (length > 0U) {
    const uint16_t chunk =
        length > USB_CONSOLE_PACKET_SIZE ? USB_CONSOLE_PACKET_SIZE
                                         : (uint16_t)length;
    if (!wait_for_tx_idle()) return false;
    memcpy(tx_buffer, data, chunk);
    (void)USBD_CDC_SetTxBuffer(&hUsbDeviceFS, tx_buffer, chunk);
    if (USBD_CDC_TransmitPacket(&hUsbDeviceFS) != USBD_OK) return false;
    if (!wait_for_tx_idle()) return false;
    data += chunk;
    length -= chunk;
  }
  return true;
}
