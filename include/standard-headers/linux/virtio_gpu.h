
#ifndef VIRTIO_GPU_HW_H
#define VIRTIO_GPU_HW_H

#include "standard-headers/linux/types.h"

#define VIRTIO_GPU_F_VIRGL               0

#define VIRTIO_GPU_F_EDID                1
#define VIRTIO_GPU_F_RESOURCE_UUID       2

#define VIRTIO_GPU_F_RESOURCE_BLOB       3
#define VIRTIO_GPU_F_CONTEXT_INIT        4

enum virtio_gpu_ctrl_type {
	VIRTIO_GPU_UNDEFINED = 0,

	VIRTIO_GPU_CMD_GET_DISPLAY_INFO = 0x0100,
	VIRTIO_GPU_CMD_RESOURCE_CREATE_2D,
	VIRTIO_GPU_CMD_RESOURCE_UNREF,
	VIRTIO_GPU_CMD_SET_SCANOUT,
	VIRTIO_GPU_CMD_RESOURCE_FLUSH,
	VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
	VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING,
	VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING,
	VIRTIO_GPU_CMD_GET_CAPSET_INFO,
	VIRTIO_GPU_CMD_GET_CAPSET,
	VIRTIO_GPU_CMD_GET_EDID,
	VIRTIO_GPU_CMD_RESOURCE_ASSIGN_UUID,
	VIRTIO_GPU_CMD_RESOURCE_CREATE_BLOB,
	VIRTIO_GPU_CMD_SET_SCANOUT_BLOB,

	VIRTIO_GPU_CMD_CTX_CREATE = 0x0200,
	VIRTIO_GPU_CMD_CTX_DESTROY,
	VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE,
	VIRTIO_GPU_CMD_CTX_DETACH_RESOURCE,
	VIRTIO_GPU_CMD_RESOURCE_CREATE_3D,
	VIRTIO_GPU_CMD_TRANSFER_TO_HOST_3D,
	VIRTIO_GPU_CMD_TRANSFER_FROM_HOST_3D,
	VIRTIO_GPU_CMD_SUBMIT_3D,
	VIRTIO_GPU_CMD_RESOURCE_MAP_BLOB,
	VIRTIO_GPU_CMD_RESOURCE_UNMAP_BLOB,

	VIRTIO_GPU_CMD_UPDATE_CURSOR = 0x0300,
	VIRTIO_GPU_CMD_MOVE_CURSOR,

	VIRTIO_GPU_RESP_OK_NODATA = 0x1100,
	VIRTIO_GPU_RESP_OK_DISPLAY_INFO,
	VIRTIO_GPU_RESP_OK_CAPSET_INFO,
	VIRTIO_GPU_RESP_OK_CAPSET,
	VIRTIO_GPU_RESP_OK_EDID,
	VIRTIO_GPU_RESP_OK_RESOURCE_UUID,
	VIRTIO_GPU_RESP_OK_MAP_INFO,

	VIRTIO_GPU_RESP_ERR_UNSPEC = 0x1200,
	VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY,
	VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID,
	VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID,
	VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID,
	VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER,
};

enum virtio_gpu_shm_id {
	VIRTIO_GPU_SHM_ID_UNDEFINED = 0,
	VIRTIO_GPU_SHM_ID_HOST_VISIBLE = 1
};

#define VIRTIO_GPU_FLAG_FENCE         (1 << 0)
#define VIRTIO_GPU_FLAG_INFO_RING_IDX (1 << 1)

struct virtio_gpu_ctrl_hdr {
	uint32_t type;
	uint32_t flags;
	uint64_t fence_id;
	uint32_t ctx_id;
	uint8_t ring_idx;
	uint8_t padding[3];
};


struct virtio_gpu_cursor_pos {
	uint32_t scanout_id;
	uint32_t x;
	uint32_t y;
	uint32_t padding;
};

struct virtio_gpu_update_cursor {
	struct virtio_gpu_ctrl_hdr hdr;
	struct virtio_gpu_cursor_pos pos;  /* update & move */
	uint32_t resource_id;           /* update only */
	uint32_t hot_x;                 /* update only */
	uint32_t hot_y;                 /* update only */
	uint32_t padding;
};


