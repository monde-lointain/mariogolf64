/*
 * aud_dma.c
 *
 * libmus sample-DMA layer: a small pool of fixed-size RDRAM buffers that cache
 * wavetable sample data streamed in from cartridge ROM on demand. The
 * synthesizer driver pulls sample bytes through the callback handed back here;
 * each request is served from a buffer that already caches the region, or by
 * evicting the least-wanted buffer and starting a fresh PI DMA to fill it. In
 * RAM-playback mode the sample data is already resident, so transfers are
 * skipped and the source address is translated directly.
 */

#ifndef SUPPORT_NAUDIO
#include <libaudio.h>
#else
#include <n_libaudio_sn_sc.h>
#endif
#include "libmus.h"
#include "lib_memory.h"
#include "aud_dma.h"

/* Sample source addresses are snapped down to 16-bit sample boundaries. */
#define SAMPLE_ALIGNMENT 2

/*
 * Large bias added to a buffer's keep_count whenever it is (re)used, so a
 * freshly touched buffer always outranks an aged one when an eviction victim
 * is chosen; __MusIntDmaProcess decays the count by one per frame.
 */
#define FRAME_LAG 0x20000000

/*
 * Source addresses tagged with a 0xff high byte name 64DD disk wave data; they
 * are rebased into the cartridge wavedata region by this offset.
 */
#define DDROM_WAVEDATA_START 0x00140000

/* Global control word; MUSCONTROL_RAM selects RAM playback (no DMA needed). */
extern u32 __muscontrol_flag;

/* One cache buffer: a node on either the active or the free list. */
typedef struct dma_list_s {
  struct dma_list_s* prev;
  struct dma_list_s* next;
  int keep_count;            /* recency rank; lowest is evicted first */
  unsigned long sample_addr; /* ROM region cached here (0xffffffff = empty) */
  unsigned char* ram_addr;   /* RDRAM scratch holding the cached bytes */
} dma_list_t;

/*
 * The cache's two lists and their backing array:
 *   dma_buffer_head - active list, sorted ascending by sample_addr
 *   dma_buffer_free - free list of unused nodes
 *   dma_buffer_list - backing array of all nodes
 */
extern dma_list_t* dma_buffer_head;
extern dma_list_t* dma_buffer_free;
extern dma_list_t* dma_buffer_list;
extern OSIoMesg* audio_IO_mess_buf;
extern OSMesg* audio_mess_buf;
extern int audio_dma_size;
extern int audio_dma_count;
extern OSMesgQueue audDMAMessageQ;
extern OSPiHandle* cartrom_handle;
extern int g_mus_dma_buffer_count;

static ALDMAproc __CallBackDmaNew(void* ignored);
static s32 __CallBackDmaProcess(s32 addr, s32 len, void* ignored);
static dma_list_t* __MusIntDmaSample(unsigned long sample_addr,
                                     int sample_length);

/*
 * Builds the DMA buffer pool and returns the driver's callback factory. Opens
 * the cartridge PI handle, allocates the I/O-message arrays and the buffer-node
 * array, then threads every node onto the free list, giving each its own
 * freshly allocated RDRAM scratch buffer and an empty (0xffffffff) cache tag.
 * The active list starts empty.
 */
ALDMANew __MusIntDmaInit(int dma_buffer_count, int dma_buffer_size) {
  int i;

  cartrom_handle = osCartRomInit();
  audio_IO_mess_buf =
      __MusIntMemMalloc(dma_buffer_count * 2 * sizeof(OSIoMesg));
  audio_mess_buf = __MusIntMemMalloc(dma_buffer_count * 2 * sizeof(OSMesg));
  dma_buffer_list = __MusIntMemMalloc(dma_buffer_count * sizeof(dma_list_t));
  __MusIntMemSet(dma_buffer_list, 0, dma_buffer_count * sizeof(dma_list_t));
  g_mus_dma_buffer_count = dma_buffer_count;

  /* Link nodes 0..count-2 to their successor and back-link the successor. */
  for (i = 0; i < dma_buffer_count - 1; i++) {
    dma_buffer_list[i].next = dma_buffer_list + i + 1;
    dma_buffer_list[i + 1].prev = dma_buffer_list + i;
    dma_buffer_list[i].ram_addr = __MusIntMemMalloc(dma_buffer_size);
    dma_buffer_list[i].sample_addr = 0xffffffff;
  }

  /* The final node has no successor to wire, but still needs its buffer/tag. */
  dma_buffer_list[i].ram_addr = __MusIntMemMalloc(dma_buffer_size);
  dma_buffer_list[i].sample_addr = 0xffffffff;

  audio_dma_size = dma_buffer_size;
  audio_dma_count = 0;
  dma_buffer_head = NULL;
  dma_buffer_free = dma_buffer_list;

  osCreateMesgQueue(&audDMAMessageQ, audio_mess_buf, dma_buffer_count * 2);
  return (__CallBackDmaNew);
}

/*
 * Per-frame maintenance. Reaps the completion messages of every DMA started
 * since the last call, then ages every buffer by one, so buffers that stop
 * being requested drift toward eviction.
 */
