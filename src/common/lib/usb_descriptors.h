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
 */

/**
 * @file usb_descriptors.h
 * @brief USB interface and HID report ID enumerations.
 *
 * Defines the USB interface numbers and HID report IDs used by the TinyUSB
 * descriptor tables and the HID interface layer.
 */

#ifndef USB_DESCRIPTORS_H_
#define USB_DESCRIPTORS_H_

/** @brief USB interface numbers assigned to HID endpoints. */
typedef enum {
    ITF_NUM_KEYBOARD,         /**< Keyboard HID interface */
    ITF_NUM_CONSUMER_CONTROL, /**< Consumer control HID interface */
    ITF_NUM_MOUSE,            /**< Mouse HID interface */
} itf_num_t;

/** @brief HID report IDs used in multi-report HID descriptors. */
typedef enum {
    REPORT_ID_KEYBOARD = 1,     /**< Keyboard report */
    REPORT_ID_CONSUMER_CONTROL, /**< Consumer control report */
    REPORT_ID_MOUSE,            /**< Mouse report */
} report_id_t;

#endif /* USB_DESCRIPTORS_H_ */
