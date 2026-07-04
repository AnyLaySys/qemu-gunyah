#include "qemu/osdep.h"
#include "qemu/accel.h"
#include "hw/boards.h"
#include "system/accel-ops.h"
#include "system/cpus.h"
#include "qemu/error-report.h"
#include "accel-system.h"
int accel_init_machine(AccelState*accel,MachineState*ms){AccelClass*acc=ACCEL_GET_CLASS(accel);int ret;ms->accelerator=accel;*(acc->allowed)=true;ret=acc->init_machine(ms);if(ret<0){ms->accelerator=NULL;*(acc->allowed)=false;object_unref(OBJECT(accel));}else{object_set_accelerator_compat_props(acc->compat_props);}return ret;}AccelState*current_accel(void){return current_machine->accelerator;}void accel_setup_post(MachineState*ms){AccelState*accel=ms->accelerator;AccelClass*acc=ACCEL_GET_CLASS(accel);if(acc->setup_post){acc->setup_post(ms,accel);}}void accel_system_init_ops_interfaces(AccelClass*ac){const char*ac_name;char*ops_name;ObjectClass*oc;AccelOpsClass*ops;ac_name=object_class_get_name(OBJECT_CLASS(ac));g_assert(ac_name!=NULL);ops_name=g_strdup_printf("%s"ACCEL_OPS_SUFFIX,ac_name);oc=module_object_class_by_name(ops_name);if(!oc){error_report("fatal: could not load module for type '%s'",ops_name);exit(1);}g_free(ops_name);ops=ACCEL_OPS_CLASS(oc);if(ops->ops_init){ops->ops_init(ops);}cpus_register_accel(ops);}static const TypeInfo accel_ops_type_info={.name=TYPE_ACCEL_OPS,.parent=TYPE_OBJECT,.abstract=true,.class_size=sizeof(AccelOpsClass),};static void accel_system_register_types(void){type_register_static(&accel_ops_type_info);}type_init(accel_system_register_types);
