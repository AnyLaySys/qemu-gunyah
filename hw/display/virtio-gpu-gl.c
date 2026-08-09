#include "qemu/osdep.h"
#include <virgl/virglrenderer.h>
#include "hw/qdev-properties.h"
#include "hw/virtio/virtio-gpu-bswap.h"
#include "hw/virtio/virtio-gpu.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/iov.h"
#include "qemu/module.h"
#include "system/system.h"
#include "ui/console.h"

struct virgl_box {
    uint32_t x, y, z, w, h, d;
};

static struct virgl_renderer_callbacks virgl_cbs;

static void virtio_gpu_gl_resource_destroy(VirtIOGPU *g,
                                           struct virtio_gpu_simple_resource *res,
                                           Error **errp);

static bool virtio_gpu_gl_fill(struct virtio_gpu_ctrl_command *cmd,
                               void *out, size_t size)
{
    if (iov_to_buf(cmd->elem.out_sg, cmd->elem.out_num, 0, out, size) != size) {
        cmd->error = VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER;
        return false;
    }
    return true;
}

static const void *virtio_gpu_gl_map_iov(const struct iovec *iov,
                                         unsigned int iov_cnt,
                                         size_t offset, size_t size)
{
    unsigned int i;

    for (i = 0; i < iov_cnt; i++) {
        if (offset >= iov[i].iov_len) {
            offset -= iov[i].iov_len;
            continue;
        }
        if (size <= iov[i].iov_len - offset) {
            const void *ptr = (uint8_t *)iov[i].iov_base + offset;

            return ((uintptr_t)ptr & 3) ? NULL : ptr;
        }
        break;
    }
    return NULL;
}

static virgl_renderer_gl_context virgl_create_context(void *cookie,
                                                      int scanout,
                                                      struct virgl_renderer_gl_ctx_param *p)
{
    VirtIOGPU *g = cookie;
    QEMUGLParams params = {
        .major_ver = p->major_ver,
        .minor_ver = p->minor_ver,
    };

    if (scanout < 0 || scanout >= g->parent_obj.conf.max_outputs) {
        return NULL;
    }
    return dpy_gl_ctx_create(g->parent_obj.scanout[scanout].con, &params);
}

static void virgl_destroy_context(void *cookie, virgl_renderer_gl_context ctx)
{
    VirtIOGPU *g = cookie;

    dpy_gl_ctx_destroy(g->parent_obj.scanout[0].con, ctx);
}

static int virgl_make_current(void *cookie, int scanout,
                              virgl_renderer_gl_context ctx)
{
    VirtIOGPU *g = cookie;

    if (scanout < 0 || scanout >= g->parent_obj.conf.max_outputs) {
        return -1;
    }
    return dpy_gl_ctx_make_current(g->parent_obj.scanout[scanout].con, ctx);
}

static void virgl_write_fence(void *cookie, uint32_t fence)
{
}

static void virgl_write_context_fence(void *cookie, uint32_t ctx_id,
                                      uint32_t ring_idx, uint64_t fence_id)
{
}

static void virgl_add_capset(VirtIOGPU *g, uint32_t id)
{
    uint32_t ver, size;

    virgl_renderer_get_cap_set(id, &ver, &size);
    if (ver && size) {
        g_array_append_val(g->capset_ids, id);
    }
}

