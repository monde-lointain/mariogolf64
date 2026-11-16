/*
 * The port's graphics utilities: the transforms that libultra's gu
 * routines build, written for the host.
 *
 * Matrices follow the N64's convention: a point is a row vector multiplied
 * on the left, so a matrix's rows are the images of the axes and its last
 * row the translation. Angles are in degrees. The fixed-point forms go
 * through the game's own guMtxF2L, which clamps each element as the
 * game's code expects.
 */

#include <math.h>
#include <string.h>
#include <ultra64.h>

#define DEGREES_TO_RADIANS (3.14159265358979323846f / 180.0f)

// Below this length, the sum of a light's direction and the view's is too
// short to say where its highlight lies
#define HIGHLIGHT_LENGTH_MIN 0.1f

typedef struct Vector {
    f32 x, y, z;
} Vector;

// The axes of a camera's view: right and up across the screen, and back,
// out of the screen towards the eye
typedef struct ViewBasis {
    Vector right;
    Vector up;
    Vector back;
} ViewBasis;

static Vector MakeVector(f32 x, f32 y, f32 z) {
    Vector v = {x, y, z};

    return v;
}

static f32 Dot(Vector a, Vector b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vector Cross(Vector a, Vector b) {
    return MakeVector(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

static Vector Scale(Vector v, f32 factor) {
    return MakeVector(v.x * factor, v.y * factor, v.z * factor);
}

static Vector Add(Vector a, Vector b) {
    return MakeVector(a.x + b.x, a.y + b.y, a.z + b.z);
}

static f32 Length(Vector v) {
    return sqrtf(Dot(v, v));
}

static Vector Normalize(Vector v) {
    return Scale(v, 1.0f / Length(v));
}

static void SetRow(f32 mf[4][4], int row, Vector v, f32 w) {
    mf[row][0] = v.x;
    mf[row][1] = v.y;
    mf[row][2] = v.z;
    mf[row][3] = w;
}

static void SetColumn(f32 mf[4][4], int column, Vector v, f32 last) {
    mf[0][column] = v.x;
    mf[1][column] = v.y;
    mf[2][column] = v.z;
    mf[3][column] = last;
}

// The view's axes for an eye at eye looking at at, with up as near to up
// as it can be while square to the view
static ViewBasis ComputeViewBasis(Vector eye, Vector at, Vector up) {
    ViewBasis basis;

    basis.back = Normalize(MakeVector(eye.x - at.x, eye.y - at.y, eye.z - at.z));
    basis.right = Normalize(Cross(up, basis.back));
    basis.up = Normalize(Cross(basis.back, basis.right));
    return basis;
}

// The matrix that takes the world into the view: the basis's axes as its
// columns, after moving the eye to the origin
static void SetViewMatrix(f32 mf[4][4], const ViewBasis *basis, Vector eye) {
    SetColumn(mf, 0, basis->right, -Dot(eye, basis->right));
    SetColumn(mf, 1, basis->up, -Dot(eye, basis->up));
    SetColumn(mf, 2, basis->back, -Dot(eye, basis->back));
    SetColumn(mf, 3, MakeVector(0.0f, 0.0f, 0.0f), 1.0f);
}

// The reflection map's directions: the view's right and up, which the RSP
// projects normals onto for the texture's s and t
static void SetReflectionLookAt(LookAt *lookAt, const ViewBasis *basis) {
    Light_t *sAxis = &lookAt->l[0].l;
    Light_t *tAxis = &lookAt->l[1].l;

    memset(lookAt, 0, sizeof(*lookAt));
    sAxis->dir[0] = (s8)FTOFRAC8(basis->right.x);
    sAxis->dir[1] = (s8)FTOFRAC8(basis->right.y);
    sAxis->dir[2] = (s8)FTOFRAC8(basis->right.z);
    tAxis->dir[0] = (s8)FTOFRAC8(basis->up.x);
    tAxis->dir[1] = (s8)FTOFRAC8(basis->up.y);
    tAxis->dir[2] = (s8)FTOFRAC8(basis->up.z);
    // The microcode reads a nonzero color as the t axis's mark
    tAxis->col[1] = 0x80;
    tAxis->colc[1] = 0x80;
}

// Where a light's highlight falls in a highlight texture of the size, in
// the tile coordinates' 10.2 fixed point: the direction half way between
// the light's and the view's back axis, projected onto the screen, about
// the texture's middle
static void PlaceHighlight(
    const ViewBasis *basis,
    Vector light,
    int width,
    int height,
    int *x,
    int *y
) {
    Vector halfway = Add(Normalize(light), basis->back);
    f32 length = Length(halfway);

    if (length <= HIGHLIGHT_LENGTH_MIN) {
        *x = width * 2;
        *y = height * 2;
        return;
    }

    halfway = Scale(halfway, 1.0f / length);
    *x = (int)(width * 4 + Dot(halfway, basis->right) * width * 2);
    *y = (int)(height * 4 + Dot(halfway, basis->up) * height * 2);
}

void guTranslateF(f32 mf[4][4], f32 x, f32 y, f32 z) {
    guMtxIdentF(mf);
    mf[3][0] = x;
    mf[3][1] = y;
    mf[3][2] = z;
}

void guTranslate(Mtx *m, f32 x, f32 y, f32 z) {
    f32 mf[4][4];

    guTranslateF(mf, x, y, z);
    guMtxF2L(mf, m);
}

void guScaleF(f32 mf[4][4], f32 x, f32 y, f32 z) {
    guMtxIdentF(mf);
    mf[0][0] = x;
    mf[1][1] = y;
    mf[2][2] = z;
}

void guScale(Mtx *m, f32 x, f32 y, f32 z) {
    f32 mf[4][4];

    guScaleF(mf, x, y, z);
    guMtxF2L(mf, m);
}

// Rotates by angle about the axis (x, y, z), counterclockwise as seen from
// the axis's tip, by Rodrigues' formula
void guRotateF(f32 mf[4][4], f32 angle, f32 x, f32 y, f32 z) {
    f32 radians = angle * DEGREES_TO_RADIANS;
    f32 s = sinf(radians);
    f32 c = cosf(radians);
    f32 t = 1.0f - c;

    guNormalize(&x, &y, &z);
    SetRow(mf, 0, MakeVector(t * x * x + c, t * x * y + s * z, t * x * z - s * y), 0.0f);
    SetRow(mf, 1, MakeVector(t * x * y - s * z, t * y * y + c, t * y * z + s * x), 0.0f);
    SetRow(mf, 2, MakeVector(t * x * z + s * y, t * y * z - s * x, t * z * z + c), 0.0f);
    SetRow(mf, 3, MakeVector(0.0f, 0.0f, 0.0f), 1.0f);
}

void guRotate(Mtx *m, f32 angle, f32 x, f32 y, f32 z) {
    f32 mf[4][4];

    guRotateF(mf, angle, x, y, z);
    guMtxF2L(mf, m);
}

// Rolls about x, then pitches about y, then turns to the heading about z
void guRotateRPYF(f32 mf[4][4], f32 roll, f32 pitch, f32 heading) {
    f32 sr = sinf(roll * DEGREES_TO_RADIANS);
    f32 cr = cosf(roll * DEGREES_TO_RADIANS);
    f32 sp = sinf(pitch * DEGREES_TO_RADIANS);
    f32 cp = cosf(pitch * DEGREES_TO_RADIANS);
    f32 sh = sinf(heading * DEGREES_TO_RADIANS);
    f32 ch = cosf(heading * DEGREES_TO_RADIANS);

    SetRow(mf, 0, MakeVector(cp * ch, cp * sh, -sp), 0.0f);
    SetRow(mf, 1, MakeVector(sr * sp * ch - cr * sh, sr * sp * sh + cr * ch, sr * cp), 0.0f);
    SetRow(mf, 2, MakeVector(cr * sp * ch + sr * sh, cr * sp * sh - sr * ch, cr * cp), 0.0f);
    SetRow(mf, 3, MakeVector(0.0f, 0.0f, 0.0f), 1.0f);
}

void guRotateRPY(Mtx *m, f32 roll, f32 pitch, f32 heading) {
    f32 mf[4][4];

    guRotateRPYF(mf, roll, pitch, heading);
    guMtxF2L(mf, m);
}

// Places an object: rotates it by roll, pitch and heading as guRotateRPYF
// does, scales it evenly, and moves it to (x, y, z)
void guPositionF(f32 mf[4][4], f32 roll, f32 pitch, f32 heading, f32 scale, f32 x, f32 y, f32 z) {
    int row;
    int column;

    guRotateRPYF(mf, roll, pitch, heading);
    for (row = 0; row < 3; row++) {
        for (column = 0; column < 3; column++) {
            mf[row][column] *= scale;
        }
    }

    SetRow(mf, 3, MakeVector(x, y, z), 1.0f);
}

void guPosition(Mtx *m, f32 roll, f32 pitch, f32 heading, f32 scale, f32 x, f32 y, f32 z) {
    f32 mf[4][4];

    guPositionF(mf, roll, pitch, heading, scale, x, y, z);
    guMtxF2L(mf, m);
}

// Turns the z axis to point against the direction (x, y, z), and the x and
// y axes by angle about it. A vertical direction leaves the identity.
void guAlignF(f32 mf[4][4], f32 angle, f32 x, f32 y, f32 z) {
    f32 radians = angle * DEGREES_TO_RADIANS;
    f32 s = sinf(radians);
    f32 c = cosf(radians);
    f32 horizontal;
    Vector across;
    Vector rising;

    guNormalize(&x, &y, &z);
    guMtxIdentF(mf);
    horizontal = sqrtf(x * x + z * z);
    if (horizontal == 0.0f) {
        return;
    }

    // Two axes square to the direction: one level, one rising from it
    across = MakeVector(-z / horizontal, 0.0f, x / horizontal);
    rising = MakeVector(-x * y / horizontal, horizontal, -y * z / horizontal);
    SetRow(mf, 0, Add(Scale(across, c), Scale(rising, s)), 0.0f);
    SetRow(mf, 1, Add(Scale(across, -s), Scale(rising, c)), 0.0f);
    SetRow(mf, 2, MakeVector(-x, -y, -z), 0.0f);
}

void guAlign(Mtx *m, f32 angle, f32 x, f32 y, f32 z) {
    f32 mf[4][4];

    guAlignF(mf, angle, x, y, z);
    guMtxF2L(mf, m);
}

void guLookAtF(
    f32 mf[4][4],
    f32 xEye,
    f32 yEye,
    f32 zEye,
    f32 xAt,
    f32 yAt,
    f32 zAt,
    f32 xUp,
    f32 yUp,
    f32 zUp
) {
    Vector eye = MakeVector(xEye, yEye, zEye);
    ViewBasis basis = ComputeViewBasis(eye, MakeVector(xAt, yAt, zAt), MakeVector(xUp, yUp, zUp));

    SetViewMatrix(mf, &basis, eye);
}

void guLookAt(
    Mtx *m,
    f32 xEye,
    f32 yEye,
    f32 zEye,
    f32 xAt,
    f32 yAt,
    f32 zAt,
    f32 xUp,
    f32 yUp,
    f32 zUp
) {
    f32 mf[4][4];

    guLookAtF(mf, xEye, yEye, zEye, xAt, yAt, zAt, xUp, yUp, zUp);
    guMtxF2L(mf, m);
}

void guLookAtReflectF(
    f32 mf[4][4],
    LookAt *lookAt,
    f32 xEye,
    f32 yEye,
    f32 zEye,
    f32 xAt,
    f32 yAt,
    f32 zAt,
    f32 xUp,
    f32 yUp,
    f32 zUp
) {
    Vector eye = MakeVector(xEye, yEye, zEye);
    ViewBasis basis = ComputeViewBasis(eye, MakeVector(xAt, yAt, zAt), MakeVector(xUp, yUp, zUp));

    SetReflectionLookAt(lookAt, &basis);
    SetViewMatrix(mf, &basis, eye);
}

void guLookAtReflect(
    Mtx *m,
    LookAt *lookAt,
    f32 xEye,
    f32 yEye,
    f32 zEye,
    f32 xAt,
    f32 yAt,
    f32 zAt,
    f32 xUp,
    f32 yUp,
    f32 zUp
) {
    f32 mf[4][4];

    guLookAtReflectF(mf, lookAt, xEye, yEye, zEye, xAt, yAt, zAt, xUp, yUp, zUp);
    guMtxF2L(mf, m);
}

void guLookAtHiliteF(
    f32 mf[4][4],
    LookAt *lookAt,
    Hilite *hilite,
    f32 xEye,
    f32 yEye,
    f32 zEye,
    f32 xAt,
    f32 yAt,
    f32 zAt,
    f32 xUp,
    f32 yUp,
    f32 zUp,
    f32 xLight1,
    f32 yLight1,
    f32 zLight1,
    f32 xLight2,
    f32 yLight2,
    f32 zLight2,
    s32 textureWidth,
    s32 textureHeight
) {
    Vector eye = MakeVector(xEye, yEye, zEye);
    ViewBasis basis = ComputeViewBasis(eye, MakeVector(xAt, yAt, zAt), MakeVector(xUp, yUp, zUp));
    Hilite_t *offsets = &hilite->h;

    PlaceHighlight(
        &basis,
        MakeVector(xLight1, yLight1, zLight1),
        textureWidth,
        textureHeight,
        &offsets->x1,
        &offsets->y1
    );
    PlaceHighlight(
        &basis,
        MakeVector(xLight2, yLight2, zLight2),
        textureWidth,
        textureHeight,
        &offsets->x2,
        &offsets->y2
    );
    SetReflectionLookAt(lookAt, &basis);
    SetViewMatrix(mf, &basis, eye);
}

void guLookAtHilite(
    Mtx *m,
    LookAt *lookAt,
    Hilite *hilite,
    f32 xEye,
    f32 yEye,
    f32 zEye,
    f32 xAt,
    f32 yAt,
    f32 zAt,
    f32 xUp,
    f32 yUp,
    f32 zUp,
    f32 xLight1,
    f32 yLight1,
    f32 zLight1,
    f32 xLight2,
    f32 yLight2,
    f32 zLight2,
    s32 textureWidth,
    s32 textureHeight
) {
    f32 mf[4][4];

    guLookAtHiliteF(
        mf,
        lookAt,
        hilite,
        xEye,
        yEye,
        zEye,
        xAt,
        yAt,
        zAt,
        xUp,
        yUp,
        zUp,
        xLight1,
        yLight1,
        zLight1,
        xLight2,
        yLight2,
        zLight2,
        textureWidth,
        textureHeight
    );
    guMtxF2L(mf, m);
}

// A projection whose clip w is the depth in front of the eye, and whose
// depth runs from -w at near to w at far, all multiplied by scale.
// perspNorm receives the factor that brings w back to about 1 for the RSP's
// reciprocal.
void guPerspectiveF(
    f32 mf[4][4],
    u16 *perspNorm,
    f32 fovy,
    f32 aspect,
    f32 near,
    f32 far,
    f32 scale
) {
    f32 cotangent = cosf(fovy * DEGREES_TO_RADIANS / 2.0f) / sinf(fovy * DEGREES_TO_RADIANS / 2.0f);
    int row;
    int column;

    for (row = 0; row < 4; row++) {
        for (column = 0; column < 4; column++) {
            mf[row][column] = 0.0f;
        }
    }

    mf[0][0] = cotangent / aspect;
    mf[1][1] = cotangent;
    mf[2][2] = (near + far) / (near - far);
    mf[2][3] = -1.0f;
    mf[3][2] = (2.0f * near * far) / (near - far);
    for (row = 0; row < 4; row++) {
        for (column = 0; column < 4; column++) {
            mf[row][column] *= scale;
        }
    }

    if (perspNorm != NULL) {
        if (near + far <= 2.0f) {
            *perspNorm = 0xFFFF;
        }
        else {
            *perspNorm = (u16)((2.0 * 65536.0) / (near + far));
            if (*perspNorm == 0) {
                *perspNorm = 1;
            }
        }
    }
}

void guPerspective(Mtx *m, u16 *perspNorm, f32 fovy, f32 aspect, f32 near, f32 far, f32 scale) {
    f32 mf[4][4];

    guPerspectiveF(mf, perspNorm, fovy, aspect, near, far, scale);
    guMtxF2L(mf, m);
}

// Maps the box from (left, bottom, -near) to (right, top, -far) onto the
// clip cube, all multiplied by scale
void guOrthoF(
    f32 mf[4][4],
    f32 left,
    f32 right,
    f32 bottom,
    f32 top,
    f32 near,
    f32 far,
    f32 scale
) {
    int row;
    int column;

    guMtxIdentF(mf);
    mf[0][0] = 2.0f / (right - left);
    mf[1][1] = 2.0f / (top - bottom);
    mf[2][2] = -2.0f / (far - near);
    mf[3][0] = -(right + left) / (right - left);
    mf[3][1] = -(top + bottom) / (top - bottom);
    mf[3][2] = -(far + near) / (far - near);
    for (row = 0; row < 4; row++) {
        for (column = 0; column < 4; column++) {
            mf[row][column] *= scale;
        }
    }
}

void guOrtho(Mtx *m, f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far, f32 scale) {
    f32 mf[4][4];

    guOrthoF(mf, left, right, bottom, top, near, far, scale);
    guMtxF2L(mf, m);
}

// result = a then b; result may be either
void guMtxCatF(f32 a[4][4], f32 b[4][4], f32 result[4][4]) {
    f32 product[4][4];
    int row;
    int column;

    for (row = 0; row < 4; row++) {
        for (column = 0; column < 4; column++) {
            product[row][column] = a[row][0] * b[0][column] + a[row][1] * b[1][column] +
                a[row][2] * b[2][column] + a[row][3] * b[3][column];
        }
    }

    memcpy(result, product, sizeof(product));
}

void guMtxCatL(Mtx *a, Mtx *b, Mtx *result) {
    f32 fa[4][4];
    f32 fb[4][4];
    f32 product[4][4];

    guMtxL2F(fa, a);
    guMtxL2F(fb, b);
    guMtxCatF(fa, fb, product);
    guMtxF2L(product, result);
}

// Transforms the point (x, y, z) by the matrix, without the divide by w
void guMtxXFMF(f32 mf[4][4], f32 x, f32 y, f32 z, f32 *outX, f32 *outY, f32 *outZ) {
    *outX = x * mf[0][0] + y * mf[1][0] + z * mf[2][0] + mf[3][0];
    *outY = x * mf[0][1] + y * mf[1][1] + z * mf[2][1] + mf[3][1];
    *outZ = x * mf[0][2] + y * mf[1][2] + z * mf[2][2] + mf[3][2];
}

void guMtxXFML(Mtx *m, f32 x, f32 y, f32 z, f32 *outX, f32 *outY, f32 *outZ) {
    f32 mf[4][4];

    guMtxL2F(mf, m);
    guMtxXFMF(mf, x, y, z, outX, outY, outZ);
}

// libultra's quadratic generator: each value comes from the last as
// ((4s + 2)(4s + 3)) / 4, in 32 bits, so that the game's random choices
// follow the same sequence as the N64's
int guRandom(void) {
    static u32 seed = 174823885;
    u32 x = (seed << 2) + 2;

    x *= x + 1;
    seed = x >> 2;
    return (int)seed;
}
