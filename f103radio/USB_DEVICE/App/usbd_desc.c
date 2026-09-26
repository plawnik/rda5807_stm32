#include "usbd_desc.h"

#include "usbd_ctlreq.h"

#define USBD_VID                      0x0483U
#define USBD_PID                      0x5740U
#define USBD_LANGID_STRING            0x0409U
#define USBD_MANUFACTURER_STRING      "RDA5807 STM32 project"
#define USBD_PRODUCT_FS_STRING        "RDA5807 STM32 Radio"
#define USBD_CONFIGURATION_FS_STRING  "CDC configuration"
#define USBD_INTERFACE_FS_STRING      "Radio VT100 terminal"
#define USB_SIZ_STRING_SERIAL         26U

#define DEVICE_ID1 ((uintptr_t)0x1FFFF7E8UL)
#define DEVICE_ID2 ((uintptr_t)0x1FFFF7ECUL)
#define DEVICE_ID3 ((uintptr_t)0x1FFFF7F0UL)

static uint8_t *device_descriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *lang_descriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *manufacturer_descriptor(USBD_SpeedTypeDef speed,
                                        uint16_t *length);
static uint8_t *product_descriptor(USBD_SpeedTypeDef speed,
                                   uint16_t *length);
static uint8_t *serial_descriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *configuration_descriptor(USBD_SpeedTypeDef speed,
                                         uint16_t *length);
static uint8_t *interface_descriptor(USBD_SpeedTypeDef speed,
                                     uint16_t *length);

USBD_DescriptorsTypeDef FS_Desc = {
    device_descriptor,
    lang_descriptor,
    manufacturer_descriptor,
    product_descriptor,
    serial_descriptor,
    configuration_descriptor,
    interface_descriptor};

__ALIGN_BEGIN static uint8_t device_desc[USB_LEN_DEV_DESC] __ALIGN_END = {
    0x12U, USB_DESC_TYPE_DEVICE,
    0x00U, 0x02U,             /* USB 2.00 */
    0x02U, 0x02U, 0x00U,     /* CDC class, ACM subclass */
    USB_MAX_EP0_SIZE,
    LOBYTE(USBD_VID), HIBYTE(USBD_VID),
    LOBYTE(USBD_PID), HIBYTE(USBD_PID),
    0x00U, 0x02U,             /* Device release 2.00 */
    USBD_IDX_MFC_STR,
    USBD_IDX_PRODUCT_STR,
    USBD_IDX_SERIAL_STR,
    USBD_MAX_NUM_CONFIGURATION};

__ALIGN_BEGIN static uint8_t lang_desc[USB_LEN_LANGID_STR_DESC]
    __ALIGN_END = {
        USB_LEN_LANGID_STR_DESC, USB_DESC_TYPE_STRING,
        LOBYTE(USBD_LANGID_STRING), HIBYTE(USBD_LANGID_STRING)};

__ALIGN_BEGIN static uint8_t string_desc[USBD_MAX_STR_DESC_SIZ] __ALIGN_END;
__ALIGN_BEGIN static uint8_t serial_desc[USB_SIZ_STRING_SERIAL] __ALIGN_END = {
    USB_SIZ_STRING_SERIAL, USB_DESC_TYPE_STRING};

static void value_to_unicode(uint32_t value, uint8_t *output, uint8_t digits) {
  for (uint8_t index = 0U; index < digits; ++index) {
    uint8_t nibble = (uint8_t)(value >> 28U);
    output[index * 2U] =
        nibble < 10U ? (uint8_t)('0' + nibble)
                     : (uint8_t)('A' + nibble - 10U);
    output[index * 2U + 1U] = 0U;
    value <<= 4U;
  }
}

static void update_serial(void) {
  uint32_t first = *(const uint32_t *)DEVICE_ID1;
  const uint32_t second = *(const uint32_t *)DEVICE_ID2;
  const uint32_t third = *(const uint32_t *)DEVICE_ID3;
  first += third;
  if (first != 0U) {
    value_to_unicode(first, &serial_desc[2], 8U);
    value_to_unicode(second, &serial_desc[18], 4U);
  }
}

static uint8_t *device_descriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
  (void)speed;
  *length = sizeof(device_desc);
  return device_desc;
}

static uint8_t *lang_descriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
  (void)speed;
  *length = sizeof(lang_desc);
  return lang_desc;
}

static uint8_t *manufacturer_descriptor(USBD_SpeedTypeDef speed,
                                        uint16_t *length) {
  (void)speed;
  USBD_GetString((uint8_t *)USBD_MANUFACTURER_STRING, string_desc, length);
  return string_desc;
}

static uint8_t *product_descriptor(USBD_SpeedTypeDef speed,
                                   uint16_t *length) {
  (void)speed;
  USBD_GetString((uint8_t *)USBD_PRODUCT_FS_STRING, string_desc, length);
  return string_desc;
}

static uint8_t *serial_descriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
  (void)speed;
  update_serial();
  *length = sizeof(serial_desc);
  return serial_desc;
}

static uint8_t *configuration_descriptor(USBD_SpeedTypeDef speed,
                                         uint16_t *length) {
  (void)speed;
  USBD_GetString((uint8_t *)USBD_CONFIGURATION_FS_STRING, string_desc, length);
  return string_desc;
}

static uint8_t *interface_descriptor(USBD_SpeedTypeDef speed,
                                     uint16_t *length) {
  (void)speed;
  USBD_GetString((uint8_t *)USBD_INTERFACE_FS_STRING, string_desc, length);
  return string_desc;
}
