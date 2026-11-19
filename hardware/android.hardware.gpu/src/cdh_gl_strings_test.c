// cdh_gl_shader_patch.h 与 cdh_gl_strings.c 宿主单元测试：
// 覆盖 GLSL ES 310/320 降级、fma 宏注入、显式 length[] 同步扩长、
// '=' 精确扩展剥离及 glGetString/glGetIntegerv(GL_NUM_EXTENSIONS)/glGetStringi 一致性。
#define CDH_GL_STRINGS_HOST_TEST 1
#define CDH_HAS_GLGETSTRING 1
#define CDH_HAS_GLGETSTRINGI 1
#define CDH_ID_GL_VENDOR "ARM"
#define CDH_ID_GL_RENDERER "Mali-G57 MC2"
#define CDH_ID_GL_VERSION "OpenGL ES 3.2 v1.r49p1"
#define CDH_ID_GL_EXT_STRIP \
    "GL_MESA_,=GL_EXT_draw_buffers,=GL_OES_texture_float,=GL_EXT_texture_storage_compression"
#define CDH_GPU_CFG_PATH "/nonexistent/cdh_test_gpu_id.cfg"

#include "cdh_gl_strings.c"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_single_chunk_320_with_extension(void) {
    const char *src =
        "#version 320 es\n"
        "#extension GL_EXT_shader_framebuffer_fetch : require\n"
        "precision mediump float;\n"
        "vec4 f(vec4 a, vec4 b, vec4 c) { return fma(a, b, c); }\n";
    const char *arr[1] = {src};
    cdh_patched_sources_t res;
    cdh_prepare_patched_sources(1, arr, NULL, &res);
    assert(res.patched == 1);
    assert(res.lens == NULL);
    assert(strncmp(res.ptrs[0], "#version 300 es\n#ifndef fma\n", 28) == 0);
    // 确保预处理宏位于 #extension 之前且保留了完整函数体（不被截断）
    const char *ext = strstr(res.ptrs[0], "#extension GL_EXT_shader_framebuffer_fetch");
    const char *endif_pos = strstr(res.ptrs[0], "#endif\n");
    assert(endif_pos != NULL && ext != NULL && endif_pos < ext);
    assert(strstr(res.ptrs[0], "return fma(a, b, c);") != NULL);
    cdh_free_patched_sources(&res);
}

static void test_explicit_length_array_prevents_truncation(void) {
    // 尾部附带垃圾字节，通过显式 length[0] 限定真实范围，验证 length 扩增且不含垃圾尾部。
    const char *chunk0 = "#version 310 es\nvoid main() { float x = fma(1.0, 2.0, 3.0); }GARBAGE";
    const char *chunk1 = "/* tail */GARBAGE";
    const char *arr[2] = {chunk0, chunk1};
    int len0 = (int)(strstr(chunk0, "GARBAGE") - chunk0);
    int len1 = (int)(strstr(chunk1, "GARBAGE") - chunk1);
    int lens[2] = {len0, len1};

    cdh_patched_sources_t res;
    cdh_prepare_patched_sources(2, arr, lens, &res);
    assert(res.patched == 1);
    assert(res.lens != NULL);
    assert(res.lens[0] == len0 + (int)(sizeof(CDH_FMA_POLYFILL) - 1));
    assert(res.ptrs[1] == chunk1);
    assert(res.lens[1] == len1);
    assert(strstr(res.ptrs[0], "GARBAGE") == NULL);
    assert(strstr(res.ptrs[0], "#version 300 es\n") != NULL);
    assert(strstr(res.ptrs[0], "float x = fma(1.0, 2.0, 3.0); }") != NULL);
    cdh_free_patched_sources(&res);
}

static void test_no_newline_header_chunk(void) {
    const char *chunk0 = "#version 320 es";
    const char *chunk1 = "\nvoid main() {}\n";
    const char *arr[2] = {chunk0, chunk1};
    int lens[2] = {15, -1};

    cdh_patched_sources_t res;
    cdh_prepare_patched_sources(2, arr, lens, &res);
    assert(res.patched == 1);
    assert(res.lens[0] == (int)(15 + 1 + (sizeof(CDH_FMA_POLYFILL) - 1)));
    assert(strcmp(res.ptrs[0],
                  "#version 300 es\n#ifndef fma\n#define fma(a, b, c) ((a) * (b) + (c))\n#endif\n") == 0);
    assert(res.ptrs[1] == chunk1);
    assert(res.lens[1] == -1);
    cdh_free_patched_sources(&res);
}

static void test_300_es_untouched(void) {
    const char *src = "#version 300 es\nvoid main() {}\n";
    const char *arr[1] = {src};
    cdh_patched_sources_t res;
    cdh_prepare_patched_sources(1, arr, NULL, &res);
    assert(res.patched == 0);
    cdh_free_patched_sources(&res);
}

static const unsigned char *host_fake_glGetString(unsigned int name) {
    if (name == CDH_GL_EXTENSIONS) {
        return (const unsigned char *)
            "GL_EXT_debug_marker "
            "GL_MESA_framebuffer_flip_y "
            "GL_EXT_draw_buffers "
            "GL_EXT_draw_buffers_indexed "
            "GL_OES_texture_float "
            "GL_OES_texture_float_linear "
            "GL_EXT_texture_storage "
            "GL_EXT_texture_storage_compression "
            "GL_OES_vertex_array_object";
    }
    return NULL;
}

