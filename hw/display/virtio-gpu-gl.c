#include "qemu/osdep.h"
#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include <virgl/virglrenderer.h>
#include "hw/qdev-properties.h"
#include "hw/virtio/virtio-gpu-bswap.h"
#include "hw/virtio/virtio-gpu.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/iov.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "ui/console.h"

struct virgl_box {
    uint32_t x, y, z, w, h, d;
};

typedef struct VirglFenceDone {
    uint32_t ctx_id;
    uint64_t fence_id;
} VirglFenceDone;

static struct virgl_renderer_callbacks virgl_cbs;
static EGLDisplay virgl_egl_display = EGL_NO_DISPLAY;
static EGLConfig virgl_egl_config;
static EGLContext virgl_egl_root = EGL_NO_CONTEXT;
static GLuint virgl_readback_fb;
static uint8_t *virgl_readback_buf;
static size_t virgl_readback_buf_size;
static uint64_t virgl_cmd_seq, virgl_submit_seq, virgl_read_seq, virgl_frame_seq;
static QEMUTimer *virgl_poll_timer;
static GArray *virgl_done_fences;

static void virtio_gpu_gl_poll_timer(void *opaque);

static int64_t gl_ms(void)
{
    return g_get_monotonic_time() / 1000;
}

static bool gl_dbg(void)
{
    static int v = -1;

    if (v < 0) {
        v = !!getenv("QEMU_GLDBG");
    }
    return v;
}

#define GLDBG(fmt, ...) do { \
    fprintf(stderr, "GLDBG t=%" PRId64 " " fmt "\n", gl_ms(), ##__VA_ARGS__); \
    fflush(stderr); \
} while (0)

static const char *gl_cmd_name(uint32_t type)
{
    switch (type) {
    case VIRTIO_GPU_CMD_GET_CAPSET_INFO:
        return "GET_CAPSET_INFO";
    case VIRTIO_GPU_CMD_GET_CAPSET:
        return "GET_CAPSET";
    case VIRTIO_GPU_CMD_CTX_CREATE:
        return "CTX_CREATE";
    case VIRTIO_GPU_CMD_CTX_DESTROY:
        return "CTX_DESTROY";
    case VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE:
        return "CTX_ATTACH_RESOURCE";
    case VIRTIO_GPU_CMD_CTX_DETACH_RESOURCE:
        return "CTX_DETACH_RESOURCE";
    case VIRTIO_GPU_CMD_RESOURCE_UNREF:
        return "RESOURCE_UNREF";
    case VIRTIO_GPU_CMD_RESOURCE_CREATE_3D:
        return "RESOURCE_CREATE_3D";
    case VIRTIO_GPU_CMD_TRANSFER_TO_HOST_3D:
        return "TRANSFER_TO_HOST_3D";
    case VIRTIO_GPU_CMD_TRANSFER_FROM_HOST_3D:
        return "TRANSFER_FROM_HOST_3D";
    case VIRTIO_GPU_CMD_SUBMIT_3D:
        return "SUBMIT_3D";
    case VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING:
        return "RESOURCE_ATTACH_BACKING";
    case VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING:
        return "RESOURCE_DETACH_BACKING";
    case VIRTIO_GPU_CMD_SET_SCANOUT:
        return "SET_SCANOUT";
    case VIRTIO_GPU_CMD_RESOURCE_FLUSH:
        return "RESOURCE_FLUSH";
    default:
        return "OTHER";
    }
}

static bool gl_every(int64_t *last, int64_t ms)
{
    int64_t now = gl_ms();

    if (now - *last >= ms) {
        *last = now;
        return true;
    }
    return false;
}

static void virtio_gpu_gl_arm_poll(VirtIOGPU *g)
{
    if (!virgl_poll_timer) {
        virgl_poll_timer = timer_new_ms(QEMU_CLOCK_REALTIME,
                                        virtio_gpu_gl_poll_timer, g);
    }
    timer_mod(virgl_poll_timer, qemu_clock_get_ms(QEMU_CLOCK_REALTIME) + 1);
}

static uint64_t *virtio_gpu_gl_done_slot(uint32_t ctx_id)
{
    VirglFenceDone v = { .ctx_id = ctx_id };

    if (!virgl_done_fences) {
        virgl_done_fences = g_array_new(false, false, sizeof(v));
    }
    for (guint i = 0; i < virgl_done_fences->len; i++) {
        VirglFenceDone *p = &g_array_index(virgl_done_fences, VirglFenceDone, i);
        if (p->ctx_id == ctx_id) {
            return &p->fence_id;
        }
    }
    g_array_append_val(virgl_done_fences, v);
    return &g_array_index(virgl_done_fences, VirglFenceDone,
                          virgl_done_fences->len - 1).fence_id;
}

static bool virtio_gpu_gl_drain_fences(VirtIOGPU *g, uint32_t ctx_id,
                                       uint64_t fence_id)
{
    struct virtio_gpu_ctrl_command *cmd, *next;
    bool done = false;

    for (cmd = QTAILQ_FIRST(&g->fenceq); cmd; cmd = next) {
        next = QTAILQ_NEXT(cmd, next);
        if (cmd->cmd_hdr.ctx_id == ctx_id && cmd->cmd_hdr.fence_id <= fence_id) {
            GLDBG("fence complete ctx=%u fence=%" PRIu64 " signaled=%" PRIu64,
                  ctx_id, cmd->cmd_hdr.fence_id, fence_id);
            QTAILQ_REMOVE(&g->fenceq, cmd, next);
            if (g->inflight) {
                g->inflight--;
            }
            virtio_gpu_ctrl_response_nodata(g, cmd, VIRTIO_GPU_RESP_OK_NODATA);
            g_free(cmd);
            done = true;
        }
    }
    return done;
}

static bool virtio_gpu_gl_complete_fence(VirtIOGPU *g, uint32_t ctx_id,
                                         uint64_t fence_id)
{
    uint64_t *done_fence = virtio_gpu_gl_done_slot(ctx_id);
    bool done;

    if (*done_fence < fence_id) {
        *done_fence = fence_id;
    }
    done = virtio_gpu_gl_drain_fences(g, ctx_id, *done_fence);
    if (!done) {
        GLDBG("fence mark ctx=%u fence=%" PRIu64, ctx_id, *done_fence);
    }
    return done;
}

