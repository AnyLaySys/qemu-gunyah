
#ifndef SPDM_REQUESTER_H
#define SPDM_REQUESTER_H

int spdm_socket_connect(uint16_t port, Error **errp);

uint32_t spdm_socket_rsp(const int socket, uint32_t transport_type,
                         void *req, uint32_t req_len,
                         void *rsp, uint32_t rsp_len);

void spdm_socket_close(const int socket, uint32_t transport_type);

#define SPDM_SOCKET_COMMAND_NORMAL                0x0001
#define SPDM_SOCKET_COMMAND_OOB_ENCAP_KEY_UPDATE  0x8001
#define SPDM_SOCKET_COMMAND_CONTINUE              0xFFFD
#define SPDM_SOCKET_COMMAND_SHUTDOWN              0xFFFE
#define SPDM_SOCKET_COMMAND_UNKOWN                0xFFFF
#define SPDM_SOCKET_COMMAND_TEST                  0xDEAD

#define SPDM_SOCKET_TRANSPORT_TYPE_MCTP           0x01
#define SPDM_SOCKET_TRANSPORT_TYPE_PCI_DOE        0x02

#define SPDM_SOCKET_MAX_MESSAGE_BUFFER_SIZE       0x1200

#endif
