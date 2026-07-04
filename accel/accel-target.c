#include "qemu/osdep.h"
#include "qemu/accel.h"
#include "cpu.h"
#include "accel/accel-cpu-target.h"
#ifndef CONFIG_USER_ONLY
#include "accel-system.h"
#endif
static const TypeInfo accel_type={.name=TYPE_ACCEL,.parent=TYPE_OBJECT,.class_size=sizeof(AccelClass),.instance_size=sizeof(AccelState),.abstract=true,};AccelClass*accel_find(const char*opt_name){char*class_name=g_strdup_printf(ACCEL_CLASS_NAME("%s"),opt_name);AccelClass*ac=ACCEL_CLASS(module_object_class_by_name(class_name));g_free(class_name);return ac;}const char*current_accel_name(void){AccelClass*ac=ACCEL_GET_CLASS(current_accel());return ac->name;}static void accel_init_cpu_int_aux(ObjectClass*klass,void*opaque){CPUClass*cc=CPU_CLASS(klass);AccelCPUClass*accel_cpu=opaque;cc->accel_cpu=accel_cpu;if(accel_cpu->cpu_class_init){accel_cpu->cpu_class_init(cc);}if(cc->init_accel_cpu){cc->init_accel_cpu(accel_cpu,cc);}}static void accel_init_cpu_interfaces(AccelClass*ac){const char*ac_name;char*acc_name;ObjectClass*acc;ac_name=object_class_get_name(OBJECT_CLASS(ac));g_assert(ac_name!=NULL);acc_name=g_strdup_printf("%s-%s",ac_name,CPU_RESOLVING_TYPE);acc=object_class_by_name(acc_name);g_free(acc_name);if(acc){object_class_foreach(accel_init_cpu_int_aux,CPU_RESOLVING_TYPE,false,acc);}}void accel_init_interfaces(AccelClass*ac){
#ifndef CONFIG_USER_ONLY
accel_system_init_ops_interfaces(ac);
#endif
accel_init_cpu_interfaces(ac);}void accel_cpu_instance_init(CPUState*cpu){if(cpu->cc->accel_cpu&&cpu->cc->accel_cpu->cpu_instance_init){cpu->cc->accel_cpu->cpu_instance_init(cpu);}}bool accel_cpu_common_realize(CPUState*cpu,Error**errp){AccelState*accel=current_accel();AccelClass*acc=ACCEL_GET_CLASS(accel);if(cpu->cc->accel_cpu&&cpu->cc->accel_cpu->cpu_target_realize&&!cpu->cc->accel_cpu->cpu_target_realize(cpu,errp)){return false;}if(acc->cpu_common_realize&&!acc->cpu_common_realize(cpu,errp)){return false;}return true;}void accel_cpu_common_unrealize(CPUState*cpu){AccelState*accel=current_accel();AccelClass*acc=ACCEL_GET_CLASS(accel);if(acc->cpu_common_unrealize){acc->cpu_common_unrealize(cpu);}}static const TypeInfo accel_cpu_type={.name=TYPE_ACCEL_CPU,.parent=TYPE_OBJECT,.abstract=true,.class_size=sizeof(AccelCPUClass),};static void register_accel_types(void){type_register_static(&accel_type);type_register_static(&accel_cpu_type);}type_init(register_accel_types);