static void virtio_gpu_gl_poll_timer(void *opaque)
{
    VirtIOGPU *g = opaque;

    if (g->virgl_inited) {
        virgl_renderer_poll();
    }
    if (virgl_done_fences) {
        for (guint i = 0; i < virgl_done_fences->len; i++) {
            VirglFenceDone *p = &g_array_index(virgl_done_fences,
                                               VirglFenceDone, i);
            virtio_gpu_gl_drain_fences(g, p->ctx_id, p->fence_id);
        }
    }
    if (!QTAILQ_EMPTY(&g->fenceq)) {
        virtio_gpu_gl_arm_poll(g);
    }
}

static void virtio_gpu_gl_resource_destroy(VirtIOGPU *g,
                                           struct virtio_gpu_simple_resource *res,
                                           Error **errp);

static EGLContext virgl_egl_ctx(EGLContext share, int major, int minor)
{
    EGLContext ctx;
    EGLint a[] = {
        EGL_CONTEXT_CLIENT_VERSION, major,
        EGL_CONTEXT_MINOR_VERSION_KHR, minor,
        EGL_NONE
    };
    EGLint b[] = {
        EGL_CONTEXT_CLIENT_VERSION, major,
        EGL_NONE
    };

    ctx = eglCreateContext(virgl_egl_display, virgl_egl_config, share, a);
    return ctx == EGL_NO_CONTEXT ?
        eglCreateContext(virgl_egl_display, virgl_egl_config, share, b) : ctx;
}

static bool virtio_gpu_gl_fill(struct virtio_gpu_ctrl_command *cmd,
                               void *out, size_t size)
{
    if (iov_to_buf(cmd->elem.out_sg, cmd->elem.out_num, 0, out, size) != size) {
        cmd->error = VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER;
        return false;
    }
    return true;
}

static int virgl_egl_init(void)
{
    EGLint major = 0, minor = 0, n = 0;
    static const EGLint cfg[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };

    if (virgl_egl_root != EGL_NO_CONTEXT) {
        return 0;
    }
    GLDBG("egl init begin");
    virgl_egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (virgl_egl_display == EGL_NO_DISPLAY ||
        !eglInitialize(virgl_egl_display, &major, &minor) ||
        !eglBindAPI(EGL_OPENGL_ES_API) ||
        !eglChooseConfig(virgl_egl_display, cfg, &virgl_egl_config, 1, &n) ||
        !n) {
        GLDBG("egl init failed display=%p err=0x%x n=%d", virgl_egl_display,
              eglGetError(), n);
        return -1;
    }
    virgl_egl_root = virgl_egl_ctx(EGL_NO_CONTEXT, 3, 2);
    if (virgl_egl_root == EGL_NO_CONTEXT) {
        virgl_egl_root = virgl_egl_ctx(EGL_NO_CONTEXT, 3, 1);
    }
    if (virgl_egl_root == EGL_NO_CONTEXT) {
        virgl_egl_root = virgl_egl_ctx(EGL_NO_CONTEXT, 3, 0);
    }
    if (virgl_egl_root == EGL_NO_CONTEXT ||
        !eglMakeCurrent(virgl_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                        virgl_egl_root)) {
        GLDBG("egl root failed ctx=%p err=0x%x", virgl_egl_root, eglGetError());
        return -1;
    }
    GLDBG("egl init ok version=%d.%d root=%p", major, minor, virgl_egl_root);
    return 0;
}

static virgl_renderer_gl_context virgl_create_context(void *cookie,
                                                      int scanout,
                                                      struct virgl_renderer_gl_ctx_param *p)
{
    int major = p->major_ver, minor = p->minor_ver;

    if (major > 3 || virgl_egl_init()) {
        return NULL;
    }
    if (major < 2) {
        major = 2;
    }
    if (major == 3 && minor > 2) {
        return NULL;
    }
    {
        EGLContext ctx = virgl_egl_ctx(virgl_egl_root, major, minor);
        GLDBG("create glctx scanout=%d req=%d.%d ctx=%p", scanout, major, minor, ctx);
        return ctx;
    }
}

static void virgl_destroy_context(void *cookie, virgl_renderer_gl_context ctx)
{
    if (ctx && virgl_egl_display != EGL_NO_DISPLAY) {
        GLDBG("destroy glctx ctx=%p", ctx);
        eglDestroyContext(virgl_egl_display, ctx);
    }
}

static int virgl_make_current(void *cookie, int scanout,
                              virgl_renderer_gl_context ctx)
{
    if (virgl_egl_init()) {
        return -1;
    }
    if (!ctx) {
        return -1;
    }
    if (!eglMakeCurrent(virgl_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                        ctx)) {
        GLDBG("make_current failed scanout=%d ctx=%p err=0x%x", scanout, ctx,
              eglGetError());
        return -1;
    }
    return 0;
}

static void virgl_write_fence(void *cookie, uint32_t fence)
{
    virtio_gpu_gl_complete_fence(cookie, 0, fence);
}

static void virgl_write_context_fence(void *cookie, uint32_t ctx_id,
                                      uint32_t ring_idx, uint64_t fence_id)
{
    (void)ring_idx;
    virtio_gpu_gl_complete_fence(cookie, ctx_id, fence_id);
}

static void virgl_add_capset(VirtIOGPU *g, uint32_t id)
{
    uint32_t ver, size;

    virgl_renderer_get_cap_set(id, &ver, &size);
    if (ver && size) {
        g_array_append_val(g->capset_ids, id);
    }
}

static bool virtio_gpu_gl_defer_fence(VirtIOGPU *g,
                                      struct virtio_gpu_ctrl_command *cmd,
                                      uint64_t seq)
{
    int r;

    glFlush();
    r = cmd->cmd_hdr.ctx_id ?
        virgl_renderer_context_create_fence(cmd->cmd_hdr.ctx_id,
                                            cmd->cmd_hdr.flags,
                                            cmd->cmd_hdr.ring_idx,
                                            cmd->cmd_hdr.fence_id) :
        virgl_renderer_create_fence((uint32_t)cmd->cmd_hdr.fence_id, 0);
    if (r) {
        GLDBG("cmd#%" PRIu64 " fence create failed ctx=%u fence=%" PRIu64 " r=%d",
              seq, cmd->cmd_hdr.ctx_id, cmd->cmd_hdr.fence_id, r);
        return false;
    }
    GLDBG("cmd#%" PRIu64 " fence defer ctx=%u fence=%" PRIu64,
          seq, cmd->cmd_hdr.ctx_id, cmd->cmd_hdr.fence_id);
    virtio_gpu_gl_arm_poll(g);
    return true;
}

