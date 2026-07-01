DEF("h", 0, QEMU_OPTION_h, "", QEMU_ARCH_ALL)

DEF("version", 0, QEMU_OPTION_version, "", QEMU_ARCH_ALL)

DEF("machine", HAS_ARG, QEMU_OPTION_machine, "", QEMU_ARCH_ALL)

DEF("M", HAS_ARG, QEMU_OPTION_M, "", QEMU_ARCH_ALL)

DEF("cpu", HAS_ARG, QEMU_OPTION_cpu, "", QEMU_ARCH_ALL)

DEF("accel", HAS_ARG, QEMU_OPTION_accel, "", QEMU_ARCH_ALL)

DEF("smp", HAS_ARG, QEMU_OPTION_smp, "", QEMU_ARCH_ALL)

DEF("set", HAS_ARG, QEMU_OPTION_set, "", QEMU_ARCH_ALL)

DEF("global", HAS_ARG, QEMU_OPTION_global, "", QEMU_ARCH_ALL)

DEF("boot", HAS_ARG, QEMU_OPTION_boot, "", QEMU_ARCH_ALL)

DEF("m", HAS_ARG, QEMU_OPTION_m, "", QEMU_ARCH_ALL)

DEF("mem-prealloc", 0, QEMU_OPTION_mem_prealloc, "", QEMU_ARCH_ALL)

DEF("k", HAS_ARG, QEMU_OPTION_k, "", QEMU_ARCH_ALL)

DEF("audio", HAS_ARG, QEMU_OPTION_audio, "", QEMU_ARCH_ALL)

DEF("audiodev", HAS_ARG, QEMU_OPTION_audiodev, "", QEMU_ARCH_ALL)

DEF("device", HAS_ARG, QEMU_OPTION_device, "", QEMU_ARCH_ALL)

DEF("name", HAS_ARG, QEMU_OPTION_name, "", QEMU_ARCH_ALL)

DEF("uuid", HAS_ARG, QEMU_OPTION_uuid, "", QEMU_ARCH_ALL)

DEF("cdrom", HAS_ARG, QEMU_OPTION_cdrom, "", QEMU_ARCH_ALL)

DEF("blockdev", HAS_ARG, QEMU_OPTION_blockdev, "", QEMU_ARCH_ALL)

DEF("drive", HAS_ARG, QEMU_OPTION_drive, "", QEMU_ARCH_ALL)

DEF("mtdblock", HAS_ARG, QEMU_OPTION_mtdblock, "", QEMU_ARCH_ALL)

DEF("sd", HAS_ARG, QEMU_OPTION_sd, "", QEMU_ARCH_ALL)

DEF("display", HAS_ARG, QEMU_OPTION_display, "", QEMU_ARCH_ALL)

DEF("nographic", 0, QEMU_OPTION_nographic, "", QEMU_ARCH_ALL)

#ifdef CONFIG_SPICE
DEF("spice", HAS_ARG, QEMU_OPTION_spice, "", QEMU_ARCH_ALL)
#endif

DEF("full-screen", 0, QEMU_OPTION_full_screen, "", QEMU_ARCH_ALL)

DEF("g", HAS_ARG, QEMU_OPTION_g, "", QEMU_ARCH_PPC | QEMU_ARCH_SPARC | QEMU_ARCH_M68K)

DEF("acpitable", HAS_ARG, QEMU_OPTION_acpitable, "", QEMU_ARCH_I386)

DEF("smbios", HAS_ARG, QEMU_OPTION_smbios, "", QEMU_ARCH_I386 | QEMU_ARCH_ARM | QEMU_ARCH_LOONGARCH | QEMU_ARCH_RISCV)

DEF("netdev", HAS_ARG, QEMU_OPTION_netdev, "", QEMU_ARCH_ALL)
DEF("nic", HAS_ARG, QEMU_OPTION_nic, "", QEMU_ARCH_ALL)
DEF("net", HAS_ARG, QEMU_OPTION_net, "", QEMU_ARCH_ALL)

DEF("chardev", HAS_ARG, QEMU_OPTION_chardev, "", QEMU_ARCH_ALL)

DEF("bios", HAS_ARG, QEMU_OPTION_bios, "", QEMU_ARCH_ALL)

DEF("kernel", HAS_ARG, QEMU_OPTION_kernel, "", QEMU_ARCH_ALL)

DEF("shim", HAS_ARG, QEMU_OPTION_shim, "", QEMU_ARCH_ALL)

DEF("append", HAS_ARG, QEMU_OPTION_append, "", QEMU_ARCH_ALL)

DEF("initrd", HAS_ARG, QEMU_OPTION_initrd, "", QEMU_ARCH_ALL)

