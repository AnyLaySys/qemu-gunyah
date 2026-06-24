#define gunyah_slots_lock(s)    qemu_mutex_lock(&s->slots_lock)
#define gunyah_slots_unlock(s)  qemu_mutex_unlock(&s->slots_lock)
static gunyah_slot *gunyah_find_overlap_slot(GUNYAHState *s, uint64_t start, uint64_t size) {
    gunyah_slot *slot;
    int i;
    for (i = 0; i < s->nr_slots; ++i) {
        slot = &s->slots[i];
        if (slot->size && start < (slot->start + slot->size) && (start + size) > slot->start) {
            return slot;
        }
    }
    return NULL;
}
gunyah_slot *gunyah_find_slot_by_addr(uint64_t addr) {
    GUNYAHState *s = GUNYAH_STATE(current_accel());
    int i;
    gunyah_slot *slot = NULL;
    gunyah_slots_lock(s);
    for (i = 0; i < s->nr_slots; ++i) {
        slot = &s->slots[i];
        if (slot->size && addr >= slot->start && addr < slot->start + slot->size) {
            gunyah_slots_unlock(s);
            return slot;
        }
    }
    gunyah_slots_unlock(s);
    return NULL;
}
static gunyah_slot *gunyah_get_free_slot(GUNYAHState *s) {
    int i;
    for (i = 0; i < s->nr_slots; i++) {
        if (s->slots[i].size == 0) {
            return &s->slots[i];
        }
    }
    return NULL;
}
#define GUNYAH_LEND_CHUNK_SIZE  (64ULL * 1024 * 1024)
static void
gunyah_add_mem_slot(GUNYAHState *s, uint8_t *hva, uint64_t gpa, uint64_t size, bool lend,
                    enum gh_mem_flags flags) {
    gunyah_slot *slot;
    struct gh_userspace_memory_region gumr;
    int ret;
    slot = gunyah_get_free_slot(s);
    if (!slot) {
        error_report("No free slots to add memory!");
        exit(1);
    }
    slot->size = size;
    slot->mem = hva;
    slot->start = gpa;
    slot->lend = lend;
    slot->flags = flags;
    gumr.label = slot->id;
    gumr.flags = flags;
    gumr.guest_phys_addr = gpa;
    gumr.memory_size = size;
    gumr.userspace_addr = (__u64) hva;
    gh_report("add_mem label=%u gpa=0x%"
    PRIx64
    " size=0x%"
    PRIx64
    " hva=0x%"
    PRIx64
    " flags=0x%x lend=%d", gumr.label, (uint64_t) gumr.guest_phys_addr, (uint64_t) gumr.memory_size, (uint64_t) gumr.userspace_addr, gumr.flags, lend);
    if (lend) {
        ret = gunyah_vm_ioctl(GH_VM_ANDROID_LEND_USER_MEM, &gumr);
    } else {
        ret = gunyah_vm_ioctl(GH_VM_SET_USER_MEM_REGION, &gumr);
    }
    if (ret) {
        if (!lend && errno == EEXIST) {
            gh_report("SHARE gpa=0x%"
            PRIx64
            " already exists — reusing"
            " (recycled RingBlob)", gpa);
        } else {
            gh_report("%s ioctl FAILED: %s (ret=%d, errno=%d)", lend ? "LEND" : "SHARE",
                         strerror(errno), ret, errno);
            exit(1);
        }
    } else {
        gh_report("add_mem_slot OK (gpa=0x%"
        PRIx64
        " size=0x%"
        PRIx64
        ")", gpa, size);
    }
}
static void
gunyah_add_mem(GUNYAHState *s, MemoryRegionSection *section, bool lend, enum gh_mem_flags flags) {
    MemoryRegion *area = section->mr;
    uint64_t total_size = int128_get64(section->size);
    uint8_t *base_hva = memory_region_get_ram_ptr(area) + section->offset_within_region;
    uint64_t base_gpa = section->offset_within_address_space;
    int ret;
    const uint64_t thp_size = 2ULL * 1024 * 1024;
    uint64_t total_chunks = total_size / thp_size;
    uint64_t large_page_bytes = 0;
    uint8_t *need_thp = NULL;
    if (lend) {
        FILE *f;
        gh_report("preparing LEND region: hva=0x%"PRIx64
                     " size=0x%"PRIx64" (%"PRIu64" MB)",
                     (uint64_t)(uintptr_t)base_hva, total_size,
                     total_size >> 20);
        f = fopen("/proc/sys/vm/drop_caches", "w");
        if (f) {
            fprintf(f, "3\n");
            fclose(f);
        }
        f = fopen("/proc/sys/vm/compact_memory", "w");
        if (f) {
            fprintf(f, "1\n");
            fclose(f);
        }
        usleep(500000);
        {
            static const char *mthp_sizes[] = {"64kB", "128kB",
                                               "256kB", "512kB", "1024kB", NULL};
            int mi;
            int mthp_enabled = 0;
            for (mi = 0; mthp_sizes[mi]; mi++) {
                char path[128];
                snprintf(path, sizeof(path),
                         "/sys/kernel/mm/transparent_hugepage/hugepages-%s/enabled",
                         mthp_sizes[mi]);
                f = fopen(path, "w");
                if (!f)
                    continue;
                fprintf(f, "always\n");
                fclose(f);
                mthp_enabled++;
            }
            gh_report("mTHP enabled=%d", mthp_enabled);
        }
        ret = madvise(base_hva, total_size, MADV_HUGEPAGE);
        gh_report("MADV_HUGEPAGE %s", ret == 0 ? "OK" : strerror(errno));
        {
            const uint64_t batch_size = 2048ULL * 1024 * 1024;
            uint64_t offset;
            gh_report("populating %"PRIu64" MB in %"PRIu64
                         " x %"PRIu64" MB batches",
                         total_size >> 20,
                         (total_size + batch_size - 1) / batch_size,
                         batch_size >> 20);
            for (offset = 0; offset < total_size; offset += batch_size) {
                uint64_t len = total_size - offset;
                if (len > batch_size) {
                    len = batch_size;
                }
                ret = madvise(base_hva + offset, len, MADV_POPULATE_WRITE);
                if (ret != 0) {
                    volatile char *p = (volatile char *)base_hva + offset;
                    uint64_t npages = len / 4096;
                    uint64_t i;
                    for (i = 0; i < npages; i++) {
                        p[i * 4096] = p[i * 4096];
                    }
                }
                if (offset + batch_size < total_size) {
                    f = fopen("/proc/sys/vm/compact_memory", "w");
                    if (f) {
                        fprintf(f, "1\n");
                        fclose(f);
                    }
                    usleep(200000);
                }
            }
        }
        {
            static const struct {
                uint64_t size;
                int order;
                const char *name;
            } collapse_levels[] = {
                {2ULL * 1024 * 1024, 9, "2MB"},
                {1ULL * 1024 * 1024, 8, "1MB"},
                {512ULL * 1024, 7, "512KB"},
                {256ULL * 1024, 6, "256KB"},
                {128ULL * 1024, 5, "128KB"},
                {64ULL * 1024, 4, "64KB"},
            };
            const uint64_t map_unit = 64ULL * 1024;
            uint64_t map_count = total_size / map_unit;
            uint8_t *order_map = calloc(map_count, 1);
            int level;
            if (!order_map) {
                need_thp = calloc(total_chunks, 1);
                if (need_thp) {
                    memset(need_thp, 1, total_chunks);
                }
                goto skip_phase3;
            }
            gh_report("cascading MADV_COLLAPSE 2MB -> 64KB");
            for (level = 0; level < (int)(sizeof(collapse_levels) / sizeof(collapse_levels[0])); level++) {
                uint64_t csize = collapse_levels[level].size;
                int corder = collapse_levels[level].order;
                uint64_t units_per_chunk = csize / map_unit;
                uint64_t num_chunks_lvl = total_size / csize;
                uint64_t collapsed = 0;
                uint64_t skipped = 0;
                uint64_t failed = 0;
                int last_err = 0;
                uint64_t ci;
                for (ci = 0; ci < num_chunks_lvl; ci++) {
                    uint64_t map_base = ci * units_per_chunk;
                    uint64_t u;
                    int all_free = 1;
                    for (u = 0; u < units_per_chunk; u++) {
                        if (order_map[map_base + u] != 0) {
                            all_free = 0;
                            break;
                        }
                    }
                    if (!all_free) {
                        skipped++;
                        continue;
                    }
                    ret = madvise(base_hva + ci * csize, csize, MADV_COLLAPSE);
                    if (ret == 0) {
                        for (u = 0; u < units_per_chunk; u++) {
                            order_map[map_base + u] = corder;
                        }
                        collapsed++;
                    } else {
                        failed++;
                        last_err = errno;
                    }
                }
                gh_report("%s collapse: %"PRIu64" OK, %"PRIu64
                             " skipped, %"PRIu64" failed (err=%d)",
                             collapse_levels[level].name, collapsed, skipped,
                             failed, last_err);
            }
            {
                uint64_t uncollapsed = 0;
                uint64_t mi_idx;
                for (mi_idx = 0; mi_idx < map_count; mi_idx++) {
                    if (order_map[mi_idx] == 0) {
                        uint64_t off = mi_idx * map_unit;
                        uint64_t pg;
                        ret = madvise(base_hva + off, map_unit, MADV_POPULATE_WRITE);
                        if (ret != 0) {
                            volatile char *p = (volatile char *)base_hva + off;
                            for (pg = 0; pg < map_unit / 4096; pg++) {
                                p[pg * 4096] = p[pg * 4096];
                            }
                        }
                        uncollapsed++;
                    }
                }
                large_page_bytes = (map_count - uncollapsed) * map_unit;
            }
            need_thp = calloc(total_chunks, 1);
            if (need_thp) {
                uint64_t ci;
                for (ci = 0; ci < total_chunks; ci++) {
                    uint64_t map_base = ci * (thp_size / map_unit);
                    uint64_t u;
                    int is_thp = 1;
                    for (u = 0; u < thp_size / map_unit; u++) {
                        if (order_map[map_base + u] < 9) {
                            is_thp = 0;
                            break;
                        }
                    }
                    need_thp[ci] = is_thp ? 0 : 1;
                }
            }
            free(order_map);
        }
    skip_phase3:
        gh_report("large-page coverage: %"PRIu64" / %"PRIu64
                     " MB (%.1f%%)", large_page_bytes >> 20,
                     total_size >> 20,
                     (double)large_page_bytes * 100.0 / (double)total_size);
        ret = mlock(base_hva, total_size);
        if (ret == 0) {
            gh_report("mlock: OK");
        } else {
            gh_report("mlock FAILED: %s", strerror(errno));
        }
    }
    if (lend && total_size > GUNYAH_LEND_CHUNK_SIZE) {
        int chunk_idx = 0;
        if (need_thp) {
            uint64_t thp_ok = 0;
            uint64_t thp_fail = 0;
            for (uint64_t c = 0; c < total_chunks; c++) {
                if (need_thp[c])
                    thp_fail++;
                else
                    thp_ok++;
            }
            gh_report("THP-aware LEND split: %"
            PRIu64
            " MB total, %"
            PRIu64
            " THP(2MB) chunks, %"
            PRIu64
            " mTHP/4KB chunks", total_size >> 20, thp_ok, thp_fail);
            uint64_t c = 0;
            while (c < total_chunks) {
                uint8_t is_small = need_thp[c];
                uint64_t run_start = c;
                while (c < total_chunks && need_thp[c] == is_small)
                    c++;
                uint64_t run_offset = run_start * thp_size;
                uint64_t run_size = (c - run_start) * thp_size;
                uint64_t sub_off = 0;
                while (sub_off < run_size) {
                    uint64_t sub_sz = run_size - sub_off;
                    if (sub_sz > GUNYAH_LEND_CHUNK_SIZE)
                        sub_sz = GUNYAH_LEND_CHUNK_SIZE;
                    gh_report("%s-chunk[%d] gpa=0x%"PRIx64
                                 " size=0x%"PRIx64,
                                 is_small ? "mTHP" : "THP", chunk_idx,
                                 base_gpa + run_offset + sub_off, sub_sz);
                    gunyah_add_mem_slot(s, base_hva + run_offset + sub_off,
                                        base_gpa + run_offset + sub_off, sub_sz, lend, flags);
                    sub_off += sub_sz;
                    chunk_idx++;
                }
            }
            gh_report("THP-aware split done: %d LEND slots used", chunk_idx);
            free(need_thp);
            return;
        }
        {
            uint64_t offset = 0;
            gh_report("splitting %"PRIu64" MB LEND into %"PRIu64
                         " x %"PRIu64" MB chunks",
                         total_size >> 20,
                         (uint64_t)((total_size + GUNYAH_LEND_CHUNK_SIZE - 1) /
                         GUNYAH_LEND_CHUNK_SIZE),
                         (uint64_t)(GUNYAH_LEND_CHUNK_SIZE >> 20));
            while (offset < total_size) {
                uint64_t chunk_sz = total_size - offset;
                if (chunk_sz > GUNYAH_LEND_CHUNK_SIZE)
                    chunk_sz = GUNYAH_LEND_CHUNK_SIZE;
                gh_report("chunk[%d] gpa=0x%"PRIx64" size=0x%"PRIx64,
                             chunk_idx, base_gpa + offset, chunk_sz);
                gunyah_add_mem_slot(s, base_hva + offset, base_gpa + offset, chunk_sz, lend, flags);
                offset += chunk_sz;
                chunk_idx++;
            }
        }
        return;
    }
    free(need_thp);
    gunyah_add_mem_slot(s, base_hva, base_gpa, total_size, lend, flags);
}
static bool is_confidential_guest(void) {
    GUNYAHState *s = GUNYAH_STATE(current_accel());
    return s->protected_vm || current_machine->cgs != NULL;
}
static bool split_mem(GUNYAHState *s, MemoryRegion *area, MemoryRegionSection *section) {
    bool writeable = !area->readonly && !area->rom_device;
    if (!is_confidential_guest()) {
        return false;
    }
    if (!s->swiotlb_size || section->size <= s->swiotlb_size) {
        return false;
    }
    if (!memory_region_is_ram(area) || !writeable) {
        return false;
    }
    if (qatomic_read(&s->preshmem_reserved)) {
        return false;
    }
    if (section->size <= s->swiotlb_size) {
        return false;
    }
    return true;
}
static void gunyah_set_phys_mem(GUNYAHState *s, MemoryRegionSection *section, bool add) {
    MemoryRegion *area = section->mr;
    bool writable = !area->readonly && !area->rom_device;
    enum gh_mem_flags flags = 0;
    uint64_t page_size = qemu_real_host_page_size();
    MemoryRegionSection mrs = *section;
    bool lend = is_confidential_guest(), split = false;
    struct gunyah_slot *slot;
    gh_report("set_phys_mem gpa=0x%"
    PRIx64
    " size=0x%"
    PRIx64
    " add=%d is_ram=%d writable=%d lend=%d", (uint64_t) section->offset_within_address_space, (uint64_t) int128_get64(
            section->size), add, memory_region_is_ram(area), writable, lend);
    bool is_hostmem_blob = false;
    bool is_ramfb_mmio = memory_region_is_ram(area) && area->name && strcmp(area->name, "ramfb-mmio") == 0;
    if (memory_region_is_ram(area) && area->name && strcmp(area->name, "blob") == 0) {
        MemoryRegion *parent = area->container;
        if (parent && parent->name && strcmp(parent->name, "virtio-gpu-hostmem") == 0) {
            is_hostmem_blob = true;
        }
    }
    if (section->offset_within_address_space < GiB && !is_hostmem_blob && !is_ramfb_mmio) {
        static uint64_t last_skip_gpa = UINT64_MAX;
        uint64_t gpa = (uint64_t) section->offset_within_address_space;
        if (gpa != last_skip_gpa) {
            last_skip_gpa = gpa;
            gh_report("skipping region gpa=0x%"
            PRIx64
            " size=0x%"
            PRIx64
            " (below 1GiB) is_ram=%d add=%d", gpa, (uint64_t) int128_get64(
                    section->size), memory_region_is_ram(area), add);
        }
        return;
    }
    if (!memory_region_is_ram(area)) {
        if (writable) {
            return;
        } else if (!memory_region_is_romd(area)) {
            add = false;
        }
    }
    if (!QEMU_IS_ALIGNED(int128_get64(section->size), page_size) ||
        !QEMU_IS_ALIGNED(section->offset_within_address_space, page_size)) {
        error_report("Not page aligned");
        add = false;
    }
    gunyah_slots_lock(s);
    slot = gunyah_find_overlap_slot(s, section->offset_within_address_space,
                                    int128_get64(section->size));
    if (!add) {
        if (slot) {
            if (is_hostmem_blob) {
                gh_report("hostmem blob removal at gpa=0x%"
                PRIx64
                " size=0x%"
                PRIx64
                " — freeing slot", (uint64_t) section->offset_within_address_space, (uint64_t) int128_get64(
                        section->size));
                slot->size = 0;
                goto done;
            }
            error_report("Memory slot removal not yet supported!");
            exit(1);
        }
        goto done;
    } else {
        if (slot) {
            if (is_hostmem_blob) {
                gh_report("hostmem blob reuse at gpa=0x%"
                PRIx64
                " — freeing old slot", (uint64_t) section->offset_within_address_space);
                slot->size = 0;
                slot = NULL;
            } else {
                error_report("Overlapping slot registration not supported!");
                exit(1);
            }
        }
        if (qatomic_read(&s->vm_started) && !is_hostmem_blob && !is_ramfb_mmio) {
            error_report("Memory map changes after VM start not supported!");
            exit(1);
        }
    }
    if (is_ramfb_mmio) {
        lend = false;
        flags = GH_MEM_ALLOW_READ | GH_MEM_ALLOW_WRITE;
        gh_report("ramfb-mmio region at gpa=0x%"
        PRIx64
        " size=0x%"
        PRIx64
        " using SHARE", (uint64_t) section->offset_within_address_space, (uint64_t) int128_get64(
                section->size));
    } else if (is_hostmem_blob) {
        lend = false;
        flags = GH_MEM_ALLOW_READ | GH_MEM_ALLOW_WRITE;
        gh_report("hostmem blob region at gpa=0x%"
        PRIx64
        " size=0x%"
        PRIx64
        " — using SHARE (no exec)", (uint64_t) section->offset_within_address_space, (uint64_t) int128_get64(
                section->size));
    } else if (area->readonly || area->rom_device ||
               (!memory_region_is_ram(area) && memory_region_is_romd(area))) {
        flags = GH_MEM_ALLOW_READ | GH_MEM_ALLOW_WRITE | GH_MEM_ALLOW_EXEC;
    } else {
        flags = GH_MEM_ALLOW_READ | GH_MEM_ALLOW_WRITE | GH_MEM_ALLOW_EXEC;
    }
    split = split_mem(s, area, &mrs);
    if (split) {
        mrs.size -= s->swiotlb_size;
        gunyah_add_mem(s, &mrs, true, flags);
        lend = false;
        mrs.offset_within_region += mrs.size;
        mrs.offset_within_address_space += mrs.size;
        mrs.size = s->swiotlb_size;
        qatomic_set(&s->preshmem_reserved, true);
    }
    gunyah_add_mem(s, &mrs, lend, flags);
    done:
    gunyah_slots_unlock(s);
}
static void gunyah_region_add(MemoryListener *listener, MemoryRegionSection *section) {
    GUNYAHState *s = GUNYAH_STATE(current_accel());
    gunyah_set_phys_mem(s, section, true);
}
static void gunyah_region_del(MemoryListener *listener, MemoryRegionSection *section) {
    GUNYAHState *s = GUNYAH_STATE(current_accel());
    gunyah_set_phys_mem(s, section, false);
}
void gunyah_set_swiotlb_size(uint64_t size) {
    GUNYAHState *s = GUNYAH_STATE(current_accel());
    s->swiotlb_size = size;
}
static uint64_t gunyah_lend_start;
static uint64_t gunyah_lend_end;
static void gunyah_cache_lend_range(void) {
    GUNYAHState *s = GUNYAH_STATE(current_accel());
    int i;
    bool found = false;
    if (!s->protected_vm) {
        return;
    }
    gunyah_lend_start = UINT64_MAX;
    gunyah_lend_end = 0;
    for (i = 0; i < s->nr_slots; i++) {
        gunyah_slot *slot = &s->slots[i];
        if (slot->size && slot->lend) {
            if (slot->start < gunyah_lend_start) {
                gunyah_lend_start = slot->start;
            }
            if (slot->start + slot->size > gunyah_lend_end) {
                gunyah_lend_end = slot->start + slot->size;
            }
            found = true;
        }
    }
    if (found) {
        gh_report("cached LEND range: 0x%"PRIx64" - 0x%"PRIx64,
                     gunyah_lend_start, gunyah_lend_end);
    }
}
bool gunyah_addr_is_lend(uint64_t gpa) {
    return gpa >= gunyah_lend_start && gpa < gunyah_lend_end;
}
static MemoryListener gunyah_memory_listener = {.name = "gunyah", .priority = MEMORY_LISTENER_PRIORITY_ACCEL, .region_add = gunyah_region_add, .region_del = gunyah_region_del,};
int gunyah_create_vm(void) {
    GUNYAHState *s;
    int i;
    s = GUNYAH_STATE(current_accel());
    s->fd = qemu_open_old("/dev/gunyah", O_RDWR);
    if (s->fd == -1) {
        error_report("Could not access Gunyah kernel module at /dev/gunyah: %s", strerror(errno));
        exit(1);
    }
    gh_report("/dev/gunyah opened, fd=%d", s->fd);
    s->vmfd = gunyah_ioctl(GH_CREATE_VM, 0);
    if (s->vmfd < 0) {
        error_report("Could not create VM: %s (errno=%d)", strerror(errno), errno);
        exit(1);
    }
    gh_report("VM created, vmfd=%d", s->vmfd);
    qemu_mutex_init(&s->slots_lock);
    s->nr_slots = GUNYAH_MAX_MEM_SLOTS;
    for (i = 0; i < s->nr_slots; ++i) {
        s->slots[i].start = 0;
        s->slots[i].size = 0;
        s->slots[i].id = i;
    }
    gunyah_install_sigsegv_handler();
    memory_listener_register(&gunyah_memory_listener, &address_space_memory);
    return 0;
}
