#ifndef _MEASURED_CERTS_H
#define _MEASURED_CERTS_H

#define AMP_PKI_SSL_PORT 7655
#define AMP_PKI_QUERY_INTERVAL (60 * 30)

int get_certificate(amp_ssl_opt_t *sslopts, char *ampname, char *collector,
        int timeout);

#endif
