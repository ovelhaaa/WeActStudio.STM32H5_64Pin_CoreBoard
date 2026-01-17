#ifndef UX_DEVICE_AUDIO_H
#define UX_DEVICE_AUDIO_H

#include "ux_api.h"
#include "audio_config.h"

/* Define Audio Class Type */
#define CLASS_TYPE_AUDIO  0x10

/* State definitions */
#define UX_DEVICE_CLASS_AUDIO_INIT       0x00
#define UX_DEVICE_CLASS_AUDIO_START      0x01
#define UX_DEVICE_CLASS_AUDIO_STOP       0x02

/* Dummy Parameter Struct */
typedef struct
{
    ULONG ux_slave_class_audio_parameter_dummy;
} UX_SLAVE_CLASS_AUDIO_PARAMETER;

/* Function Prototypes */
UINT ux_device_class_audio_entry(UX_SLAVE_CLASS_COMMAND *command);
void Audio_Feedback_Calculation(void);

#endif /* UX_DEVICE_AUDIO_H */
