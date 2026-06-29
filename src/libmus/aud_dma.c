/*
 * aud_dma.c
 *
 * libmus streaming-DMA buffer manager. Streams sample data from cartridge ROM
 * into a pool of fixed-size RAM buffers and serves it to the audio synthesizer
 * through libaudio's DMA callbacks. Buffers live in an address-sorted "active"
 * list with a "free" list behind it: a fetch reuses a buffer that already
 * covers the requested range, else it claims a buffer (reclaiming the
 * least-recently-used one when the pool is full) and launches a PI DMA to fill
 * it from ROM.
 */

// Audio-library API; the SUPPORT_NAUDIO build links the n_audio variant.
#ifndef SUPPORT_NAUDIO
#include <libaudio.h>
#else
#include <n_libaudio_sn_sc.h>
#endif
#include "libmus.h"
#include "lib_memory.h"
#include "aud_dma.h"

// Tuning constants for the streaming buffer pool.
// Floor sample source addresses to a 16-bit boundary.
#define SAMPLE_ALIGNMENT 2
// Keep-alive bias loaded into a touched buffer's keep_count.
#define FRAME_LAG 0x20000000
// Base offset of wave data within the 64DD ROM.
#define DDROM_WAVEDATA_START 0x00140000

extern u32 g_mus_control_flag;  // global libmus mode flags (MUSCONTROL_RAM ==
                                // samples already in RAM)

// One pool buffer. Active nodes form a doubly-linked list sorted by
// sample_addr; the rest sit on the free list.
typedef struct dma_list_s {
  struct dma_list_s* prev;
  struct dma_list_s* next;
  int keep_count;  // recency rank; aged each frame, lowest == evict first
  unsigned long
      sample_addr;  // ROM source address cached here (0xffffffff == invalid)
  unsigned char* ram_addr;  // RAM address of this buffer's data
} dma_list_t;

// Shared buffer-pool state (defined elsewhere in libmus).
extern dma_list_t* dma_buffer_head;  // head of the active, address-sorted list
extern dma_list_t* dma_buffer_free;  // head of the free list
extern dma_list_t* dma_buffer_list;  // backing array of all pool nodes
extern OSIoMesg* audio_IO_mess_buf;  // PI DMA request blocks (2 per buffer)
extern OSMesg* audio_mess_buf;       // completion-queue message storage
extern int audio_dma_size;           // bytes per buffer / per DMA transfer
extern int audio_dma_count;          // outstanding DMAs; also next request slot
extern OSMesgQueue audDMAMessageQ;   // receives DMA-completion notifications
extern OSPiHandle* cartrom_handle;   // PI channel the samples stream from
extern int g_mus_dma_buffer_count;   // number of buffers in the pool

static ALDMAproc __CallBackDmaNew(void* ignored);
static s32 __CallBackDmaProcess(s32 addr, s32 len, void* ignored);
static dma_list_t* __MusIntDmaSample(unsigned long sample_addr,
                                     int sample_length);

/*
 * Subsystem initializer: open the cart-ROM PI channel, allocate the message
 * and buffer-node pools, build the free list, and create the completion queue.
 * Returns the libaudio DMA-new callback the synthesizer installs to obtain
 * per-voice sample fetches. dma_buffer_count is the pool size; dma_buffer_size
 * is the byte size of each buffer (== one DMA).
 */
