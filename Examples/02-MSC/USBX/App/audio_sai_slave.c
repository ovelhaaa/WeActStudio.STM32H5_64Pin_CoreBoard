#include "audio_sai_slave.h"
#include "main.h"

/* Global Handles */
SAI_HandleTypeDef hsai_BlockA1; /* RX */
SAI_HandleTypeDef hsai_BlockB1; /* TX */
DMA_HandleTypeDef handle_GPDMA1_Channel0; /* SAI_A RX */
DMA_HandleTypeDef handle_GPDMA1_Channel1; /* SAI_B TX */

/* Buffers */
#if defined ( __ICCARM__ )
#pragma data_alignment=4
#endif
__ALIGN_BEGIN uint8_t Audio_RX_Buffer[AUDIO_BUFFER_SIZE] __ALIGN_END;
#if defined ( __ICCARM__ )
#pragma data_alignment=4
#endif
__ALIGN_BEGIN uint8_t Audio_TX_Buffer[AUDIO_BUFFER_SIZE] __ALIGN_END;

/* Initialization */
void Audio_SAI_Init(void)
{
  /* Configure SAI1 Clock Source */
  /* Using PLL1_Q as it is usually configured in SystemClock_Config for USB */
  /* We need a kernel clock for SAI even in slave mode */
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SAI1;
  PeriphClkInitStruct.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLL1Q;
  HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);

  /* Enable Clocks */
  __HAL_RCC_SAI1_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE(); /* Changed from GPIOE to GPIOB */
  __HAL_RCC_GPDMA1_CLK_ENABLE();

  /* GPIO Configuration */
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIOB: PB12(FS), PB10(SCK), PB15(SD_A), PB5(SD_B) */
  /* Note: Check HAL_GPIO_Init logic for multiple pins. They must share same Alternate Function.
     On STM32H5, SAI1 pins are AF6. */

  GPIO_InitStruct.Pin = SAI_PIN_FS | SAI_PIN_SCK | SAI_PIN_SD_A | SAI_PIN_SD_B;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF6_SAI1;
  HAL_GPIO_Init(SAI_PORT, &GPIO_InitStruct);

  /* SAI Block A (RX - Slave) */
  hsai_BlockA1.Instance = SAI1_Block_A;
  hsai_BlockA1.Init.AudioMode = SAI_MODESLAVE_RX;
  hsai_BlockA1.Init.Synchro = SAI_ASYNCHRONOUS; /* Sync to external clock */
  hsai_BlockA1.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  hsai_BlockA1.Init.NoDivider = SAI_MASTERDIVIDER_ENABLE; /* Don't care in Slave */
  hsai_BlockA1.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
  hsai_BlockA1.Init.AudioFrequency = AUDIO_FREQUENCY;
  hsai_BlockA1.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  hsai_BlockA1.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockA1.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockA1.Init.TriState = SAI_OUTPUT_NOTRELEASED;

  /* Added DataSize to avoid uninitialized behavior */
  /* We use 24-bit data in 32-bit slot */
  hsai_BlockA1.Init.DataSize = SAI_DATASIZE_24;

  /* Frame Config */
  hsai_BlockA1.FrameInit.FrameLength = 64; /* 32 bits * 2 */
  hsai_BlockA1.FrameInit.ActiveFrameLength = 32;
  hsai_BlockA1.FrameInit.FSDefinition = SAI_FS_STARTFRAME;
  hsai_BlockA1.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
  hsai_BlockA1.FrameInit.FSOffset = SAI_FS_BEFOREFIRSTBIT;

  /* Slot Config */
  hsai_BlockA1.SlotInit.FirstBitOffset = 0;
  hsai_BlockA1.SlotInit.SlotSize = SAI_SLOTSIZE_32B;
  hsai_BlockA1.SlotInit.SlotNumber = 2;
  hsai_BlockA1.SlotInit.SlotActive = SAI_SLOTACTIVE_0 | SAI_SLOTACTIVE_1;

  HAL_SAI_Init(&hsai_BlockA1);

  /* SAI Block B (TX - Slave) */
  /* Synchronous to Block A (internally) or just configured same way?
     Since signals are external, we configure it as Synchronous Slave
     or Asynchronous Slave connected to same pins.
     If we set Sync to Block A, it uses internal routing.
     If we set Async, it expects pins.
     Since we share pins PE4/PE5, we should set Block B to Sync with A
     so it doesn't try to re-configure the GPIOs or expect separate ones?
     Wait, on STM32 SAI, usually one block is Master/Slave and other Syncs to it.
     But here BOTH are Slaves to External.
     The best way is to configure Block A as ASYNC SLAVE, and Block B as SYNC SLAVE to A.
     This way Block B uses the clocks seen by Block A. */

  hsai_BlockB1.Instance = SAI1_Block_B;
  hsai_BlockB1.Init.AudioMode = SAI_MODESLAVE_TX;
  hsai_BlockB1.Init.Synchro = SAI_SYNCHRONOUS; /* Sync to Block A */
  hsai_BlockB1.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  hsai_BlockB1.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
  hsai_BlockB1.Init.AudioFrequency = AUDIO_FREQUENCY;
  hsai_BlockB1.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockB1.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockB1.Init.TriState = SAI_OUTPUT_NOTRELEASED;

  hsai_BlockB1.Init.DataSize = SAI_DATASIZE_24;

  /* Frame/Slot must match */
  hsai_BlockB1.FrameInit.FrameLength = 64;
  hsai_BlockB1.FrameInit.ActiveFrameLength = 32;
  hsai_BlockB1.FrameInit.FSDefinition = SAI_FS_STARTFRAME;
  hsai_BlockB1.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
  hsai_BlockB1.FrameInit.FSOffset = SAI_FS_BEFOREFIRSTBIT;

  hsai_BlockB1.SlotInit.FirstBitOffset = 0;
  hsai_BlockB1.SlotInit.SlotSize = SAI_SLOTSIZE_32B;
  hsai_BlockB1.SlotInit.SlotNumber = 2;
  hsai_BlockB1.SlotInit.SlotActive = SAI_SLOTACTIVE_0 | SAI_SLOTACTIVE_1;

  HAL_SAI_Init(&hsai_BlockB1);

  /* DMA Init (GPDMA) */
  /* Note: GPDMA Config is complex on H5. Assuming Standard Linked List or Circular */

  /* RX DMA */
  handle_GPDMA1_Channel0.Instance = GPDMA1_Channel0;
  handle_GPDMA1_Channel0.Init.Request = GPDMA1_REQUEST_SAI1_A;
  handle_GPDMA1_Channel0.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
  handle_GPDMA1_Channel0.Init.Direction = DMA_PERIPH_TO_MEMORY;
  handle_GPDMA1_Channel0.Init.SrcInc = DMA_SINC_FIXED;
  handle_GPDMA1_Channel0.Init.DestInc = DMA_DINC_INCREMENTED;
  handle_GPDMA1_Channel0.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE; /* SAI FIFO is accessed as bytes usually or words? 24-bit needs care. Using Byte allows packing. */
  /* Wait, SAI Data Reg is 32-bit. But we want 24-bit packed?
     No, SAI usually puts 32-bit words. If we want 24-bit, we might get 0-padding.
     The config says Slot 32-bit.
     If we use DMA_SRC_DATAWIDTH_WORD, we get 32-bit.
     USB expects 3 bytes. We would need to repack.
     For simplicity now, let's assume we transfer 32-bit (4 bytes) per sample and skip one in USB,
     OR we configure SAI to 24-bit slot?
     If Slot is 32-bit, SAI fills 32-bit.
     Let's stick to 32-bit in Buffer and handle conversion in USB loop. */

  handle_GPDMA1_Channel0.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_WORD;
  handle_GPDMA1_Channel0.Init.DestDataWidth = DMA_DEST_DATAWIDTH_WORD;
  handle_GPDMA1_Channel0.Init.Priority = DMA_HIGH_PRIORITY;
  handle_GPDMA1_Channel0.Init.SrcBurstLength = 1;
  handle_GPDMA1_Channel0.Init.DestBurstLength = 1;
  handle_GPDMA1_Channel0.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0|DMA_DEST_ALLOCATED_PORT0;
  handle_GPDMA1_Channel0.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
  handle_GPDMA1_Channel0.Init.Mode = DMA_CIRCULAR;

  HAL_DMA_Init(&handle_GPDMA1_Channel0);
  __HAL_LINKDMA(&hsai_BlockA1, hdmarx, handle_GPDMA1_Channel0);

  /* TX DMA */
  handle_GPDMA1_Channel1.Instance = GPDMA1_Channel1;
  handle_GPDMA1_Channel1.Init.Request = GPDMA1_REQUEST_SAI1_B;
  handle_GPDMA1_Channel1.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
  handle_GPDMA1_Channel1.Init.Direction = DMA_MEMORY_TO_PERIPH;
  handle_GPDMA1_Channel1.Init.SrcInc = DMA_SINC_INCREMENTED;
  handle_GPDMA1_Channel1.Init.DestInc = DMA_DINC_FIXED;
  handle_GPDMA1_Channel1.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_WORD;
  handle_GPDMA1_Channel1.Init.DestDataWidth = DMA_DEST_DATAWIDTH_WORD;
  handle_GPDMA1_Channel1.Init.Priority = DMA_HIGH_PRIORITY;
  handle_GPDMA1_Channel1.Init.SrcBurstLength = 1;
  handle_GPDMA1_Channel1.Init.DestBurstLength = 1;
  handle_GPDMA1_Channel1.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0|DMA_DEST_ALLOCATED_PORT0;
  handle_GPDMA1_Channel1.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
  handle_GPDMA1_Channel1.Init.Mode = DMA_CIRCULAR;

  HAL_DMA_Init(&handle_GPDMA1_Channel1);
  __HAL_LINKDMA(&hsai_BlockB1, hdmatx, handle_GPDMA1_Channel1);
}

void Audio_SAI_Start(void)
{
  HAL_SAI_Receive_DMA(&hsai_BlockA1, Audio_RX_Buffer, AUDIO_BUFFER_SIZE / 4);
  HAL_SAI_Transmit_DMA(&hsai_BlockB1, Audio_TX_Buffer, AUDIO_BUFFER_SIZE / 4);
}

void Audio_SAI_Stop(void)
{
  HAL_SAI_DMAStop(&hsai_BlockA1);
  HAL_SAI_DMAStop(&hsai_BlockB1);
}

uint32_t Audio_SAI_Get_TX_Head(void)
{
   return (AUDIO_BUFFER_SIZE / 4) - __HAL_DMA_GET_COUNTER(&handle_GPDMA1_Channel1);
}

uint32_t Audio_SAI_Get_RX_Head(void)
{
   return (AUDIO_BUFFER_SIZE / 4) - __HAL_DMA_GET_COUNTER(&handle_GPDMA1_Channel0);
}
