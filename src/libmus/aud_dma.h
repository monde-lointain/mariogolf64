/*
 * aud_dma.h
 *
 * Public interface of the libmus streaming-DMA buffer layer (aud_dma.c). Sample
 * data lives in cartridge ROM; this layer streams the windows the synthesizer
 * needs into a pool of RDRAM buffers on demand, then resolves each playback
 * read to the physical RDRAM address holding that sample.
 */
#ifndef _LIBMUS_AUD_DMA_H_
#define _LIBMUS_AUD_DMA_H_

/*
 * Bring up the streaming-DMA buffer pool: dma_buffer_count buffers of
 * dma_buffer_size bytes each, plus the PI handle and DMA message queue. Returns
 * the libaudio "DMA new" callback the synthesizer installs to obtain its
 * per-voice DMA read callback.
 */
ALDMANew __MusIntDmaInit(int dma_buffer_count, int dma_buffer_size);

/*
 * Per-frame tick: reap completed DMA reads off the queue and age every buffer's
 * keep-count one step toward eviction.
 */
void __MusIntDmaProcess(void);

#endif