static int virtio_gpu_gl_init(VirtIOGPU *g)
{
    if (g->virgl_inited) {
        return 0;
    }
    if (virgl_egl_init()) {
        return -1;
    }

    memset(&virgl_cbs, 0, sizeof(virgl_cbs));
    virgl_cbs.version = VIRGL_RENDERER_CALLBACKS_VERSION;
    virgl_cbs.write_fence = virgl_write_fence;
    virgl_cbs.create_gl_context = virgl_create_context;
    virgl_cbs.destroy_gl_context = virgl_destroy_context;
    virgl_cbs.make_current = virgl_make_current;
    virgl_cbs.write_context_fence = virgl_write_context_fence;
    if (virgl_renderer_init(g, VIRGL_RENDERER_USE_GLES, &virgl_cbs)) {
        GLDBG("virgl init failed");
        return -1;
    }

    g_array_set_size(g->capset_ids, 0);
    virgl_add_capset(g, VIRTIO_GPU_CAPSET_VIRGL);
    virgl_add_capset(g, VIRTIO_GPU_CAPSET_VIRGL2);
    g->parent_obj.virtio_config.num_capsets = cpu_to_le32(g->capset_ids->len);
    g->virgl_inited = true;
    GLDBG("virgl init ok capsets=%u", g->capset_ids->len);
    return 0;
}

static void virtio_gpu_gl_resource_create_3d(VirtIOGPU *g,
                                             struct virtio_gpu_ctrl_command *cmd)
{
    struct virtio_gpu_resource_create_3d c3d;
    struct virgl_renderer_resource_create_args a = { 0 };
    struct virtio_gpu_simple_resource *res;

    VIRTIO_GPU_FILL_CMD(c3d);
    virtio_gpu_bswap_32(&c3d, sizeof(c3d));
    if (!c3d.resource_id || virtio_gpu_find_resource(g, c3d.resource_id)) {
        cmd->error = VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID;
        return;
    }

    a.handle = c3d.resource_id;
    a.target = c3d.target;
    a.format = c3d.format;
    a.bind = c3d.bind;
    a.width = c3d.width;
    a.height = c3d.height;
    a.depth = c3d.depth;
    a.array_size = c3d.array_size;
    a.last_level = c3d.last_level;
    a.nr_samples = c3d.nr_samples;
    a.flags = c3d.flags;
    GLDBG("res3d create id=%u ctx=%u target=%u fmt=%u bind=0x%x %ux%ux%u array=%u samples=%u flags=0x%x",
          c3d.resource_id, c3d.hdr.ctx_id, c3d.target, c3d.format,
          c3d.bind, c3d.width, c3d.height, c3d.depth, c3d.array_size,
          c3d.nr_samples, c3d.flags);
    if (virgl_renderer_resource_create(&a, NULL, 0)) {
        GLDBG("res3d create failed id=%u ctx=%u target=%u fmt=%u bind=0x%x %ux%ux%u",
              c3d.resource_id, c3d.hdr.ctx_id, c3d.target, c3d.format,
              c3d.bind, c3d.width, c3d.height, c3d.depth);
        cmd->error = VIRTIO_GPU_RESP_ERR_UNSPEC;
        return;
    }

    res = g_new0(struct virtio_gpu_simple_resource, 1);
    res->resource_id = c3d.resource_id;
    res->width = c3d.width;
    res->height = c3d.height;
    res->format = c3d.format;
    res->virgl = true;
    QTAILQ_INSERT_HEAD(&g->reslist, res, next);
}

static void virtio_gpu_gl_transfer(VirtIOGPU *g,
                                   struct virtio_gpu_ctrl_command *cmd,
                                   bool to_host)
{
    struct virtio_gpu_transfer_host_3d t;
    struct virtio_gpu_simple_resource *res;
    struct virgl_box box;
    int r;

    VIRTIO_GPU_FILL_CMD(t);
    virtio_gpu_bswap_32(&t, sizeof(t));
    res = virtio_gpu_find_resource(g, t.resource_id);
    if (!res || !res->virgl || !res->iov) {
        cmd->error = VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID;
        return;
    }

    box.x = t.box.x;
    box.y = t.box.y;
    box.z = t.box.z;
    box.w = t.box.w;
    box.h = t.box.h;
    box.d = t.box.d;
    GLDBG("xfer %s res=%u ctx=%u level=%u off=%" PRIu64 " stride=%u layer=%u box=%u,%u,%u %ux%ux%u iov=%d",
          to_host ? "to-host" : "from-host", t.resource_id, t.hdr.ctx_id,
          t.level, (uint64_t)t.offset, t.stride, t.layer_stride, box.x,
          box.y, box.z, box.w, box.h, box.d, res->iov_cnt);
    r = to_host ?
        virgl_renderer_transfer_write_iov(t.resource_id, t.hdr.ctx_id,
                                          t.level, t.stride, t.layer_stride,
                                          &box, t.offset, res->iov, res->iov_cnt) :
        virgl_renderer_transfer_read_iov(t.resource_id, t.hdr.ctx_id,
                                         t.level, t.stride, t.layer_stride,
                                         &box, t.offset, res->iov, res->iov_cnt);
    if (r) {
        GLDBG("xfer failed %s res=%u ctx=%u r=%d off=%" PRIu64 " stride=%u layer=%u box=%u,%u,%u %ux%ux%u iov=%d",
              to_host ? "to-host" : "from-host", t.resource_id, t.hdr.ctx_id,
              r, (uint64_t)t.offset, t.stride, t.layer_stride, box.x, box.y,
              box.z, box.w, box.h, box.d, res->iov_cnt);
        cmd->error = VIRTIO_GPU_RESP_ERR_UNSPEC;
    }
}