ALDMANew __MusIntDmaInit(int dma_buffer_count, int dma_buffer_size) {
  int i;

  // PI channel the streamed sample data is read from.
  cartrom_handle = osCartRomInit();

  // Allocate the pools: two OS messages per buffer (request + completion) and
  // one list node per buffer, with the node array zeroed.
  audio_IO_mess_buf =
      __MusIntMemMalloc(dma_buffer_count * 2 * sizeof(OSIoMesg));
  audio_mess_buf = __MusIntMemMalloc(dma_buffer_count * 2 * sizeof(OSMesg));
  dma_buffer_list = __MusIntMemMalloc(dma_buffer_count * sizeof(dma_list_t));
  __MusIntMemSet(dma_buffer_list, 0, dma_buffer_count * sizeof(dma_list_t));
  g_mus_dma_buffer_count = dma_buffer_count;

  // Chain the nodes into one list and give each its own RAM buffer; a
  // 0xffffffff sample_addr marks the buffer as holding no valid data yet.
  for (i = 0; i < dma_buffer_count - 1; i++) {
    dma_buffer_list[i].next = dma_buffer_list + i + 1;
    dma_buffer_list[i + 1].prev = dma_buffer_list + i;
    dma_buffer_list[i].ram_addr = __MusIntMemMalloc(dma_buffer_size);
    dma_buffer_list[i].sample_addr = 0xffffffff;
  }
  dma_buffer_list[i].ram_addr = __MusIntMemMalloc(dma_buffer_size);
  dma_buffer_list[i].sample_addr = 0xffffffff;

  // Start with every buffer free and the active list empty.
  audio_dma_size = dma_buffer_size;
  audio_dma_count = 0;
  dma_buffer_head = NULL;
  dma_buffer_free = dma_buffer_list;
  osCreateMesgQueue(&audDMAMessageQ, audio_mess_buf, dma_buffer_count * 2);
  return (__CallBackDmaNew);
}

/*
 * Per-frame service tick: drain completed-DMA notifications, then age every
 * buffer by decrementing its keep_count so idle buffers become eviction
 * candidates. The aging walk runs with interrupts masked because the audio
 * interrupt also touches the buffer list.
 */
void __MusIntDmaProcess(void) {
  OSIoMesg* iomsg;
  int i;
  OSIntMask prev_mask;

  // Reap each outstanding DMA-completion message.
  while (audio_dma_count) {
    osRecvMesg(&audDMAMessageQ, (OSMesg*)&iomsg, OS_MESG_NOBLOCK);
    audio_dma_count--;
  }

  // Age the buffers (lower keep_count == less recently used).
  prev_mask = osSetIntMask(OS_IM_NONE);
  for (i = 0; i < g_mus_dma_buffer_count; i++) {
    if (dma_buffer_list[i].keep_count) {
      dma_buffer_list[i].keep_count--;
    }
  }
  osSetIntMask(prev_mask);
}

/*
 * libaudio ALDMANew callback: the synthesizer calls this once per voice to
 * obtain the per-sample fetch routine; the sample-state argument is unused.
 */
static ALDMAproc __CallBackDmaNew(void* ignored) {
  return (__CallBackDmaProcess);
}

/*
 * libaudio ALDMAproc callback: map a sample source address to the physical RAM
 * address the RSP should read. Ensures [addr, addr+len) is resident in a DMA
 * buffer, then returns the physical address of that byte within it. When no
 * buffer is used (RAM mode) the address is returned as-is.
 */
static s32 __CallBackDmaProcess(s32 addr, s32 len, void* ignored) {
  dma_list_t* buffer;
  unsigned char* ram_addr;
  buffer = __MusIntDmaSample(addr, len);
  if (!buffer) {
    return (osVirtualToPhysical((void*)addr));
  }

  // A 0xff high byte tags a 64DD wave-data address; rebase it into the DD ROM.
  if ((addr & 0xff000000) == 0xff000000) {
    addr &= 0xffffff;
    addr += DDROM_WAVEDATA_START;
  }

  // Point at the requested byte's offset within the cached buffer.
  ram_addr = buffer->ram_addr + (u32)addr - buffer->sample_addr;
  return (osVirtualToPhysical(ram_addr));
}

/* Empty routine; performs no work. */
void func_8009DBA0(void) {}

/*
 * Buffer lookup/claim: find a buffer already covering [sample_addr,
 * sample_addr+length), or claim one (off the free list, else by evicting the
 * least-recently-used active buffer) and start a PI DMA to fill it. The active
 * list stays sorted by sample_addr ascending. Returns the covering/filling
 * buffer, NULL in RAM mode, or the head buffer when nothing can be reclaimed.
 */
