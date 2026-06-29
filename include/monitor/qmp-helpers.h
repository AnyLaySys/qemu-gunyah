
#ifndef MONITOR_QMP_HELPERS_H

bool qmp_add_client_char(int fd, bool has_skipauth, bool skipauth,
                         bool has_tls, bool tls, const char *protocol,
                         Error **errp);

#endif