static void virtio_gpu_gl_submit(VirtIOGPU *g,
                                 struct virtio_gpu_ctrl_command *cmd)
{
    struct virtio_gpu_cmd_submit cs;
    void *buf;
    size_t n;
    int r = 0;
    uint64_t seq;
    int64_t t0;
    bool log;

    VIRTIO_GPU_FILL_CMD(cs);
    virtio_gpu_bswap_32(&cs, sizeof(cs));
    if (!cs.size || (cs.size & 3)) {
        cmd->error = VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER;
        return;
    }

    seq = ++virgl_submit_seq;
    log = true;
    t0 = gl_ms();
    buf = g_malloc0(cs.size);
    n = iov_to_buf(cmd->elem.out_sg, cmd->elem.out_num, sizeof(cs), buf, cs.size);
    if (log) {
        uint32_t *dw = buf;
        uint32_t ndw = cs.size / 4, a = ndw > 0 ? dw[0] : 0;
        uint32_t b = ndw > 1 ? dw[1] : 0, c = ndw > 2 ? dw[2] : 0;
        uint32_t d = ndw > 3 ? dw[3] : 0;
        GLDBG("submit#%" PRIu64 " begin ctx=%u bytes=%u ndw=%u head=%08x,%08x,%08x,%08x fence=%" PRIu64,
              seq, cs.hdr.ctx_id, cs.size, ndw, a, b, c, d, cs.hdr.fence_id);
    }
    if (n == cs.size) {
        r = virgl_renderer_submit_cmd(buf, cs.hdr.ctx_id, cs.size / 4);
    }
    if (n != cs.size || r) {
        uint32_t *dw = buf;
        uint32_t ndw = cs.size / 4, a = ndw > 0 ? dw[0] : 0;
        uint32_t b = ndw > 1 ? dw[1] : 0, c = ndw > 2 ? dw[2] : 0;
        uint32_t d = ndw > 3 ? dw[3] : 0, e = ndw > 4 ? dw[4] : 0;
        uint32_t f = ndw > 5 ? dw[5] : 0, h = ndw > 6 ? dw[6] : 0;
        uint32_t i = ndw > 7 ? dw[7] : 0;
        GLDBG("submit#%" PRIu64 " failed ctx=%u r=%d copied=%zu/%u ndw=%u head=%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x",
              seq, cs.hdr.ctx_id, r, n, cs.size, ndw, a, b, c, d, e, f, h, i);
        cmd->error = VIRTIO_GPU_RESP_ERR_UNSPEC;
    } else {
        glFlush();
        if (log || gl_ms() - t0 > 20) {
            GLDBG("submit#%" PRIu64 " ok ctx=%u bytes=%u dt=%" PRId64 "ms",
                  seq, cs.hdr.ctx_id, cs.size, gl_ms() - t0);
        }
    }
    g_free(buf);
}

static void virtio_gpu_gl_get_capset_info(VirtIOGPU *g,
                                          struct virtio_gpu_ctrl_command *cmd)
{
    struct virtio_gpu_get_capset_info gcsi;
    struct virtio_gpu_resp_capset_info resp = { 0 };
    uint32_t id, ver, size;

    VIRTIO_GPU_FILL_CMD(gcsi);
    virtio_gpu_bswap_32(&gcsi, sizeof(gcsi));
    if (gcsi.capset_index >= g->capset_ids->len) {
        cmd->error = VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER;
        return;
    }
    id = g_array_index(g->capset_ids, uint32_t, gcsi.capset_index);
    virgl_renderer_get_cap_set(id, &ver, &size);
    resp.hdr.type = VIRTIO_GPU_RESP_OK_CAPSET_INFO;
    resp.capset_id = id;
    resp.capset_max_version = ver;
    resp.capset_max_size = size;
    virtio_gpu_ctrl_response(g, cmd, &resp.hdr, sizeof(resp));
}

static void virtio_gpu_gl_get_capset(VirtIOGPU *g,
                                     struct virtio_gpu_ctrl_command *cmd)
{
    struct virtio_gpu_get_capset gc;
    struct virtio_gpu_resp_capset *resp;
    uint32_t ver, size;

    VIRTIO_GPU_FILL_CMD(gc);
    virtio_gpu_bswap_32(&gc, sizeof(gc));
    virgl_renderer_get_cap_set(gc.capset_id, &ver, &size);
    if (!ver || !size || gc.capset_version > ver) {
        cmd->error = VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER;
        return;
    }
    resp = g_malloc0(sizeof(*resp) + size);
    resp->hdr.type = VIRTIO_GPU_RESP_OK_CAPSET;
    virgl_renderer_fill_caps(gc.capset_id, gc.capset_version,
                             resp->capset_data);
    virtio_gpu_ctrl_response(g, cmd, &resp->hdr, sizeof(*resp) + size);
    g_free(resp);
}

static void virtio_gpu_gl_ctx_create(VirtIOGPU *g,
                                     struct virtio_gpu_ctrl_command *cmd)
{
    struct virtio_gpu_ctx_create cc;
    uint32_t flags;
    int r;

    VIRTIO_GPU_FILL_CMD(cc);
    virtio_gpu_bswap_32(&cc, sizeof(cc));
    if (cc.nlen >= sizeof(cc.debug_name)) {
        cmd->error = VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER;
        return;
    }
    cc.debug_name[cc.nlen] = 0;
    flags = cc.context_init & VIRTIO_GPU_CONTEXT_INIT_CAPSET_ID_MASK;
    GLDBG("ctx create id=%u flags=0x%x name=%s", cc.hdr.ctx_id, flags,
          cc.debug_name);
    r = flags ? virgl_renderer_context_create_with_flags(cc.hdr.ctx_id, flags,
                                                         cc.nlen, cc.debug_name) :
                virgl_renderer_context_create(cc.hdr.ctx_id, cc.nlen,
                                              cc.debug_name);
    if (r) {
        GLDBG("ctx create failed id=%u r=%d name=%s", cc.hdr.ctx_id, r,
              cc.debug_name);
        cmd->error = VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID;
    }
}

static void virtio_gpu_gl_ctx_destroy(VirtIOGPU *g,
                                      struct virtio_gpu_ctrl_command *cmd)
{
    struct virtio_gpu_ctx_destroy cd;

