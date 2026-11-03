/* GPU-vs-CPU renderer equivalence selftest for the optional vertex-shader
 * path in runtime/d3d8_renderer.c (DS_GPU_VERTEX=1 / THING_GPU_VERTEX=1).
 *
 * HOST EVIDENCE ONLY: x86_64 Linux + Xvfb + Mesa llvmpipe GLES (software).
 * This says nothing about the H700/Mali device and is not a performance test.
 *
 * The renderer reads DS_GPU_VERTEX exactly once, inside d8_renderer_init(), so
 * both backends can be created in one GL context:
 *   1. initialize the CPU reference renderer with DS_GPU_VERTEX/THING_GPU_VERTEX
 *      unset,
 *   2. setenv("DS_GPU_VERTEX","1",1), initialize the GPU renderer,
 *   3. unsetenv again so no later code can observe the probe's toggle.
 * Both instances then interleave draws in the same context; the renderer's
 * raster/attribute/texture owner epochs are designed for that (see
 * probe/d3d8_raster_selftest.c).
 *
 * For every case the same D8State + raw D3D FVF vertex/index bytes + texture
 * objects are drawn through the CPU renderer into FBO A and through the GPU
 * renderer into FBO B; RGBA is read back and compared with a per-case
 * tolerance. The GL_CURRENT_PROGRAM after each draw proves which program the
 * renderer selected (GPU program vs. CPU reference program), including the
 * "GPU vertex path declined a draw; CPU reference path used" fallback.
 *
 * Usage: check [evidence_dir]   (default docs/evidence/gpu-equivalence)
 */
#include <SDL.h>
#include <GLES2/gl2.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>
#include "d3d8_renderer.h"

/* Count actual GL submissions, independently of the renderer cache layout. */
static unsigned lighting_matrix_uploads,lighting_vector_uploads;
void __real_glUniformMatrix3fv(GLint,GLsizei,GLboolean,const GLfloat*);
void __real_glUniform3fv(GLint,GLsizei,const GLfloat*);
void __wrap_glUniformMatrix3fv(GLint location,GLsizei count,GLboolean transpose,const GLfloat*value){
 lighting_matrix_uploads++;__real_glUniformMatrix3fv(location,count,transpose,value);
}
void __wrap_glUniform3fv(GLint location,GLsizei count,const GLfloat*value){
 lighting_vector_uploads++;__real_glUniform3fv(location,count,value);
}

#define WIDTH 64
#define HEIGHT 64
#define MAX_CASES 24
#define PATH_MAX_LOCAL 512

/* ------------------------------------------------------------------ */
/* Deterministic host-reference helpers                                */
/* ------------------------------------------------------------------ */
static void identity4(float m[16]) { memset(m, 0, 64); m[0] = m[5] = m[10] = m[15] = 1.f; }
static void normalize3(float v[3]) {
    float n = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (n > 0.f) { v[0] /= n; v[1] /= n; v[2] /= n; }
}
/* D3D row-major matrices, matching the row-vector convention used by the
 * renderer: p' = p * M is equivalent to the GLSL column-vector M^T * p that
 * the renderer gets by uploading with transpose=GL_FALSE. */
static void rotate_y(float m[16], float radians) {
    float c = cosf(radians), s = sinf(radians);
    identity4(m); m[0] = c; m[2] = -s; m[8] = s; m[10] = c;
}
static void rotate_x(float m[16], float radians) {
    float c = cosf(radians), s = sinf(radians);
    identity4(m); m[5] = c; m[6] = s; m[9] = -s; m[10] = c;
}
static void perspective(float m[16], float fovy_radians, float aspect, float zn, float zf) {
    float f = 1.f / tanf(fovy_radians * 0.5f);
    memset(m, 0, 64);
    m[0] = f / aspect; m[5] = f;
    m[10] = zf / (zf - zn); m[11] = 1.f;
    m[14] = -zn * zf / (zf - zn); m[15] = 0.f;
}
static void view_matrix_ortho(float m[16]) { identity4(m); }
static void view_matrix_perspective(float m[16]) { identity4(m); }

/* ------------------------------------------------------------------ */
/* Minimal zlib-backed PNG writer (no libpng on this host)             */
/* ------------------------------------------------------------------ */
static void put_be32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8); p[3] = (unsigned char)v;
}
static void png_chunk(FILE *f, const char type[4], const unsigned char *data, unsigned len) {
    unsigned char head[4];
    put_be32(head, len);
    fwrite(head, 1, 4, f);
    fwrite(type, 1, 4, f);
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const Bytef *)type, 4);
    if (len) crc = crc32(crc, (const Bytef *)data, len);
    if (len) fwrite(data, 1, len, f);
    unsigned char tail[4];
    put_be32(tail, (uint32_t)crc);
    fwrite(tail, 1, 4, f);
}
/* rgba is bottom-up as returned by glReadPixels; PNG rows are top-down. */
static int save_png_flipped(const char *path, const unsigned char *rgba, unsigned w, unsigned h) {
    size_t row = (size_t)w * 4, rawlen = (size_t)h * (1 + row);
    unsigned char *raw = (unsigned char *)malloc(rawlen);
    if (!raw) return -1;
    for (unsigned y = 0; y < h; y++) {
        unsigned char *dst = raw + y * (1 + row);
        dst[0] = 0;
        memcpy(dst + 1, rgba + (size_t)(h - 1 - y) * row, row);
    }
    uLongf zlen = compressBound((uLong)rawlen);
    unsigned char *z = (unsigned char *)malloc(zlen);
    if (!z) { free(raw); return -1; }
    if (compress2(z, &zlen, raw, (uLong)rawlen, 9) != Z_OK) { free(z); free(raw); return -1; }
    FILE *f = fopen(path, "wb");
    if (!f) { free(z); free(raw); return -1; }
    static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    fwrite(sig, 1, 8, f);
    unsigned char ihdr[13];
    put_be32(ihdr, w); put_be32(ihdr + 4, h);
    ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    png_chunk(f, "IHDR", ihdr, 13);
    png_chunk(f, "IDAT", z, (unsigned)zlen);
    png_chunk(f, "IEND", NULL, 0);
    fclose(f); free(z); free(raw);
    return 0;
}

/* ------------------------------------------------------------------ */
/* stderr capture (renderer diagnostics, fallback logging)             */
/* ------------------------------------------------------------------ */
static int g_stderr_saved = -1;
static FILE *g_stderr_tmp;
static void stderr_capture_begin(const char *evidence_dir) {
    fflush(stderr);
    g_stderr_tmp = tmpfile();
    if (!g_stderr_tmp) {
        char path[PATH_MAX_LOCAL];
        snprintf(path, sizeof path, "%s/.stderr-capture", evidence_dir);
        g_stderr_tmp = fopen(path, "w+");
    }
    if (!g_stderr_tmp) return;
    g_stderr_saved = dup(STDERR_FILENO);
    if (g_stderr_saved < 0) { fclose(g_stderr_tmp); g_stderr_tmp = NULL; return; }
    if (dup2(fileno(g_stderr_tmp), STDERR_FILENO) < 0) { close(g_stderr_saved); g_stderr_saved = -1; fclose(g_stderr_tmp); g_stderr_tmp = NULL; }
}
static size_t stderr_capture_end(char *buf, size_t cap) {
    if (!g_stderr_tmp || g_stderr_saved < 0) { if (cap) buf[0] = 0; return 0; }
    fflush(stderr);
    fseek(g_stderr_tmp, 0, SEEK_SET);
    size_t n = fread(buf, 1, cap - 1, g_stderr_tmp);
    buf[n] = 0;
    dup2(g_stderr_saved, STDERR_FILENO);
    close(g_stderr_saved);
    g_stderr_saved = -1;
    fclose(g_stderr_tmp);
    g_stderr_tmp = NULL;
    return n;
}
static int early_fail(const char *message) {
    printf("%s\n", message);
    char buf[65536];
    size_t n = stderr_capture_end(buf, sizeof buf);
    if (n) printf("--- captured stderr ---\n%s--- end ---\n", buf);
    fflush(stdout);
    return 2;
}
static unsigned count_substring(const char *hay, const char *needle) {
    unsigned n = 0;
    const char *p = hay;
    size_t len = strlen(needle);
    while ((p = strstr(p, needle)) != NULL) { n++; p += len; }
    return n;
}

/* ------------------------------------------------------------------ */
/* GL targets/textures                                                  */
/* ------------------------------------------------------------------ */
typedef struct { GLuint fbo, color, depth; } Target;
static Target g_target_cpu, g_target_gpu;
static GLuint g_tex[2];
static GLuint g_native_small_vbo;
static D8Renderer g_cpu, g_gpu;

