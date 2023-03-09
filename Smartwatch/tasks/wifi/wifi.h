#ifndef WIFI_H
#define WIFI_H

/* Standard header files. */
#include <time.h>

/* WiFi Credentials */
#include "wifi_creds.h"

/* Maximum number of connection retries to the Wi-Fi network. */
#define MAX_WIFI_CONN_RETRIES             (3u)

/* Wi-Fi re-connection time interval in milliseconds */
#define WIFI_CONN_RETRY_INTERVAL_MSEC     (1000)

#define MAKE_IPV4_ADDRESS(a, b, c, d)     ((((uint32_t) d) << 24) | \
                                          (((uint32_t) c) << 16) | \
                                          (((uint32_t) b) << 8) |\
                                          ((uint32_t) a))

/* Change the server IP address to match the NTP server address
*   pool.ntp.org -> 162.159.200.123:123
 */
#define UDP_SERVER_IP_ADDRESS             MAKE_IPV4_ADDRESS(162, 159, 200, 123)
#define UDP_SERVER_PORT                   (123)

/* External variables */
extern struct tm *ntp_time;

/* Function Declarations */
cy_rslt_t fetch_NTP_time(void);

#endif