    VIRTIO_GPU_FILL_CMD(cd);
    virtio_gpu_bswap_32(&cd, sizeof(cd));
    GLDBG("ctx destroy id=%u", cd.hdr.ctx_id);
    virgl_renderer_context_destroy(cd.hdr.ctx_id);
}

static void virtio_gpu_gl_ctx_resource(VirtIOGPU *g,
                                       struct virtio_gpu_ctrl_command *cmd,
                                       bool attach)
{
    struct virtio_gpu_ctx_resource cr;

    VIRTIO_GPU_FILL_CMD(cr);
    virtio_gpu_bswap_32(&cr, sizeof(cr));
    GLDBG("ctx %s ctx=%u res=%u", attach ? "attach" : "detach",
          cr.hdr.ctx_id, cr.resource_id);
    if (attach) {
        virgl_renderer_ctx_attach_resource(cr.hdr.ctx_id, cr.resource_id);
    } else {
        int64_t t0 = gl_ms();
        glFinish();
        GLDBG("ctx detach finish ctx=%u res=%u dt=%" PRId64 "ms",
              cr.hdr.ctx_id, cr.resource_id, gl_ms() - t0);
        virgl_renderer_ctx_detach_resource(cr.hdr.ctx_id, cr.resource_id);
    }
}

static bool virtio_gpu_gl_unref(VirtIOGPU *g,
                                struct virtio_gpu_ctrl_command *cmd)
{
    struct virtio_gpu_resource_unref unref;
    struct virtio_gpu_simple_resource *res;

    if (!virtio_gpu_gl_fill(cmd, &unref, sizeof(unref))) {
        return true;
    }
    virtio_gpu_bswap_32(&unref, sizeof(unref));
    GLDBG("res unref id=%u ctx=%u", unref.resource_id, unref.hdr.ctx_id);
    res = virtio_gpu_find_resource(g, unref.resource_id);
    if (!res || !res->virgl) {
        return false;
    }
    virtio_gpu_gl_resource_destroy(g, res, NULL);
    return true;
}

static void virtio_gpu_gl_attach_backing(VirtIOGPU *g,
                                         struct virtio_gpu_ctrl_command *cmd)
{
    struct virtio_gpu_resource_attach_backing ab;
    struct virtio_gpu_simple_resource *res;

    VIRTIO_GPU_FILL_CMD(ab);
    virtio_gpu_bswap_32(&ab, sizeof(ab));
    GLDBG("backing attach begin res=%u ctx=%u entries=%u", ab.resource_id,
          ab.hdr.ctx_id, ab.nr_entries);
    virtio_gpu_resource_attach_backing(g, cmd);
    if (cmd->error) {
        GLDBG("backing attach simple failed res=%u ctx=%u err=0x%x",
              ab.resource_id, ab.hdr.ctx_id, cmd->error);
        return;
    }
    res = virtio_gpu_find_resource(g, ab.resource_id);
    if (res && res->virgl &&
        virgl_renderer_resource_attach_iov(ab.resource_id, res->iov,
                                           res->iov_cnt)) {
        GLDBG("backing attach virgl failed res=%u ctx=%u iov=%d",
              ab.resource_id, ab.hdr.ctx_id, res->iov_cnt);
        cmd->error = VIRTIO_GPU_RESP_ERR_UNSPEC;
    } else if (res && res->virgl) {
        GLDBG("backing attach ok res=%u ctx=%u iov=%d", ab.resource_id,
              ab.hdr.ctx_id, res->iov_cnt);
    }
}

static void virtio_gpu_gl_detach_backing(VirtIOGPU *g,
                                         struct virtio_gpu_ctrl_command *cmd)
{
    struct virtio_gpu_resource_detach_backing db;
    struct virtio_gpu_simple_resource *res;
    struct iovec *iov = NULL;
    int niov = 0;

    VIRTIO_GPU_FILL_CMD(db);
    virtio_gpu_bswap_32(&db, sizeof(db));
    GLDBG("backing detach res=%u ctx=%u", db.resource_id, db.hdr.ctx_id);
    res = virtio_gpu_find_resource(g, db.resource_id);
    if (res && res->virgl) {
        virgl_renderer_resource_detach_iov(db.resource_id, &iov, &niov);
    }
    virtio_gpu_resource_detach_backing(g, cmd);
}

static bool virtio_gpu_gl_readback(VirtIOGPU *g,
                                   uint32_t scanout_id,
                                   struct virtio_gpu_simple_resource *res,
                                   struct virtio_gpu_rect *r)
{
    struct virgl_renderer_resource_info info = { 0 };
    struct virtio_gpu_scanout *scanout = &g->parent_obj.scanout[scanout_id];
    uint32_t w = r->width, h = r->height;
    size_t size = (size_t)w * h * 4;
    uint8_t *dst;
    int stride;
    uint64_t seq = ++virgl_read_seq;
    int64_t t0 = gl_ms();
    GLenum status, err;
    bool trace = true;
    static int64_t last_read_log;

