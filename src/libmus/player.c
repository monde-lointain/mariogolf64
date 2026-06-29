#include "common.h"

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_envelope);

INCLUDE_ASM("asm/nonmatchings/libmus/player", MusInitialize);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_set_master_volume);

INCLUDE_ASM("asm/nonmatchings/libmus/player", MusStartSong);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_start_song_from_marker);

INCLUDE_ASM("asm/nonmatchings/libmus/player", try_spawn_global_object);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_80099CF0);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_stop);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_ask);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_stop);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_ask);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_set_volume);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_set_pan);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_set_freq_offset);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_set_tempo);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_set_reverb);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_ptr_bank_initialize);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A2F0);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A318);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A33C);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_get_ptr_bank);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_pause);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_start_song);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_set_fx_type);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_set_song_fx_change);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_fx_bank_initialize);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A500);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A508);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A514);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A520);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A52C);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A534);

INCLUDE_ASM("asm/nonmatchings/libmus/player", MusSetScheduler);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_wave_count);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_wave_address);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A624);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A630);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A64C);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_fifo_dispatch);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_fifo_enqueue);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A7C8);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_player_frame_handler);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009AB18);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B060);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B0DC);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_set_volume_and_pan);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B254);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B360);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B3D0);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B5C4);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B5E0);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B698);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B6F0);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B754);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B818);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B92C);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_remap_ptr_bank);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009BC58);

INCLUDE_ASM("asm/nonmatchings/libmus/player", init_struct_defaults);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_alloc_channel);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009BFF0);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009C028);

INCLUDE_ASM("asm/nonmatchings/libmus/player", allocate_object_slot);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_start_song);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_set_flag);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_stop);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_wave);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_port_on);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_port_off);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_default_adsr);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_tempo);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_endit);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_cutoff);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_vibrato_up);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_vibrato_down);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_vibrato_off);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_length);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_ignore);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_transpose);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_ignore_transpose);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_distort);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_env_off);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_env_on);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_trigger_off);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_trigger_on);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_for);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_next);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_wobble);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_wobble_off);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_velocity_on);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_velocity_off);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_velocity);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_pan);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_stereo);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_drums_on);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_drums_off);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_print);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_goto);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_reverb);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_rand_note);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_rand_volume);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_rand_pan);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_volume);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_start_fx);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_bend_range);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_sweep);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_change_fx);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_marker);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_length0);