void __MusIntDmaProcess(void) {
  OSIoMesg* iomsg;
  int i;
  OSIntMask prev_mask;

  while (audio_dma_count) {
    osRecvMesg(&audDMAMessageQ, (OSMesg*)&iomsg, OS_MESG_NOBLOCK);
    audio_dma_count--;
  }

  /* Mask interrupts: the DMA callback also touches keep_count. */
  prev_mask = osSetIntMask(OS_IM_NONE);
  for (i = 0; i < g_mus_dma_buffer_count; i++) {
    if (dma_buffer_list[i].keep_count) {
      dma_buffer_list[i].keep_count--;
    }
  }
  osSetIntMask(prev_mask);
}

/* Driver hook: hands back the actual sample-translation callback. */
static ALDMAproc __CallBackDmaNew(void* ignored) {
  return (__CallBackDmaProcess);
}

/*
 * Sample-translation callback the synthesizer invokes when it needs sample
 * bytes. Ensures [addr, addr+len) is cached (or now streaming) in RDRAM and
 * returns the physical address the RSP should read from. In RAM-playback mode
 * no buffer is used and the source address is translated directly.
 */
static s32 __CallBackDmaProcess(s32 addr, s32 len, void* ignored) {
  dma_list_t* buffer;
  unsigned char* ram_addr;

  buffer = __MusIntDmaSample(addr, len);
  if (!buffer) {
    /* No cache buffer: the sample already lives in RAM, read it in place. */
    return (osVirtualToPhysical((void*)addr));
  }

  /* Rebase a 64DD disk wave address into the cartridge wavedata region. */
  if ((addr & 0xff000000) == 0xff000000) {
    addr &= 0xffffff;
    addr += DDROM_WAVEDATA_START;
  }

  /* Offset to where this sample landed inside the cached buffer. */
  ram_addr = buffer->ram_addr + (u32)addr - buffer->sample_addr;
  return (osVirtualToPhysical(ram_addr));
}

/* No-op stub retained at its original address. */
void func_8009DBA0(void) {}

/*
 * Cache lookup and admission for one sample region. Returns the buffer that
 * holds (or is now streaming in) [sample_addr, sample_addr+sample_length). The
 * active list stays sorted by sample_addr ascending: a covering buffer is a hit
 * and is refreshed; otherwise a free buffer is taken (or the least-wanted
 * active buffer is evicted), spliced in at the sorted position, and filled by a
 * fresh cartridge->RDRAM DMA. Returns NULL in RAM-playback mode, where no DMA
 * is needed.
 */
static dma_list_t* __MusIntDmaSample(unsigned long sample_addr,
                                     int sample_length) {
  dma_list_t* current_dma_buffer;
  dma_list_t* free_buffer;
  dma_list_t* last_buffer;
  unsigned long sample_addr_end;
  OSPiHandle* pi_handle;
  OSIoMesg* io_msg;

  if (__muscontrol_flag & MUSCONTROL_RAM) {
    return (NULL);
  }

  pi_handle = cartrom_handle;
  sample_addr_end = sample_addr + sample_length;
  last_buffer = NULL;

  /* Scan the sorted active list for a buffer that already covers the region. */
  for (current_dma_buffer = dma_buffer_head; current_dma_buffer;
       current_dma_buffer = current_dma_buffer->next) {
    if (sample_addr < current_dma_buffer->sample_addr) {
      /* Past the insertion point; no covering buffer exists. */
      break;
    }
    if (sample_addr_end <= current_dma_buffer->sample_addr + audio_dma_size) {
      /* Region covered: refresh recency and reuse. */
      current_dma_buffer->keep_count = 1 + FRAME_LAG;
      return (current_dma_buffer);
    }
    /* Remember the node the newcomer would follow in sorted order. */
    last_buffer = current_dma_buffer;
  }

  /* A new buffer is needed; prefer one off the free list. */
  free_buffer = dma_buffer_free;
  if (!free_buffer) {
    /* Free list empty: evict the active buffer with the lowest keep_count. */
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

    /* Evicting the follow-after node: advance the insertion anchor past it. */
    if (free_buffer == last_buffer) {
      last_buffer = free_buffer->prev;
    }

    /* Unlink the victim from the active list, fixing head if it was first. */
    if (free_buffer->next) {
      free_buffer->next->prev = free_buffer->prev;
    }
    if (free_buffer->prev) {
      free_buffer->prev->next = free_buffer->next;
    } else {
      dma_buffer_head = free_buffer->next;
    }

    /* Push the reclaimed node onto the free list. */
    free_buffer->prev = NULL;
    free_buffer->next = dma_buffer_free;
    dma_buffer_free = free_buffer;
    goto got_free_buffer;
  no_free_buffer:
    /* Nothing to reclaim: fall back to whatever heads the active list. */
    return (dma_buffer_head);
  }

got_free_buffer:
  dma_buffer_free = free_buffer->next;

  /* Splice the buffer into the active list at its sorted position. */
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

  /* Tag the buffer with the aligned region and mark it freshly used. */
  free_buffer->sample_addr = sample_addr & (~(SAMPLE_ALIGNMENT - 1));
  free_buffer->keep_count = 1 + FRAME_LAG;

  /* Start the cartridge->RDRAM transfer that fills the buffer. */
  io_msg = &audio_IO_mess_buf[audio_dma_count++];
  io_msg->hdr.pri = OS_MESG_PRI_NORMAL;
  io_msg->hdr.retQueue = &audDMAMessageQ;
  io_msg->dramAddr = free_buffer->ram_addr;
  io_msg->devAddr = free_buffer->sample_addr;
  io_msg->size = audio_dma_size;
  osEPiStartDma(pi_handle, io_msg, OS_READ);
  return (free_buffer);
}