    if (trace) {
        GLDBG("read#%" PRIu64 " begin scanout=%u res=%u rect=%u,%u %ux%u",
              seq, scanout_id, res->resource_id, r->x, r->y, w, h);
    }
    if (!w || !h) {
        GLDBG("read#%" PRIu64 " empty scanout=%u res=%u rect=%u,%u %ux%u",
              seq, scanout_id, res->resource_id, r->x, r->y, w, h);
        return false;
    }
    if (virgl_renderer_resource_get_info(res->resource_id, &info)) {
        GLDBG("read#%" PRIu64 " get_info failed scanout=%u res=%u",
              seq, scanout_id, res->resource_id);
        return false;
    }
    if (r->x + w > info.width || r->y + h > info.height) {
        GLDBG("read#%" PRIu64 " oob scanout=%u res=%u rect=%u,%u %ux%u info=%ux%u tex=%u",
              seq, scanout_id, res->resource_id, r->x, r->y, w, h,
              info.width, info.height, info.tex_id);
        return false;
    }
    if (virgl_make_current(g, 0, virgl_egl_root)) {
        GLDBG("read#%" PRIu64 " make_current failed scanout=%u res=%u",
              seq, scanout_id, res->resource_id);
        return false;
    }
    if (!scanout->ds || surface_width(scanout->ds) != w ||
        surface_height(scanout->ds) != h) {
        scanout->ds = qemu_create_displaysurface(w, h);
        dpy_gfx_replace_surface(scanout->con, scanout->ds);
    }
    if (!virgl_readback_fb) {
        glGenFramebuffers(1, &virgl_readback_fb);
    }
    if (virgl_readback_buf_size < size) {
        g_free(virgl_readback_buf);
        virgl_readback_buf = g_malloc(size);
        virgl_readback_buf_size = size;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, virgl_readback_fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, info.tex_id, 0);
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        GLDBG("read#%" PRIu64 " fbo failed status=0x%x scanout=%u res=%u tex=%u info=%ux%u stride=%u",
              seq, status, scanout_id, res->resource_id, info.tex_id,
              info.width, info.height, info.stride);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        virgl_renderer_force_ctx_0();
        return false;
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    if (trace) {
        GLDBG("read#%" PRIu64 " pixels begin tex=%u src=%u,%u %ux%u",
              seq, info.tex_id, r->x, r->y, w, h);
    }
    glReadPixels(r->x, r->y, w, h, GL_RGBA, GL_UNSIGNED_BYTE,
                 virgl_readback_buf);
    err = glGetError();
    if (err) {
        GLDBG("read#%" PRIu64 " pixels glerr=0x%x scanout=%u res=%u tex=%u rect=%u,%u %ux%u",
              seq, err, scanout_id, res->resource_id, info.tex_id, r->x,
              r->y, w, h);
    } else if (trace) {
        GLDBG("read#%" PRIu64 " pixels ok", seq);
    }
    dst = surface_data(scanout->ds);
    stride = surface_stride(scanout->ds);
    for (uint32_t y = 0; y < h; y++) {
        uint32_t *sp = (uint32_t *)(virgl_readback_buf + y * w * 4);
        uint32_t *dp = (uint32_t *)(dst + y * stride);
        for (uint32_t x = 0; x < w; x++) {
            uint32_t p = sp[x];
            dp[x] = ((p & 0x00ff0000) >> 16) |
                    (p & 0xff00ff00) |
                    ((p & 0x000000ff) << 16);
        }
    }
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, 0, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    virgl_renderer_force_ctx_0();
    if (trace) {
        GLDBG("read#%" PRIu64 " update begin", seq);
    }
    dpy_gfx_update(scanout->con, 0, 0, w, h);
    if (trace || err || gl_ms() - t0 > 20 || gl_every(&last_read_log, 1000)) {
        GLDBG("read#%" PRIu64 " done scanout=%u res=%u tex=%u %ux%u dt=%" PRId64 "ms frame=%" PRIu64,
              seq, scanout_id, res->resource_id, info.tex_id, w, h,
              gl_ms() - t0, ++virgl_frame_seq);
    } else {
        virgl_frame_seq++;
    }
    return true;
}

static bool virtio_gpu_gl_scanout(VirtIOGPU *g,
                                  struct virtio_gpu_ctrl_command *cmd)
{
    struct virtio_gpu_set_scanout ss;
    struct virtio_gpu_simple_resource *res;
    struct virgl_renderer_resource_info info = { 0 };
    struct virtio_gpu_framebuffer fb = { 0 };

    if (!virtio_gpu_gl_fill(cmd, &ss, sizeof(ss))) {
        return true;
    }
    virtio_gpu_bswap_32(&ss, sizeof(ss));
    if (ss.scanout_id >= g->parent_obj.conf.max_outputs) {
        GLDBG("scanout invalid id=%u res=%u rect=%u,%u %ux%u",
              ss.scanout_id, ss.resource_id, ss.r.x, ss.r.y, ss.r.width,
              ss.r.height);
        cmd->error = VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID;
        return true;
    }
    if (!ss.resource_id) {
        GLDBG("scanout disable id=%u", ss.scanout_id);
        virtio_gpu_disable_scanout(g, ss.scanout_id);
        return true;
    }
    res = virtio_gpu_find_resource(g, ss.resource_id);
    if (!res || !res->virgl) {
        GLDBG("scanout fallback id=%u res=%u virgl=%d", ss.scanout_id,
              ss.resource_id, res ? res->virgl : 0);
        return false;
    }
    if (virgl_renderer_resource_get_info(ss.resource_id, &info)) {
        GLDBG("scanout get_info failed id=%u res=%u", ss.scanout_id,
              ss.resource_id);
        cmd->error = VIRTIO_GPU_RESP_ERR_UNSPEC;
        return true;
    }
    GLDBG("scanout id=%u res=%u tex=%u info=%ux%u stride=%u rect=%u,%u %ux%u",
          ss.scanout_id, ss.resource_id, info.tex_id, info.width, info.height,
          info.stride, ss.r.x, ss.r.y, ss.r.width, ss.r.height);
    fb.width = info.width;
    fb.height = info.height;
    fb.stride = info.stride;
    fb.bytes_pp = 4;
    virtio_gpu_update_scanout(g, ss.scanout_id, res, &fb, &ss.r);
    if (!virtio_gpu_gl_readback(g, ss.scanout_id, res, &ss.r)) {
        cmd->error = VIRTIO_GPU_RESP_ERR_UNSPEC;
    }
    return true;
}

static bool virtio_gpu_gl_flush(VirtIOGPU *g,
                                struct virtio_gpu_ctrl_command *cmd)
{
    struct virtio_gpu_resource_flush rf;
    struct virtio_gpu_simple_resource *res;
    int i;
    static int64_t last_flush_log;
    bool log;

