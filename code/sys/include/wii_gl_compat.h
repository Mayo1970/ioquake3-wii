#ifndef WII_GL_COMPAT_H
#define WII_GL_COMPAT_H

/*
 * wii_gl_compat.h
 * Provides GL type definitions and constants for Wii builds.
 * Replaces SDL_opengl.h so the ioQ3 renderer source compiles
 * without an actual OpenGL implementation.
 *
 * Only the types and constants actually used by the renderer
 * source are defined here — nothing more.
 */

/* ---------- Basic GL types -------------------------------------------- */
typedef unsigned int    GLuint;
typedef unsigned int    GLenum;
typedef unsigned int    GLbitfield;
typedef unsigned char   GLboolean;
typedef int             GLint;
typedef unsigned int    GLsizei;
typedef float           GLfloat;
typedef double          GLdouble;
typedef double          GLclampd;
typedef float           GLclampf;
typedef unsigned char   GLubyte;
typedef signed char     GLbyte;
typedef unsigned short  GLushort;
typedef short           GLshort;
typedef void            GLvoid;
typedef long            GLintptr;
typedef long            GLsizeiptr;
typedef char            GLchar;

/* ---------- GL constants used by renderer ----------------------------- */

/* Boolean */
#define GL_FALSE                    0
#define GL_TRUE                     1

/* Primitives */
#define GL_POINTS                   0x0000
#define GL_LINES                    0x0001
#define GL_LINE_STRIP               0x0003
#define GL_TRIANGLES                0x0004
#define GL_TRIANGLE_STRIP           0x0005
#define GL_TRIANGLE_FAN             0x0006
#define GL_QUADS                    0x0007
#define GL_POLYGON                  0x0009

/* Blending */
#define GL_ZERO                     0
#define GL_ONE                      1
#define GL_SRC_COLOR                0x0300
#define GL_ONE_MINUS_SRC_COLOR      0x0301
#define GL_SRC_ALPHA                0x0302
#define GL_ONE_MINUS_SRC_ALPHA      0x0303
#define GL_DST_ALPHA                0x0304
#define GL_ONE_MINUS_DST_ALPHA      0x0305
#define GL_DST_COLOR                0x0306
#define GL_ONE_MINUS_DST_COLOR      0x0307
#define GL_SRC_ALPHA_SATURATE       0x0308

/* Blend equations */
#define GL_BLEND                    0x0BE2
#define GL_BLEND_DST                0x0BE0
#define GL_BLEND_SRC                0x0BE1

/* Alpha test */
#define GL_ALPHA_TEST               0x0BC0
#define GL_NEVER                    0x0200
#define GL_LESS                     0x0201
#define GL_EQUAL                    0x0202
#define GL_LEQUAL                   0x0203
#define GL_GREATER                  0x0204
#define GL_NOTEQUAL                 0x0205
#define GL_GEQUAL                   0x0206
#define GL_ALWAYS                   0x0207

/* Depth */
#define GL_DEPTH_TEST               0x0B71
#define GL_DEPTH_WRITEMASK          0x0B72
#define GL_DEPTH_BUFFER_BIT         0x0100

/* Stencil */
#define GL_STENCIL_TEST             0x0B90
#define GL_STENCIL_BUFFER_BIT       0x0400
#define GL_KEEP                     0x1E00
#define GL_REPLACE                  0x1E01
#define GL_INCR                     0x1E02
#define GL_DECR                     0x1E03
#define GL_INVERT                   0x150A

/* Color buffer */
#define GL_COLOR_BUFFER_BIT         0x4000

/* Cull face */
#define GL_CULL_FACE                0x0B44
#define GL_FRONT                    0x0404
#define GL_BACK                     0x0405
#define GL_FRONT_AND_BACK           0x0408

/* Polygon */
#define GL_POLYGON_OFFSET_FILL      0x8037
#define GL_POLYGON_OFFSET_LINE      0x2A02
#define GL_FILL                     0x1B02
#define GL_LINE                     0x1B01

