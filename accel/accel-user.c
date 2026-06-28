#include "qemu/osdep.h"
#include "qemu/accel.h"
AccelState*current_accel(void){static AccelState*accel;if(!accel){AccelClass*ac=accel_find("tcg");g_assert(ac!=NULL);accel=ACCEL(object_new_with_class(OBJECT_CLASS(ac)));}return accel;}
