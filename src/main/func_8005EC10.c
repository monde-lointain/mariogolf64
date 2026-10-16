#include "common.h"
#include <nusys.h>
#include <nualstl.h>

extern musConfig default_audio_config;
extern u8* audio_wave_bank_data;
extern const char WAVE_BANK_LOAD_FORMAT[];
extern const char WAVE_BANK_BYTE_FORMAT[];
extern const char WAVE_BANK_LINE_SEPARATOR[];

extern void* __MusIntMemMalloc(s32 allocation_size);
extern void bgm_allocate_song_buffer(s32 song_buffer_size);
extern void allocate_sound_effect_bank_buffer(s32 sound_effect_bank_buffer_size);

enum {
  AUDIO_PRE_NMI_FX_STOP_FRAME = 20,
  AUDIO_PRE_NMI_TASK_STOP_FRAME = 28,
  AUDIO_WAVE_BANK_LOG_BYTE_COUNT = 0x40,
  AUDIO_WAVE_BANK_LOG_LINE_BYTE_MASK = 7,
  AUDIO_SONG_BUFFER_SIZE = 0xC000,
  AUDIO_SOUND_EFFECT_BANK_BUFFER_SIZE = 0x4000
};

void handle_audio_scheduler_message(NUScMsg scheduler_message_type, u32 frame_count);

void set_audio_pre_nmi_callback(NUAuPreNMIFunc pre_nmi_callback) {
  OSIntMask previous_interrupt_mask = osSetIntMask(OS_IM_NONE);

  nuAuPreNMIFunc = pre_nmi_callback;
  osSetIntMask(previous_interrupt_mask);
}

void handle_audio_scheduler_message(NUScMsg scheduler_message_type, u32 frame_count) {
  if (scheduler_message_type != NU_SC_RETRACE_MSG) {
    if (scheduler_message_type == NU_SC_PRENMI_MSG) {
      MusStop(MUSFLAG_EFFECTS, 0);
      MusSetFxType(0);
    }
    return;
  }

  if (frame_count == AUDIO_PRE_NMI_FX_STOP_FRAME) {
    MusSetFxType(0);
  }

  if (frame_count == AUDIO_PRE_NMI_TASK_STOP_FRAME) {
    nuAuTaskStop = NU_AU_TASK_STOP;
  }
}

void load_audio_wave_bank(u32 wave_bank_rom_address, s32 wave_bank_size, void* wave_bank_table) {
  u32 wave_bank_log_index;

  audio_wave_bank_data = __MusIntMemMalloc(wave_bank_size);
  nuPiReadRom(wave_bank_rom_address, audio_wave_bank_data, wave_bank_size);
  osSyncPrintf(WAVE_BANK_LOAD_FORMAT, audio_wave_bank_data, wave_bank_size);

  for (wave_bank_log_index = 0;
       wave_bank_log_index < AUDIO_WAVE_BANK_LOG_BYTE_COUNT;
       wave_bank_log_index++) {
    osSyncPrintf(WAVE_BANK_BYTE_FORMAT, audio_wave_bank_data[wave_bank_log_index]);
    if ((wave_bank_log_index & AUDIO_WAVE_BANK_LOG_LINE_BYTE_MASK) ==
        AUDIO_WAVE_BANK_LOG_LINE_BYTE_MASK) {
      osSyncPrintf(WAVE_BANK_LINE_SEPARATOR);
    }
  }

  MusPtrBankInitialize(audio_wave_bank_data, wave_bank_table);
}

void audio_system_boot(musConfig* requested_config) {
  musConfig* audio_config = requested_config;

  if (audio_config == NULL) {
    audio_config = &default_audio_config;
  }

  nuAuStlMgrInit(audio_config);
  bgm_allocate_song_buffer(AUDIO_SONG_BUFFER_SIZE);
  allocate_sound_effect_bank_buffer(AUDIO_SOUND_EFFECT_BANK_BUFFER_SIZE);
  set_audio_pre_nmi_callback(handle_audio_scheduler_message);
}
