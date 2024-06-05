/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "usb_descriptors.h"

#include "config.h"
#include "pico/unique_id.h"
#include "tusb.h"

/* A combination of interfaces must have a unique product id, since PC will save device driver after
 * the first plug. Same VID/PID with different interface e.g MSC (first), then CDC (later) will
 * possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]         HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n) ((CFG_TUD_##itf) << (n))
#define USB_PID                                                                          \
  (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | _PID_MAP(MIDI, 3) | \
   _PID_MAP(VENDOR, 4))

#define USB_VID 0x5515
#define USB_BCD 0x0110

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device = {.bLength = sizeof(tusb_desc_device_t),
                                        .bDescriptorType = TUSB_DESC_DEVICE,
                                        .bcdUSB = USB_BCD,
                                        .bDeviceClass = 0x00,
                                        .bDeviceSubClass = 0x00,
                                        .bDeviceProtocol = 0x00,
                                        .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

                                        .idVendor = USB_VID,
                                        .idProduct = USB_PID,
                                        .bcdDevice = 0x0100,

                                        .iManufacturer = 0x01,
                                        .iProduct = 0x02,
                                        .iSerialNumber = 0x03,

                                        .bNumConfigurations = 0x01};

/**
 * @brief Invoked when received GET DEVICE DESCRIPTOR.
 * This function returns a pointer to the device descriptor.
 *
 * @return Pointer to the device descriptor.
 */
uint8_t const* tud_descriptor_device_cb(void) { return (uint8_t const*)&desc_device; }

//--------------------------------------------------------------------+
// HID Report Descriptor
//--------------------------------------------------------------------+

uint8_t const desc_hid_report_keyboard[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD))};

uint8_t const desc_hid_report_consumer[] = {
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(REPORT_ID_CONSUMER_CONTROL))};

uint8_t const desc_hid_report_mouse[] = {TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(REPORT_ID_MOUSE))};

/**
 * @brief Invoked when received GET HID REPORT DESCRIPTOR.
 * This function returns a pointer to the HID report descriptor based on the interface number.
 * The descriptor contents must exist long enough for the transfer to complete.
 *
 * @param interface The interface number.
 *
 * @return Pointer to the HID report descriptor.
 */
uint8_t const* tud_hid_descriptor_report_cb(uint8_t interface) {
  switch (interface) {
    case ITF_NUM_KEYBOARD:
      return KEYBOARD_ENABLED ? desc_hid_report_keyboard : desc_hid_report_mouse;
    case ITF_NUM_CONSUMER_CONTROL:
      return KEYBOARD_ENABLED ? desc_hid_report_consumer : NULL;
    case ITF_NUM_MOUSE:
      return MOUSE_ENABLED ? desc_hid_report_mouse : NULL;
    default:
      return NULL;
  }
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

const uint ITF_NUM_TOTAL = (KEYBOARD_ENABLED == 1 ? 2 : 0) + (MOUSE_ENABLED == 1 ? 1 : 0);

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + ITF_NUM_TOTAL * TUD_HID_DESC_LEN)

#define EPNUM_KEYBOARD 0x81
#define EPNUM_CONSUMER_CONTROL 0x82
#define EPNUM_MOUSE 0x83

uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, bmAttributes, power in mA
    // setting bmAttributes to 0x80 means bus-powered, and will prevent the host from trying to
    // suspend the device.
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 250),

#if KEYBOARD_ENABLED
    TUD_HID_DESCRIPTOR(ITF_NUM_KEYBOARD, 0, HID_ITF_PROTOCOL_KEYBOARD,
                       sizeof(desc_hid_report_keyboard), EPNUM_KEYBOARD, KEYBOARD_EP_BUFSIZE, 8),

    TUD_HID_DESCRIPTOR(ITF_NUM_CONSUMER_CONTROL, 0, HID_ITF_PROTOCOL_NONE,
                       sizeof(desc_hid_report_consumer), EPNUM_CONSUMER_CONTROL,
                       CONSUMER_EP_BUFSIZE, 8),
#endif

#if MOUSE_ENABLED
    TUD_HID_DESCRIPTOR(KEYBOARD_ENABLED ? ITF_NUM_MOUSE : ITF_NUM_KEYBOARD, 0,
                       HID_ITF_PROTOCOL_MOUSE, sizeof(desc_hid_report_mouse),
                       KEYBOARD_ENABLED ? EPNUM_MOUSE : EPNUM_KEYBOARD, CFG_TUD_HID_EP_BUFSIZE, 8),
#endif
};

/**
 * @brief Invoked when received GET CONFIGURATION DESCRIPTOR.
 * This function returns a pointer to the configuration descriptor based on the configuration index.
 * The descriptor contents must exist long enough for the transfer to complete.
 *
 * @param index The configuration index.
 *
 * @return Pointer to the configuration descriptor.
 */
uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;  // for multiple configurations

  // This example uses the same configuration for both high and full speed mode
  return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
char const* string_desc_arr[] = {
    (const char[]){0x09, 0x04},  // 0: is supported language is English (0x0409)
    "paulbramhall.uk",           // 1: Manufacturer
    "RP2040 Device Converter",   // 2: Product
    "",                          // 3: Serial, We will set this later to the unique Flash ID
};

static uint16_t _desc_str[32];

/**
 * @brief Invoked when received GET STRING DESCRIPTOR request.
 * This function returns a pointer to the string descriptor based on the index and language ID.
 * The descriptor contents must exist long enough for the transfer to complete.
 *
 * @param index  The string index.
 * @param langid The language ID.
 *
 * @return Pointer to the string descriptor.
 */
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void)langid;

  uint8_t chr_count;

  // Ensure we set the USB device serial number to the Pico's unique ID
  // This is read from the FLASH Chip, and is unique to each one.
  // The RP2040 itself does not have a unique ID.
  // https://cec-code-lab.aps.edu/robotics/resources/pico-c-api/group__pico__unique__id.html
  char pico_unique_id[32];
  pico_get_unique_board_id_string(pico_unique_id, sizeof(pico_unique_id));
  string_desc_arr[3] = pico_unique_id;

  if (index == 0) {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  } else {
    // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

    if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) return NULL;

    const char* str = string_desc_arr[index];

    // Cap at max char
    chr_count = (uint8_t)strlen(str);
    if (chr_count > 31) chr_count = 31;

    // Convert ASCII string into UTF-16
    for (uint8_t i = 0; i < chr_count; i++) {
      _desc_str[1 + i] = str[i];
    }
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

  return _desc_str;
}
