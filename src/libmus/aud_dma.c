
/*************************************************************

  aud_dma.c : Nintendo 64 Music Tools Programmers Library
  (c) Copyright 1997/1998, Software Creations (Holdings) Ltd.

  Version 3.14

  Music library DMA buffer manager.

  MG64 rev: cart-only (the N64DD/diskrom path is stripped) and the DMA-buffer
  lifecycle is reworked -- __MusIntDmaInit persists dma_buffer_count to the
  g_mus_dma_buffer_count global, __MusIntDmaProcess ages the flat
dma_buffer_list[] array under an interrupt mask instead of the upstream
linked-list free-walk, and
  __MusIntDmaSample checks g_mus_control_flag first + keeps buffers ~forever
  (keep_count = 1+FRAME_LAG with FRAME_LAG redefined to 0x20000000).

**************************************************************/

/* include configuartion */
#include "libmus_config.h"

/* include system headers */
#include <ultra64.h>
#ifndef SUPPORT_NAUDIO
#include <libaudio.h>
#else
#include <n_libaudio_sc.h>
#include <n_libaudio_sn_sc.h>
#endif

/* include other header files */
#include "libmus.h"
#include "lib_memory.h"

/* include current header file */
#include "aud_dma.h"

/* internal macros */
#define SAMPLE_ALIGNMENT 2   /* must be power of two */
#define FRAME_LAG 0x20000000 /* MG64: buffers persist (stock value 1) */
#define DDROM_WAVEDATA_START \
  0x00140000 /* address of N64DD ROM samples - from <leo.h> */

/* MG64-curated audio control flag (upstream __muscontrol_flag) @ 0x80132368 */
extern u32 g_mus_control_flag;

/* internal data structures */
typedef struct dma_list_s {
  struct dma_list_s* prev;
  struct dma_list_s* next;
  int keep_count;
  unsigned long sample_addr;
  unsigned char* ram_addr;
} dma_list_t;

/* internal workspace (drop-static-mirror: the file-statics live at curated
   main_bss vrams, so the bodies' %hi/%lo bind to the ROM addresses; nothing is
   defined here) */
extern dma_list_t*
    dma_buffer_head; /* first used DMA buffer descriptor @ 0x800E70A0 */
extern dma_list_t*
    dma_buffer_free; /* first unused DMA buffer descriptor @ 0x800E70A4 */
extern dma_list_t*
    dma_buffer_list; /* DMA buffer descriptor list @ 0x800E70A8 */

/* dma message buffers */
extern OSIoMesg* audio_IO_mess_buf; /* @ 0x800E70AC */
extern OSMesg* audio_mess_buf;      /* @ 0x800E70B0 */

extern int audio_dma_size;  /* @ 0x800E70B4 */
extern int audio_dma_count; /* @ 0x800E70B8 */

/* Queues and storage for use with audio DMA's */
extern OSMesgQueue audDMAMessageQ; /* @ 0x800E70C0 */

/* EPI handle for cart ROM access */
extern OSPiHandle* cartrom_handle; /* @ 0x800E70D8 */

/* MG64: persisted DMA buffer count (read by __MusIntDmaProcess) @ 0x800C7AC0 */
extern int g_mus_dma_buffer_count;

/* internal function prototypes */
static ALDMAproc __CallBackDmaNew(void* ignored);
static s32 __CallBackDmaProcess(s32 addr, s32 len, void* ignored);
static dma_list_t* __MusIntDmaSample(unsigned long sample_addr,
                                     int sample_length);

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
  [EXTERNAL FUNCTION]
  __MusIntDmaInit(dma_buffer_count, dma_buffer_size)

  [Explanation]
  Initialise DMA buffer manager.

  [Return value]
  Address of audio library DMA initialisation function
*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

