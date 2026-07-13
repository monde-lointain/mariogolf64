#include "common.h"
#include <nusys.h>
#include <nualstl.h>

extern musHandle try_spawn_global_object(int number);
extern void gen_terrain_detail_texture(s32 arg0, s32 arg1);
extern void func_80407D64(void);

extern s32 get_lowest_height_at_position(void);
extern void func_8005B7BC(s32 arg0);
extern void func_80216B74(s32 arg0);

extern u32 g_bgm_song_buffer;
extern u8 g_terrain_tile_cull_exclude[];
extern u8 D_800C30A4;
extern s32 D_800FBE88;
extern s32 D_800B67C4;
extern const char D_800D1238[];
extern u32 D_800C2FF4;
extern const char D_800D1220[];
extern s32 D_800FBE68;
extern s8 g_terrain_vtx_xform_mode;
extern u8 D_800FEDD8[][8];
extern u8 D_80105F40[][8];
extern u16 D_80104F78;
extern s32 D_80104F7C;
extern s32 D_80104F80;
extern s32 D_80104F84;
extern s32 D_80104F88;
extern s32 D_80104F8C;
extern s32 D_80104F90;
extern s32 D_800C306C;
extern s32 D_801B6088;
extern s32 D_800C30A8;
extern s32 D_800C30AC;

void bgm_load_song_from_rom(u32 rom_addr, s32 size) {
  nuPiReadRom(rom_addr, (void*)g_bgm_song_buffer, size + (size & 1));
}

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_8005F0BC);

void spawn_object_simple(int number) { try_spawn_global_object(number); }

void func_8005F0FC(int number, int volume, int pan, int restartflag,
                   int priority) {
  MusStartEffect2(number, volume, pan, restartflag, priority);
}

void func_8005F11C(u32 rom_addr, s32 size) {
  s32 rounded = (size & 1) + size;

  if ((u32)rounded >= 0x4001) {
    osSyncPrintf(D_800D1220);
  }
  nuPiReadRom(rom_addr, (void*)D_800C2FF4, rounded);
  MusFxBankInitialize((void*)D_800C2FF4);
}

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_8005F180);

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_8005F1C8);

s32 func_8005F224(void) {
  s32 temp = D_800FBE88;

  if (temp >= 0x1F) {
    temp = 0x3C - temp;
  }
  return temp;
}

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_8005F248);

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_8005F290);

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_8005F30C);

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_8005F360);

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_8005F4AC);

void func_8005F518(s32 arg0, char* arg1) {
  sprintf(arg1, D_800D1238, arg0 + 1);
}

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom",
            init_per_player_state);

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_8005F838);

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_8005F964);

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_8005FB58);

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_8005FCB8);

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_8005FE8C);

void func_800600C0(s32 arg0) {
  u8(*rows)[8] = D_800FEDD8;
  u8* p = rows[arg0];

  D_80104F78 = *(u16*)(p + 0x44);
  D_80104F7C = *(s8*)(p + 0x46);
  D_80104F80 = *(s8*)(p + 0x47);
  D_80104F84 = *(s8*)(p + 0x48);
  D_80104F88 = *(s8*)(p + 0x49);
  D_80104F8C = *(s8*)(p + 0x4A);
  D_80104F90 = *(s8*)(p + 0x4B);
}

void func_80060128(s32 arg0) {
  u8(*rows)[8] = D_80105F40;
  u8* p = rows[arg0];

  D_80104F78 = *(u16*)(p + 0x40);
  D_80104F7C = *(s8*)(p + 0x42);
  D_80104F80 = *(s8*)(p + 0x43);
  D_80104F84 = *(s8*)(p + 0x44);
  D_80104F88 = *(s8*)(p + 0x45);
  D_80104F8C = *(s8*)(p + 0x46);
  D_80104F90 = *(s8*)(p + 0x47);
}

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_80060190);

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_80060210);

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_800602B4);

void func_8006034C(u8 arg0) { D_800C30A4 = arg0; }

void func_80060358(void) {
  D_800C306C = 0;
  func_8005B7BC(0);
  func_80216B74(0);
  D_801B6088 = 7;
  D_800C30A8 = 3;
  D_800C30AC = 9;
}

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_800603A8);

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_80060434);

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_800604F4);

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_800605EC);

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom",
            gen_terrain_detail_texture);

void setup_terrain_detail_default(void) {
  if (D_800B67C4 != 0) {
    func_80407D64();
  }
  gen_terrain_detail_texture(0x60, 0xBE);
}

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom",
            emit_per_phase_fog_state);

void func_800627F8(f64* src, f64* dst) {
  dst[0] = src[0];
  dst[1] = src[1];
}

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_8006280C);

void clear_terrain_emit_scratch(void) {
  bzero(g_terrain_tile_cull_exclude, 0x80);
}

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom",
            init_terrain_vertex_texcoords);

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom",
            emit_terrain_state_prefix_block);

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom",
            emit_course_terrain_dl);
