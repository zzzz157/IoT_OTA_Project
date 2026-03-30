#ifndef __SOCKET_H
#define __SOCKET_H
#include "main.h"
#include "ESP8266.h"

#define IP4_ADDR(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

typedef struct _Net_Device Net_Device;
typedef struct _Net_Device {
    char *name;
	
    int (*socket)(int domain, int type, int protocol);
    int (*connect)(int sockfd, const struct sockaddr *addr, uint32_t addrlen);
    int (*send)(int sockfd, const void *buf, size_t len, int flags);
    int (*recv)(int sockfd, void *buf, size_t len, int flags);
    int (*close)(int sockfd);
	
	int (*init)(void* at_device,char* wifi_name,char* wifi_password);
	uint8_t (*socket_status)(int sockfd);
}Net_Device;

void socket_register_device(Net_Device *dev);

int socket(int domain, int type, int protocol);
int connect(int sockfd, const struct sockaddr *addr, uint32_t addrlen);
int send(int sockfd, const void *buf, size_t len, int flags);
int recv(int sockfd, void *buf, size_t len, int flags);
int close(int sockfd);

int init(void* at_device,char* wifi_name,char* wifi_password);
uint8_t socket_status(int sockfd);

extern Net_Device esp8266_net_device;

#endif