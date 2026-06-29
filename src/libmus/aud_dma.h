/*
 * aud_dma.h
 *
 * Public entry points of the libmus streaming-DMA manager (aud_dma.c):
 * subsystem bring-up and the per-frame buffer-servicing tick.
 */
#ifndef _LIBMUS_AUD_DMA_H_
#define _LIBMUS_AUD_DMA_H_

// Bring up the streaming-DMA pool: dma_buffer_count buffers of dma_buffer_size
// bytes each. Returns the libaudio DMA-new callback for the synthesizer.
ALDMANew __MusIntDmaInit(int dma_buffer_count, int dma_buffer_size);

// Per-frame tick: reap completed DMAs and age the buffers for eviction.
void __MusIntDmaProcess(void);

#endif
