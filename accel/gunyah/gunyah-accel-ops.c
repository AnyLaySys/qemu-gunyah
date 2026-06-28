#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qemu/thread.h"
#include "hw/boards.h"
#include "hw/core/cpu.h"
#include "system/accel-ops.h"
#include "system/cpus.h"
#include "system/gunyah.h"
#include "system/gunyah_int.h"
#include "qapi/visitor.h"
#include "qapi/error.h"
bool gunyah_allowed;
#define GUNYAH_DEFAULT_SWIOTLB_SIZE (64ULL * 1024 * 1024)
static int gunyah_init(MachineState*ms){GUNYAHState*s=GUNYAH_STATE(current_accel());int ret;ret=gunyah_create_vm();if(ret){return ret;}if(s->protected_vm&&!s->swiotlb_size){gunyah_set_swiotlb_size(GUNYAH_DEFAULT_SWIOTLB_SIZE);}return 0;}static bool gunyah_get_protected(Object*obj,Error**errp){GUNYAHState*s=GUNYAH_STATE(obj);return s->protected_vm;}static void gunyah_set_protected(Object*obj,bool value,Error**errp){GUNYAHState*s=GUNYAH_STATE(obj);s->protected_vm=value;}static void gunyah_accel_instance_init(Object*obj){GUNYAHState*s=GUNYAH_STATE(obj);s->fd=-1;s->vmfd=-1;s->protected_vm=false;object_property_add_bool(obj,"protected",gunyah_get_protected,gunyah_set_protected);}static void gunyah_setup_post(MachineState*ms,AccelState*accel){gunyah_start_vm();}static void gunyah_accel_class_init(ObjectClass*oc,void*data){AccelClass*ac=ACCEL_CLASS(oc);ac->name="GUNYAH";ac->init_machine=gunyah_init;ac->allowed=&gunyah_allowed;ac->setup_post=gunyah_setup_post;}static const TypeInfo gunyah_accel_type={.name=TYPE_GUNYAH_ACCEL,.parent=TYPE_ACCEL,.instance_init=gunyah_accel_instance_init,.class_init=gunyah_accel_class_init,.instance_size=sizeof(GUNYAHState),};static void gunyah_type_init(void){type_register_static(&gunyah_accel_type);}type_init(gunyah_type_init);static void gunyah_start_vcpu_thread(CPUState*cpu){char thread_name[VCPU_THREAD_NAME_SIZE];snprintf(thread_name,VCPU_THREAD_NAME_SIZE,"CPU %d/Gunyah",cpu->cpu_index);qemu_thread_create(cpu->thread,thread_name,gunyah_cpu_thread_fn,cpu,QEMU_THREAD_JOINABLE);}static void gunyah_kick_vcpu_thread(CPUState*cpu){cpus_kick_thread(cpu);}static bool gunyah_vcpu_thread_is_idle(CPUState*cpu){return false;}static void gunyah_accel_ops_class_init(ObjectClass*oc,void*data){AccelOpsClass*ops=ACCEL_OPS_CLASS(oc);ops->create_vcpu_thread=gunyah_start_vcpu_thread;ops->kick_vcpu_thread=gunyah_kick_vcpu_thread;ops->cpu_thread_is_idle=gunyah_vcpu_thread_is_idle;ops->synchronize_post_reset=gunyah_cpu_synchronize_post_reset;};static const TypeInfo gunyah_accel_ops_type={.name=ACCEL_OPS_NAME("gunyah"),.parent=TYPE_ACCEL_OPS,.class_init=gunyah_accel_ops_class_init,.abstract=true,};static void gunyah_accel_ops_register_types(void){type_register_static(&gunyah_accel_ops_type);}type_init(gunyah_accel_ops_register_types);
