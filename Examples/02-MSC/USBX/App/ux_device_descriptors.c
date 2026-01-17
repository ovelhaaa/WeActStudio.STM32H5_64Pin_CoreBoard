/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ux_device_descriptors.c
  * @author  Jules
  * @brief   USBX Device descriptor file for Audio Class 2.0
  ******************************************************************************
  */
/* USER CODE END Header */

#include "ux_device_descriptors.h"
#include "ux_device_audio.h"

/* Audio Class Descriptors (UAC 2.0) */

#define USBD_VID     1155
#define USBD_PID     22288

/* Descriptor Definitions */
/* Standard Device Descriptor */
__ALIGN_BEGIN const uint8_t USBD_DeviceDesc_Audio[] __ALIGN_END = {
  0x12,                       /* bLength */
  USB_DESC_TYPE_DEVICE,       /* bDescriptorType */
  0x00, 0x02,                 /* bcdUSB (2.00) */
  0xEF,                       /* bDeviceClass (Misc) */
  0x02,                       /* bDeviceSubClass */
  0x01,                       /* bDeviceProtocol */
  64,                         /* bMaxPacketSize */
  LOBYTE(USBD_VID), HIBYTE(USBD_VID), /* idVendor */
  LOBYTE(USBD_PID), HIBYTE(USBD_PID), /* idProduct */
  0x00, 0x01,                 /* bcdDevice */
  0x01,                       /* iManufacturer */
  0x02,                       /* iProduct */
  0x03,                       /* iSerialNumber */
  0x01                        /* bNumConfigurations */
};

/* Configuration Descriptor (Composite: IAD + AC + AS_OUT + AS_IN) */
/* Length Calculation:
   Config: 9
   IAD: 8
   AC Interface (If 0):
     Std: 9
     Class Header: 9
     Clock Source: 8
     Input Terminal (USB Out): 17
     Output Terminal (Speaker): 12
     Input Terminal (Mic): 17
     Output Terminal (USB In): 12
     Feature Unit (Spk): 18
     Feature Unit (Mic): 18
   AS Interface Out (If 1):
     Alt 0 (Zero BW): 9
     Alt 1 (Operational): 9
     AS General: 16
     Format Type: 6
     Endpoint ISO OUT: 7
     Endpoint Feedback: 7
     CS EP ISO OUT: 8
   AS Interface In (If 2):
     Alt 0: 9
     Alt 1: 9
     AS General: 16
     Format Type: 6
     Endpoint ISO IN: 7
     CS EP ISO IN: 8

   Total roughly: ~254 bytes.
*/

/* Corrected Size: 14 -> 18 for Feature Units, Input Terminals 12 -> 17 */
/*
AC: 9(Conf)+8(IAD)+9(Std)+111(CS) = 137
AS Out (Alt 0/1): 9+9 + 16(Gen)+6(Fmt)+7(EpData)+8(EpCS)+7(EpFb) = 62
AS In (Alt 0/1):  9+9 + 16(Gen)+6(Fmt)+7(EpData)+8(EpCS) = 55
Total: 137 + 62 + 55 = 254
*/
#define USB_AUDIO_CONFIG_DESC_SIZE  (254)

