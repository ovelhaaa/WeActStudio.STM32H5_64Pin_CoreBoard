#include "ux_device_audio.h"
#include "audio_sai_slave.h"

/* Local handles */
static UX_SLAVE_INTERFACE *audio_interface_control;
static UX_SLAVE_INTERFACE *audio_interface_stream_out;
static UX_SLAVE_INTERFACE *audio_interface_stream_in;

static UX_SLAVE_ENDPOINT *audio_endpoint_iso_out;
static UX_SLAVE_ENDPOINT *audio_endpoint_iso_in;
static UX_SLAVE_ENDPOINT *audio_endpoint_feedback;

static volatile uint32_t audio_active_out = 0;
static volatile uint32_t audio_active_in = 0;

/* Internal Buffers */
/* USB buffers need to hold one packet. */
static uint8_t usb_rx_packet[USB_AUDIO_EP_SIZE];
static uint8_t usb_tx_packet[USB_AUDIO_EP_SIZE];
static uint8_t feedback_data[4]; /* 4 bytes for UAC 2.0 (16.16) */

/* Feedback Logic */
static uint32_t feedback_accum = 0;
static uint32_t feedback_count = 0;
static uint32_t last_dma_head = 0;
/* Initial guess: 48.0 samples/frame */
static uint32_t current_feedback = 0x00300000;

/* Ring Buffer Pointers (Software) */
static volatile uint32_t tx_write_ptr = 0; /* Index in frames (0 to AUDIO_BUFFER_FRAMES-1) */
static volatile uint32_t rx_read_ptr = 0;  /* Index in frames */

/* Forward Decls */
static void _ux_device_class_audio_iso_out_callback(UX_SLAVE_TRANSFER *transfer);
static void _ux_device_class_audio_iso_in_callback(UX_SLAVE_TRANSFER *transfer);
static void _ux_device_class_audio_feedback_callback(UX_SLAVE_TRANSFER *transfer);

UINT ux_device_class_audio_entry(UX_SLAVE_CLASS_COMMAND *command)
{
    UX_SLAVE_INTERFACE  *interface;

    switch(command->ux_slave_class_command_request)
    {
        case UX_SLAVE_CLASS_COMMAND_INITIALIZE:
            /* Start SAI hardware init */
            /* Optimization: Prevent multiple init calls */
            {
                static uint8_t sai_initialized = 0;
                if (!sai_initialized)
                {
                    Audio_SAI_Init();
                    sai_initialized = 1;
                }
            }
            break;

        case UX_SLAVE_CLASS_COMMAND_ACTIVATE:
            interface = (UX_SLAVE_INTERFACE *)command->ux_slave_class_command_interface;

            if (interface->ux_slave_interface_descriptor.bInterfaceClass == 0x01) /* Audio */
            {
                if (interface->ux_slave_interface_descriptor.bInterfaceSubClass == 0x01) /* AC */
                {
                    audio_interface_control = interface;
                }
                else if (interface->ux_slave_interface_descriptor.bInterfaceSubClass == 0x02) /* AS */
                {
                    /* Check Alt Setting. Alt 0 is Zero Bandwidth. Alt 1 is Active. */
                    if (interface->ux_slave_interface_descriptor.bAlternateSetting == 1)
                    {
                        /* Check endpoints to determine direction */
                        if (interface->ux_slave_interface_first_endpoint->ux_slave_endpoint_descriptor.bEndpointAddress & 0x80)
                        {
                            /* IN Endpoint -> Record */
                            audio_interface_stream_in = interface;
                            audio_endpoint_iso_in = interface->ux_slave_interface_first_endpoint;
                            audio_active_in = 1;

                            /* Reset Read Pointer to current DMA Head to avoid reading stale data */
                            /* Note: DMA Head is in Words (32-bit). We need index in Frames (2 words) */
                            /* But SAI might be running. Safe to sync. */
                            rx_read_ptr = Audio_SAI_Get_RX_Head() / 2; /* Convert 32-bit words to Frames (LR) */

                            /* Start the transfer chain */
                            UX_SLAVE_TRANSFER *transfer = &audio_endpoint_iso_in->ux_slave_endpoint_transfer_request;
                            transfer->ux_slave_transfer_request_completion_function = _ux_device_class_audio_iso_in_callback;
                            _ux_device_class_audio_iso_in_callback(transfer);
                        }
                        else
                        {
                            /* OUT Endpoint -> Playback */
                            audio_interface_stream_out = interface;
                            audio_endpoint_iso_out = interface->ux_slave_interface_first_endpoint;

                            /* Find Feedback Endpoint (IN) */
                            if (interface->ux_slave_interface_first_endpoint->ux_slave_endpoint_next_endpoint)
                            {
                                audio_endpoint_feedback = interface->ux_slave_interface_first_endpoint->ux_slave_endpoint_next_endpoint;
                            }

                            audio_active_out = 1;

                            /* Reset Write Pointer to current DMA Head + Margin */
                            /* tx_write_ptr is where we write. DMA reads from Head. */
                            /* We want to write ahead of DMA. */
                            tx_write_ptr = (Audio_SAI_Get_TX_Head() / 2 + (AUDIO_BUFFER_FRAMES / 2)) % AUDIO_BUFFER_FRAMES;
                            last_dma_head = Audio_SAI_Get_TX_Head(); /* Initialize tracker */

                            /* Start OUT reception */
                            UX_SLAVE_TRANSFER *transfer_out = &audio_endpoint_iso_out->ux_slave_endpoint_transfer_request;
                            transfer_out->ux_slave_transfer_request_completion_function = _ux_device_class_audio_iso_out_callback;
                            transfer_out->ux_slave_transfer_request_data_pointer = usb_rx_packet;
                            transfer_out->ux_slave_transfer_request_requested_length = USB_AUDIO_EP_SIZE;
                            ux_device_stack_transfer_request(transfer_out, USB_AUDIO_EP_SIZE, USB_AUDIO_EP_SIZE);

                            /* Start Feedback transmission */
                            UX_SLAVE_TRANSFER *transfer_fb = &audio_endpoint_feedback->ux_slave_endpoint_transfer_request;
                            transfer_fb->ux_slave_transfer_request_completion_function = _ux_device_class_audio_feedback_callback;
                            _ux_device_class_audio_feedback_callback(transfer_fb);
                        }

                        /* If both active, Start SAI */
                        Audio_SAI_Start();
                    }
                }
            }
            break;

        case UX_SLAVE_CLASS_COMMAND_DEACTIVATE:
             /* Simple check: if interfaces are being torn down */
             /* In a real driver we would track which interface is closing. */
             /* For this task, if we receive Deactivate, we assume host is closing. */
             /* But we should be careful not to stop RX if only TX is closed. */
             /* USBX doesn't pass the interface pointer in DEACTIVATE usually? */
             /* Actually command->ux_slave_class_command_interface is valid. */
             interface = (UX_SLAVE_INTERFACE *)command->ux_slave_class_command_interface;
             if (interface == audio_interface_stream_out) {
                 audio_active_out = 0;
             }
             if (interface == audio_interface_stream_in) {
                 audio_active_in = 0;
             }

             if (!audio_active_out && !audio_active_in) {
                 Audio_SAI_Stop();
             }
             break;

        case UX_SLAVE_CLASS_COMMAND_REQUEST:
             /* Handle Class Requests (Volume, Mute) */
             break;
    }
    return UX_SUCCESS;
}

