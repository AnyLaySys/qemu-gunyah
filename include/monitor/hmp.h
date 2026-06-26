
#ifndef HMP_H
#define HMP_H

#include "qemu/readline.h"
#include "qapi/qapi-types-common.h"

bool hmp_handle_error(Monitor *mon, Error *err);
void hmp_help_cmd(Monitor *mon, const char *name);
strList *hmp_split_at_comma(const char *str);

void hmp_info_name(Monitor *mon, const QDict *qdict);
void hmp_info_version(Monitor *mon, const QDict *qdict);
void hmp_info_kvm(Monitor *mon, const QDict *qdict);
void hmp_info_status(Monitor *mon, const QDict *qdict);
void hmp_info_uuid(Monitor *mon, const QDict *qdict);
void hmp_info_chardev(Monitor *mon, const QDict *qdict);
void hmp_info_mice(Monitor *mon, const QDict *qdict);
void hmp_info_cpus(Monitor *mon, const QDict *qdict);
void hmp_info_spice(Monitor *mon, const QDict *qdict);
void hmp_info_balloon(Monitor *mon, const QDict *qdict);
void hmp_info_pic(Monitor *mon, const QDict *qdict);
void hmp_info_pci(Monitor *mon, const QDict *qdict);
void hmp_info_tpm(Monitor *mon, const QDict *qdict);
void hmp_info_iothreads(Monitor *mon, const QDict *qdict);
void hmp_quit(Monitor *mon, const QDict *qdict);
void hmp_stop(Monitor *mon, const QDict *qdict);
void hmp_sync_profile(Monitor *mon, const QDict *qdict);
void hmp_system_reset(Monitor *mon, const QDict *qdict);
void hmp_system_powerdown(Monitor *mon, const QDict *qdict);
void hmp_exit_preconfig(Monitor *mon, const QDict *qdict);
void hmp_cpu(Monitor *mon, const QDict *qdict);
void hmp_memsave(Monitor *mon, const QDict *qdict);
void hmp_pmemsave(Monitor *mon, const QDict *qdict);
void hmp_ringbuf_write(Monitor *mon, const QDict *qdict);
void hmp_ringbuf_read(Monitor *mon, const QDict *qdict);
void hmp_cont(Monitor *mon, const QDict *qdict);
void hmp_system_wakeup(Monitor *mon, const QDict *qdict);
void hmp_nmi(Monitor *mon, const QDict *qdict);
void hmp_info_network(Monitor *mon, const QDict *qdict);
void hmp_set_link(Monitor *mon, const QDict *qdict);
void hmp_balloon(Monitor *mon, const QDict *qdict);
void hmp_set_password(Monitor *mon, const QDict *qdict);
void hmp_expire_password(Monitor *mon, const QDict *qdict);
void hmp_change(Monitor *mon, const QDict *qdict);
void hmp_change_medium(Monitor *mon, const char *device, const char *target,
                       const char *arg, const char *read_only, bool force,
                       Error **errp);
void hmp_device_add(Monitor *mon, const QDict *qdict);
void hmp_device_del(Monitor *mon, const QDict *qdict);
void hmp_netdev_add(Monitor *mon, const QDict *qdict);
void hmp_netdev_del(Monitor *mon, const QDict *qdict);
void hmp_getfd(Monitor *mon, const QDict *qdict);
void hmp_closefd(Monitor *mon, const QDict *qdict);
void hmp_mouse_move(Monitor *mon, const QDict *qdict);
void hmp_mouse_button(Monitor *mon, const QDict *qdict);
void hmp_mouse_set(Monitor *mon, const QDict *qdict);
void hmp_sendkey(Monitor *mon, const QDict *qdict);
void coroutine_fn hmp_screendump(Monitor *mon, const QDict *qdict);
void hmp_chardev_add(Monitor *mon, const QDict *qdict);
void hmp_chardev_change(Monitor *mon, const QDict *qdict);
void hmp_chardev_remove(Monitor *mon, const QDict *qdict);
void hmp_chardev_send_break(Monitor *mon, const QDict *qdict);
void hmp_object_add(Monitor *mon, const QDict *qdict);
void hmp_object_del(Monitor *mon, const QDict *qdict);
void hmp_info_memdev(Monitor *mon, const QDict *qdict);
void hmp_info_memory_devices(Monitor *mon, const QDict *qdict);
void hmp_qom_list(Monitor *mon, const QDict *qdict);
void hmp_qom_get(Monitor *mon, const QDict *qdict);
void hmp_qom_set(Monitor *mon, const QDict *qdict);
void hmp_info_qom_tree(Monitor *mon, const QDict *dict);
void hmp_virtio_query(Monitor *mon, const QDict *qdict);
void hmp_virtio_status(Monitor *mon, const QDict *qdict);
void hmp_virtio_queue_status(Monitor *mon, const QDict *qdict);
void hmp_vhost_queue_status(Monitor *mon, const QDict *qdict);
void hmp_virtio_queue_element(Monitor *mon, const QDict *qdict);
void object_add_completion(ReadLineState *rs, int nb_args, const char *str);
void object_del_completion(ReadLineState *rs, int nb_args, const char *str);
void device_add_completion(ReadLineState *rs, int nb_args, const char *str);
void device_del_completion(ReadLineState *rs, int nb_args, const char *str);
void sendkey_completion(ReadLineState *rs, int nb_args, const char *str);
void chardev_remove_completion(ReadLineState *rs, int nb_args, const char *str);
void chardev_add_completion(ReadLineState *rs, int nb_args, const char *str);
void set_link_completion(ReadLineState *rs, int nb_args, const char *str);
void netdev_add_completion(ReadLineState *rs, int nb_args, const char *str);
void netdev_del_completion(ReadLineState *rs, int nb_args, const char *str);
void ringbuf_write_completion(ReadLineState *rs, int nb_args, const char *str);
void info_trace_events_completion(ReadLineState *rs, int nb_args, const char *str);
void trace_event_completion(ReadLineState *rs, int nb_args, const char *str);
void hmp_hotpluggable_cpus(Monitor *mon, const QDict *qdict);
void hmp_info_vm_generation_id(Monitor *mon, const QDict *qdict);
void hmp_info_memory_size_summary(Monitor *mon, const QDict *qdict);
void hmp_human_readable_text_helper(Monitor *mon,
                                    HumanReadableText *(*qmp_handler)(Error **));
void hmp_one_insn_per_tb(Monitor *mon, const QDict *qdict);
void hmp_pcie_aer_inject_error(Monitor *mon, const QDict *qdict);
void hmp_trace_event(Monitor *mon, const QDict *qdict);
void hmp_trace_file(Monitor *mon, const QDict *qdict);
void hmp_info_trace_events(Monitor *mon, const QDict *qdict);
void hmp_help(Monitor *mon, const QDict *qdict);
void hmp_info_help(Monitor *mon, const QDict *qdict);
void hmp_info_sync_profile(Monitor *mon, const QDict *qdict);
void hmp_info_history(Monitor *mon, const QDict *qdict);
void hmp_logfile(Monitor *mon, const QDict *qdict);
void hmp_log(Monitor *mon, const QDict *qdict);
void hmp_print(Monitor *mon, const QDict *qdict);
void hmp_sum(Monitor *mon, const QDict *qdict);
void hmp_ioport_read(Monitor *mon, const QDict *qdict);
void hmp_ioport_write(Monitor *mon, const QDict *qdict);
void hmp_boot_set(Monitor *mon, const QDict *qdict);
void hmp_info_mtree(Monitor *mon, const QDict *qdict);
void hmp_dumpdtb(Monitor *mon, const QDict *qdict);

#endif
