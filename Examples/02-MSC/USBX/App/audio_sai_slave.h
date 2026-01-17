#ifndef AUDIO_SAI_SLAVE_H
#define AUDIO_SAI_SLAVE_H

#include "audio_config.h"

/* Exported Functions */
void Audio_SAI_Init(void);
void Audio_SAI_Start(void);
void Audio_SAI_Stop(void);

/* Buffer Access */
/* These pointers point to the Ring Buffers managed by DMA */
extern uint8_t Audio_RX_Buffer[AUDIO_BUFFER_SIZE];
extern uint8_t Audio_TX_Buffer[AUDIO_BUFFER_SIZE];

/* Pointers for USBX to read/write */
/* Current DMA position (Head) */
uint32_t Audio_SAI_Get_TX_Head(void);
uint32_t Audio_SAI_Get_RX_Head(void);

#endif /* AUDIO_SAI_SLAVE_H */
