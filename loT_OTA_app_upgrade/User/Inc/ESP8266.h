#ifndef __ESP8266_H
#define __ESP8266_H
#include <stdint.h>
#define htons(x) ((((uint16_t)(x) & 0xFF00) >> 8) | (((uint16_t)(x) & 0x00FF) << 8))
#define AF_INET         2   /* 代表 IPv4 网络 */
#define SOCK_STREAM     1   /* 代表 TCP 协议 */
#define SOCK_DGRAM      2   /* 代表 UDP 协议 */
struct in_addr
{
    uint32_t s_addr;
};
struct sockaddr 
{
    uint8_t         sa_len;
    uint8_t         sa_family;
    char            sa_data[14];
};
struct sockaddr_in 
{
    uint8_t         sin_len;
    uint8_t         sin_family; /* AF_INET */
    uint16_t        sin_port;   /* 端口号  */
    struct in_addr  sin_addr;   /* IP 地址 */
    char            sin_zero[8];/* 填充对齐用的，不用管 */
};

int esp8266_socket(int domain, int type, int protocol);
int esp8266_connect(int sockfd, const struct sockaddr *addr, uint32_t addrlen);
int esp8266_send(int sockfd, const void *buf, size_t len, int flags);
int esp8266_recv(int sockfd, void *buf, size_t len, int flags);
int esp8266_close(int sockfd);
uint8_t esp8266_state(int sockfd);
int esp8266_init(void* at_device,char* wifi_name,char* wifi_password);

#endif