struct virtio_gpu_rect {
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;
};

struct virtio_gpu_resource_unref {
	struct virtio_gpu_ctrl_hdr hdr;
	uint32_t resource_id;
	uint32_t padding;
};

struct virtio_gpu_resource_create_2d {
	struct virtio_gpu_ctrl_hdr hdr;
	uint32_t resource_id;
	uint32_t format;
	uint32_t width;
	uint32_t height;
};

struct virtio_gpu_set_scanout {
	struct virtio_gpu_ctrl_hdr hdr;
	struct virtio_gpu_rect r;
	uint32_t scanout_id;
	uint32_t resource_id;
};

struct virtio_gpu_resource_flush {
	struct virtio_gpu_ctrl_hdr hdr;
	struct virtio_gpu_rect r;
	uint32_t resource_id;
	uint32_t padding;
};

struct virtio_gpu_transfer_to_host_2d {
	struct virtio_gpu_ctrl_hdr hdr;
	struct virtio_gpu_rect r;
	uint64_t offset;
	uint32_t resource_id;
	uint32_t padding;
};

struct virtio_gpu_mem_entry {
	uint64_t addr;
	uint32_t length;
	uint32_t padding;
};

struct virtio_gpu_resource_attach_backing {
	struct virtio_gpu_ctrl_hdr hdr;
	uint32_t resource_id;
	uint32_t nr_entries;
};

struct virtio_gpu_resource_detach_backing {
	struct virtio_gpu_ctrl_hdr hdr;
	uint32_t resource_id;
	uint32_t padding;
};

#define VIRTIO_GPU_MAX_SCANOUTS 16
struct virtio_gpu_resp_display_info {
	struct virtio_gpu_ctrl_hdr hdr;
	struct virtio_gpu_display_one {
		struct virtio_gpu_rect r;
		uint32_t enabled;
		uint32_t flags;
	} pmodes[VIRTIO_GPU_MAX_SCANOUTS];
};


struct virtio_gpu_box {
	uint32_t x, y, z;
	uint32_t w, h, d;
};

struct virtio_gpu_transfer_host_3d {
	struct virtio_gpu_ctrl_hdr hdr;
	struct virtio_gpu_box box;
	uint64_t offset;
	uint32_t resource_id;
	uint32_t level;
	uint32_t stride;
	uint32_t layer_stride;
};

#define VIRTIO_GPU_RESOURCE_FLAG_Y_0_TOP (1 << 0)
struct virtio_gpu_resource_create_3d {
	struct virtio_gpu_ctrl_hdr hdr;
	uint32_t resource_id;
	uint32_t target;
	uint32_t format;
	uint32_t bind;
	uint32_t width;
	uint32_t height;
	uint32_t depth;
	uint32_t array_size;
	uint32_t last_level;
	uint32_t nr_samples;
	uint32_t flags;
	uint32_t padding;
};

#define VIRTIO_GPU_CONTEXT_INIT_CAPSET_ID_MASK 0x000000ff
struct virtio_gpu_ctx_create {
	struct virtio_gpu_ctrl_hdr hdr;
	uint32_t nlen;
	uint32_t context_init;
	char debug_name[64];
};

struct virtio_gpu_ctx_destroy {
	struct virtio_gpu_ctrl_hdr hdr;
};

struct virtio_gpu_ctx_resource {
	struct virtio_gpu_ctrl_hdr hdr;
	uint32_t resource_id;
	uint32_t padding;
};

struct virtio_gpu_cmd_submit {
	struct virtio_gpu_ctrl_hdr hdr;
	uint32_t size;
	uint32_t padding;
};

#define VIRTIO_GPU_CAPSET_VIRGL 1
#define VIRTIO_GPU_CAPSET_VIRGL2 2
#define VIRTIO_GPU_CAPSET_VENUS 4
#define VIRTIO_GPU_CAPSET_DRM 6

struct virtio_gpu_get_capset_info {
	struct virtio_gpu_ctrl_hdr hdr;
	uint32_t capset_index;
	uint32_t padding;
};