    if (!virtio_gpu_gl_fill(cmd, &rf, sizeof(rf))) {
        return true;
    }
    virtio_gpu_bswap_32(&rf, sizeof(rf));
    log = gl_dbg() || gl_every(&last_flush_log, 1000);
    res = virtio_gpu_find_resource(g, rf.resource_id);
    if (!res || !res->virgl) {
        GLDBG("flush fallback res=%u ctx=%u rect=%u,%u %ux%u", rf.resource_id,
              rf.hdr.ctx_id, rf.r.x, rf.r.y, rf.r.width, rf.r.height);
        return false;
    }
    if (log) {
        GLDBG("flush res=%u ctx=%u mask=0x%x rect=%u,%u %ux%u",
              rf.resource_id, rf.hdr.ctx_id, res->scanout_bitmask, rf.r.x,
              rf.r.y, rf.r.width, rf.r.height);
    }
    for (i = 0; i < g->parent_obj.conf.max_outputs; i++) {
        if (res->scanout_bitmask & (1 << i)) {
            struct virtio_gpu_scanout *scanout = &g->parent_obj.scanout[i];
            struct virtio_gpu_rect r = {
                .x = scanout->x,
                .y = scanout->y,
                .width = scanout->width,
                .height = scanout->height,
            };

            if (!virtio_gpu_gl_readback(g, i, res, &r)) {
                GLDBG("flush readback failed res=%u ctx=%u scanout=%d",
                      rf.resource_id, rf.hdr.ctx_id, i);
                cmd->error = VIRTIO_GPU_RESP_ERR_UNSPEC;
            }
        }
    }
    return true;
}

static void virtio_gpu_gl_process_cmd(VirtIOGPU *g,
                                      struct virtio_gpu_ctrl_command *cmd)
{
    uint64_t seq;
    bool log;

    VIRTIO_GPU_FILL_CMD(cmd->cmd_hdr);
    virtio_gpu_ctrl_hdr_bswap(&cmd->cmd_hdr);
    seq = ++virgl_cmd_seq;
    log = gl_dbg() || (cmd->cmd_hdr.type != VIRTIO_GPU_CMD_SUBMIT_3D &&
                       cmd->cmd_hdr.type != VIRTIO_GPU_CMD_RESOURCE_FLUSH);
    if (log) {
        GLDBG("cmd#%" PRIu64 " begin %s(0x%x) ctx=%u flags=0x%x fence=%" PRIu64,
              seq, gl_cmd_name(cmd->cmd_hdr.type), cmd->cmd_hdr.type,
              cmd->cmd_hdr.ctx_id, cmd->cmd_hdr.flags, cmd->cmd_hdr.fence_id);
    }

    switch (cmd->cmd_hdr.type) {
    case VIRTIO_GPU_CMD_GET_CAPSET_INFO:
        if (virtio_gpu_gl_init(g)) {
            cmd->error = VIRTIO_GPU_RESP_ERR_UNSPEC;
            break;
        }
        virtio_gpu_gl_get_capset_info(g, cmd);
        break;
    case VIRTIO_GPU_CMD_GET_CAPSET:
        if (virtio_gpu_gl_init(g)) {
            cmd->error = VIRTIO_GPU_RESP_ERR_UNSPEC;
            break;
        }
        virtio_gpu_gl_get_capset(g, cmd);
        break;
    case VIRTIO_GPU_CMD_CTX_CREATE:
        if (virtio_gpu_gl_init(g)) {
            cmd->error = VIRTIO_GPU_RESP_ERR_UNSPEC;
            break;
        }
        virtio_gpu_gl_ctx_create(g, cmd);
        break;
    case VIRTIO_GPU_CMD_CTX_DESTROY:
        if (virtio_gpu_gl_init(g)) {
            cmd->error = VIRTIO_GPU_RESP_ERR_UNSPEC;
            break;
        }
        virtio_gpu_gl_ctx_destroy(g, cmd);
        break;
    case VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE:
        if (virtio_gpu_gl_init(g)) {
            cmd->error = VIRTIO_GPU_RESP_ERR_UNSPEC;
            break;
        }
        virtio_gpu_gl_ctx_resource(g, cmd, true);
        break;
    case VIRTIO_GPU_CMD_CTX_DETACH_RESOURCE:
        if (virtio_gpu_gl_init(g)) {
            cmd->error = VIRTIO_GPU_RESP_ERR_UNSPEC;
            break;
        }
        virtio_gpu_gl_ctx_resource(g, cmd, false);
        break;
    case VIRTIO_GPU_CMD_RESOURCE_UNREF:
        if (!virtio_gpu_gl_unref(g, cmd)) {
            virtio_gpu_simple_process_cmd(g, cmd);
        }
        break;
    case VIRTIO_GPU_CMD_RESOURCE_CREATE_3D:
        if (virtio_gpu_gl_init(g)) {
            cmd->error = VIRTIO_GPU_RESP_ERR_UNSPEC;
            break;
        }
        virtio_gpu_gl_resource_create_3d(g, cmd);
        break;
    case VIRTIO_GPU_CMD_TRANSFER_TO_HOST_3D:
        if (virtio_gpu_gl_init(g)) {
            cmd->error = VIRTIO_GPU_RESP_ERR_UNSPEC;
            break;
        }
        virtio_gpu_gl_transfer(g, cmd, true);
        break;
    case VIRTIO_GPU_CMD_TRANSFER_FROM_HOST_3D:
        if (virtio_gpu_gl_init(g)) {
            cmd->error = VIRTIO_GPU_RESP_ERR_UNSPEC;
            break;
        }
        virtio_gpu_gl_transfer(g, cmd, false);
        break;
    case VIRTIO_GPU_CMD_SUBMIT_3D:
        if (virtio_gpu_gl_init(g)) {
            cmd->error = VIRTIO_GPU_RESP_ERR_UNSPEC;
            break;
        }
        virtio_gpu_gl_submit(g, cmd);
        break;
    case VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING:
        if (virtio_gpu_gl_init(g)) {
            cmd->error = VIRTIO_GPU_RESP_ERR_UNSPEC;
            break;
        }
        virtio_gpu_gl_attach_backing(g, cmd);
        break;
    case VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING:
        if (virtio_gpu_gl_init(g)) {
            cmd->error = VIRTIO_GPU_RESP_ERR_UNSPEC;
            break;
        }
        virtio_gpu_gl_detach_backing(g, cmd);
        break;
    case VIRTIO_GPU_CMD_SET_SCANOUT:
        if (!virtio_gpu_gl_scanout(g, cmd)) {
            virtio_gpu_simple_process_cmd(g, cmd);
        }
        break;
    case VIRTIO_GPU_CMD_RESOURCE_FLUSH:
        if (!virtio_gpu_gl_flush(g, cmd)) {
            virtio_gpu_simple_process_cmd(g, cmd);
        }
        break;
    default:
        virtio_gpu_simple_process_cmd(g, cmd);
        break;
    }
    if (!cmd->finished && !cmd->error && g->virgl_inited &&
        (cmd->cmd_hdr.flags & VIRTIO_GPU_FLAG_FENCE)) {
        if (virtio_gpu_gl_defer_fence(g, cmd, seq)) {
            if (log) {
                GLDBG("cmd#%" PRIu64 " end %s ctx=%u deferred err=0x0",
                      seq, gl_cmd_name(cmd->cmd_hdr.type), cmd->cmd_hdr.ctx_id);
            }
            return;
        }
    }
    if (log || cmd->error) {
        GLDBG("cmd#%" PRIu64 " end %s ctx=%u finished=%d err=0x%x",
              seq, gl_cmd_name(cmd->cmd_hdr.type), cmd->cmd_hdr.ctx_id,
              cmd->finished, cmd->error);
    }
    if (!cmd->finished) {
        virtio_gpu_ctrl_response_nodata(g, cmd, cmd->error ? cmd->error :
                                        VIRTIO_GPU_RESP_OK_NODATA);
    }
}

