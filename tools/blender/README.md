# Custom characters

A custom character takes the place of one of the game's characters, such as
Charlie, and plays that character's animations. You model it in Blender on
the character's own skeleton with the Mario Golf Character add-on, export a
package, and the game's build turns the package into the character's model
files.

Each character has two models, and the package replaces both: the main
model, on the course and in the menus, and the reaction model, in the
close-up after each shot. They share a skeleton and the package's parts,
and differ in their animations, so you model once.

## What the game can draw

The game's characters are rigid, in the N64 manner: each vertex moves with
exactly one bone, and none blends between two. Every one of the game's 36
character models is built this way, so the add-on has no weight painting.
A character made of separate pieces, a head, an upper arm, a forearm and so
on, each kept whole, looks best; a continuous skin creases at the joints,
as the game can't stretch it.

Rather than have Blender guess weights, you put each face in its bone's
vertex group, then bind. Automatic weights would spread vertices across
bones, which the game can't draw, and each such guess would have to be
undone by hand; assigning faces is the choice the artist makes anyway, and
the add-on checks it at once.

Materials are drawn in one of three ways:

- **Color**: the material's Base Color, lit. This is the cheapest and suits
  most of a character.
- **Texture**: the Image Texture that feeds its Base Color, lit. An image
  can hold at most 2048 pixels, such as 64x32, and its width must be a
  multiple of 4. Transparent pixels cut holes; the build fills the colors
  under them from their neighbors so their edges don't darken.
- **Face**: images that the game's animations swap, as for blinking. Each
  of the template's face images needs one of yours in its place, at most
  64x64. An image mixed over a color by its alpha, through a Mix Shader, is
  drawn over that color, as Blender shows it.

**Alternate Colors** gives a material other colors in the three color sets
that a player picks with the C buttons.

How much room a character may take depends on its triangles, materials and
textures. The export reports the room each model uses, as in "Room used:
main 60%, reaction 88%", and says what to cut when there's too little.

## Setting up

1. Build the decompilation once (`make setup`, `make extract`), so that the
   add-on can read the characters' skeletons from the game's files.
2. In Blender 5.2 or later, build and install the add-on: run `blender
   --command extension build --source-dir tools/blender/mario_golf_character`
   and install the zip it writes with Edit > Preferences > Get Extensions >
   Install from Disk.
3. In Edit > Preferences > Add-ons > Mario Golf Character, set the
   **Decompilation Folder** to this repository.

## Making a character

The 3D View's sidebar (N) has a Mario Golf tab with the five steps.

1. **Rig.** Add Template Rig adds the skeleton of the character to replace,
   with its model as a wireframe guide. Its redo panel picks another
   character.
2. **Fit.** In Edit Mode, put each bone's head on your model's joint: the
   hip, knee, ankle and so on. Only the heads count, so point the bones
   wherever reads best to you. Longer or shorter bones change the
   character's proportions; the animations follow.
3. **Parts.** Add Bone Groups gives your meshes a vertex group per bone.
   Assign each face to one bone's group, then Bind to Rig. Binding refuses
   faces in no group or in two bones' groups, and selects them for you.
4. **Pose.** Match Template Pose stands your model as the template's guide
   does, which the game's animations start from. A model built in a T-pose
   drops its arms. Then, in Pose Mode, turn any part that still doesn't
   line up with the guide; feet, hands and the head often need it, as
   nothing tells which way they point. The game starts from the pose you
   see.
5. **Export.** Name the character, pick a Package Folder and Export
   Character. The export checks the package with the game's build tool and
   reports the room used, or what to fix.

Each material's Mario Golf panel, under Material Properties, sets how it's
drawn. For a face material, pick its Face Part, such as Eyes, then List Face
Images to see the template's images and pick yours in their place.

## Building the game with it

```sh
make -j CHARACTERS=path/to/package
```

builds `build/nonmatching/custom/mariogolf64.z64` with the character in
place of the one it replaces; `MODERN=1` builds it with the modern
toolchain, in `build/modern/custom`. `CHARACTERS` takes several packages,
one per character replaced. The ROM no longer matches the original, and
the original's build is left alone.

`tools/build_character.py --check PACKAGE` checks a package without
building the game, as the export does.

## Name and face pictures

The name you give at export replaces the character's name everywhere the
game shows it: menus, scorecards, the tournament's contestants and its
tournament, such as "Charlie Tournament". A name has at most 11 letters,
digits, spaces, periods and hyphens, which is what the menus' font has.

The menus and scorecards also show small pictures of the character's face:
one at rest, four pleased, four beaten and a head for the menus, each in
the four color sets. The build draws them from your model, with the camera,
pose, expression and light of the original's pictures, and writes them as
one sheet to see them by:

    build/nonmatching/custom/custom_characters/SLOT_face_sprites.png

where SLOT is the character replaced, such as `charlie`. A column is one
picture, a row one color set. To touch them up, paint over a copy of that
sheet and save it in the package folder as `face_sprites.png`; the build
then uses yours instead of drawing them. Keep its size and layout, and
its three kinds of pixel:

- **Clear**: the background.
- **Half clear**: the outline's soft edge, which the game blends into
  what's behind. Its color is darkened, as the original's is.
- **Opaque**: the character.

A pixel's palette entry is shared by the four color sets, so each spot
should change color the same way in every row. The pictures hold about 250
colors in all; the build picks the closest. Delete `face_sprites.png` to
go back to drawn pictures, for instance after changing the model.

The build draws pictures only for characters whose originals' cameras it
knows, Charlie's so far; others keep their original pictures.

## Testing the tools

`make tool-tests` tests the build tools, some against the game's own
files; `make blender-tests BLENDER=path/to/blender` tests the add-on in a
Blender without a window.