DEF("dtb", HAS_ARG, QEMU_OPTION_dtb, "", QEMU_ARCH_ALL)

DEF("compat", HAS_ARG, QEMU_OPTION_compat, "", QEMU_ARCH_ALL)

DEF("fw_cfg", HAS_ARG, QEMU_OPTION_fwcfg, "", QEMU_ARCH_ALL)

DEF("serial", HAS_ARG, QEMU_OPTION_serial, "", QEMU_ARCH_ALL)

DEF("parallel", HAS_ARG, QEMU_OPTION_parallel, "", QEMU_ARCH_ALL)

DEF("monitor", HAS_ARG, QEMU_OPTION_monitor, "", QEMU_ARCH_ALL)
DEF("mon", HAS_ARG, QEMU_OPTION_mon, "", QEMU_ARCH_ALL)

DEF("debugcon", HAS_ARG, QEMU_OPTION_debugcon, "", QEMU_ARCH_ALL)

DEF("pidfile", HAS_ARG, QEMU_OPTION_pidfile, "", QEMU_ARCH_ALL)

DEF("preconfig", 0, QEMU_OPTION_preconfig, "", QEMU_ARCH_ALL)

DEF("S", 0, QEMU_OPTION_S, "", QEMU_ARCH_ALL)

DEF("overcommit", HAS_ARG, QEMU_OPTION_overcommit, "", QEMU_ARCH_ALL)

DEF("d", HAS_ARG, QEMU_OPTION_d, "", QEMU_ARCH_ALL)

DEF("D", HAS_ARG, QEMU_OPTION_D, "", QEMU_ARCH_ALL)

DEF("dfilter", HAS_ARG, QEMU_OPTION_DFILTER, "", QEMU_ARCH_ALL)

DEF("seed", HAS_ARG, QEMU_OPTION_seed, "", QEMU_ARCH_ALL)

DEF("L", HAS_ARG, QEMU_OPTION_L, "", QEMU_ARCH_ALL)

DEF("no-reboot", 0, QEMU_OPTION_no_reboot, "", QEMU_ARCH_ALL)

DEF("no-shutdown", 0, QEMU_OPTION_no_shutdown, "", QEMU_ARCH_ALL)

DEF("action", HAS_ARG, QEMU_OPTION_action, "", QEMU_ARCH_ALL)

#ifndef _WIN32
DEF("daemonize", 0, QEMU_OPTION_daemonize, "", QEMU_ARCH_ALL)
#endif

DEF("option-rom", HAS_ARG, QEMU_OPTION_option_rom, "", QEMU_ARCH_ALL)

DEF("rtc", HAS_ARG, QEMU_OPTION_rtc, "", QEMU_ARCH_ALL)

DEF("echr", HAS_ARG, QEMU_OPTION_echr, "", QEMU_ARCH_ALL)

DEF("nodefaults", 0, QEMU_OPTION_nodefaults, "", QEMU_ARCH_ALL)

DEF("prom-env", HAS_ARG, QEMU_OPTION_prom_env, "", QEMU_ARCH_PPC | QEMU_ARCH_SPARC)
DEF("old-param", 0, QEMU_OPTION_old_param, "", QEMU_ARCH_ARM)

DEF("sandbox", HAS_ARG, QEMU_OPTION_sandbox, "", QEMU_ARCH_ALL)

DEF("readconfig", HAS_ARG, QEMU_OPTION_readconfig, "", QEMU_ARCH_ALL)

DEF("no-user-config", 0, QEMU_OPTION_nouserconfig, "", QEMU_ARCH_ALL)

DEF("trace", HAS_ARG, QEMU_OPTION_trace, "", QEMU_ARCH_ALL)
DEF("plugin", HAS_ARG, QEMU_OPTION_plugin, "", QEMU_ARCH_ALL)

#ifdef CONFIG_POSIX
DEF("run-with", HAS_ARG, QEMU_OPTION_run_with, "", QEMU_ARCH_ALL)
#endif

DEF("msg", HAS_ARG, QEMU_OPTION_msg, "", QEMU_ARCH_ALL)

DEF("enable-sync-profile", 0, QEMU_OPTION_enable_sync_profile, "", QEMU_ARCH_ALL)

#if defined(CONFIG_TCG) && defined(CONFIG_LINUX)
DEF("perfmap", 0, QEMU_OPTION_perfmap, "", QEMU_ARCH_ALL)

DEF("jitdump", 0, QEMU_OPTION_jitdump, "", QEMU_ARCH_ALL)
#endif

DEF("object", HAS_ARG, QEMU_OPTION_object, "", QEMU_ARCH_ALL)

#undef DEF
#undef DEFHEADING
#undef ARCHHEADING