__ALIGN_BEGIN const uint8_t USBD_ConfigDesc_Audio[] __ALIGN_END = {
  /* Configuration Descriptor */
  0x09,                                     /* bLength */
  USB_DESC_TYPE_CONFIGURATION,              /* bDescriptorType */
  LOBYTE(USB_AUDIO_CONFIG_DESC_SIZE),       /* wTotalLength */
  HIBYTE(USB_AUDIO_CONFIG_DESC_SIZE),
  0x03,                                     /* bNumInterfaces (AC, AS_OUT, AS_IN) */
  0x01,                                     /* bConfigurationValue */
  0x00,                                     /* iConfiguration */
  0xC0,                                     /* bmAttributes (Self-powered) */
  0x32,                                     /* bMaxPower (100mA) */

  /* Interface Association Descriptor (IAD) */
  0x08,                                     /* bLength */
  0x0B,                                     /* bDescriptorType (IAD) */
  0x00,                                     /* bFirstInterface */
  0x03,                                     /* bInterfaceCount */
  0x01,                                     /* bFunctionClass (Audio) */
  0x00,                                     /* bFunctionSubClass (Function) */
  0x20,                                     /* bFunctionProtocol (IP 2.0) */
  0x00,                                     /* iFunction */

  /* Audio Control Interface (Interface 0) */
  0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x20, 0x00, /* Std Interface */

  /* Class-Specific AC Interface Header */
  0x09, 0x24, 0x01, /* Header, ADC 2.0 */
  0x00, 0x02,       /* bcdADC 2.0 */
  0x01,             /* bCategory (Desktop Speaker) */
  0x6F, 0x00,       /* wTotalLength (Calculated: 9+8+17+12+17+12+18+18 = 111 (0x6F)) */
  0x00,             /* bmControls */

  /* Clock Source (ID 1) */
  0x08, 0x24, 0x02, 0x01, 0x01, 0x03, 0x00, 0x00,
  /* ID 1, Internal Fixed Clock, Ctrl: Freq Read Only */

  /* Input Terminal (USB Streaming OUT) (ID 2) */
  0x11, 0x24, 0x02, 0x02, 0x01, 0x01, 0x00, 0x01, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* ID 2, Type USB Stream, Assoc 0, Clk 1, 2 Channels, Config 3 (L/R) */

  /* Feature Unit (Play Volume/Mute) (ID 3) */
  /* Length 18 (0x12): Header 5 + Master(4) + Ch1(4) + Ch2(4) + iFeature(1) */
  0x12, 0x24, 0x06, 0x03, 0x02,
  0x0F, 0x00, 0x00, 0x00, /* Master Controls (Mute, Vol, etc) */
  0x00, 0x00, 0x00, 0x00, /* Ch 1 (None - use Master) */
  0x00, 0x00, 0x00, 0x00, /* Ch 2 (None - use Master) */
  0x00,                   /* iFeature */

  /* Output Terminal (Speaker) (ID 4) */
  0x0C, 0x24, 0x03, 0x04, 0x01, 0x03, 0x00, 0x03, 0x01, 0x00, 0x00, 0x00,
  /* ID 4, Type Speaker, Assoc 0, Source 3, Clk 1 */

  /* Input Terminal (Microphone) (ID 5) */
  0x11, 0x24, 0x02, 0x05, 0x01, 0x02, 0x00, 0x01, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* ID 5, Type Mic, Assoc 0, Clk 1, 2 Ch, Config 3 */

  /* Feature Unit (Rec Volume/Mute) (ID 6) */
  /* Length 18 */
  0x12, 0x24, 0x06, 0x06, 0x05,
  0x0F, 0x00, 0x00, 0x00, /* Master */
  0x00, 0x00, 0x00, 0x00, /* Ch 1 */
  0x00, 0x00, 0x00, 0x00, /* Ch 2 */
  0x00,                   /* iFeature */

  /* Output Terminal (USB Streaming IN) (ID 7) */
  0x0C, 0x24, 0x03, 0x07, 0x01, 0x01, 0x00, 0x06, 0x01, 0x00, 0x00, 0x00,
  /* ID 7, Type USB Stream, Assoc 0, Source 6, Clk 1 */

  /* ----------------------------------------------------------------------- */
  /* Audio Streaming Interface OUT (Interface 1) - Playback */
  /* Alt 0 (Zero Bandwidth) */
  0x09, 0x04, 0x01, 0x00, 0x00, 0x01, 0x02, 0x20, 0x00,

  /* Alt 1 (Operational) */
  0x09, 0x04, 0x01, 0x01, 0x02, 0x01, 0x02, 0x20, 0x00, /* 2 Endpoints (Data + Feedback) */

  /* Class-Specific AS General */
  0x10, 0x24, 0x01, 0x02, 0x00, 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* TermLink 2, Ctl 0, Fmt Type I, PCM, 2 Channels, Config 3 */

  /* Format Type I */
  0x06, 0x24, 0x02, 0x01, 0x03, 0x18, /* Type I, PCM, 3 bytes/slot, 24 bits */

  /* Endpoint 1: ISO OUT (Async) */
  0x07, 0x05, 0x01, 0x05, LOBYTE(USB_AUDIO_EP_SIZE), HIBYTE(USB_AUDIO_EP_SIZE), 0x01,
  /* Addr 0x01, Async ISO, Size, Interval 1 (1ms FS or 125us HS) */
  /* Note: For FS, Interval=1 means 1ms. For HS, Interval=1 means 125us. */

  /* Class-Specific AS Isochronous Audio Data Endpoint (CS_ENDPOINT) */
  0x08, 0x25, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,

  /* Endpoint 2: Feedback IN */
  0x07, 0x05, 0x82, 0x11, 0x04, 0x00, 0x01, /* 4 bytes for 16.16 feedback */
  /* Addr 0x82, Sync Feedback, Size 4, Interval 1 */

  /* ----------------------------------------------------------------------- */
  /* Audio Streaming Interface IN (Interface 2) - Record */
  /* Alt 0 */
  0x09, 0x04, 0x02, 0x00, 0x00, 0x01, 0x02, 0x20, 0x00,

  /* Alt 1 */
  0x09, 0x04, 0x02, 0x01, 0x01, 0x01, 0x02, 0x20, 0x00, /* 1 Endpoint (Data) - Asynchronous */

  /* AS General */
  0x10, 0x24, 0x01, 0x07, 0x00, 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* TermLink 7 */

  /* Format Type I */
  0x06, 0x24, 0x02, 0x01, 0x03, 0x18,

  /* Endpoint 3: ISO IN (Async) */
  0x07, 0x05, 0x83, 0x05, LOBYTE(USB_AUDIO_EP_SIZE), HIBYTE(USB_AUDIO_EP_SIZE), 0x01,

  /* Class-Specific AS Isochronous Audio Data Endpoint */
  0x08, 0x25, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* Strings */
__ALIGN_BEGIN UCHAR USBD_string_framework[] __ALIGN_END = {
    /* LangID */
    0x09, 0x04, 0x00, 0x01,
    /* Manufacturer */
    18, 0x03, 'W',0,'e',0,'A',0,'c',0,'t',0,' ',0,'S',0,'t',0,
    /* Product */
    24, 0x03, 'U',0,'S',0,'B',0,' ',0,'A',0,'u',0,'d',0,'i',0,'o',0,' ',0,'2',0,'.',0,'0',0,
    /* Serial */
    10, 0x03, '1',0,'2',0,'3',0,'4',0
};

/* Helper Functions to return these descriptors */

uint8_t *USBD_Get_Device_Framework_Speed(uint8_t Speed, ULONG *Length)
{
    *Length = sizeof(USBD_DeviceDesc_Audio) + sizeof(USBD_ConfigDesc_Audio);

    /* For simplicity, we concatenate them or return a buffer containing both?
       USBX expects the Device Descriptor followed by Config?
       No, USBX usually gets passed "Device Framework" which contains everything.
       We need to build a single buffer or handle the logic.
       The standard ST/USBX builder builds one large buffer.
       I will create a static buffer with both.
    */
    static uint8_t full_framework[1024]; /* Enough space */
    static uint8_t init = 0;

    if (!init) {
        memcpy(full_framework, USBD_DeviceDesc_Audio, sizeof(USBD_DeviceDesc_Audio));
        memcpy(full_framework + sizeof(USBD_DeviceDesc_Audio), USBD_ConfigDesc_Audio, sizeof(USBD_ConfigDesc_Audio));
        init = 1;
    }

    *Length = sizeof(USBD_DeviceDesc_Audio) + sizeof(USBD_ConfigDesc_Audio);
    return full_framework;
}

uint8_t *USBD_Get_String_Framework(ULONG *Length)
{
    *Length = sizeof(USBD_string_framework);
    return USBD_string_framework;
}

uint8_t *USBD_Get_Language_Id_Framework(ULONG *Length)
{
    static uint8_t lang_id[] = {0x09, 0x04};
    *Length = 2;
    return lang_id;
}

uint16_t USBD_Get_Interface_Number(uint8_t class_type, uint8_t interface_type)
{
    return 0; /* Stub */
}

uint16_t USBD_Get_Configuration_Number(uint8_t class_type, uint8_t interface_type)
{
    return 1;
}

/* Restore dummy variables to prevent build failure if referenced by legacy code */
USBD_DevClassHandleTypeDef  USBD_Device_FS, USBD_Device_HS;
uint8_t UserClassInstance[USBD_MAX_CLASS_INTERFACES] = {
  CLASS_TYPE_AUDIO,
};