static int virtio_gpu_gl_init(VirtIOGPU *g)
{
    if (g->virgl_inited) {
        return 0;
    }

    memset(&virgl_cbs, 0, sizeof(virgl_cbs));
    virgl_cbs.version = VIRGL_RENDERER_CALLBACKS_VERSION;
    virgl_cbs.write_fence = virgl_write_fence;
    virgl_cbs.create_gl_context = virgl_create_context;
    virgl_cbs.destroy_gl_context = virgl_destroy_context;
    virgl_cbs.make_current = virgl_make_current;
    virgl_cbs.write_context_fence = virgl_write_context_fence;
    if (virgl_renderer_init(g, VIRGL_RENDERER_USE_GLES, &virgl_cbs)) {
        return -1;
    }

    g_array_set_size(g->capset_ids, 0);
    virgl_add_capset(g, VIRTIO_GPU_CAPSET_VIRGL);
    virgl_add_capset(g, VIRTIO_GPU_CAPSET_VIRGL2);
    g->parent_obj.virtio_config.num_capsets = cpu_to_le32(g->capset_ids->len);
    g->virgl_inited = true;
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
    if (virgl_renderer_resource_create(&a, NULL, 0)) {
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
    r = to_host ?
        virgl_renderer_transfer_write_iov(t.resource_id, t.hdr.ctx_id,
                                          t.level, t.stride, t.layer_stride,
                                          &box, t.offset, res->iov, res->iov_cnt) :
        virgl_renderer_transfer_read_iov(t.resource_id, t.hdr.ctx_id,
                                         t.level, t.stride, t.layer_stride,
                                         &box, t.offset, res->iov, res->iov_cnt);
    if (r) {
        cmd->error = VIRTIO_GPU_RESP_ERR_UNSPEC;
    }
}

static void virtio_gpu_gl_submit(VirtIOGPU *g,
                                 struct virtio_gpu_ctrl_command *cmd)
{
    struct virtio_gpu_cmd_submit cs;
    const void *buf;
    void *copy = NULL;
    size_t n;

    VIRTIO_GPU_FILL_CMD(cs);
    virtio_gpu_bswap_32(&cs, sizeof(cs));
    if (!cs.size || (cs.size & 3)) {
        cmd->error = VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER;
        return;
    }

    buf = virtio_gpu_gl_map_iov(cmd->elem.out_sg, cmd->elem.out_num,
                               sizeof(cs), cs.size);
    if (!buf) {
        copy = g_malloc(cs.size);
        n = iov_to_buf(cmd->elem.out_sg, cmd->elem.out_num,
                       sizeof(cs), copy, cs.size);
        if (n != cs.size) {
            g_free(copy);
            cmd->error = VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER;
            return;
        }
        buf = copy;
    }
    if (virgl_renderer_submit_cmd((void *)buf, cs.hdr.ctx_id, cs.size / 4)) {
        cmd->error = VIRTIO_GPU_RESP_ERR_UNSPEC;
    }
    g_free(copy);
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
    r = flags ? virgl_renderer_context_create_with_flags(cc.hdr.ctx_id, flags,
                                                         cc.nlen, cc.debug_name) :
                virgl_renderer_context_create(cc.hdr.ctx_id, cc.nlen,
                                              cc.debug_name);
    if (r) {
        cmd->error = VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID;
    }
}

static void virtio_gpu_gl_ctx_destroy(VirtIOGPU *g,
                                      struct virtio_gpu_ctrl_command *cmd)
{
    struct virtio_gpu_ctx_destroy cd;

    VIRTIO_GPU_FILL_CMD(cd);
    virtio_gpu_bswap_32(&cd, sizeof(cd));
    virgl_renderer_context_destroy(cd.hdr.ctx_id);
}

static void virtio_gpu_gl_ctx_resource(VirtIOGPU *g,
                                       struct virtio_gpu_ctrl_command *cmd,
                                       bool attach)
{
    struct virtio_gpu_ctx_resource cr;

    VIRTIO_GPU_FILL_CMD(cr);
    virtio_gpu_bswap_32(&cr, sizeof(cr));
    if (attach) {
        virgl_renderer_ctx_attach_resource(cr.hdr.ctx_id, cr.resource_id);
    } else {
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
    virtio_gpu_resource_attach_backing(g, cmd);
    if (cmd->error) {
        return;
    }
    res = virtio_gpu_find_resource(g, ab.resource_id);
    if (res && res->virgl &&
        virgl_renderer_resource_attach_iov(ab.resource_id, res->iov,
                                           res->iov_cnt)) {
        cmd->error = VIRTIO_GPU_RESP_ERR_UNSPEC;
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
    res = virtio_gpu_find_resource(g, db.resource_id);
    if (res && res->virgl) {
        virgl_renderer_resource_detach_iov(db.resource_id, &iov, &niov);
    }
    virtio_gpu_resource_detach_backing(g, cmd);
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
        cmd->error = VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID;
        return true;
    }
    if (!ss.resource_id || !ss.r.width || !ss.r.height) {
        dpy_gl_scanout_disable(g->parent_obj.scanout[ss.scanout_id].con);
        virtio_gpu_disable_scanout(g, ss.scanout_id);
        return true;
    }
    res = virtio_gpu_find_resource(g, ss.resource_id);
    if (!res || !res->virgl) {
        return false;
    }
    if (virgl_renderer_resource_get_info(ss.resource_id, &info)) {
        cmd->error = VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID;
        return true;
    }
    if (ss.r.x > info.width || ss.r.y > info.height ||
        ss.r.width > info.width - ss.r.x ||
        ss.r.height > info.height - ss.r.y) {
        cmd->error = VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER;
        return true;
    }
    fb.width = info.width;
    fb.height = info.height;
    fb.stride = info.stride;
    fb.bytes_pp = 4;
    g->parent_obj.enable = 1;
    virgl_renderer_force_ctx_0();
    qemu_console_resize(g->parent_obj.scanout[ss.scanout_id].con,
                        ss.r.width, ss.r.height);
    virtio_gpu_update_scanout(g, ss.scanout_id, res, &fb, &ss.r);
    dpy_gl_scanout_texture(g->parent_obj.scanout[ss.scanout_id].con,
                           info.tex_id,
                           info.flags & VIRTIO_GPU_RESOURCE_FLAG_Y_0_TOP,
                           info.width, info.height,
                           ss.r.x, ss.r.y, ss.r.width, ss.r.height, NULL);
    dpy_gl_update(g->parent_obj.scanout[ss.scanout_id].con,
                  0, 0, ss.r.width, ss.r.height);
    return true;
}

static bool virtio_gpu_gl_flush(VirtIOGPU *g,
                                struct virtio_gpu_ctrl_command *cmd)
{
    struct virtio_gpu_resource_flush rf;
    struct virtio_gpu_simple_resource *res;
    int i;

    if (!virtio_gpu_gl_fill(cmd, &rf, sizeof(rf))) {
        return true;
    }
    virtio_gpu_bswap_32(&rf, sizeof(rf));
    res = virtio_gpu_find_resource(g, rf.resource_id);
    if (!res || !res->virgl) {
        return false;
    }
    for (i = 0; i < g->parent_obj.conf.max_outputs; i++) {
        if (res->scanout_bitmask & (1 << i)) {
            struct virtio_gpu_scanout *scanout = &g->parent_obj.scanout[i];

            dpy_gl_update(scanout->con, 0, 0,
                          scanout->width, scanout->height);
        }
    }
    return true;
}

static void virtio_gpu_gl_process_cmd(VirtIOGPU *g,
                                      struct virtio_gpu_ctrl_command *cmd)
{
    VIRTIO_GPU_FILL_CMD(cmd->cmd_hdr);
    virtio_gpu_ctrl_hdr_bswap(&cmd->cmd_hdr);

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
    if (data && s->current_cursor && w == s->current_cursor->width &&
        h == s->current_cursor->height) {
        memcpy(s->current_cursor->data, data, w * h * 4);
    }
}

static void virtio_gpu_gl_resource_destroy(VirtIOGPU *g,
                                           struct virtio_gpu_simple_resource *res,
                                           Error **errp)
{
    int i;

    for (i = 0; i < g->parent_obj.conf.max_outputs; i++) {
        if (res->scanout_bitmask & (1 << i)) {
            dpy_gl_scanout_disable(g->parent_obj.scanout[i].con);
        }
    }
    if (res->virgl) {
        virgl_renderer_resource_unref(res->resource_id);
    }
    virtio_gpu_resource_destroy(g, res, errp);
}

static void virtio_gpu_gl_device_realize(DeviceState *qdev, Error **errp)
{
    VirtIOGPU *g = VIRTIO_GPU(qdev);

    if (!display_opengl) {
        error_setg(errp, "virtio-gpu-gl requires an OpenGL display");
        return;
    }
    g->parent_obj.conf.flags |= 1 << VIRTIO_GPU_FLAG_VIRGL_ENABLED;
    g->parent_obj.conf.flags |= 1 << VIRTIO_GPU_FLAG_CONTEXT_INIT_ENABLED;
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
    int i;

    for (i = 0; i < g->parent_obj.conf.max_outputs; i++) {
        if (g->parent_obj.scanout[i].con) {
            dpy_gl_scanout_disable(g->parent_obj.scanout[i].con);
        }
    }
    if (g->virgl_inited) {
        virgl_renderer_cleanup(g);
        g->virgl_inited = false;
    }
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
