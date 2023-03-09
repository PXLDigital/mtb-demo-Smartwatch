/* BSP */
#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

/* FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"

/* Secure Sockets */
#include "cy_secure_sockets.h"

/* Connection Manager */
#include "cy_wcm.h"
#include "cy_wcm_error.h"

/* Standard */
#include <inttypes.h>

/* Function Header */
#include "wifi.h"

/* Global Variables */
char wifi_lanIP[30];

/* External Variables */
struct tm *ntp_time;

/* NTP Packet Config. */
#define UNIX_OFFSET 2208988800L
#define ENDIAN_SWAP32(data)  	((data >> 24) | /* right shift 3 bytes */ \
				                ((data & 0x00ff0000) >> 8) | /* right shift 1 byte */ \
			                    ((data & 0x0000ff00) << 8) | /* left shift 1 byte */ \
				                ((data & 0x000000ff) << 24)) /* left shift 3 bytes */

/* Function Declarations */
static cy_rslt_t connect_to_wifi_ap(void);
static cy_rslt_t create_udp_client_socket(void);

/* NTP client socket handle */
cy_socket_t client_handle;
cy_socket_sockaddr_t peer_addr;

cy_rslt_t fetch_NTP_time(void)
{
    cy_rslt_t result;
    uint32_t bytes_sent = 0;

    /* IP address and NTP port number of the NTP server */
    cy_socket_sockaddr_t udp_server_addr = {
        .ip_address.ip.v4 = UDP_SERVER_IP_ADDRESS,
        .ip_address.version = CY_SOCKET_IP_VER_V4,
        .port = UDP_SERVER_PORT
    };

    /* Connect to Wi-Fi AP */
    result = connect_to_wifi_ap();
    if(result != CY_RSLT_SUCCESS)
    {
        printf("\n Failed to connect to Wi-FI AP.\n");
        return result;
    }

    /* Secure Sockets initialized */
    result = cy_socket_init();
    if (result != CY_RSLT_SUCCESS)
    {
        printf("Secure Sockets initialization failed!\n");
        return result;
    }
    printf("Secure Sockets initialized\n");

    result = create_udp_client_socket();
    if (result != CY_RSLT_SUCCESS)
    {
        printf("UDP Client Socket creation failed!\n");
        return result;
    }

    /* NTP Packet */
    struct ntpPacket {
    	uint8_t li_vn_mode;
    	uint8_t stratum;
    	uint8_t poll;
    	uint8_t precision;
    	uint32_t root_delay;
    	uint32_t root_dispersion;
    	uint8_t referenceID[4];
    	uint32_t ref_ts_sec;
    	uint32_t ref_ts_frac;
    	uint32_t origin_ts_sec;
    	uint32_t origin_ts_frac;
    	uint32_t recv_ts_sec;
    	uint32_t recv_ts_frac;
    	uint32_t trans_ts_sec;
    	uint32_t trans_ts_frac;
    } ntp_packet;

    /* initialize the packet */
    memset(&ntp_packet, 0, sizeof(ntp_packet));
    ntp_packet.li_vn_mode = 0x1b;

     /* First send data to Server and wait to receive command */
    result = cy_socket_sendto(client_handle, &ntp_packet, sizeof(ntp_packet), CY_SOCKET_FLAGS_NONE,
                                &udp_server_addr, sizeof(cy_socket_sockaddr_t), &bytes_sent);
    if(result == CY_RSLT_SUCCESS)
    {
        printf("Data sent to server\n");
    }
    else
    {
    	printf("Failed to send data to server. Error : %"PRIu32"\n", result);
        return result;
    }

    /* Variable to store the number of bytes received. */
    uint32_t bytes_received = 0;

        for (int i = 0; i < 100; i++)
        {
            /* Receive incoming message from NTP server. */
            result = cy_socket_recvfrom(client_handle, &ntp_packet, sizeof(ntp_packet),
                                    CY_SOCKET_FLAGS_RECVFROM_NONE, NULL, 0, &bytes_received);
            if (result != CY_RSLT_SUCCESS)
            {
                printf("\nError receiving NTP packet!");
                return result;
            }
            else if (bytes_received > 0)
            {
                break;
            }
        }

    /* Correct for right endianess */
    ntp_packet.recv_ts_sec = ENDIAN_SWAP32(ntp_packet.recv_ts_sec);

    time_t total_secs;
    unsigned int recv_secs;

    /* print date with receive timestamp */
	recv_secs = ntp_packet.recv_ts_sec - UNIX_OFFSET; /* convert to unix time */
	total_secs = recv_secs;
	printf("Unix time: %u\n", (unsigned int)total_secs);

	ntp_time = localtime(&total_secs);
    ntp_time->tm_hour += 1;

	//printf("%02d/%02d/%d %02d:%02d:%02d\n", ntp_time->tm_mday, ntp_time->tm_mon+1, ntp_time->tm_year+1900, ntp_time->tm_hour+1, ntp_time->tm_min, ntp_time->tm_sec);

    return result;
}