static void virtio_gpu_gl_update_cursor_data(VirtIOGPU *g,
                                             struct virtio_gpu_scanout *s,
                                             uint32_t resource_id)
{
    struct virtio_gpu_simple_resource *res = virtio_gpu_find_resource(g, resource_id);
    uint32_t w = 0, h = 0;
    void *data;

    if (!res || !res->virgl) {
        virtio_gpu_update_cursor_data(g, s, resource_id);
        return;
    }
    data = virgl_renderer_get_cursor_data(resource_id, &w, &h);
    if (!data) {
        GLDBG("cursor data missing res=%u", resource_id);
    } else if (gl_dbg()) {
        GLDBG("cursor data res=%u %ux%u", resource_id, w, h);
    }
    if (data && s->current_cursor && w == s->current_cursor->width &&
        h == s->current_cursor->height) {
        memcpy(s->current_cursor->data, data, w * h * 4);
    }
}

static void virtio_gpu_gl_resource_destroy(VirtIOGPU *g,
                                           struct virtio_gpu_simple_resource *res,
                                           Error **errp)
{
    if (res->virgl) {
        int64_t t0 = gl_ms();
        glFinish();
        GLDBG("res destroy id=%u %ux%u fmt=%u finish_dt=%" PRId64 "ms",
              res->resource_id, res->width, res->height, res->format,
              gl_ms() - t0);
        virgl_renderer_resource_unref(res->resource_id);
    }
    virtio_gpu_resource_destroy(g, res, errp);
}

static void virtio_gpu_gl_device_realize(DeviceState *qdev, Error **errp)
{
    VirtIOGPU *g = VIRTIO_GPU(qdev);

    g->parent_obj.conf.flags |= 1 << VIRTIO_GPU_FLAG_VIRGL_ENABLED;
    g->parent_obj.conf.flags |= 1 << VIRTIO_GPU_FLAG_CONTEXT_INIT_ENABLED;
    GLDBG("device realize virtio-gpu-gl");
    virtio_gpu_device_realize(qdev, errp);
    if (*errp) {
        return;
    }
    g->capset_ids = g_array_new(false, false, sizeof(uint32_t));
    {
        uint32_t id = VIRTIO_GPU_CAPSET_VIRGL;
        g_array_append_val(g->capset_ids, id);
        id = VIRTIO_GPU_CAPSET_VIRGL2;
        g_array_append_val(g->capset_ids, id);
    }
    g->parent_obj.virtio_config.num_capsets = cpu_to_le32(g->capset_ids->len);
}

static void virtio_gpu_gl_device_unrealize(DeviceState *qdev)
{
    VirtIOGPU *g = VIRTIO_GPU(qdev);

    if (g->virgl_inited) {
        GLDBG("device unrealize virgl cleanup");
        virgl_renderer_cleanup(g);
        g->virgl_inited = false;
    }
    if (virgl_egl_root != EGL_NO_CONTEXT) {
        eglMakeCurrent(virgl_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       virgl_egl_root);
    }
    if (virgl_readback_fb) {
        glDeleteFramebuffers(1, &virgl_readback_fb);
        virgl_readback_fb = 0;
    }
    if (virgl_egl_root != EGL_NO_CONTEXT) {
        eglDestroyContext(virgl_egl_display, virgl_egl_root);
        eglTerminate(virgl_egl_display);
        virgl_egl_root = EGL_NO_CONTEXT;
        virgl_egl_display = EGL_NO_DISPLAY;
    }
    g_clear_pointer(&virgl_readback_buf, g_free);
    virgl_readback_buf_size = 0;
    if (virgl_poll_timer) {
        timer_free(virgl_poll_timer);
        virgl_poll_timer = NULL;
    }
    g_clear_pointer(&virgl_done_fences, g_array_unref);
    g_clear_pointer(&g->capset_ids, g_array_unref);
    virtio_gpu_device_unrealize(qdev);
}

static void virtio_gpu_gl_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    VirtioDeviceClass *vdc = VIRTIO_DEVICE_CLASS(klass);
    VirtIOGPUClass *vgc = VIRTIO_GPU_CLASS(klass);

    vgc->process_cmd = virtio_gpu_gl_process_cmd;
    vgc->update_cursor_data = virtio_gpu_gl_update_cursor_data;
    vgc->resource_destroy = virtio_gpu_gl_resource_destroy;
    vdc->realize = virtio_gpu_gl_device_realize;
    vdc->unrealize = virtio_gpu_gl_device_unrealize;
    dc->user_creatable = false;
}

static const TypeInfo virtio_gpu_gl_info = {
    .name = TYPE_VIRTIO_GPU_GL,
    .parent = TYPE_VIRTIO_GPU,
    .instance_size = sizeof(VirtIOGPU),
    .class_init = virtio_gpu_gl_class_init,
};

static void virtio_gpu_gl_register_types(void)
{
    type_register_static(&virtio_gpu_gl_info);
}

type_init(virtio_gpu_gl_register_types)