struct virtio_gpu_resp_capset_info {
	struct virtio_gpu_ctrl_hdr hdr;
	uint32_t capset_id;
	uint32_t capset_max_version;
	uint32_t capset_max_size;
	uint32_t padding;
};

struct virtio_gpu_get_capset {
	struct virtio_gpu_ctrl_hdr hdr;
	uint32_t capset_id;
	uint32_t capset_version;
};

struct virtio_gpu_resp_capset {
	struct virtio_gpu_ctrl_hdr hdr;
	uint8_t capset_data[];
};

struct virtio_gpu_cmd_get_edid {
	struct virtio_gpu_ctrl_hdr hdr;
	uint32_t scanout;
	uint32_t padding;
};

struct virtio_gpu_resp_edid {
	struct virtio_gpu_ctrl_hdr hdr;
	uint32_t size;
	uint32_t padding;
	uint8_t edid[1024];
};

#define VIRTIO_GPU_EVENT_DISPLAY (1 << 0)

struct virtio_gpu_config {
	uint32_t events_read;
	uint32_t events_clear;
	uint32_t num_scanouts;
	uint32_t num_capsets;
};

enum virtio_gpu_formats {
	VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM  = 1,
	VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM  = 2,
	VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM  = 3,
	VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM  = 4,

	VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM  = 67,
	VIRTIO_GPU_FORMAT_X8B8G8R8_UNORM  = 68,

	VIRTIO_GPU_FORMAT_A8B8G8R8_UNORM  = 121,
	VIRTIO_GPU_FORMAT_R8G8B8X8_UNORM  = 134,
};

struct virtio_gpu_resource_assign_uuid {
	struct virtio_gpu_ctrl_hdr hdr;
	uint32_t resource_id;
	uint32_t padding;
};

struct virtio_gpu_resp_resource_uuid {
	struct virtio_gpu_ctrl_hdr hdr;
	uint8_t uuid[16];
};

struct virtio_gpu_resource_create_blob {
	struct virtio_gpu_ctrl_hdr hdr;
	uint32_t resource_id;
#define VIRTIO_GPU_BLOB_MEM_GUEST             0x0001
#define VIRTIO_GPU_BLOB_MEM_HOST3D            0x0002
#define VIRTIO_GPU_BLOB_MEM_HOST3D_GUEST      0x0003

#define VIRTIO_GPU_BLOB_FLAG_USE_MAPPABLE     0x0001
#define VIRTIO_GPU_BLOB_FLAG_USE_SHAREABLE    0x0002
#define VIRTIO_GPU_BLOB_FLAG_USE_CROSS_DEVICE 0x0004
	uint32_t blob_mem;
	uint32_t blob_flags;
	uint32_t nr_entries;
	uint64_t blob_id;
	uint64_t size;
};

struct virtio_gpu_set_scanout_blob {
	struct virtio_gpu_ctrl_hdr hdr;
	struct virtio_gpu_rect r;
	uint32_t scanout_id;
	uint32_t resource_id;
	uint32_t width;
	uint32_t height;
	uint32_t format;
	uint32_t padding;
	uint32_t strides[4];
	uint32_t offsets[4];
};

struct virtio_gpu_resource_map_blob {
	struct virtio_gpu_ctrl_hdr hdr;
	uint32_t resource_id;
	uint32_t padding;
	uint64_t offset;
};

#define VIRTIO_GPU_MAP_CACHE_MASK     0x0f
#define VIRTIO_GPU_MAP_CACHE_NONE     0x00
#define VIRTIO_GPU_MAP_CACHE_CACHED   0x01
#define VIRTIO_GPU_MAP_CACHE_UNCACHED 0x02
#define VIRTIO_GPU_MAP_CACHE_WC       0x03
struct virtio_gpu_resp_map_info {
	struct virtio_gpu_ctrl_hdr hdr;
	uint32_t map_info;
	uint32_t padding;
};

struct virtio_gpu_resource_unmap_blob {
	struct virtio_gpu_ctrl_hdr hdr;
	uint32_t resource_id;
	uint32_t padding;
};

#endif