/* Texture */
#define GL_TEXTURE_2D               0x0DE1
#define GL_TEXTURE0_ARB             0x84C0
#define GL_TEXTURE1_ARB             0x84C1
#define GL_TEXTURE0                 0x84C0
#define GL_TEXTURE1                 0x84C1
#define GL_UNSIGNED_BYTE            0x1401
#define GL_UNSIGNED_INT             0x1405
#define GL_UNSIGNED_SHORT           0x1403
#define GL_FLOAT                    0x1406
#define GL_RGBA                     0x1908
#define GL_RGB                      0x1907
#define GL_LUMINANCE                0x1909
#define GL_LUMINANCE_ALPHA          0x190A
#define GL_BGRA                     0x80E1
#define GL_BGR                      0x80E0
#define GL_RGBA8                    0x8058
#define GL_RGB8                     0x8051
#define GL_RGBA4                    0x8056
#define GL_RGB5                     0x8050
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT  0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT  0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT  0x83F3
#define GL_TEXTURE_COMPRESSION_HINT 0x84EF
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#define GL_CLAMP_TO_EDGE            0x812F
#define GL_CLAMP                    0x2900
#define GL_REPEAT                   0x2901
#define GL_TEXTURE_WRAP_S           0x2802
#define GL_TEXTURE_WRAP_T           0x2803
#define GL_TEXTURE_MIN_FILTER       0x2801
#define GL_TEXTURE_MAG_FILTER       0x2800
#define GL_LINEAR                   0x2601
#define GL_NEAREST                  0x2600
#define GL_LINEAR_MIPMAP_LINEAR     0x2703
#define GL_LINEAR_MIPMAP_NEAREST    0x2701
#define GL_NEAREST_MIPMAP_NEAREST   0x2700
#define GL_NEAREST_MIPMAP_LINEAR    0x2702
#define GL_GENERATE_MIPMAP          0x8191
#define GL_TEXTURE_ENV              0x2300
#define GL_TEXTURE_ENV_MODE         0x2200
#define GL_MODULATE                 0x2100
#define GL_DECAL                    0x2101
#define GL_ADD                      0x0104
#define GL_REPLACE                  0x1E01

/* Matrix mode */
#define GL_MODELVIEW                0x1700
#define GL_PROJECTION               0x1701
#define GL_TEXTURE                  0x1702

/* Fog */
#define GL_FOG                      0x0B60
#define GL_FOG_MODE                 0x0B65
#define GL_FOG_DENSITY              0x0B62
#define GL_FOG_COLOR                0x0B66
#define GL_FOG_START                0x0B63
#define GL_FOG_END                  0x0B64
#define GL_LINEAR                   0x2601
#define GL_EXP                      0x0800
#define GL_EXP2                     0x0801

/* Misc */
#define GL_VENDOR                   0x1F00
#define GL_RENDERER                 0x1F01
#define GL_VERSION                  0x1F02
#define GL_EXTENSIONS               0x1F03
#define GL_NUM_EXTENSIONS           0x821D
#define GL_MAX_TEXTURE_SIZE         0x0D33
#define GL_MAX_TEXTURE_UNITS_ARB    0x84E2
#define GL_MULTISAMPLE_ARB          0x809D
#define GL_SAMPLES_ARB              0x80A9
#define GL_SCISSOR_TEST             0x0C11
#define GL_SCISSOR_BOX              0x0C10
#define GL_VIEWPORT                 0x0BA2
#define GL_NO_ERROR                 0
#define GL_NICEST                   0x1102
#define GL_FASTEST                  0x1101
#define GL_DONT_CARE                0x1100
#define GL_PACK_ALIGNMENT           0x0D05

/* GL error codes */
#define GL_INVALID_ENUM             0x0500
#define GL_INVALID_VALUE            0x0501
#define GL_INVALID_OPERATION        0x0502
#define GL_STACK_OVERFLOW           0x0503
#define GL_STACK_UNDERFLOW          0x0504
#define GL_OUT_OF_MEMORY            0x0505

/* Array types */
#define GL_VERTEX_ARRAY             0x8074
#define GL_NORMAL_ARRAY             0x8075
#define GL_COLOR_ARRAY              0x8076
#define GL_TEXTURE_COORD_ARRAY      0x8078

/* Shade model */
#define GL_FLAT                     0x1D00
#define GL_SMOOTH                   0x1D01

/* Draw buffer */
#define GL_NONE                     0
#define GL_FRONT_LEFT               0x0400
#define GL_BACK_LEFT                0x0402

/* Occlusion query */
#define GL_SAMPLES_PASSED           0x8914
#define GL_QUERY_RESULT             0x8866
#define GL_QUERY_RESULT_AVAILABLE   0x8867

/* Framebuffer */
#define GL_FRAMEBUFFER              0x8D40
#define GL_RENDERBUFFER             0x8D41
#define GL_COLOR_ATTACHMENT0        0x8CE0
#define GL_DEPTH_ATTACHMENT         0x8D00
#define GL_STENCIL_ATTACHMENT       0x8D20
#define GL_FRAMEBUFFER_COMPLETE     0x8CD5
#define GL_DEPTH_COMPONENT          0x1902
#define GL_DEPTH_COMPONENT24        0x81A6

/* Buffer objects */
#define GL_ARRAY_BUFFER             0x8892
#define GL_ELEMENT_ARRAY_BUFFER     0x8893
#define GL_STATIC_DRAW              0x88B4
#define GL_DYNAMIC_DRAW             0x88B8
#define GL_STREAM_DRAW              0x88B0

/* Shaders (stubs - not used on Wii) */
#define GL_FRAGMENT_SHADER          0x8B30
#define GL_VERTEX_SHADER            0x8B31
#define GL_COMPILE_STATUS           0x8B81
#define GL_LINK_STATUS              0x8B82

/* APIENTRYP for function pointer declarations */
#ifndef APIENTRYP
#define APIENTRYP *
#endif
#ifndef APIENTRY
#define APIENTRY
#endif

#endif /* WII_GL_COMPAT_H */
