#ifndef __WIFI_H__
#define __WIFI_H__

//!<<<< definitions
#define MODULE_NAME          "wifi.c"
#define DIAG_NAME            "dt_wifi"
#define WIFI_DEFAULT_INTERFACE    "wlan0"
#define WIFI_DEFAULT_ITERATIONS   "5"
#define WIFI_DEFAULT_DELAY_THRESH  "1000"
#define WIFI_DEFAULT_JITTER_THRESH "1000"
#define WIFI_DEFAULT_PINGBACKADDR "localhost"
#define WIFI_DEFAULT_SSID         "B-GUEST"
#define WIFI_PASSPHRASE_DEFAULT   ""
#define WIFI_KILLALL_STR1    "killall wpa_supplicant > /dev/null 2>&1"
#define WIFI_KILLALL_STR2    "killall dhcpcd > /dev/null 2>&1"
#define WIFI_OPENSETUP_STR1  "cp /opt/Bose/etc/wpa_supplicant.conf_open /opt/Bose/etc/wpa_supplicant.conf"
#define WIFI_OPENSETUP_STR2  "sed 's/any/%s/' /opt/Bose/etc/wpa_supplicant.conf > /dev/null 2>&1"
#define WIFI_OPENSETUP_STR3  "cat /opt/Bose/etc/wpa_supplicant.conf"
#define WIFI_SETUP_STR1      "wpa_passphrase %s %s > /opt/Bose/etc/wpa_supplicant.conf"
#define WIFI_SETUP_STR2      "wpa_supplicant -qq -f %s/wpa_supplicant.log -i wlan0 -c /opt/Bose/etc/wpa_supplicant.conf &"
#define WIFI_SETUP_STR3         "dhcpcd %s > /dev/null 2>&1"
#define WIFI_SETUP_STR3_VERBOSE "dhcpcd %s"
#define WIFI_UNLINK_STR1     "unlink %s/%s > /dev/null 2>&1"
#define WIFI_PING_STR1       "ping -U -c%d -s%d %s > /dev/null 2>&1"
#define WIFI_PING_STR2       "ping -U -c%d -s%d %s > %s/%s"
#define WIFI_LINK_STATS      "iw %s link > %s/%s"
#define PING_OUTPUT_PATTERN2 "transmitted"
#define PING_OUTPUT_PATTERN1 "min/avg/max/mdev"
#define PING_OUTPUT_PATTERN3 "bytes from "
//#define PING_OUTPUT_PATTERN3 "bytes from server" //!<<<< could be "bytes from localhost" 
#define WIFI_PING_FILE       "wifi_ping.txt"
#define WIFI_IWLINK_FILE     "wifi_iwlink.txt"
#define WIFI_SCAN            "iw %s scan | grep SSID"
#define WIFI_DEFAULT_COUNT   "2"
#define WIFI_DEFAULT_LENGTH  "64"
#define WIFI_PACKET_SIZE_MIN (64)
#define WIFI_PACKET_SIZE_MAX (1500)

//!<<<< auth test error codes
typedef enum {
  NOK_PING_DROPS=2,
  NOK_OPERSTATUS=3,
  NOK_CARRIER=4,
  NOK_TXPACKETCOUNT=5,
  NOK_RXPACKETCOUNT=6,
  NOK_RXBYTECOUNT=7,
  NOK_DELAYTHRESH=8,
  NOK_JITTERTHRESH=9,
} WIFI_ERR;

//!<<<< thread parameters
typedef struct {
  unsigned int secs;
  char cmd [GP_STR_LEN];
} PING_THREAD_ARGS, *pPING_THREAD_ARGS;
typedef struct {
  unsigned int secs;
  char cmd [GP_STR_LEN];
} OUT_THREAD_ARGS, *pOUT_THREAD_ARGS;
typedef struct STATS {
  char ssid [GP_STR_LEN];
  float freq;
  float signal;
  char operstatus [24];
  int carrier;
  int ping_samples;
  float max_time;
  float min_time;
  float sum_time;
  float avg_time;
  float max_jitter;
  float min_jitter;
  float avg_jitter;
  float sum_jitter;
  int perf_samples;
  float transfer;
  float max_bandwidth;
  float min_bandwidth;
  float sum_bandwidth;
  float avg_bandwidth;
  int tx;
  int rx;
  int loss;
  int time;
  int interval;
} STATS, *pSTATS;
//!<<<< prototypes
int main (int argc, char *argv[]);
void wifi_usage (void);
void wifi_usage_short (void);
int wifi_getIpAddr (char *interface, char *ip_address_str);
void *ping_thread (void *args);
void *out_thread (int first_time, int interval, jWriteControl *p_jwc, char *p_buffer, int buflen, int len, int rx_bytes, int tx_bytes);
void iperf_parse_procnetdev_output (long long int *init_rx_bytes, long long int *init_rx_packets, long long int *init_tx_bytes, long long int *init_tx_packets);
void iperf_parse_ping_output (float *max, float *min, float *avg, float *jitter, int *tx, int *rx, int *loss, int *time, int *rx_bytes);
void iperf_parse_procnetsnmp_output (long long int *in_msgs, long long int *in_errors, long long int *in_dst_unreach, long long int *out_msgs, long long int *out_errors, long long int *out_dst_unreach);
void iperf_parse_iwlink_output (char *interface, char *ssid, float *freq, float *signal);
void wifi_scan (void);
#endif
