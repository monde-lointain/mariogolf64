/*
 * Each character's name: as it's written, as the menus spell it, and as a
 * tournament contestant. The menus' spellings add LOWERCASE_CODE_OFFSET to
 * each lowercase letter, and a contestant's name joins its words with an
 * underscore. Where a capital that is also a hex digit follows an escape,
 * which would otherwise absorb it, a name breaks into two strings.
 *
 * A custom build's characters can rename the ones they replace, which
 * tools/build_character.py writes to custom_characters/names.h.
 */

#ifndef CHARACTER_NAMES_H
#define CHARACTER_NAMES_H

#ifdef CUSTOM_CHARACTERS
#    include "custom_characters/names.h"
#endif

#ifndef PLUM_NAME
#    define PLUM_NAME "Plum"
#    define PLUM_MENU_NAME "P\xEC\xF5\xED"
#    define PLUM_CONTESTANT_NAME "P\xEC\xF5\xED"
#endif
#ifndef CHARLIE_NAME
#    define CHARLIE_NAME "Charlie"
#    define CHARLIE_MENU_NAME "C\xE8\xE1\xF2\xEC\xE9\xE5"
#    define CHARLIE_CONTESTANT_NAME "C\xE8\xE1\xF2\xEC\xE9\xE5"
#endif
#ifndef MAPLE_NAME
#    define MAPLE_NAME "Maple"
#    define MAPLE_MENU_NAME "M\xE1\xF0\xEC\xE5"
#    define MAPLE_CONTESTANT_NAME "M\xE1\xF0\xEC\xE5"
#endif
#ifndef BABY_MARIO_NAME
#    define BABY_MARIO_NAME "Baby Mario"
#    define BABY_MARIO_MENU_NAME "B\xE1\xE2\xF9 \xED\xE1\xF2\xE9\xEF"
#    define BABY_MARIO_CONTESTANT_NAME "B\xE1\xE2\xF9_M\xE1\xF2\xE9\xEF"
#endif
#ifndef LUIGI_NAME
#    define LUIGI_NAME "Luigi"
#    define LUIGI_MENU_NAME "L\xF5\xE9\xE7\xE9"
#    define LUIGI_CONTESTANT_NAME "L\xF5\xE9\xE7\xE9"
#endif
#ifndef PEACH_NAME
#    define PEACH_NAME "Peach"
#    define PEACH_MENU_NAME "P\xE5\xE1\xE3\xE8"
#    define PEACH_CONTESTANT_NAME "P\xE5\xE1\xE3\xE8"
#endif
#ifndef SONNY_NAME
#    define SONNY_NAME "Sonny"
#    define SONNY_MENU_NAME "S\xEF\xEE\xEE\xF9"
#    define SONNY_CONTESTANT_NAME "S\xEF\xEE\xEE\xF9"
#endif
#ifndef WARIO_NAME
#    define WARIO_NAME "Wario"
#    define WARIO_MENU_NAME "W\xE1\xF2\xE9\xEF"
#    define WARIO_CONTESTANT_NAME "W\xE1\xF2\xE9\xEF"
#endif
#ifndef HARRY_NAME
#    define HARRY_NAME "Harry"
#    define HARRY_MENU_NAME "H\xE1\xF2\xF2\xF9"
#    define HARRY_CONTESTANT_NAME "H\xE1\xF2\xF2\xF9"
#endif
#ifndef MARIO_NAME
#    define MARIO_NAME "Mario"
#    define MARIO_MENU_NAME "M\xE1\xF2\xE9\xEF"
#    define MARIO_CONTESTANT_NAME "M\xE1\xF2\xE9\xEF"
#endif
#ifndef YOSHI_NAME
#    define YOSHI_NAME "Yoshi"
#    define YOSHI_MENU_NAME "Y\xEF\xF3\xE8\xE9"
#    define YOSHI_CONTESTANT_NAME "Y\xEF\xF3\xE8\xE9"
#endif
#ifndef DONKEY_KONG_NAME
#    define DONKEY_KONG_NAME "DK"
#    define DONKEY_KONG_MENU_NAME "DK"
#    define DONKEY_KONG_CONTESTANT_NAME "DK"
#endif
#ifndef BOWSER_NAME
#    define BOWSER_NAME "Bowser"
#    define BOWSER_MENU_NAME "B\xEF\xF7\xF3\xE5\xF2"
#    define BOWSER_CONTESTANT_NAME "B\xEF\xF7\xF3\xE5\xF2"
#endif
#ifndef METAL_MARIO_NAME
#    define METAL_MARIO_NAME "Metal Mario"
#    define METAL_MARIO_MENU_NAME "M\xE5\xF4\xE1\xEC \xED\xE1\xF2\xE9\xEF"
#    define METAL_MARIO_CONTESTANT_NAME "M\xE5\xF4\xE1\xEC_M\xE1\xF2\xE9\xEF"
#endif
#ifndef KID_NAME
#    define KID_NAME "Kid"
#    define KID_MENU_NAME "K\xE9\xE4"
#    define KID_CONTESTANT_NAME "K\xE9\xE4"
#endif
#ifndef SHERRY_NAME
#    define SHERRY_NAME "Sherry"
#    define SHERRY_MENU_NAME "S\xE8\xE5\xF2\xF2\xF9"
#    define SHERRY_CONTESTANT_NAME "S\xE8\xE5\xF2\xF2\xF9"
#endif
#ifndef AZALEA_NAME
#    define AZALEA_NAME "Azalea"
#    define AZALEA_MENU_NAME "A\xFA\xE1\xEC\xE5\xE1"
#    define AZALEA_CONTESTANT_NAME "A\xFA\xE1\xEC\xE5\xE1"
#endif
#ifndef JOE_NAME
#    define JOE_NAME "Joe"
#    define JOE_MENU_NAME "J\xEF\xE5"
#    define JOE_CONTESTANT_NAME "J\xEF\xE5"
#endif

#endif