static void test_ext_strip_exact_and_glgetstringi(void) {
    cdh_ovr_glGetString = (void *)&host_fake_glGetString;

    assert(cdh_stripped("GL_MESA_framebuffer_flip_y", strlen("GL_MESA_framebuffer_flip_y")) == 1);
    assert(cdh_stripped("GL_EXT_draw_buffers", strlen("GL_EXT_draw_buffers")) == 1);
    assert(cdh_stripped("GL_OES_texture_float", strlen("GL_OES_texture_float")) == 1);
    assert(cdh_stripped("GL_EXT_texture_storage_compression",
                        strlen("GL_EXT_texture_storage_compression")) == 1);
    // 同前缀但非精确匹配的真机扩展必须保留
    assert(cdh_stripped("GL_EXT_draw_buffers_indexed", strlen("GL_EXT_draw_buffers_indexed")) == 0);
    assert(cdh_stripped("GL_OES_texture_float_linear", strlen("GL_OES_texture_float_linear")) == 0);
    assert(cdh_stripped("GL_EXT_texture_storage", strlen("GL_EXT_texture_storage")) == 0);

    const char *expected[] = {
        "GL_EXT_debug_marker",
        "GL_EXT_draw_buffers_indexed",
        "GL_OES_texture_float_linear",
        "GL_EXT_texture_storage",
        "GL_OES_vertex_array_object",
        "GL_ARM_mali_program_binary",
        "GL_ARM_mali_shader_binary",
        "GL_ARM_rgba8",
    };
    const int expected_n = (int)(sizeof(expected) / sizeof(expected[0]));

    int num_exts = -1;
    glGetIntegerv(CDH_GL_NUM_EXTENSIONS, &num_exts);
    assert(num_exts == expected_n);

    const char *joined = (const char *)glGetString(CDH_GL_EXTENSIONS);
    assert(joined != NULL);

    char rebuilt[2048] = {0};
    size_t pos = 0;
    for (int i = 0; i < num_exts; i++) {
        const char *item = (const char *)glGetStringi(CDH_GL_EXTENSIONS, (unsigned int)i);
        assert(item != NULL);
        assert(strcmp(item, expected[i]) == 0);
        size_t len = strlen(item);
        if (i > 0) {
            rebuilt[pos++] = ' ';
        }
        memcpy(rebuilt + pos, item, len);
        pos += len;
        rebuilt[pos] = '\0';
    }
    assert(strcmp(rebuilt, joined) == 0);
    assert(glGetStringi(CDH_GL_EXTENSIONS, (unsigned int)num_exts) == NULL);
}

static void test_virgl_unsafe_extra_exts_filtered(void) {
    assert(cdh_is_virgl_unsafe_extra_ext("GL_EXT_multisampled_render_to_texture",
                                         strlen("GL_EXT_multisampled_render_to_texture")) == 1);
    assert(cdh_is_virgl_unsafe_extra_ext("GL_EXT_multisampled_render_to_texture2",
                                         strlen("GL_EXT_multisampled_render_to_texture2")) == 1);
    assert(cdh_is_virgl_unsafe_extra_ext("GL_EXT_shader_framebuffer_fetch",
                                         strlen("GL_EXT_shader_framebuffer_fetch")) == 1);
    assert(cdh_is_virgl_unsafe_extra_ext("GL_EXT_shader_pixel_local_storage",
                                         strlen("GL_EXT_shader_pixel_local_storage")) == 1);
    assert(cdh_is_virgl_unsafe_extra_ext("GL_KHR_blend_equation_advanced",
                                         strlen("GL_KHR_blend_equation_advanced")) == 1);
    assert(cdh_is_virgl_unsafe_extra_ext("GL_QCOM_shader_framebuffer_fetch_noncoherent",
                                         strlen("GL_QCOM_shader_framebuffer_fetch_noncoherent")) == 1);
    assert(cdh_is_virgl_unsafe_extra_ext("GL_QCOM_shader_framebuffer_fetch_rate",
                                         strlen("GL_QCOM_shader_framebuffer_fetch_rate")) == 1);
    assert(cdh_is_virgl_unsafe_extra_ext("GL_ARM_mali_shader_binary",
                                         strlen("GL_ARM_mali_shader_binary")) == 0);
    assert(cdh_is_virgl_unsafe_extra_ext("GL_QCOM_tiled_rendering",
                                         strlen("GL_QCOM_tiled_rendering")) == 0);

    size_t used = 0;
    size_t items_used = 0;
    unsigned int count = 0;
    cdh_ext_ready = 0;
    cdh_ext_count = 0;
    cdh_ext_joined[0] = '\0';
    cdh_append_filtered_extra_exts(
        "GL_ARM_mali_shader_binary,GL_EXT_multisampled_render_to_texture,"
        "GL_EXT_shader_framebuffer_fetch,GL_KHR_blend_equation_advanced,"
        "GL_QCOM_shader_framebuffer_fetch_noncoherent,GL_QCOM_shader_framebuffer_fetch_rate,"
        "GL_QCOM_tiled_rendering",
        &used, &items_used, &count);
    assert(count == 2);
    assert(strcmp(cdh_ext_list[0], "GL_ARM_mali_shader_binary") == 0);
    assert(strcmp(cdh_ext_list[1], "GL_QCOM_tiled_rendering") == 0);
}

int main(void) {
    test_single_chunk_320_with_extension();
    test_explicit_length_array_prevents_truncation();
    test_no_newline_header_chunk();
    test_300_es_untouched();
    test_ext_strip_exact_and_glgetstringi();
    test_virgl_unsafe_extra_exts_filtered();
    printf("[pass] cdh_gl_strings_test: 6/6 passed\n");
    return 0;
}