static dma_list_t* __MusIntDmaSample(unsigned long sample_addr,
                                     int sample_length) {
  dma_list_t* current_dma_buffer;
  dma_list_t* free_buffer;
  dma_list_t* last_buffer;
  unsigned long sample_addr_end;
  OSPiHandle* pi_handle;
  OSIoMesg* io_msg;

  // In RAM mode the samples are already resident, so no transfer is needed.
  if (g_mus_control_flag & MUSCONTROL_RAM) {
    return (NULL);
  }
  pi_handle = cartrom_handle;
  sample_addr_end = sample_addr + sample_length;

  // Scan the sorted active list for a buffer whose cached range covers the
  // request. Stop once addresses sort past it; last_buffer tracks the
  // predecessor used for an in-order insert.
  last_buffer = NULL;
  for (current_dma_buffer = dma_buffer_head; current_dma_buffer;
       current_dma_buffer = current_dma_buffer->next) {
    if (sample_addr < current_dma_buffer->sample_addr) {
      break;
    }
    if (sample_addr_end <= current_dma_buffer->sample_addr + audio_dma_size) {
      current_dma_buffer->keep_count = 1 + FRAME_LAG;
      return (current_dma_buffer);
    }
    last_buffer = current_dma_buffer;
  }

  // Miss: take a node off the free list.
  free_buffer = dma_buffer_free;
  if (!free_buffer) {
    // Free list empty: evict the active buffer with the lowest keep_count.
    free_buffer = dma_buffer_head;
    if (!free_buffer) {
      goto no_free_buffer;
    }
    for (current_dma_buffer = dma_buffer_head; current_dma_buffer;
         current_dma_buffer = current_dma_buffer->next) {
      if (free_buffer->keep_count > current_dma_buffer->keep_count) {
        free_buffer = current_dma_buffer;
      }
    }
    if (!free_buffer) {
      goto no_free_buffer;
    }

    // Unlink the victim from the active list, fixing up the insert predecessor
    // and head, then push it onto the free list.
    if (free_buffer == last_buffer) {
      last_buffer = free_buffer->prev;
    }
    if (free_buffer->next) {
      free_buffer->next->prev = free_buffer->prev;
    }
    if (free_buffer->prev) {
      free_buffer->prev->next = free_buffer->next;
    } else {
      dma_buffer_head = free_buffer->next;
    }
    free_buffer->prev = NULL;
    free_buffer->next = dma_buffer_free;
    dma_buffer_free = free_buffer;
    goto got_free_buffer;
  no_free_buffer:
    // Nothing reclaimable; hand back the head buffer as a fallback.
    return (dma_buffer_head);
  }
got_free_buffer:
  // Pop the chosen node off the free list.
  dma_buffer_free = free_buffer->next;

  // Splice it into the active list in sorted position: after last_buffer, or
  // at the head when the request sorts before everything.
  if (last_buffer) {
    free_buffer->next = last_buffer->next;
    if (free_buffer->next) {
      free_buffer->next->prev = free_buffer;
    }
    free_buffer->prev = last_buffer;
    last_buffer->next = free_buffer;
  } else {
    free_buffer->next = dma_buffer_head;
    free_buffer->prev = NULL;
    if (dma_buffer_head) {
      dma_buffer_head->prev = free_buffer;
    }
    dma_buffer_head = free_buffer;
  }

  // Record the aligned source address and refresh the keep-alive counter.
  free_buffer->sample_addr = sample_addr & (~(SAMPLE_ALIGNMENT - 1));
  free_buffer->keep_count = 1 + FRAME_LAG;

  // Queue and launch the PI read that fills the buffer from ROM.
  io_msg = &audio_IO_mess_buf[audio_dma_count++];
  io_msg->hdr.pri = OS_MESG_PRI_NORMAL;
  io_msg->hdr.retQueue = &audDMAMessageQ;
  io_msg->dramAddr = free_buffer->ram_addr;
  io_msg->devAddr = free_buffer->sample_addr;
  io_msg->size = audio_dma_size;
  osEPiStartDma(pi_handle, io_msg, OS_READ);
  return (free_buffer);
}
