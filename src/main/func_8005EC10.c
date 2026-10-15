#include "common.h"
#include <nusys.h>
#include <nualstl.h>

extern musConfig default_audio_config;
extern u8* audio_wave_bank_data;
extern const char WAVE_BANK_LOAD_FORMAT[];
extern const char WAVE_BANK_BYTE_FORMAT[];
extern const char WAVE_BANK_LINE_SEPARATOR[];

extern void* __MusIntMemMalloc(s32 size);
extern void bgm_alloc_song_buffer(s32 size);
extern void func_8005F0BC(s32 size);

void handle_audio_scheduler_message(NUScMsg message_type, u32 frame_count);

void set_audio_pre_nmi_callback(NUAuPreNMIFunc callback) {
  OSIntMask previous_mask = osSetIntMask(OS_IM_NONE);

  nuAuPreNMIFunc = callback;
  osSetIntMask(previous_mask);
}

void handle_audio_scheduler_message(NUScMsg message_type, u32 frame_count) {
  if (message_type != NU_SC_RETRACE_MSG) {
    if (message_type == NU_SC_PRENMI_MSG) {
      MusStop(1, 0);
      MusSetFxType(0);
    }
    return;
  }

  if (frame_count == 20) {
    MusSetFxType(0);
  }

  if (frame_count == 28) {
    nuAuTaskStop = NU_AU_TASK_STOP;
  }
}

void load_audio_wave_bank(u32 rom_address, s32 size, void* wave_bank) {
  u32 byte_index;

  audio_wave_bank_data = __MusIntMemMalloc(size);
  nuPiReadRom(rom_address, audio_wave_bank_data, size);
  osSyncPrintf(WAVE_BANK_LOAD_FORMAT, audio_wave_bank_data, size);

  for (byte_index = 0; byte_index < 0x40; byte_index++) {
    osSyncPrintf(WAVE_BANK_BYTE_FORMAT, audio_wave_bank_data[byte_index]);
    if ((byte_index & 7) == 7) {
      osSyncPrintf(WAVE_BANK_LINE_SEPARATOR);
    }
  }

  MusPtrBankInitialize(audio_wave_bank_data, wave_bank);
}

void audio_system_boot(musConfig* config) {
  if (config == NULL) {
    config = &default_audio_config;
  }

  nuAuStlMgrInit(config);
  bgm_alloc_song_buffer(0xC000);
  func_8005F0BC(0x4000);
  set_audio_pre_nmi_callback(handle_audio_scheduler_message);
}
