#ifndef AUDIO_CONFIG_H
#define AUDIO_CONFIG_H

#include "stm32h5xx_hal.h"

/* Audio Format Settings */
#define AUDIO_FREQUENCY             48000
#define AUDIO_BIT_DEPTH             24
#define AUDIO_CHANNELS              2

/* Sizes in Bytes */
/* USB Transfer (Packed 24-bit): 3 bytes per sample */
#define USB_FRAME_SIZE              (AUDIO_CHANNELS * 3)

/* SAI Storage (32-bit Slots): 4 bytes per sample */
#define SAI_FRAME_SIZE              (AUDIO_CHANNELS * 4)

/* USB Settings */
/* Full Speed (1ms frame) */
/* Samples per frame = 48 */
/* Packet Size = 48 * 6 = 288 bytes */
#define USB_AUDIO_MAX_PACKET_SIZE   ((AUDIO_FREQUENCY / 1000) * USB_FRAME_SIZE)
/* Add some margin for Async adjustments (e.g. + 1 sample pair) */
#define USB_AUDIO_EP_SIZE           (USB_AUDIO_MAX_PACKET_SIZE + USB_FRAME_SIZE)

/* Ring Buffer Settings */
/* We need a buffer large enough to handle jitter.
   Say 10ms worth of data for safety. */
#define AUDIO_BUFFER_FRAMES         480
/* Buffer Size in Bytes (for SAI 32-bit storage) */
#define AUDIO_BUFFER_SIZE           (AUDIO_BUFFER_FRAMES * SAI_FRAME_SIZE)

/* Pin Definitions for STM32H562RGT6 (LQFP64) */
/* SAI1 Block A (RX) and Block B (TX) on GPIOB */
/* PB12: SAI1_FS_A */
/* PB10: SAI1_SCK_A */
/* PB15: SAI1_SD_A */
/* PB5:  SAI1_SD_B */

#define SAI_PORT                    GPIOB
#define SAI_PIN_FS                  GPIO_PIN_12
#define SAI_PIN_SCK                 GPIO_PIN_10
#define SAI_PIN_SD_A                GPIO_PIN_15
#define SAI_PIN_SD_B                GPIO_PIN_5

#endif /* AUDIO_CONFIG_H */