cy_rslt_t connect_to_wifi_ap(void)
{
    cy_rslt_t result;

    /* Variables used by Wi-Fi connection manager.*/
    cy_wcm_connect_params_t wifi_conn_param;

    cy_wcm_config_t wifi_config = { .interface = CY_WCM_INTERFACE_TYPE_STA };

    cy_wcm_ip_address_t ip_address;

     /* Initialize Wi-Fi connection manager. */
    result = cy_wcm_init(&wifi_config);

    if (result != CY_RSLT_SUCCESS)
    {
        printf("Wi-Fi Connection Manager initialization failed!\n");
        return result;
    }
    printf("Wi-Fi Connection Manager initialized.\r\n");

     /* Set the Wi-Fi SSID, password and security type. */
    memset(&wifi_conn_param, 0, sizeof(cy_wcm_connect_params_t));
    memcpy(wifi_conn_param.ap_credentials.SSID, WIFI_SSID, sizeof(WIFI_SSID));
    memcpy(wifi_conn_param.ap_credentials.password, WIFI_PASSWORD, sizeof(WIFI_PASSWORD));
    wifi_conn_param.ap_credentials.security = WIFI_SECURITY_TYPE;

    /* Join the Wi-Fi AP. */
    for(uint32_t conn_retries = 0; conn_retries < MAX_WIFI_CONN_RETRIES; conn_retries++ )
    {
        result = cy_wcm_connect_ap(&wifi_conn_param, &ip_address);
        if(result == CY_RSLT_SUCCESS)
        {
            printf("Successfully connected to Wi-Fi network '%s'.\n",
                                wifi_conn_param.ap_credentials.SSID);
            sprintf(wifi_lanIP, "%d.%d.%d.%d",
                        (uint8)ip_address.ip.v4, (uint8)(ip_address.ip.v4 >> 8),
                        (uint8)(ip_address.ip.v4 >> 16), (uint8)(ip_address.ip.v4 >> 24));
            printf("IP Address Assigned: %s\n", wifi_lanIP);
            return result;
        }

        printf("Connection to Wi-Fi network failed with error code %d."
               "Retrying in %d ms...\n", (int)result, WIFI_CONN_RETRY_INTERVAL_MSEC);

        vTaskDelay(pdMS_TO_TICKS(WIFI_CONN_RETRY_INTERVAL_MSEC));
    }

    /* Stop retrying after maximum retry attempts. */
    printf("Exceeded maximum Wi-Fi connection attempts\n");

    return !CY_RSLT_SUCCESS;
}

cy_rslt_t create_udp_client_socket(void)
{
    cy_rslt_t result;

    /* Create a UDP socket. */
    result = cy_socket_create(CY_SOCKET_DOMAIN_AF_INET, CY_SOCKET_TYPE_DGRAM, CY_SOCKET_IPPROTO_UDP, &client_handle);
    if(result != CY_RSLT_SUCCESS)
    {
        return result;
    }

    return result;
}

