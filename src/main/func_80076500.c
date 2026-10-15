#include "common.h"
#include "vector.h"

extern Vec3f vector_reference_first;
extern Vec3f vector_reference_second;
extern Vec3f vector_position_buffer[];
extern f32 vector_value_buffer[];
extern s32 vector_buffer_count;
extern u16 D_800FBDA6[];
extern u8 VECTOR_POSITION_FORMAT[];

void copy_reference_vectors(Vec3f* first_destination,
                            Vec3f* second_destination) {
  first_destination->x = vector_reference_first.x;
  first_destination->y = vector_reference_first.y;
  first_destination->z = vector_reference_first.z;
  second_destination->x = vector_reference_second.x;
  second_destination->y = vector_reference_second.y;
  second_destination->z = vector_reference_second.z;
}

void reset_vector_samples(void) { vector_buffer_count = 0; }

void append_vector_sample(Vec3f* position, f32 value) {
  s32 point_index = vector_buffer_count;

  if (point_index < 8) {
    vector_position_buffer[point_index].x = position->x;
    vector_position_buffer[point_index].y = position->y;
    vector_position_buffer[point_index].z = position->z;
    vector_value_buffer[point_index] = value;
    if (D_800FBDA6[0] & 4) {
      osSyncPrintf((const char*)VECTOR_POSITION_FORMAT,
                   (f64)vector_position_buffer[point_index].x,
                   (f64)vector_position_buffer[point_index].y,
                   (f64)vector_position_buffer[point_index].z);
    }
    vector_buffer_count = vector_buffer_count + 1;
  }
}
