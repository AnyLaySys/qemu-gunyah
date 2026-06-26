
#include "qemu/osdep.h"
#include "hw/acpi/acpi.h"
#include "hw/acpi/aml-build.h"

void aml_append(Aml *parent_ctx, Aml *child)
{
}

Aml *aml_return(Aml *val)
{
    return NULL;
}

Aml *aml_method(const char *name, int arg_count, AmlSerializeFlag sflag)
{
    return NULL;
}

Aml *aml_resource_template(void)
{
    return NULL;
}

Aml *aml_device(const char *name_format, ...)
{
    return NULL;
}

Aml *aml_eisaid(const char *str)
{
    return NULL;
}

Aml *aml_name_decl(const char *name, Aml *val)
{
    return NULL;
}

Aml *aml_io(AmlIODecode dec, uint16_t min_base, uint16_t max_base,
            uint8_t aln, uint8_t len)
{
    return NULL;
}

Aml *aml_irq_no_flags(uint8_t irq)
{
    return NULL;
}

Aml *aml_interrupt(AmlConsumerAndProducer con_and_pro,
                   AmlLevelAndEdge level_and_edge,
                   AmlActiveHighAndLow high_and_low, AmlShared shared,
                   uint32_t *irq_list, uint8_t irq_count)
{
    return NULL;
}

Aml *aml_memory32_fixed(uint32_t addr, uint32_t size,
                        AmlReadAndWrite read_and_write)
{
    return NULL;
}

Aml *aml_int(const uint64_t val)
{
    return NULL;
}

Aml *aml_package(uint8_t num_elements)
{
    return NULL;
}

Aml *aml_dma(AmlDmaType typ, AmlDmaBusMaster bm, AmlTransferSize sz,
             uint8_t channel)
{
    return NULL;
}

Aml *aml_buffer(int buffer_size, uint8_t *byte_list)
{
    return NULL;
}