static void _ux_device_class_audio_iso_out_callback(UX_SLAVE_TRANSFER *transfer)
{
    if (!audio_active_out) return;

    uint8_t *data = transfer->ux_slave_transfer_request_data_pointer;
    uint32_t len = transfer->ux_slave_transfer_request_actual_length;

    /* Calculate frames received (3 bytes/sample * 2 channels = 6 bytes/frame) */
    uint32_t frames = len / 6;

    /* Write to Ring Buffer */
    uint32_t current_tx_head_frames = Audio_SAI_Get_TX_Head() / 2; /* DMA Read Ptr (Frames) */

    /* Check available space to avoid overwriting DMA Read Ptr */
    /* Space = (ReadPtr - WritePtr - 1 + Size) % Size */
    uint32_t space = (current_tx_head_frames - tx_write_ptr - 1 + AUDIO_BUFFER_FRAMES) % AUDIO_BUFFER_FRAMES;

    if (space >= frames)
    {
        for (uint32_t i = 0; i < frames; i++)
        {
            /* Left Channel (3 bytes) */
            uint32_t idx = i * 6;
            /* Correct Packing for SAI_DATASIZE_24 (Right Aligned in 32-bit word) -> 0x00HHMMLL */
            uint32_t sample_l = (data[idx+2] << 16) | (data[idx+1] << 8) | (data[idx]);

            /* Right Channel (3 bytes) */
            uint32_t sample_r = (data[idx+5] << 16) | (data[idx+4] << 8) | (data[idx+3]);

            /* Write 32-bit words to TX Buffer */
            /* Buffer is cast to uint32_t*. 2 words per frame. */
            ((uint32_t*)Audio_TX_Buffer)[tx_write_ptr * 2] = sample_l;
            ((uint32_t*)Audio_TX_Buffer)[tx_write_ptr * 2 + 1] = sample_r;

            tx_write_ptr = (tx_write_ptr + 1) % AUDIO_BUFFER_FRAMES;
        }
    }
    else
    {
        /* Buffer Overflow / Underrun imminent. Packet Dropped. */
        /* In a real app we might error out or partial write. */
    }

    /* Feedback Calculation Logic (Every SOF usually, but here every packet ~1ms) */
    Audio_Feedback_Calculation();

    /* Re-arm transfer */
    ux_device_stack_transfer_request(transfer, USB_AUDIO_EP_SIZE, USB_AUDIO_EP_SIZE);
}

