
#ifndef TPM_BACKEND_H
#define TPM_BACKEND_H

#include "qom/object.h"
#include "qemu/option.h"
#include "system/tpm.h"
#include "qapi/error.h"

#ifdef CONFIG_TPM

#define TYPE_TPM_BACKEND "tpm-backend"
OBJECT_DECLARE_TYPE(TPMBackend, TPMBackendClass,
                    TPM_BACKEND)


typedef struct TPMBackendCmd {
    uint8_t locty;
    const uint8_t *in;
    uint32_t in_len;
    uint8_t *out;
    uint32_t out_len;
    bool selftest_done;
} TPMBackendCmd;

struct TPMBackend {
    Object parent;

    TPMIf *tpmif;
    bool opened;
    bool had_startup_error;
    TPMBackendCmd *cmd;

    char *id;

    QLIST_ENTRY(TPMBackend) list;
};

struct TPMBackendClass {
    ObjectClass parent_class;

    enum TpmType type;
    const QemuOptDesc *opts;
    const char *desc;

    TPMBackend *(*create)(QemuOpts *opts);

    int (*startup_tpm)(TPMBackend *t, size_t buffersize);

    void (*reset)(TPMBackend *t);

    void (*cancel_cmd)(TPMBackend *t);

    bool (*get_tpm_established_flag)(TPMBackend *t);

    int (*reset_tpm_established_flag)(TPMBackend *t, uint8_t locty);

    TPMVersion (*get_tpm_version)(TPMBackend *t);

    size_t (*get_buffer_size)(TPMBackend *t);

    TpmTypeOptions *(*get_tpm_options)(TPMBackend *t);

    void (*handle_request)(TPMBackend *s, TPMBackendCmd *cmd, Error **errp);
};

enum TpmType tpm_backend_get_type(TPMBackend *s);

int tpm_backend_init(TPMBackend *s, TPMIf *tpmif, Error **errp);

int tpm_backend_startup_tpm(TPMBackend *s, size_t buffersize);

bool tpm_backend_had_startup_error(TPMBackend *s);

void tpm_backend_deliver_request(TPMBackend *s, TPMBackendCmd *cmd);

void tpm_backend_reset(TPMBackend *s);

void tpm_backend_cancel_cmd(TPMBackend *s);

bool tpm_backend_get_tpm_established_flag(TPMBackend *s);

int tpm_backend_reset_tpm_established_flag(TPMBackend *s, uint8_t locty);

TPMVersion tpm_backend_get_tpm_version(TPMBackend *s);

size_t tpm_backend_get_buffer_size(TPMBackend *s);

void tpm_backend_finish_sync(TPMBackend *s);

TPMInfo *tpm_backend_query_tpm(TPMBackend *s);

TPMBackend *qemu_find_tpm_be(const char *id);

#endif /* CONFIG_TPM */

#endif /* TPM_BACKEND_H */