static int target_create(Target *t, unsigned w, unsigned h) {
    glGenTextures(1, &t->color);
    glBindTexture(GL_TEXTURE_2D, t->color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenRenderbuffers(1, &t->depth);
    glBindRenderbuffer(GL_RENDERBUFFER, t->depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, (GLsizei)w, (GLsizei)h);
    glGenFramebuffers(1, &t->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, t->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t->color, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, t->depth);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) { fprintf(stdout, "FBO incomplete: %#x\n", status); return -1; }
    return 0;
}
static const float CLEAR_R = 0.10f, CLEAR_G = 0.20f, CLEAR_B = 0.30f;
static void target_clear(Target *t) {
    glBindFramebuffer(GL_FRAMEBUFFER, t->fbo);
    glClearColor(CLEAR_R, CLEAR_G, CLEAR_B, 1.f);
    glClearDepthf(1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
static void target_read(Target *t, unsigned char *out) {
    glBindFramebuffer(GL_FRAMEBUFFER, t->fbo);
    glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, out);
}
static int texture_create(GLuint *tex, unsigned kind) {
    unsigned char px[4 * 4 * 4];
    for (unsigned y = 0; y < 4; y++) {
        for (unsigned x = 0; x < 4; x++) {
            unsigned char *p = &px[(y * 4 + x) * 4];
            p[0] = (unsigned char)(30 + x * 55 + kind * 17);
            p[1] = (unsigned char)(40 + y * 50 + kind * 11);
            p[2] = (unsigned char)(70 + ((x + y + kind) & 1) * 90);
            p[3] = (unsigned char)(255 - (((x + y) & 1) ? 48 : 0));
        }
    }
    glGenTextures(1, tex);
    glBindTexture(GL_TEXTURE_2D, *tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    return glGetError() == GL_NO_ERROR ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* Raw D3D FVF vertex packing (renderer consumes guest bytes, not a     */
/* host Vertex struct)                                                  */
/* ------------------------------------------------------------------ */
typedef struct {
    float p[4];
    float n[3];
    uint32_t diffuse;
    uint32_t specular;
    float uv[8][4];
} VertexInput;

static size_t pack_vertices(uint32_t fvf, const VertexInput *v, unsigned n, unsigned char *out) {
    size_t off = 0;
    unsigned pos = fvf & 14, tc = (fvf >> 8) & 15;
    for (unsigned i = 0; i < n; i++) {
        if (pos == 4) { memcpy(out + off, v[i].p, 16); off += 16; }
        else { memcpy(out + off, v[i].p, 12); off += 12; }
        if (fvf & 16) { memcpy(out + off, v[i].n, 12); off += 12; }
        if (fvf & 32) { float skip = 0.f; memcpy(out + off, &skip, 4); off += 4; }
        if (fvf & 64) { memcpy(out + off, &v[i].diffuse, 4); off += 4; }
        if (fvf & 128) { memcpy(out + off, &v[i].specular, 4); off += 4; }
        for (unsigned t = 0; t < tc; t++) {
            unsigned dim = (fvf >> (16 + 2 * t)) & 3;
            dim = dim == 0 ? 2 : dim == 1 ? 3 : dim == 2 ? 4 : 1;
            memcpy(out + off, v[i].uv[t], dim * 4);
            off += dim * 4;
        }
    }
    return off;
}

#define GRID 4
static const float GX[GRID] = {-0.87f, -0.29f, 0.29f, 0.87f};
static const float GY[GRID] = {-0.83f, -0.31f, 0.27f, 0.81f};
static unsigned grid_indices(unsigned short *idx) {
    unsigned n = 0;
    for (unsigned r = 0; r < GRID - 1; r++) {
        for (unsigned c = 0; c < GRID - 1; c++) {
            unsigned short v00 = (unsigned short)(r * GRID + c);
            unsigned short v01 = (unsigned short)(r * GRID + c + 1);
            unsigned short v10 = (unsigned short)((r + 1) * GRID + c);
            unsigned short v11 = (unsigned short)((r + 1) * GRID + c + 1);
            idx[n++] = v00; idx[n++] = v01; idx[n++] = v11;
            idx[n++] = v00; idx[n++] = v11; idx[n++] = v10;
        }
    }
    return n;
}

static void grid_common_rgba(VertexInput *v, unsigned r, unsigned c) {
    unsigned rr = 48 + 40 * c, gg = 40 + 45 * r, bb = 170 - 20 * ((r + c) & 1);
    v->diffuse = 0xff000000u | (rr << 16) | (gg << 8) | bb;
    v->specular = 0x80000000u | (200u << 16) | (180u << 8) | 160u;
    v->uv[0][0] = 0.13f + 0.27f * (float)c;
    v->uv[0][1] = 0.11f + 0.31f * (float)r;
    v->uv[0][2] = 0.50f + 0.15f * (float)c;
    v->uv[0][3] = 1.f;
    v->uv[1][0] = 0.21f + 0.19f * (float)c;
    v->uv[1][1] = 0.37f + 0.23f * (float)r;
    v->uv[1][2] = 0.f;
    v->uv[1][3] = 1.f;
}
/* Orthographic grid: world space is clip space (world/view/projection
 * identity); z runs 0.15..0.84 so fog and depth vary across the frame. */
static void build_ortho_grid(VertexInput *v, uint32_t fvf) {
    for (unsigned r = 0; r < GRID; r++) {
        for (unsigned c = 0; c < GRID; c++) {
            VertexInput *o = &v[r * GRID + c];
            memset(o, 0, sizeof *o);
            o->p[0] = GX[c]; o->p[1] = GY[r];
            o->p[2] = 0.15f + 0.18f * (float)r + 0.05f * (float)c;
            o->p[3] = 1.f;
            float n[3] = {0.22f * (float)c - 0.33f, 0.17f * (float)r - 0.25f, 1.f};
            normalize3(n);
            o->n[0] = n[0]; o->n[1] = n[1]; o->n[2] = n[2];
            grid_common_rgba(o, r, c);
            (void)fvf;
        }
    }
}
/* Perspective grid: local coordinates behind a D3D perspective projection,
 * with a rotated world matrix so normal-matrix and view-space fog paths run. */
static void build_perspective_grid(VertexInput *v) {
    static const float LX[GRID] = {-1.50f, -0.50f, 0.50f, 1.50f};
    static const float LY[GRID] = {-1.35f, -0.45f, 0.45f, 1.35f};
    for (unsigned r = 0; r < GRID; r++) {
        for (unsigned c = 0; c < GRID; c++) {
            VertexInput *o = &v[r * GRID + c];
            memset(o, 0, sizeof *o);
            o->p[0] = LX[c]; o->p[1] = LY[r];
            o->p[2] = 2.20f + 0.25f * (float)r + 0.60f * (float)c;
            o->p[3] = 1.f;
            float n[3] = {0.18f * (float)c - 0.27f, 0.14f * (float)r - 0.21f, 1.f};
            normalize3(n);
            o->n[0] = n[0]; o->n[1] = n[1]; o->n[2] = n[2];
            grid_common_rgba(o, r, c);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Results                                                              */
/* ------------------------------------------------------------------ */
typedef struct {
    char name[48];
    char description[320];
    uint32_t fvf;
    double tolerance;
    int diagnostic;
    int cpu_rc, gpu_rc, gpu_rc2;
    int cpu_program_ok, gpu_program_ok, gpu_program_ok2;
    int used_gpu_path;
    char renderer_error[192];
    unsigned covered_pixels;
    double max_r, max_g, max_b, max_a, max_any, mean_any;
    unsigned diff_pixels, over_tolerance;
    int pass;
} CaseResult;
static CaseResult g_results[MAX_CASES];
static unsigned g_result_count;

static void compare_images(const unsigned char *a, const unsigned char *b, CaseResult *out) {
    unsigned npix = WIDTH * HEIGHT;
    out->max_r = out->max_g = out->max_b = out->max_a = out->max_any = 0.0;
    double sum = 0.0;
    out->diff_pixels = out->over_tolerance = 0;
    out->covered_pixels = 0;
    const unsigned char *clear = a; /* bottom-left texel is background in every case */
    for (unsigned i = 0; i < npix; i++) {
        const unsigned char *pa = a + i * 4, *pb = b + i * 4;
        double dr = fabs((double)pa[0] - pb[0]);
        double dg = fabs((double)pa[1] - pb[1]);
        double db = fabs((double)pa[2] - pb[2]);
        double da = fabs((double)pa[3] - pb[3]);
        if (dr > out->max_r) out->max_r = dr;
        if (dg > out->max_g) out->max_g = dg;
        if (db > out->max_b) out->max_b = db;
        if (da > out->max_a) out->max_a = da;
        double any = dr;
        if (dg > any) any = dg;
        if (db > any) any = db;
        if (da > any) any = da;
        if (any > out->max_any) out->max_any = any;
        if (any > 0.5) out->diff_pixels++;
        sum += dr + dg + db + da;
        if (any > out->tolerance) out->over_tolerance++;
        if (memcmp(pa, clear, 4) != 0) out->covered_pixels++;
    }
    out->mean_any = sum / (double)npix / 4.0;
}

/* ------------------------------------------------------------------ */
/* Undisturbed GL state between cases; the per-renderer raster caches   */
/* are invalidated because these external calls are not renderer-owned. */
/* ------------------------------------------------------------------ */
static void gl_extra_reset(void) {
    glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST); glDisable(GL_STENCIL_TEST); glDisable(GL_POLYGON_OFFSET_FILL);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glViewport(0, 0, WIDTH, HEIGHT);
    glDepthRangef(0.f, 1.f);
    d8_renderer_invalidate_raster(&g_cpu);
    d8_renderer_invalidate_raster(&g_gpu);
}

/* ------------------------------------------------------------------ */
/* One case: identical inputs through CPU renderer -> FBO A and through */
/* GPU renderer -> FBO B, read back and compare.                        */
/* ------------------------------------------------------------------ */
typedef struct {
    const char *name;
    const char *description;
    const D8State *state;
    const unsigned char *vertices;
    size_t stride;
    unsigned vertex_count;
    const unsigned short *indices;
    unsigned index_size;
    unsigned primitive;
    unsigned primitive_count;
    unsigned base;
    const GLuint *textures;
    const D8NativeBuffers *native_gpu; /* deliberately invalid -> fallback */
    double tolerance;
    int expect_gpu_path;
    int repeat_gpu_draw;
    int diagnostic;
} DrawCase;

static int run_case(const DrawCase *c, const char *images_dir) {
    static unsigned char img_cpu[WIDTH * HEIGHT * 4], img_gpu[WIDTH * HEIGHT * 4];
    static unsigned char img_diff[WIDTH * HEIGHT * 4];
    if (g_result_count >= MAX_CASES) return -1;
    CaseResult *out = &g_results[g_result_count++];
    memset(out, 0, sizeof *out);
    snprintf(out->name, sizeof out->name, "%s", c->name);
    snprintf(out->description, sizeof out->description, "%s", c->description);
    out->fvf = c->state->fvf;
    out->tolerance = c->tolerance;
    out->diagnostic = c->diagnostic;
    GLint program = 0;

    /* --- CPU reference renderer --- */
    gl_extra_reset();
    target_clear(&g_target_cpu);
    out->cpu_rc = d8_renderer_draw_native(&g_cpu, c->state, HEIGHT, c->primitive, c->primitive_count,
                                          c->vertices, c->stride, c->vertex_count, c->indices,
                                          c->index_size, c->base, c->textures, NULL);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    out->cpu_program_ok = program == (GLint)g_cpu.program && g_cpu.gpu_program == 0;
    if (glGetError() != GL_NO_ERROR) out->cpu_rc = out->cpu_rc ? out->cpu_rc : -2;
    target_read(&g_target_cpu, img_cpu);

    /* --- GPU renderer (or CPU fallback inside it) --- */
    gl_extra_reset();
    target_clear(&g_target_gpu);
    lighting_matrix_uploads=lighting_vector_uploads=0;
    out->gpu_rc = d8_renderer_draw_native(&g_gpu, c->state, HEIGHT, c->primitive, c->primitive_count,
                                          c->vertices, c->stride, c->vertex_count, c->indices,
                                          c->index_size, c->base, c->textures, c->native_gpu);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    out->used_gpu_path = program == (GLint)g_gpu.gpu_program;
    out->gpu_program_ok = c->expect_gpu_path ? (program == (GLint)g_gpu.gpu_program)
                                             : (program == (GLint)g_gpu.program);
    if (glGetError() != GL_NO_ERROR) out->gpu_rc = out->gpu_rc ? out->gpu_rc : -2;
    if (c->native_gpu || out->gpu_rc)
        snprintf(out->renderer_error, sizeof out->renderer_error, "%s", d8_renderer_error());
    /* Second draw: proves a declined draw falls back every time and that the
     * renderer logs the decline only once (static one-shot diagnostic). */
    if (c->repeat_gpu_draw) {
        gl_extra_reset();
        target_clear(&g_target_gpu);
        out->gpu_rc2 = d8_renderer_draw_native(&g_gpu, c->state, HEIGHT, c->primitive, c->primitive_count,
                                               c->vertices, c->stride, c->vertex_count, c->indices,
                                               c->index_size, c->base, c->textures, c->native_gpu);
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        out->gpu_program_ok2 = c->expect_gpu_path ? (program == (GLint)g_gpu.gpu_program)
                                                  : (program == (GLint)g_gpu.program);
    }
    target_read(&g_target_gpu, img_gpu);

    compare_images(img_cpu, img_gpu, out);
    out->pass = out->cpu_rc == 0 && out->gpu_rc == 0 &&
                (!c->repeat_gpu_draw || out->gpu_rc2 == 0) &&
                out->cpu_program_ok && out->gpu_program_ok &&
                (!c->repeat_gpu_draw || out->gpu_program_ok2) &&
                out->max_any <= c->tolerance;

    if(!strcmp(c->name,"unlit_changed_light_transforms")||!strcmp(c->name,"relit_deferred_transforms")){
        printf("lighting_uploads case=%s matrices=%u vectors=%u\n",c->name,lighting_matrix_uploads,lighting_vector_uploads);
        if(getenv("THING_EXPECT_DEFERRED_LIGHTING")){
            int relit=!strcmp(c->name,"relit_deferred_transforms");
            if(lighting_matrix_uploads!=(unsigned)relit||lighting_vector_uploads!=(unsigned)(relit*2))out->pass=0;
        }
    }

    /* PNG evidence (bottom-up readback flipped to top-down PNG). */
    char path[PATH_MAX_LOCAL];
    snprintf(path, sizeof path, "%s/%s_cpu.png", images_dir, c->name);
    save_png_flipped(path, img_cpu, WIDTH, HEIGHT);
    snprintf(path, sizeof path, "%s/%s_gpu.png", images_dir, c->name);
    save_png_flipped(path, img_gpu, WIDTH, HEIGHT);
    if (out->max_any > 0.5) {
        for (unsigned i = 0; i < WIDTH * HEIGHT; i++) {
            unsigned char *d = img_diff + i * 4;
            d[0] = (unsigned char)((abs((int)img_cpu[i * 4 + 0] - img_gpu[i * 4 + 0]) * 8) > 255 ? 255 : abs((int)img_cpu[i * 4 + 0] - img_gpu[i * 4 + 0]) * 8);
            d[1] = (unsigned char)((abs((int)img_cpu[i * 4 + 1] - img_gpu[i * 4 + 1]) * 8) > 255 ? 255 : abs((int)img_cpu[i * 4 + 1] - img_gpu[i * 4 + 1]) * 8);
            d[2] = (unsigned char)((abs((int)img_cpu[i * 4 + 2] - img_gpu[i * 4 + 2]) * 8) > 255 ? 255 : abs((int)img_cpu[i * 4 + 2] - img_gpu[i * 4 + 2]) * 8);
            d[3] = 255;
        }
        snprintf(path, sizeof path, "%s/%s_diff.png", images_dir, c->name);
        save_png_flipped(path, img_diff, WIDTH, HEIGHT);
    }
    if (out->cpu_rc || out->gpu_rc || !out->cpu_program_ok || !out->gpu_program_ok)
        printf("  debug name=%s cpu_rc=%d gpu_rc=%d gpu_rc2=%d cpu_prog_ok=%d gpu_prog_ok=%d used_gpu=%d renderer_error=%s\n",
               c->name, out->cpu_rc, out->gpu_rc, out->gpu_rc2, out->cpu_program_ok,
               out->gpu_program_ok, out->used_gpu_path, d8_renderer_error());
    printf("case=%-30s result=%s%sfvf=%#06x tol=%.0f max=%.0f mean=%.4f diffpixels=%u over=%u covered=%u gpu_path=%d\n",
           c->name, out->pass ? "PASS" : "FAIL", c->diagnostic ? " (diagnostic) " : " ",
           out->fvf, out->tolerance, out->max_any, out->mean_any, out->diff_pixels,
           out->over_tolerance, out->covered_pixels, out->used_gpu_path);
    fflush(stdout);
    return out->pass ? 0 : 1;
}

/* ------------------------------------------------------------------ */
/* Shared-rejection probes: states the renderer rejects before either   */
/* backend is selected. These cannot reach the new GPU fallback branch, */
/* and the selftest reports that explicitly instead of pretending.      */
/* ------------------------------------------------------------------ */
typedef struct {
    const char *name;
    int cpu_rc, gpu_rc;
    char cpu_error[160], gpu_error[160];
} RejectResult;
static RejectResult g_reject[4];
static unsigned g_reject_count;

static void probe_shared_rejection(const char *name, const D8State *s, const unsigned char *verts,
                                   size_t stride, unsigned vcount, const GLuint *textures) {
    if (g_reject_count >= 4) return;
    RejectResult *r = &g_reject[g_reject_count++];
    memset(r, 0, sizeof *r);
    r->name = name;
    r->cpu_rc = d8_renderer_draw_native(&g_cpu, s, HEIGHT, 4, 18, verts, stride, vcount, NULL, 0, 0, textures, NULL);
    snprintf(r->cpu_error, sizeof r->cpu_error, "%s", d8_renderer_error());
    r->gpu_rc = d8_renderer_draw_native(&g_gpu, s, HEIGHT, 4, 18, verts, stride, vcount, NULL, 0, 0, textures, NULL);
    snprintf(r->gpu_error, sizeof r->gpu_error, "%s", d8_renderer_error());
    printf("shared_reject=%-26s cpu_rc=%d gpu_rc=%d cpu_error=%s\n", name, r->cpu_rc, r->gpu_rc, r->cpu_error);
}

/* D3DLIGHT8.type is a uint32 enum stored in the first 4 bytes of the
 * float[26] slot; both the CPU path (memcpy) and the GPU uploader ((float)type)
 * read it that way. */
static void set_light_type(D8State *s, int index, uint32_t type) {
    memcpy(&s->light[index][0], &type, 4);
    s->light[index][4] = 1.f;   /* diffuse.a */
    s->light[index][8] = 1.f;   /* specular.a */
    s->light[index][12] = 1.f;  /* ambient.a */
    s->light_on[index] = 1;
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */
int main(int argc, char **argv) {
    const char *evidence_dir = argc > 1 ? argv[1] : "docs/evidence/gpu-equivalence";
    char images_dir[PATH_MAX_LOCAL];
    snprintf(images_dir, sizeof images_dir, "%s/images", evidence_dir);
    mkdir(evidence_dir, 0755);
    mkdir(images_dir, 0755);
    stderr_capture_begin(evidence_dir);

    printf("gpu-equivalence: HOST x86_64 Mesa llvmpipe GLES software evidence; NOT H700/Mali device evidence\n");
    if (SDL_Init(SDL_INIT_VIDEO)) { char m[256]; snprintf(m, sizeof m, "SDL_Init failed: %s", SDL_GetError()); return early_fail(m); }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_Window *window = SDL_CreateWindow("d3d8 gpu equivalence", 0, 0, WIDTH, HEIGHT,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!window) { char m[320]; snprintf(m, sizeof m, "SDL_CreateWindow failed: %s", SDL_GetError()); return early_fail(m); }
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) { char m[320]; snprintf(m, sizeof m, "SDL_GL_CreateContext failed: %s", SDL_GetError()); return early_fail(m); }
    const char *gl_version = (const char *)glGetString(GL_VERSION);
    const char *gl_renderer = (const char *)glGetString(GL_RENDERER);
    const char *gl_vendor = (const char *)glGetString(GL_VENDOR);
    const char *gl_sl = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
    printf("gl_version=%s\ngl_renderer=%s\ngl_vendor=%s\ngl_sl=%s\n",
           gl_version ? gl_version : "?", gl_renderer ? gl_renderer : "?",
           gl_vendor ? gl_vendor : "?", gl_sl ? gl_sl : "?");

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (target_create(&g_target_cpu, WIDTH, HEIGHT) || target_create(&g_target_gpu, WIDTH, HEIGHT)) return early_fail("FBO creation failed");
    if (texture_create(&g_tex[0], 0) || texture_create(&g_tex[1], 1)) return early_fail("test texture creation failed");
    glGenBuffers(1, &g_native_small_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_native_small_vbo);
    unsigned char small_data[8] = {0};
    glBufferData(GL_ARRAY_BUFFER, sizeof small_data, small_data, GL_STATIC_DRAW);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    /* Renderer initialization ordering: DS_GPU_VERTEX is read once by
     * d8_renderer_init(); alias THING_GPU_VERTEX takes priority, so both are
     * cleared for the CPU reference instance. */
    unsetenv("DS_GPU_VERTEX"); unsetenv("THING_GPU_VERTEX"); unsetenv("DS_GPU_TRIM_VB"); unsetenv("THING_GPU_TRIM_VB");
    if (d8_renderer_init(&g_cpu)) return early_fail("CPU reference renderer init failed");
    setenv("DS_GPU_VERTEX", "1", 1);
    if (d8_renderer_init(&g_gpu)) return early_fail("GPU renderer init failed");
    unsetenv("DS_GPU_VERTEX");
    printf("cpu_renderer.gpu_program=%u cpu_renderer.program=%u\n", g_cpu.gpu_program, g_cpu.program);
    printf("gpu_renderer.gpu_program=%u gpu_renderer.program=%u\n", g_gpu.gpu_program, g_gpu.program);
    if (!g_cpu.program || g_cpu.gpu_program != 0) { printf("overall=FAIL (CPU reference renderer is not the CPU path)\n"); return 1; }
    if (!g_gpu.program || !g_gpu.gpu_program) { printf("overall=FAIL (optional GPU vertex program did not initialize)\n"); return 1; }

    unsigned short indices[256];
    unsigned index_count = grid_indices(indices);
    unsigned char verts[GRID * GRID * 64];
    unsigned char expanded[256 * 64];
    VertexInput v[GRID * GRID];
    GLuint no_textures[2] = {0, 0};
    GLuint one_texture[2] = {g_tex[0], 0};
    GLuint two_textures[2] = {g_tex[0], g_tex[1]};

    D8State base;
    d8_state_defaults(&base, WIDTH, HEIGHT);
    base.rs[22] = 1; /* D3DCULL_NONE: keep both triangle windings visible */

    /* ================= case 1: unlit untextured FVF 0x44 (array path) ====
     * Note FVF bits: 0x04 is D3DFVF_XYZRHW, 0x40 is D3DFVF_DIFFUSE, so
     * 0x44 is XYZRHW+DIFFUSE, not XYZ+DIFFUSE. This case keeps p.w == 1 and
     * the CPU/GPU array-draw (no index buffer) path; the XYZ+DIFFUSE variant
     * (0x42) follows as its own case and rhw_screen_space covers p.w != 1. */
    {
        D8State s = base;
        s.fvf = 0x44;
        s.rs[137] = 0;
        /* Screen-space coordinates matching probe/d3d8_raster_selftest.c. */
        static const float SX[GRID] = {4.3f, 22.7f, 41.3f, 59.8f};
        static const float SY[GRID] = {6.1f, 23.4f, 41.9f, 58.6f};
        for (unsigned r = 0; r < GRID; r++) {
            for (unsigned c = 0; c < GRID; c++) {
                VertexInput *o = &v[r * GRID + c];
                memset(o, 0, sizeof *o);
                o->p[0] = SX[c]; o->p[1] = SY[r];
                o->p[2] = 0.15f + 0.18f * (float)r + 0.05f * (float)c;
                o->p[3] = 1.f; /* RHW == 1: no perspective shrink */
                unsigned rr = 48 + 40 * c, gg = 40 + 45 * r, bb = 170 - 20 * ((r + c) & 1);
                o->diffuse = 0xff000000u | (rr << 16) | (gg << 8) | bb;
            }
        }
        size_t stride = d8_fvf_stride(s.fvf);
        VertexInput vexp[256];
        for (unsigned i = 0; i < index_count; i++) vexp[i] = v[indices[i]];
        pack_vertices(s.fvf, vexp, index_count, expanded);
        DrawCase c = {"unlit_untextured", "FVF 0x44 (XYZRHW|DIFFUSE) untextured unlit, p.w=1, screen-space coords; CPU/GPU array draw, no index buffer", &s,
                      expanded, stride, index_count, NULL, 0, 4, index_count / 3, 0, no_textures, NULL, 2.0, 1, 0, 0};
        run_case(&c, images_dir);
    }
    /* ================= extra: unlit untextured XYZ FVF 0x42 ============= */
    {
        D8State s = base;
        s.fvf = 0x42; /* D3DFVF_XYZ|D3DFVF_DIFFUSE */
        s.rs[137] = 0;
        build_ortho_grid(v, s.fvf);
        pack_vertices(s.fvf, v, GRID * GRID, verts);
        DrawCase c = {"unlit_untextured_xyz", "FVF 0x42 (XYZ|DIFFUSE) untextured unlit; the XYZ flavor of the raster selftest quad", &s,
                      verts, d8_fvf_stride(s.fvf), GRID * GRID, indices, 2, 4, index_count / 3, 0,
                      no_textures, NULL, 2.0, 1, 0, 0};
        run_case(&c, images_dir);
    }
    /* ================= case 2: unlit textured FVF 0x142 ================= */
    {
        D8State s = base;
        s.fvf = 0x142;
        s.rs[137] = 0;
        build_ortho_grid(v, s.fvf);
        pack_vertices(s.fvf, v, GRID * GRID, verts);
        DrawCase c = {"unlit_textured", "FVF 0x142 XYZ+DIFFUSE+TEX0, COLOROP=MODULATE, texture_flipy=1", &s,
                      verts, d8_fvf_stride(s.fvf), GRID * GRID, indices, 2, 4, index_count / 3, 0,
                      one_texture, NULL, 2.0, 1, 0, 0};
        run_case(&c, images_dir);
    }
    /* ================= case 3: lit textured, ambient + directional ====== */
    {
        D8State s = base;
        s.fvf = 0x152; /* XYZ + NORMAL + DIFFUSE + TEX0 */
        s.rs[137] = 1;
        s.rs[141] = 1;
        s.rs[145] = 1; /* diffuse material source: vertex color */
        s.rs[139] = 0xff303840u;
        s.material[0] = 0.9f; s.material[1] = 0.85f; s.material[2] = 0.8f; s.material[3] = 1.f;
        s.material[4] = 0.20f; s.material[5] = 0.22f; s.material[6] = 0.25f; s.material[7] = 1.f;
        s.material[8] = 0.9f; s.material[9] = 0.9f; s.material[10] = 0.9f; s.material[11] = 1.f;
        s.material[12] = 0.02f; s.material[13] = 0.02f; s.material[14] = 0.03f; s.material[15] = 1.f;
        s.material[16] = 16.f;
        float world[16], ry[16], rx[16];
        rotate_y(ry, 0.2094f); rotate_x(rx, 0.1222f);
        d8_matrix_multiply(world, ry, rx);
        world[12] = 0.08f; world[13] = -0.06f;
        memcpy(s.transform[256], world, 64);
        view_matrix_perspective(s.transform[2]);
        perspective(s.transform[3], 1.2217f, 1.f, 0.5f, 10.f);
        float dir[3] = {0.45f, -0.55f, -0.70f}; normalize3(dir);
        { uint32_t d3d_light_type = 3; memcpy(&s.light[0][0], &d3d_light_type, 4); } /* D3DLIGHT_DIRECTIONAL */
        s.light[0][1] = 0.85f; s.light[0][2] = 0.75f; s.light[0][3] = 0.65f; s.light[0][4] = 1.f;
        s.light[0][5] = 0.7f; s.light[0][6] = 0.65f; s.light[0][7] = 0.55f; s.light[0][8] = 1.f;
        s.light[0][9] = 0.12f; s.light[0][10] = 0.14f; s.light[0][11] = 0.18f; s.light[0][12] = 1.f;
        s.light[0][13] = 0.f; s.light[0][14] = 0.f; s.light[0][15] = 0.f;
        s.light[0][16] = dir[0]; s.light[0][17] = dir[1]; s.light[0][18] = dir[2];
        s.light_on[0] = 1;
        build_perspective_grid(v);
        pack_vertices(s.fvf, v, GRID * GRID, verts);
        DrawCase c = {"lit_textured_directional", "FVF 0x152, LIGHTING+COLORVERTEX, ambient rs139 + one directional light, perspective", &s,
                      verts, d8_fvf_stride(s.fvf), GRID * GRID, indices, 2, 4, index_count / 3, 0,
                      one_texture, NULL, 4.0, 1, 0, 0};
        run_case(&c, images_dir);
    }
    /* ================= case 4: lit specular/power + local viewer ======== */
    {
        D8State s = base;
        s.fvf = 0xD2; /* XYZ + NORMAL + DIFFUSE + SPECULAR */
        s.rs[137] = 1;
        s.rs[141] = 1;
        s.rs[145] = 1; /* diffuse from vertex color */
        s.rs[146] = 1; /* specular material source: vertex color2 */
        s.rs[29] = 1;  /* SPECULARENABLE */
        s.rs[142] = 1; /* LOCALVIEWER */
        s.rs[139] = 0xff20262cu;
        s.material[4] = 0.18f; s.material[5] = 0.20f; s.material[6] = 0.24f; s.material[7] = 1.f;
        s.material[12] = 0.01f; s.material[13] = 0.01f; s.material[14] = 0.02f; s.material[15] = 1.f;
        s.material[16] = 24.f;
        float world[16], ry[16], rx[16];
        rotate_y(ry, 0.2094f); rotate_x(rx, 0.1222f);
        d8_matrix_multiply(world, ry, rx);
        world[12] = 0.08f; world[13] = -0.06f;
        memcpy(s.transform[256], world, 64);
        view_matrix_perspective(s.transform[2]);
        perspective(s.transform[3], 1.2217f, 1.f, 0.5f, 10.f);
        float dir[3] = {0.45f, -0.55f, -0.70f}; normalize3(dir);
        { uint32_t d3d_light_type = 3; memcpy(&s.light[0][0], &d3d_light_type, 4); } /* D3DLIGHT_DIRECTIONAL */
        s.light[0][1] = 0.50f; s.light[0][2] = 0.48f; s.light[0][3] = 0.45f; s.light[0][4] = 1.f;
        s.light[0][5] = 0.85f; s.light[0][6] = 0.82f; s.light[0][7] = 0.78f; s.light[0][8] = 1.f;
        s.light[0][9] = 0.06f; s.light[0][10] = 0.07f; s.light[0][11] = 0.09f; s.light[0][12] = 1.f;
        s.light[0][16] = dir[0]; s.light[0][17] = dir[1]; s.light[0][18] = dir[2];
        { uint32_t d3d_light_type = 1; memcpy(&s.light[1][0], &d3d_light_type, 4); } /* D3DLIGHT_POINT with attenuation */
        s.light[1][1] = 0.60f; s.light[1][2] = 0.50f; s.light[1][3] = 0.40f; s.light[1][4] = 1.f;
        s.light[1][5] = 0.9f; s.light[1][6] = 0.9f; s.light[1][7] = 0.9f; s.light[1][8] = 1.f;
        s.light[1][9] = 0.05f; s.light[1][10] = 0.05f; s.light[1][11] = 0.05f; s.light[1][12] = 1.f;
        s.light[1][13] = 0.4f; s.light[1][14] = 0.5f; s.light[1][15] = 0.8f;
        s.light[1][19] = 8.f; s.light[1][20] = 2.f;
        s.light[1][21] = 1.f; s.light[1][22] = 0.15f; s.light[1][23] = 0.02f;
        s.light_on[0] = 1; s.light_on[1] = 1;
        build_perspective_grid(v);
        pack_vertices(s.fvf, v, GRID * GRID, verts);
        DrawCase c = {"lit_specular_localviewer", "FVF 0xD2, SPECULARENABLE+LOCALVIEWER+power 24, directional+point light", &s,
                      verts, d8_fvf_stride(s.fvf), GRID * GRID, indices, 2, 4, index_count / 3, 0,
                      no_textures, NULL, 4.0, 1, 0, 0};
        run_case(&c, images_dir);
        /* Immediate identical redraw: the per-program uniform caches and the
         * raster/attribute/texture owner epochs must be idempotent, so a cached
         * second draw has to stay equivalent too. */
        DrawCase repeat = {"lit_specular_localviewer_repeat", "same D8State/geometry as lit_specular_localviewer drawn immediately again; exercises uniform-cache/raster owner-epoch idempotency", &s,
                           verts, d8_fvf_stride(s.fvf), GRID * GRID, indices, 2, 4, index_count / 3, 0,
                           no_textures, NULL, 4.0, 1, 0, 0};
        run_case(&repeat, images_dir);
        /* Deferred lighting uploads must catch up after unlit transforms,
         * even when the next lit draw has exactly the same matrices. */
        s.rs[137]=0;
        rotate_y(s.transform[256], -0.31f);
        s.transform[256][12]=-0.12f;
        s.transform[2][12]+=0.17f;
        DrawCase transition=repeat;
        transition.name="unlit_changed_light_transforms";
        transition.description="unlit draw changes world and view before lighting resumes";
        run_case(&transition, images_dir);
        s.rs[137]=1;
        transition.name="relit_deferred_transforms";
        transition.description="lighting resumes with unchanged matrices after unlit transform changes";
        run_case(&transition, images_dir);
    }
    /* ================= extra: multi-light with point + spot branches ==== */
    {
        D8State s = base;
        s.fvf = 0xD2; /* XYZ + NORMAL + DIFFUSE + SPECULAR */
        s.rs[137] = 1;
        s.rs[141] = 1;
        s.rs[145] = 1;  /* diffuse from vertex color */
        s.rs[146] = 1;  /* specular from vertex color2 */
        s.rs[29] = 1;
        s.rs[139] = 0xff101418u;
        for (int i = 0; i < 4; i++) s.material[i] = 0.35f;
        s.material[8] = 0.8f; s.material[9] = 0.8f; s.material[10] = 0.8f; s.material[11] = 1.f;
        s.material[16] = 32.f;
        float world[16], ry[16], rx[16];
        rotate_y(ry, 0.2094f); rotate_x(rx, 0.1222f);
        d8_matrix_multiply(world, ry, rx);
        world[12] = 0.08f; world[13] = -0.06f;
        memcpy(s.transform[256], world, 64);
        view_matrix_perspective(s.transform[2]);
        perspective(s.transform[3], 1.2217f, 1.f, 0.5f, 10.f);
        /* light 0: directional. D3DLIGHT8.Direction points from the light into
         * the scene; the renderer negates it to get the surface-to-light vector,
         * so -z here yields a +z lighting direction toward the +z-facing normals. */
        float dir[3] = {0.45f, -0.55f, -0.70f}; normalize3(dir);
        set_light_type(&s, 0, 3);
        s.light[0][1] = 0.40f; s.light[0][2] = 0.35f; s.light[0][3] = 0.30f;
        s.light[0][5] = 0.50f; s.light[0][6] = 0.50f; s.light[0][7] = 0.50f;
        s.light[0][9] = 0.05f; s.light[0][10] = 0.05f; s.light[0][11] = 0.06f;
        s.light[0][16] = dir[0]; s.light[0][17] = dir[1]; s.light[0][18] = dir[2];
        /* light 1: point inside range */
        set_light_type(&s, 1, 1);
        s.light[1][1] = 0.50f; s.light[1][2] = 0.40f; s.light[1][3] = 0.30f;
        s.light[1][5] = 0.60f; s.light[1][6] = 0.60f; s.light[1][7] = 0.60f;
        s.light[1][9] = 0.03f; s.light[1][10] = 0.03f; s.light[1][11] = 0.03f;
        s.light[1][13] = 0.4f; s.light[1][14] = 0.5f; s.light[1][15] = 5.5f;
        s.light[1][19] = 8.f; s.light[1][20] = 2.f;
        s.light[1][21] = 1.f; s.light[1][22] = 0.10f; s.light[1][23] = 0.02f;
        /* light 2: spot cone covering the geometry */
        set_light_type(&s, 2, 2);
        s.light[2][1] = 0.30f; s.light[2][2] = 0.35f; s.light[2][3] = 0.40f;
        s.light[2][5] = 0.40f; s.light[2][6] = 0.40f; s.light[2][7] = 0.40f;
        s.light[2][9] = 0.02f; s.light[2][10] = 0.02f; s.light[2][11] = 0.02f;
        s.light[2][13] = 0.3f; s.light[2][14] = 0.4f; s.light[2][15] = 5.5f;
        s.light[2][16] = 0.f; s.light[2][17] = 0.f; s.light[2][18] = -1.f; /* spot axis toward scene */
        s.light[2][19] = 10.f; s.light[2][20] = 2.f;
        s.light[2][21] = 1.f; s.light[2][22] = 0.05f; s.light[2][23] = 0.01f;
        s.light[2][24] = 0.6f; s.light[2][25] = 1.6f;
        /* light 3: point whose range excludes every vertex (exercises continue) */
        set_light_type(&s, 3, 1);
        s.light[3][1] = 1.f; s.light[3][2] = 1.f; s.light[3][3] = 1.f;
        s.light[3][5] = 1.f; s.light[3][6] = 1.f; s.light[3][7] = 1.f;
        s.light[3][13] = 5.0f; s.light[3][14] = 0.f; s.light[3][15] = 0.f;
        s.light[3][19] = 1.f; /* range too small: every vertex skips this light */
        s.light[3][21] = 1.f;
        build_perspective_grid(v);
        pack_vertices(s.fvf, v, GRID * GRID, verts);
        DrawCase c = {"lit_spot_multi_light", "FVF 0xD2, 4 enabled lights: directional, point (in range, attenuation), spot cone (theta/phi/falloff), point with range cutoff; SPECULARENABLE + power 32", &s,
                      verts, d8_fvf_stride(s.fvf), GRID * GRID, indices, 2, 4, index_count / 3, 0,
                      no_textures, NULL, 4.0, 1, 0, 0};
        run_case(&c, images_dir);
    }
    /* ================= extra: all eight light slots enabled ============= */
    {
        D8State s = base;
        s.fvf = 0x152; /* XYZ + NORMAL + DIFFUSE + TEX0 */
        s.rs[137] = 1;
        s.rs[141] = 1;
        s.rs[29] = 1;
        s.rs[139] = 0xff181c20u;
        for (int i = 0; i < 4; i++) s.material[i] = 0.30f;
        s.material[4] = 0.15f; s.material[5] = 0.16f; s.material[6] = 0.18f; s.material[7] = 1.f;
        s.material[8] = 0.7f; s.material[9] = 0.7f; s.material[10] = 0.7f; s.material[11] = 1.f;
        s.material[16] = 12.f;
        float world[16], ry[16], rx[16];
        rotate_y(ry, 0.1745f); rotate_x(rx, 0.0873f);
        d8_matrix_multiply(world, ry, rx);
        memcpy(s.transform[256], world, 64);
        view_matrix_perspective(s.transform[2]);
        perspective(s.transform[3], 1.2217f, 1.f, 0.5f, 10.f);
        for (int i = 0; i < 8; i++) {
            uint32_t type = (uint32_t)(i % 3 == 0 ? 3 : (i % 3 == 1 ? 1 : 2));
            float phase = 0.4f + 0.11f * (float)i;
            set_light_type(&s, i, type);
            s.light[i][1] = 0.10f + 0.05f * (float)i;
            s.light[i][2] = 0.20f;
            s.light[i][3] = 0.12f + 0.03f * (float)i;
            s.light[i][5] = 0.25f; s.light[i][6] = 0.25f; s.light[i][7] = 0.25f;
            s.light[i][9] = 0.010f; s.light[i][10] = 0.008f; s.light[i][11] = 0.012f;
            float ld[3] = {0.45f - 0.04f * (float)i, -0.55f + 0.03f * (float)i, -0.70f};
            normalize3(ld);
            s.light[i][16] = ld[0]; s.light[i][17] = ld[1]; s.light[i][18] = ld[2];
            s.light[i][13] = 1.2f * cosf(phase); s.light[i][14] = 0.9f * sinf(phase);
            s.light[i][15] = 5.2f + 0.05f * (float)i;
            s.light[i][19] = 6.f + (float)i; s.light[i][20] = 2.f;
            s.light[i][21] = 1.f; s.light[i][22] = 0.08f; s.light[i][23] = 0.015f;
            s.light[i][24] = 0.5f + 0.05f * (float)i; s.light[i][25] = 1.4f;
        }
        build_perspective_grid(v);
        pack_vertices(s.fvf, v, GRID * GRID, verts);
        DrawCase c = {"lit_eight_lights", "FVF 0x152 textured, all 8 light slots enabled (directional/point/spot mix) exercising lights[56] and enabled[2] uniform packing, SPECULARENABLE", &s,
                      verts, d8_fvf_stride(s.fvf), GRID * GRID, indices, 2, 4, index_count / 3, 0,
                      one_texture, NULL, 4.0, 1, 0, 0};
        run_case(&c, images_dir);
    }
    /* ================= case 5: linear table fog over a depth range ====== */
    {
        D8State s = base;
        s.fvf = 0x142;
        s.rs[137] = 0;
        s.rs[28] = 1;   /* D3DRS_FOGENABLE */
        s.rs[35] = 3;   /* D3DRS_FOGTABLEMODE = D3DFOG_LINEAR */
        s.rs[34] = 0xff405060u; /* D3DRS_FOGCOLOR */
        float start = 0.15f, end = 0.85f, density = 1.f;
        memcpy(&s.rs[36], &start, 4);
        memcpy(&s.rs[37], &end, 4);
        memcpy(&s.rs[38], &density, 4);
        build_ortho_grid(v, s.fvf);
        pack_vertices(s.fvf, v, GRID * GRID, verts);
        DrawCase c = {"fog_linear_depth", "FOGENABLE rs28=1, FOGTABLEMODE rs35=3 linear, fogcolor rs34, start/end rs36/37 over view z 0.15..0.84", &s,
                      verts, d8_fvf_stride(s.fvf), GRID * GRID, indices, 2, 4, index_count / 3, 0,
                      one_texture, NULL, 4.0, 1, 0, 0};
        run_case(&c, images_dir);
    }
    /* ================= case 6: exponential-squared table fog ============ */
    {
        D8State s = base;
        s.fvf = 0x142;
        s.rs[137] = 0;
        s.rs[28] = 1;
        s.rs[35] = 2; /* D3DFOG_EXP2 */
        s.rs[34] = 0xff2050a0u;
        float start = 0.15f, end = 0.85f, density = 1.8f;
        memcpy(&s.rs[36], &start, 4);
        memcpy(&s.rs[37], &end, 4);
        memcpy(&s.rs[38], &density, 4);
        build_ortho_grid(v, s.fvf);
        pack_vertices(s.fvf, v, GRID * GRID, verts);
        DrawCase c = {"fog_exp2_density", "FOGENABLE rs28=1, FOGTABLEMODE rs35=2 EXP2, density rs38=1.8 over view z 0.15..0.84", &s,
                      verts, d8_fvf_stride(s.fvf), GRID * GRID, indices, 2, 4, index_count / 3, 0,
                      one_texture, NULL, 4.0, 1, 0, 0};
        run_case(&c, images_dir);
    }
    /* ================= case 7: RHW screen-space vertices with p.w ======= */
    {
        D8State s = base;
        s.fvf = 0x44; /* XYZRHW + DIFFUSE */
        s.rs[137] = 0;
        /* Non-default depth range: the RHW branch maps p.z through viewport[4..5]. */
        float zmin = 0.25f, zmax = 0.75f;
        memcpy(&s.viewport[4], &zmin, 4);
        memcpy(&s.viewport[5], &zmax, 4);
        static const float PX[GRID] = {10.3f, 25.7f, 40.9f, 54.6f};
        static const float PY[GRID] = {9.8f, 24.3f, 39.6f, 52.7f};
        for (unsigned r = 0; r < GRID; r++) {
            for (unsigned c = 0; c < GRID; c++) {
                VertexInput *o = &v[r * GRID + c];
                memset(o, 0, sizeof *o);
                o->p[0] = PX[c];
                o->p[1] = PY[r];
                o->p[2] = 0.35f + 0.10f * (float)c + 0.06f * (float)r; /* inside zrange 0.25..0.75 */
                o->p[3] = 0.80f + 0.05f * (float)r + 0.10f * (float)c; /* RHW != 1 */
                unsigned rr = 60 + 40 * c, gg = 40 + 45 * r, bb = 150;
                o->diffuse = 0xff000000u | (rr << 16) | (gg << 8) | bb;
            }
        }
        pack_vertices(s.fvf, v, GRID * GRID, verts);
        DrawCase c = {"rhw_screen_space", "FVF 0x44 XYZRHW+DIFFUSE, screen pixel coords, per-vertex RHW 0.8..1.25 (p.w!=1), depth range 0.25..0.75", &s,
                      verts, d8_fvf_stride(s.fvf), GRID * GRID, indices, 2, 4, index_count / 3, 0,
                      no_textures, NULL, 2.0, 1, 0, 0};
        run_case(&c, images_dir);
    }
    /* ================= case 8: two stages, projected texgen matrix ===== */
    {
        D8State s = base;
        s.fvf = 0x10242; /* XYZ + DIFFUSE + TEX0(3D) + TEX1(2D) */
        s.rs[137] = 0;
        s.tss[0][1] = 4; s.tss[0][2] = 2; s.tss[0][3] = 1; /* COLOROP=MODULATE, arg1=TEXTURE, arg2=CURRENT */
        s.tss[0][4] = 2; s.tss[0][5] = 2; s.tss[0][6] = 1; /* ALPHAOP=SELECTARG1, arg1=TEXTURE */
        s.tss[0][11] = 0;    /* TEXCOORDINDEX=0 */
        s.tss[0][24] = 0x103; /* TEXTURETRANSFORMFLAGS: PROJECTED | COUNT3 */
        s.tss[1][1] = 4; s.tss[1][2] = 2; s.tss[1][3] = 1;
        s.tss[1][4] = 2; s.tss[1][5] = 2; s.tss[1][6] = 1;
        s.tss[1][11] = 1;    /* second stage samples uv1 */
        s.tss[1][24] = 0;
        s.texture_flipy[0] = 0;
        s.texture_flipy[1] = 1;
        float m0[16], m1[16];
        identity4(m0);
        m0[0] = 0.55f; m0[1] = 0.08f;
        m0[4] = -0.06f; m0[5] = 0.62f;
        m0[8] = 0.02f; m0[9] = 0.05f; m0[10] = 0.80f;
        m0[12] = 0.07f; m0[13] = 0.12f; m0[14] = 0.60f;
        identity4(m1);
        m1[0] = 0.32f; m1[5] = 0.41f; m1[12] = 0.21f; m1[13] = 0.18f;
        memcpy(s.transform[16], m0, 64);
        memcpy(s.transform[17], m1, 64);
        build_ortho_grid(v, s.fvf);
        pack_vertices(s.fvf, v, GRID * GRID, verts);
        DrawCase c = {"two_stage_projected_texgen", "FVF 0x10242, 2 stages: stage0 projected COUNT3 via texmat0 (tss[0][24]=0x103), stage1 samples uv1 via texmat1 + flipy", &s,
                      verts, d8_fvf_stride(s.fvf), GRID * GRID, indices, 2, 4, index_count / 3, 0,
                      two_textures, NULL, 2.0, 1, 0, 0};
        run_case(&c, images_dir);
    }
    /* ================= case 9: GPU-only decline -> CPU fallback ========= */
    {
        D8State s = base;
        s.fvf = 0x142;
        s.rs[137] = 0;
        build_ortho_grid(v, s.fvf);
        pack_vertices(s.fvf, v, GRID * GRID, verts);
        D8NativeBuffers native;
        memset(&native, 0, sizeof native);
        native.vertex = g_native_small_vbo;
        native.vertex_offset = 0;
        native.vertex_bytes = 8; /* too small: gpu_prepare rejects before GL mutations */
        DrawCase c = {"fallback_gpu_declined", "same unlit textured draw, but the GPU renderer gets a native vertex buffer whose 8-byte allocation cannot hold the 16 indexed vertices; gpu_prepare declines and the CPU reference path must fill the frame", &s,
                      verts, d8_fvf_stride(s.fvf), GRID * GRID, indices, 2, 4, index_count / 3, 0,
                      one_texture, &native, 2.0, 0, 1, 0};
        run_case(&c, images_dir);
    }
    /* ================= diagnostic: singular view + LOCALVIEWER ==========
     * Deliberate edge case: a singular view matrix (determinant below the
     * renderer's 1e-20 inversion threshold) with LOCALVIEWER enabled and
     * specular lighting. The CPU reference (light_vertex/cpu_lighting_prepare)
     * keeps the fixed-function eye when inverse3(view) fails; make_gpu_program()
     * switches on the LOCALVIEWER uniform alone and the uploaded camera term
     * stays (0,0,0) when the inversion fails, so the specular eye vector
     * diverges. Normals face the camera and the diffuse term is dark so the
     * specular term dominates the pixel delta. */
    {
        D8State s = base;
        s.fvf = 0xD2; /* XYZ + NORMAL + DIFFUSE + SPECULAR */
        s.rs[137] = 1;
        s.rs[141] = 1;
        s.rs[145] = 0; /* diffuse material source: D3DMCS_MATERIAL */
        s.rs[146] = 0; /* specular material source: D3DMCS_MATERIAL */
        s.rs[29] = 1;
        s.rs[142] = 1; /* LOCALVIEWER */
        s.rs[139] = 0xff000000u;
        for (int i = 0; i < 4; i++) s.material[i] = 0.05f;       /* dark diffuse */
        s.material[8] = 1.f; s.material[9] = 1.f; s.material[10] = 1.f; s.material[11] = 1.f; /* specular */
        s.material[16] = 24.f;
        identity4(s.transform[256]);
        view_matrix_ortho(s.transform[2]);
        s.transform[2][10] = 1e-21f; /* det below threshold: CPU keeps fixed eye, GPU camera stays (0,0,0) */
        float dir[3] = {-0.45f, 0.55f, 0.70f}; normalize3(dir); /* renderer negates Direction */
        { uint32_t d3d_light_type = 3; memcpy(&s.light[0][0], &d3d_light_type, 4); } /* D3DLIGHT_DIRECTIONAL */
        s.light[0][1] = 0.05f; s.light[0][2] = 0.05f; s.light[0][3] = 0.05f; s.light[0][4] = 1.f;
        s.light[0][5] = 1.f; s.light[0][6] = 1.f; s.light[0][7] = 1.f; s.light[0][8] = 1.f;
        s.light[0][9] = 0.02f; s.light[0][10] = 0.02f; s.light[0][11] = 0.02f; s.light[0][12] = 1.f;
        s.light[0][16] = dir[0]; s.light[0][17] = dir[1]; s.light[0][18] = dir[2];
        s.light_on[0] = 1;
        build_ortho_grid(v, s.fvf);
        for (unsigned r = 0; r < GRID; r++) {
            for (unsigned c = 0; c < GRID; c++) {
                VertexInput *o = &v[r * GRID + c];
                float n[3] = {-0.25f + 0.20f * (float)c, -0.20f + 0.15f * (float)r, -1.f};
                normalize3(n);
                o->n[0] = n[0]; o->n[1] = n[1]; o->n[2] = n[2];
            }
        }
        pack_vertices(s.fvf, v, GRID * GRID, verts);
        DrawCase c = {"diag_singular_view_localviewer", "diagnostic: LOCALVIEWER + singular view matrix (det 1e-21). CPU reference falls back to the fixed-function eye; GPU shader uses camera=(0,0,0)", &s,
                      verts, d8_fvf_stride(s.fvf), GRID * GRID, indices, 2, 4, index_count / 3, 0,
                      no_textures, NULL, 4.0, 1, 0, 1};
        run_case(&c, images_dir);
    }

    /* ---- states rejected by shared validation: never reach either backend's
     * preparation, hence cannot reach the new GPU fallback branch. Report the
     * truth instead of claiming the fallback was proven by these states. ---- */
    {
        D8State s = base;
        s.fvf = 0x142;
        s.rs[137] = 0;
        build_ortho_grid(v, s.fvf);
        pack_vertices(s.fvf, v, GRID * GRID, verts);
        D8State bad = s;
        bad.tss[0][11] = 9; /* D3DTTFF/texture coordinate index out of range for any fixed-function stage */
        probe_shared_rejection("invalid_texcoord_index_9", &bad, verts, d8_fvf_stride(bad.fvf), GRID * GRID, one_texture);
        D8State bad2 = s;
        bad2.tss[0][1] = 17; /* COLOROP outside the implemented D3DTOP set */
        probe_shared_rejection("unsupported_colorop_17", &bad2, verts, d8_fvf_stride(bad2.fvf), GRID * GRID, one_texture);
    }

    /* ---- diagnostics log accounting ---- */
    char captured[65536];
    size_t captured_len = stderr_capture_end(captured, sizeof captured);
    unsigned declined = count_substring(captured, "GPU vertex path declined a draw");
    unsigned gpu_enabled = count_substring(captured, "optional GPU vertex path enabled");
    unsigned gpu_unavailable = count_substring(captured, "GPU vertex shader unavailable");
    printf("fallback.declined_draw_log_count=%u expected=1\n", declined);
    printf("fallback.gpu_path_enabled_log_count=%u expected=1 gpu_shader_unavailable=%u expected=0\n", gpu_enabled, gpu_unavailable);
    printf("fallback.declined_draw_message=%s\n",
           strstr(captured, "GPU vertex path declined a draw") ? "present" : "absent");
    printf("fallback.declined_draw_excerpt=");
    const char *ex = strstr(captured, "GPU vertex path declined a draw");
    if (ex) { const char *nl = strchr(ex, '\n'); int n = nl ? (int)(nl - ex + 1) : (int)strlen(ex); fwrite(ex, 1, (size_t)n, stdout); }
    printf("\n");
    if (captured_len) printf("--- captured renderer stderr ---\n%s--- end captured renderer stderr ---\n", captured);

    /* ---- overall verdict: required (non-diagnostic) cases only ---- */
    unsigned required = 0, required_pass = 0, diag_pass = 0, diag_total = 0;
    for (unsigned i = 0; i < g_result_count; i++) {
        if (g_results[i].diagnostic) { diag_total++; if (g_results[i].pass) diag_pass++; }
        else { required++; if (g_results[i].pass) required_pass++; }
    }
    int fallback_ok = 0;
    for (unsigned i = 0; i < g_result_count; i++)
        if (!strcmp(g_results[i].name, "fallback_gpu_declined"))
            fallback_ok = g_results[i].pass && g_results[i].gpu_rc == 0 && g_results[i].gpu_rc2 == 0 &&
                          g_results[i].max_any == 0.0 && !g_results[i].used_gpu_path &&
                          g_results[i].covered_pixels > 0 &&
                          strstr(g_results[i].renderer_error, "native vertex buffer range exceeds allocation") != NULL;
    int logging_ok = declined == 1 && gpu_enabled == 1 && gpu_unavailable == 0;
    int shared_reject_ok = 1;
    for (unsigned i = 0; i < g_reject_count; i++)
        if (g_reject[i].cpu_rc == 0 || g_reject[i].gpu_rc == 0) shared_reject_ok = 0;
    int overall = required == required_pass && fallback_ok && logging_ok && shared_reject_ok;
    printf("required_cases=%u/%u pass, diagnostic_cases=%u/%u pass (diagnostic failures are documented edge cases)\n",
           required_pass, required, diag_pass, diag_total);
    printf("overall=%s\n", overall ? "PASS" : "FAIL");
    fflush(stdout);

    /* ---- result.json ---- */
    {
        char json_path[PATH_MAX_LOCAL];
        snprintf(json_path, sizeof json_path, "%s/result.json", evidence_dir);
        FILE *f = fopen(json_path, "w");
        if (f) {
            time_t now = time(NULL);
            struct tm tm_utc;
            gmtime_r(&now, &tm_utc);
            char stamp[32];
            strftime(stamp, sizeof stamp, "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
            fprintf(f, "{\n");
            fprintf(f, "  \"task\": \"task-4\",\n");
            fprintf(f, "  \"test\": \"d3d8_gpu_vs_cpu_renderer_equivalence\",\n");
            fprintf(f, "  \"evidence_kind\": \"HOST x86_64 Linux + Xvfb + Mesa llvmpipe GLES software rasterization; NOT H700/Mali device evidence\",\n");
            fprintf(f, "  \"generated_utc\": \"%s\",\n", stamp);
            fprintf(f, "  \"device_claim\": \"none\",\n");
            fprintf(f, "  \"context_note\": \"SDL2 window context requested as GLES2; Mesa reports OpenGL ES 3.2, so d8_renderer_draw_native's uint32-index GPU path is available (it checks GL_VERSION for 'OpenGL ES 3') and was not the fallback path exercised here. SDL had to be forced onto EGL (SDL_VIDEO_X11_FORCE_EGL=1) because this host's Xvfb GLX path fails with GLXBadProfileARB; all rendering is still Mesa llvmpipe software on the x86_64 host CPU.\",\n");
            fprintf(f, "  \"gl\": {\"version\": \"%s\", \"renderer\": \"%s\", \"vendor\": \"%s\", \"shading_language\": \"%s\"},\n",
                    gl_version ? gl_version : "?", gl_renderer ? gl_renderer : "?", gl_vendor ? gl_vendor : "?", gl_sl ? gl_sl : "?");
            fprintf(f, "  \"initialization_order_note\": \"d8_renderer_init() reads DS_GPU_VERTEX (alias THING_GPU_VERTEX, alias wins) once. The CPU reference renderer was initialized with both variables unset, then DS_GPU_VERTEX=1 was set and the GPU renderer was initialized in the SAME GLES context, then the variable was unset. Two renderer instances interleave draws via the renderer's raster/attribute/texture owner epochs.\",\n");
            fprintf(f, "  \"tolerance_units\": \"8-bit channel absolute delta (0..255)\",\n");
            fprintf(f, "  \"tolerances\": {\"unlit\": 2.0, \"unlit_textured\": 2.0, \"lit\": 4.0, \"fog\": 4.0, \"rhw\": 2.0, \"two_stage\": 2.0, \"fallback\": 2.0},\n");
            fprintf(f, "  \"fog_state_note\": \"D3D8 D3DRS_FOGENABLE is rs[28] and D3DRS_FOGTABLEMODE is rs[35]; the renderer uses rs28 as the enable and rs35 as the table mode (3=LINEAR, 2=EXP2) with fog color rs34 and start/end/density rs36/37/38. The fog cases set both rs28=1 and the rs35 mode, exactly as the renderer consumes them.\",\n");
            fprintf(f, "  \"cases\": [\n");
            for (unsigned i = 0; i < g_result_count; i++) {
                CaseResult *r = &g_results[i];
                fprintf(f, "    {\"name\": \"%s\", \"description\": \"%s\", \"fvf\": %u, \"tolerance\": %.1f, \"diag\": %d, \"result\": \"%s\", \"cpu_rc\": %d, \"gpu_rc\": %d, \"gpu_rc2\": %d, \"used_gpu_program\": %d, \"max_diff\": %.0f, \"max_diff_rgba\": [%.0f, %.0f, %.0f, %.0f], \"mean_diff\": %.5f, \"diff_pixels\": %u, \"pixels_over_tolerance\": %u, \"covered_pixels\": %u, \"renderer_error\": \"%s\", \"images\": \"images/%s_{cpu,gpu%s}.png\"}%s\n",
                        r->name, r->description, r->fvf, r->tolerance, r->diagnostic, r->pass ? "PASS" : "FAIL",
                        r->cpu_rc, r->gpu_rc, r->gpu_rc2, r->used_gpu_path, r->max_any,
                        r->max_r, r->max_g, r->max_b, r->max_a, r->mean_any, r->diff_pixels,
                        r->over_tolerance, r->covered_pixels, r->renderer_error, r->name, r->max_any > 0.5 ? ",diff" : "",
                        i + 1 < g_result_count ? "," : "");
            }
            fprintf(f, "  ],\n");
            CaseResult *fb = NULL;
            for (unsigned i = 0; i < g_result_count; i++)
                if (!strcmp(g_results[i].name, "fallback_gpu_declined")) fb = &g_results[i];
            {
                fprintf(f, "  \"fallback\": {\"case\": \"fallback_gpu_declined\", \"gpu_only_decline\": \"invalid native vertex buffer bounds: gpu_prepare rejects before its GL mutations; the CPU reference path does not use native buffers\", \"fallback_used_cpu_program\": %d, \"cpu_reference_pixels_equal\": %d, \"max_diff\": %.0f, \"covered_pixels\": %u, \"second_declined_draw_also_rendered\": %d, \"decline_reason\": \"%s\", \"declined_draw_log_count\": %u, \"declined_draw_logged_once\": %d, \"gpu_path_enabled_log_count\": %u},\n",
                        (fb && !fb->used_gpu_path && fb->gpu_program_ok) ? 1 : 0,
                        (fb && fb->max_any == 0.0 && fb->covered_pixels > 0) ? 1 : 0,
                        fb ? fb->max_any : -1.0, fb ? fb->covered_pixels : 0u,
                        (fb && fb->gpu_rc2 == 0 && fb->gpu_program_ok2) ? 1 : 0,
                        fb ? fb->renderer_error : "",
                        declined, declined == 1 ? 1 : 0, gpu_enabled);
            }
            fprintf(f, "  \"shared_rejection\": {\"note\": \"Invalid texcoord index / unsupported COLOROP are rejected by d8_renderer_draw_native's shared pre-validation before either backend is selected, so they cannot reach the new GPU-decline fallback branch; the fallback branch is exercised by the GPU-only native-buffer bound violation above.\", \"probes\": [\n");
            for (unsigned i = 0; i < g_reject_count; i++)
                fprintf(f, "    {\"name\": \"%s\", \"cpu_rc\": %d, \"gpu_rc\": %d, \"cpu_error\": \"%s\", \"gpu_error\": \"%s\"}%s\n",
                        g_reject[i].name, g_reject[i].cpu_rc, g_reject[i].gpu_rc,
                        g_reject[i].cpu_error, g_reject[i].gpu_error, i + 1 < g_reject_count ? "," : "");
            fprintf(f, "  ]},\n");
            fprintf(f, "  \"diagnostics\": {\n");
            fprintf(f, "    \"singular_view_localviewer\": {\"expected_divergence\": true, \"suspected_cause\": \"runtime/d3d8_renderer.c:210 (make_gpu_program vertex shader) selects the local-viewer eye purely from the LOCALVIEWER uniform (rs142): 'vec3 e=localviewer?norm(camera-wp.xyz):norm(eye);'. The CPU reference (light_vertex, :178) keeps the fixed-function eye unless inverse3(view) succeeds (cpu_lighting_prepare, :167, sets view_valid = rs142 && !inverse3(view)); when inversion fails the uploaded camera stays (0,0,0) because gpu_prepare only fills it on success (:286-288, float camera[3]={0}). Specular highlights therefore diverge for non-invertible view matrices.\"},\n");
            fprintf(f, "    \"host_note\": \"All numbers are host Mesa llvmpipe evidence for renderer equivalence; no H700/Mali, driver, or performance conclusion may be drawn from them.\"\n");
            fprintf(f, "  },\n");
            {
                unsigned required_diag = 0, diag_diverged = 0;
                double max_required = 0.0, max_diag = 0.0;
                for (unsigned i = 0; i < g_result_count; i++) {
                    if (g_results[i].diagnostic) {
                        required_diag++;
                        if (!g_results[i].pass) diag_diverged++;
                        if (g_results[i].max_any > max_diag) max_diag = g_results[i].max_any;
                    } else if (g_results[i].max_any > max_required) max_required = g_results[i].max_any;
                }
                fprintf(f, "  \"findings\": [\n");
                fprintf(f, "    \"%u of %u required cases draw the identical D8State/geometry through the optional GPU vertex program and the CPU reference program; the largest channel delta across required cases is %.0f/255 (llvmpipe computes both programs in the same context, so a correct shader is expected to be bit-identical or within 1/255 float-order noise).\",\n", required_pass, required, max_required);
                fprintf(f, "    \"The GPU program was actually selected for every required functional case (GL_CURRENT_PROGRAM == gpu_program after the draw), so the comparisons exercise make_gpu_program() and gpu_prepare(), not just the CPU path.\",\n");
                fprintf(f, "    \"GPU-only decline + fallback: a native vertex buffer whose 8-byte allocation cannot hold the 16 indexed vertices makes gpu_prepare() decline before any GL mutation; the CPU reference path then draws the frame with pixels identical to a normal CPU reference draw (max delta %.0f, %u covered pixels), for both the first and the repeated declined draw. The decline is logged exactly once (%u log line) as designed.\",\n", fb ? fb->max_any : -1.0, fb ? fb->covered_pixels : 0u, declined);
                fprintf(f, "    \"Invalid texcoord index and unsupported COLOROP states are rejected by d8_renderer_draw_native's shared pre-validation for BOTH backends (draw returns -1), so they do not reach the GPU-decline fallback branch; geometry is still dropped for those states. That is a separate lead-owned concern, not a GPU-vs-CPU equivalence difference.\",\n");
                fprintf(f, "    \"%u of %u diagnostic edge cases diverged (max channel delta %.0f/255): LOCALVIEWER combined with a singular view matrix (determinant below the renderer's 1e-20 inversion threshold). CPU keeps the fixed-function eye; the GPU shader switches to camera=(0,0,0) because the camera uniform is only filled when inverse3(view) succeeds. Exact shader expression: runtime/d3d8_renderer.c:210 'vec3 e=localviewer?norm(camera-wp.xyz):norm(eye);' vs CPU runtime/d3d8_renderer.c:167 'cache->view_valid=s->rs[142]&&!inverse3(...)' and :178 'if(cache->view_valid)...'; the camera uniform is only filled when inverse3 succeeds (runtime/d3d8_renderer.c:286-288, float camera[3]={0}). Only reachable with a non-invertible view matrix.\"\n", diag_diverged, required_diag, max_diag);
            }
            fprintf(f, "  ],\n");
            fprintf(f, "  \"overall\": \"%s\",\n", overall ? "PASS" : "FAIL");
            fprintf(f, "  \"required_cases\": \"%u/%u\",\n", required_pass, required);
            fprintf(f, "  \"diagnostic_cases\": \"%u/%u\"\n", diag_pass, diag_total);
            fprintf(f, "}\n");
            fclose(f);
        }
    }
    /* ---- findings.md (concise human-readable list) ---- */
    {
        char md_path[PATH_MAX_LOCAL];
        snprintf(md_path, sizeof md_path, "%s/findings.md", evidence_dir);
        FILE *f = fopen(md_path, "w");
        if (f) {
            fprintf(f, "# GPU-vs-CPU renderer equivalence (task-4)\n\n");
            fprintf(f, "HOST EVIDENCE: x86_64 Linux + Xvfb + Mesa llvmpipe GLES (software). NOT H700/Mali device evidence.\n\n");
            fprintf(f, "GL: %s / %s / %s\n\n", gl_version ? gl_version : "?", gl_renderer ? gl_renderer : "?", gl_vendor ? gl_vendor : "?");
            fprintf(f, "| case | result | fvf | tol | max diff | mean diff | diff px | covered px |\n");
            fprintf(f, "|------|--------|-----|-----|----------|-----------|---------|------------|\n");
            for (unsigned i = 0; i < g_result_count; i++) {
                CaseResult *r = &g_results[i];
                fprintf(f, "| %s%s | %s | %#06x | %.0f | %.0f | %.4f | %u | %u |\n",
                        r->name, r->diagnostic ? " (diagnostic)" : "", r->pass ? "PASS" : "FAIL",
                        r->fvf, r->tolerance, r->max_any, r->mean_any, r->diff_pixels, r->covered_pixels);
            }
            double max_required_md = 0.0;
            for (unsigned i = 0; i < g_result_count; i++)
                if (!g_results[i].diagnostic && g_results[i].max_any > max_required_md) max_required_md = g_results[i].max_any;
            fprintf(f, "Required cases: %u/%u PASS (max channel delta across required cases %.0f/255).\n", required_pass, required, max_required_md);
            fprintf(f, "\nHOST GL: SDL was forced onto EGL (SDL_VIDEO_X11_FORCE_EGL=1) because this Xvfb GLX stack fails with GLXBadProfileARB; GL is Mesa llvmpipe software (see result.json gl/context_note). Mesa reports OpenGL ES 3.2 for the GLES2 context request, so the renderer's uint32-index GPU path exists but is not exercised here.\n");
            fprintf(f, "\nFallback: GPU path declined the draw (native vertex buffer out of range), CPU reference path used, pixels identical to a normal CPU draw (max delta 0), decline logged once (%u log line(s) over two declined draws).\n", declined);
            fprintf(f, "\nShared rejection: invalid texcoord index and unsupported COLOROP are rejected for BOTH backends before backend selection, so they cannot reach the GPU fallback branch.\n");
            fprintf(f, "\nEdge-case bug found (diagnostic): with LOCALVIEWER enabled and a singular view matrix, `make_gpu_program()` uses `vec3 e=localviewer?norm(camera-wp.xyz):norm(eye);` while the CPU reference (`light_vertex`) keeps the fixed-function eye unless `inverse3(view)` succeeds. The uploaded `camera` term stays (0,0,0) on singular inversion (`gpu_prepare`: `float camera[3]={0};` filled only on success), so specular highlights diverge. Not reachable for invertible view matrices.\n");
            fclose(f);
        }
    }

    d8_renderer_close(&g_gpu);
    d8_renderer_close(&g_cpu);
    glDeleteTextures(2, g_tex);
    glDeleteFramebuffers(1, &g_target_cpu.fbo);
    glDeleteFramebuffers(1, &g_target_gpu.fbo);
    glDeleteTextures(1, &g_target_cpu.color);
    glDeleteTextures(1, &g_target_gpu.color);
    glDeleteRenderbuffers(1, &g_target_cpu.depth);
    glDeleteRenderbuffers(1, &g_target_gpu.depth);
    glDeleteBuffers(1, &g_native_small_vbo);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return overall ? 0 : 1;
}