static void _ux_device_class_audio_iso_in_callback(UX_SLAVE_TRANSFER *transfer)
{
    if (!audio_active_in) return;

    /* Read from Ring Buffer */
    uint32_t current_rx_head_frames = Audio_SAI_Get_RX_Head() / 2; /* DMA Write Ptr */

    /* Calculate Available Frames */
    /* Available = (WritePtr - ReadPtr + Size) % Size */
    uint32_t available = (current_rx_head_frames - rx_read_ptr + AUDIO_BUFFER_FRAMES) % AUDIO_BUFFER_FRAMES;

    /* Async IN: We send exactly what is available in the buffer since last SOF */
    /* Clamp to Max Packet Size (plus margin) */
    uint32_t frames_to_send = available;

    if (frames_to_send > (USB_AUDIO_EP_SIZE / 6))
    {
        frames_to_send = (USB_AUDIO_EP_SIZE / 6);
    }

    /* If available is 0, we send 0-length packet (ZLP) or small packet? */
    /* UAC 2.0 allows ZLP for Asynchronous if no data is ready. */
    /* However, continuous stream is better. */

    uint32_t byte_count = 0;

    for (uint32_t i = 0; i < frames_to_send; i++)
    {
        uint32_t sample_l = 0;
        uint32_t sample_r = 0;

        if (available > 0)
        {
            sample_l = ((uint32_t*)Audio_RX_Buffer)[rx_read_ptr * 2];
            sample_r = ((uint32_t*)Audio_RX_Buffer)[rx_read_ptr * 2 + 1];

            rx_read_ptr = (rx_read_ptr + 1) % AUDIO_BUFFER_FRAMES;
            available--;
        }

        /* Pack to 24-bit */
        /* Source is Right Aligned: 0x00HHMMLL */
        /* We want USB Format: LL MM HH */

        usb_tx_packet[byte_count++] = (sample_l) & 0xFF;        /* LL */
        usb_tx_packet[byte_count++] = (sample_l >> 8) & 0xFF;   /* MM */
        usb_tx_packet[byte_count++] = (sample_l >> 16) & 0xFF;  /* HH */

        usb_tx_packet[byte_count++] = (sample_r) & 0xFF;
        usb_tx_packet[byte_count++] = (sample_r >> 8) & 0xFF;
        usb_tx_packet[byte_count++] = (sample_r >> 16) & 0xFF;
    }

    transfer->ux_slave_transfer_request_data_pointer = usb_tx_packet;
    transfer->ux_slave_transfer_request_requested_length = byte_count;

    ux_device_stack_transfer_request(transfer, byte_count, byte_count);
}

void Audio_Feedback_Calculation(void)
{
    /* Calculate samples consumed by SAI since last check */
    /* We assume this function is called roughly every 1ms (Frame) inside ISO OUT callback */

    uint32_t current_dma = Audio_SAI_Get_TX_Head(); /* Words */

    /* Calculate delta in Words */
    uint32_t delta_words;
    if (current_dma >= last_dma_head)
    {
        delta_words = current_dma - last_dma_head;
    }
    else
    {
        /* Wrapped */
        delta_words = (AUDIO_BUFFER_SIZE / 4) - last_dma_head + current_dma;
    }

    last_dma_head = current_dma;

    /* Convert Words to Samples (Frames). 2 Words per Frame (Stereo) */
    uint32_t samples = delta_words / 2;

    /* Accumulate for averaging (simple IIR or window) */
    /* Using a simple moving average or just raw? Raw is too jittery. */
    /* Filter: New = (Old * 3 + New) / 4 */
    /* Note: We need 16.16 format. */
    /* Measured 'samples' is e.g. 48. */
    /* In 16.16, 48 is 0x00300000. */

    uint32_t measured_fixed = samples << 16;

    /* Filter */
    current_feedback = (current_feedback * 3 + measured_fixed) / 4;
}

static void _ux_device_class_audio_feedback_callback(UX_SLAVE_TRANSFER *transfer)
{
    if (!audio_active_out) return;

    /* Send the calculated feedback value */
    /* UAC 2.0 Full Speed: 4 bytes, 16.16 format. */

    /* Value is Samples Per Frame (1ms). Nominal 48.0 = 0x00300000 */
    /* If SAI is faster (consumes 48.1), feedback = 48.1 */
    /* Host will speed up sending. */

    feedback_data[0] = (current_feedback) & 0xFF;
    feedback_data[1] = (current_feedback >> 8) & 0xFF;
    feedback_data[2] = (current_feedback >> 16) & 0xFF;
    feedback_data[3] = (current_feedback >> 24) & 0xFF;

    transfer->ux_slave_transfer_request_data_pointer = feedback_data;
    transfer->ux_slave_transfer_request_requested_length = 4;

    ux_device_stack_transfer_request(transfer, 4, 4);
}
