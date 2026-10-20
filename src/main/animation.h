#ifndef ANIMATION_H
#define ANIMATION_H

#include "common.h"
#include "vector.h"

typedef struct CharacterModelResource CharacterModelResource;
typedef struct ModelAttachmentPoint ModelAttachmentPoint;
typedef struct KeyframeTrack KeyframeTrack;
typedef struct TextureDesc TextureDesc;
typedef struct AnimTexOrder AnimTexOrder;
typedef struct TexturedFace TexturedFace;
typedef struct TexKeyframe TexKeyframe;
typedef struct FaceTexAnim FaceTexAnim;
typedef struct KfEntry KfEntry;

/* One of 0x20 per-character texture-animation slots. */
typedef struct {
  s32 frame;                            /* 0x00 */
  u8 state;                             /* 0x04 */
  u8 reserved_05[3];                    /* 0x05 */
} TextureAnimSlot;

/* Recovered prefix of the model resource, through offset 0x2C. */
struct CharacterModelResource {
  u32 rom_start;                        /* 0x00 */
  u32 rom_end;                          /* 0x04 */
  u8 unknown_08[4];                     /* 0x08 */
  u32* display_list;                    /* 0x0C */
  s32* animation_display_lists;         /* 0x10; -1 ends available entries */
  KeyframeTrack* keyframe_tracks;       /* 0x14 */
  ModelAttachmentPoint* attachment_points; /* 0x18 */
  u8 unknown_1C[4];                     /* 0x1C */
  AnimTexOrder* texture_animation_order; /* 0x20 */
  TexturedFace** textured_face_table;   /* 0x24 */
  TextureDesc* texture_table;           /* 0x28 */
};

/* A tagged model attachment point; matrix_indices ends with -1. */
struct ModelAttachmentPoint {
  s16* matrix_indices;                  /* 0x00 */
  Vec3f position;                       /* 0x04; model-local attachment offset */
  s16 tag;                              /* 0x10 */
  u8 unknown_12[2];                     /* 0x12 */
};

/* Runtime state returned by get_character_state; records have 0x18C stride.
 * polychara_state and D_801F4424 are interior symbols at +0x08 and +0x34. */
typedef struct {
  CharacterModelResource* model_resource; /* 0x00 */
  void* segment_data;                   /* 0x04; polychara_buf */
  s32 resource_index;                   /* 0x08; -1 means unloaded */
  s32 keyframe_track_index;             /* 0x0C */
  f32 current_frame;                    /* 0x10 */
  s32 start_frame;                      /* 0x14 */
  s32 end_frame;                        /* 0x18; start_frame + frame_span */
  s32 keyframe_track_count;             /* 0x1C */
  u32 segment_bases[3];                 /* 0x20; polychara_seg_ptr is 0x0C */
  Mtx* bone_matrices;                   /* 0x2C; indexed by attachment matrix_indices */
  s32 segment_index;                    /* 0x30 */
  s32 last_update_frame;                /* 0x34; D_801F4424 */
  Vec3f position;                      /* 0x38; game-space coordinates */
  Vec3f rotation;                      /* 0x44; radians, heading is y */
  Vec3f scale;                         /* 0x50 */
  f32 frame_step;                       /* 0x5C; added once per animation update */
  u8 unknown_60;                        /* 0x60 */
  u8 unknown_61;                        /* 0x61 */
  u8 unknown_62[0x20];                  /* 0x62 */
  u8 unknown_82;                        /* 0x82 */
  s8 loop_enabled;                      /* 0x83; permits the loop-marker event */
  s8 next_animation_id;                 /* 0x84; -1 means no queued animation */
  s8 auto_slot_zero_enabled;            /* 0x85 */
  s8 auto_slot_zero_delay;              /* 0x86; updates until slot zero activates */
  u8 unknown_87;                        /* 0x87 */
  TextureAnimSlot texture_slots[0x20];  /* 0x88 */
  u8 unknown_188[4];                    /* 0x188 */
} CharacterState;

struct KeyframeTrack {
  KfEntry* entries;                     /* 0x00 */
  s16 start_frame;                      /* 0x04 */
  s16 frame_span;                       /* 0x06; end_frame - start_frame */
  u8 animation_id;                      /* 0x08 */
  u8 unknown_09[3];                     /* 0x09; stride is 0x0C */
};

struct KfEntry {
  s16 value;                            /* 0x00: signed keyframe offset */
  u8 tag;                               /* 0x02 */
  u8 reserved_03;                       /* 0x03 */
};

struct TextureDesc {
  void* image;                          /* 0x00 */
  u8 width;                             /* 0x04 */
  u8 height;                            /* 0x05 */
  u8 reserved_06[2];                    /* 0x06 */
};

struct TexKeyframe {
  s16 threshold;                        /* 0x00 */
  s16 texture_index;                    /* 0x02 */
  u16 tex_s[3];                         /* 0x04 */
  u16 tex_t[3];                         /* 0x0A */
};

struct FaceTexAnim {
  s16 keyframe_count;                   /* 0x00 */
  s16 slot_index;                       /* 0x02 */
  TexKeyframe* keyframes;               /* 0x04 */
};

struct AnimTexOrder {
  s16 slot_index;                       /* 0x00; negative terminates */
  s16 frame_length;                     /* 0x02 */
};

struct TexturedFace {
  u32 vertices;                         /* 0x00: segmented address */
  u32 display_list;                     /* 0x04: segmented address */
  FaceTexAnim* animations;              /* 0x08 */
  s16 animation_count;                  /* 0x0C */
  s16 texture_index;                    /* 0x0E */
  u16 tex_s[3];                         /* 0x10 */
  u16 tex_t[3];                         /* 0x16 */
};

extern CharacterState D_801F43F0[];
CharacterState* get_character_state(s32 id);

s32 func_8005483C(s32 character_index, s32 point_index, Vec3f* out);

#endif /* ANIMATION_H */