ALDMANew __MusIntDmaInit(int dma_buffer_count, int dma_buffer_size) {
  int i;

  /* get ROM access handle */
  cartrom_handle = osCartRomInit();

  /* allocate memory for DMA messages */
  audio_IO_mess_buf =
      __MusIntMemMalloc(dma_buffer_count * 2 * sizeof(OSIoMesg));
  audio_mess_buf = __MusIntMemMalloc(dma_buffer_count * 2 * sizeof(OSMesg));
  /* allocate memory for DMA buffer descriptor list */
  dma_buffer_list = __MusIntMemMalloc(dma_buffer_count * sizeof(dma_list_t));
  __MusIntMemSet(dma_buffer_list, 0, dma_buffer_count * sizeof(dma_list_t));

  /* MG64: persist the buffer count for the per-frame ageing pass */
  g_mus_dma_buffer_count = dma_buffer_count;

  /* initialise descriptor list and allocate DMA buffers */
  for (i = 0; i < dma_buffer_count - 1; i++) {
    dma_buffer_list[i].next = dma_buffer_list + i + 1;
    dma_buffer_list[i + 1].prev = dma_buffer_list + i;
    dma_buffer_list[i].ram_addr = __MusIntMemMalloc(dma_buffer_size);
    dma_buffer_list[i].sample_addr = 0xffffffff;
  }
  /* allocate last DMA buffer */
  dma_buffer_list[i].ram_addr = __MusIntMemMalloc(dma_buffer_size);
  dma_buffer_list[i].sample_addr = 0xffffffff;

  /* set working vars */
  audio_dma_size = dma_buffer_size;
  audio_dma_count = 0;
  dma_buffer_head = NULL;
  dma_buffer_free = dma_buffer_list;

  /* create DMA message queue */
  osCreateMesgQueue(&audDMAMessageQ, audio_mess_buf, dma_buffer_count * 2);

  /* return address of audio library DMA new function */
  return (__CallBackDmaNew);
}

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
  [EXTERNAL FUNCTION]
  __MusIntDmaProcess()

  [Explanation]
  Process the DMA buffers. The DMA message queue is emptied, then every DMA
buffer in the flat descriptor list is aged (MG64 rework: a flat-array keep_count
ageing pass under an interrupt mask, replacing the upstream linked-list
free-walk).

  [Return value]
  NONE
*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

void __MusIntDmaProcess(void) {
  OSIoMesg* iomsg;
  int i;
  OSIntMask prev_mask;

  /* empty message queue */
  while (audio_dma_count) {
    osRecvMesg(&audDMAMessageQ, (OSMesg*)&iomsg, OS_MESG_NOBLOCK);
    audio_dma_count--;
  }
  /* age every DMA buffer under interrupt protection */
  prev_mask = osSetIntMask(OS_IM_NONE);
  for (i = 0; i < g_mus_dma_buffer_count; i++) {
    if (dma_buffer_list[i].keep_count) dma_buffer_list[i].keep_count--;
  }
  osSetIntMask(prev_mask);
}

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
  [CALLBACK FUNCTION]
  __CallBackDmaNew(ignored)

  [Explanation]
  Audio library DMA processing initialisation function which simply returns the
  address of the normal DMA callback function.

  [Return value]
  Address of audio library DMA processing function
*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

static ALDMAproc __CallBackDmaNew(void* ignored) {
  return (__CallBackDmaProcess);
}

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
  [CALLBACK FUNCTION]
  __CallBackDmaProcess(addr, len, ignored)

  [Explanation]
  Audio library DMA processing function. Find the requested sample in RAM and
return its address.

  [Return value]
  RAM address of sample
*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

static s32 __CallBackDmaProcess(s32 addr, s32 len, void* ignored) {
  dma_list_t* buffer;
  unsigned char* ram_addr;

  buffer = __MusIntDmaSample(addr, len);
  if (!buffer) return (osVirtualToPhysical((void*)addr));

  if ((addr & 0xff000000) == 0xff000000) {
    addr &= 0xffffff;
    addr += DDROM_WAVEDATA_START;
  }

  ram_addr = buffer->ram_addr + (u32)addr - buffer->sample_addr;
  return (osVirtualToPhysical(ram_addr));
}

void func_8009DBA0(void) {}

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
  [INTERNAL FUNCTION]
  __MusIntDmaSample(sample_addr, sample_length)

  [Explanation]
  Find requested sample in current DMA buffer or download requested sample into
a new DMA buffer. MG64 rev: cart-only (the N64DD path is stripped) and the
RAM-only control-flag test is checked first.

  [Return value]
  Address of dma buffer containing sample
*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

static dma_list_t* __MusIntDmaSample(unsigned long sample_addr,
                                     int sample_length) {
  dma_list_t *current_dma_buffer, *free_buffer, *last_buffer;
  unsigned long sample_addr_end;
  OSPiHandle* pi_handle;
  OSIoMesg* io_msg;

  /* MG64 cart-only: no N64DD path, RAM-only test first */
  if (g_mus_control_flag & MUSCONTROL_RAM)
    return (NULL);
  else
    pi_handle = cartrom_handle;

  /* get address of sample end */
  sample_addr_end = sample_addr + sample_length;
  /* find sample if already downloaded */
  last_buffer = NULL;
  for (current_dma_buffer = dma_buffer_head; current_dma_buffer;
       current_dma_buffer = current_dma_buffer->next) {
    /* have we gone past it? */
    if (sample_addr < current_dma_buffer->sample_addr) break;
    /* is it contained in this buffer? */
    if (sample_addr_end <= current_dma_buffer->sample_addr + audio_dma_size) {
      current_dma_buffer->keep_count = 1 + FRAME_LAG;
      return (current_dma_buffer);
    }

    last_buffer = current_dma_buffer;
  }
  /* get next free buffer */
  free_buffer = dma_buffer_free;
  if (!free_buffer) {
    /* MG64: no free buffer - evict the used buffer with the lowest keep_count
     */
    free_buffer = dma_buffer_head;
    if (!free_buffer) goto no_free_buffer;
    for (current_dma_buffer = dma_buffer_head; current_dma_buffer;
         current_dma_buffer = current_dma_buffer->next) {
      if (free_buffer->keep_count > current_dma_buffer->keep_count)
        free_buffer = current_dma_buffer;
    }
    if (!free_buffer) goto no_free_buffer;
    /* if we are evicting last_buffer, step it back */
    if (free_buffer == last_buffer) last_buffer = free_buffer->prev;
    /* unlink the victim from the used list */
    if (free_buffer->next) free_buffer->next->prev = free_buffer->prev;
    if (free_buffer->prev)
      free_buffer->prev->next = free_buffer->next;
    else
      dma_buffer_head = free_buffer->next;
    /* push the victim onto the free list */
    free_buffer->prev = NULL;
    free_buffer->next = dma_buffer_free;
    dma_buffer_free = free_buffer;
    goto got_free_buffer;
  no_free_buffer:
    return (dma_buffer_head);
  }
got_free_buffer:
  /* free buffer is to be used so move it along */
  dma_buffer_free = free_buffer->next;

  /* insert free buffer into list */
  if (last_buffer) {
    /* link free with next and fix next */
    free_buffer->next = last_buffer->next;
    if (free_buffer->next) free_buffer->next->prev = free_buffer;
    /* line free with last and fix last */
    free_buffer->prev = last_buffer;
    last_buffer->next = free_buffer;
  } else {
    free_buffer->next = dma_buffer_head;
    free_buffer->prev = NULL;
    if (dma_buffer_head) dma_buffer_head->prev = free_buffer;
    dma_buffer_head = free_buffer;
  }

  /* DMA sample to buffer */
  free_buffer->sample_addr = sample_addr & (~(SAMPLE_ALIGNMENT - 1));
  free_buffer->keep_count = 1 + FRAME_LAG;

  io_msg = &audio_IO_mess_buf[audio_dma_count++];
  io_msg->hdr.pri = OS_MESG_PRI_NORMAL;
  io_msg->hdr.retQueue = &audDMAMessageQ;
  io_msg->dramAddr = free_buffer->ram_addr;
  io_msg->devAddr = free_buffer->sample_addr;
  io_msg->size = audio_dma_size;
  osEPiStartDma(pi_handle, io_msg, OS_READ);
  /* return dma buffer address */
  return (free_buffer);
}

/* end of file */
