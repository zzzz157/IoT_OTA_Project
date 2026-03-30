#include <stdio.h>
#include <string.h>
#include "main.h"
#include "ESP8266.h"
#include "Socket.h"
#include "FreeRTOS.h"
#include "Task.h"

static Net_Device *current_net_dev = NULL;

void socket_register_device(Net_Device *dev)
{
    if (dev != NULL) 
	{
        current_net_dev = dev;
    }
}

int init(void* at_device,char* wifi_name,char* wifi_password)
{
	return current_net_dev->init(at_device,wifi_name,wifi_password);
}
int socket(int domain, int type, int protocol)
{
    if (current_net_dev != NULL && current_net_dev->socket != NULL) {
        return current_net_dev->socket(domain, type, protocol);
    }
    return -1;
}

int connect(int sockfd, const struct sockaddr *addr, uint32_t addrlen)
{
    if (current_net_dev != NULL && current_net_dev->connect != NULL) {
        return current_net_dev->connect(sockfd, addr, addrlen);
    }
    return -1;
}

int send(int sockfd, const void *buf, size_t len, int flags)
{
    if (current_net_dev != NULL && current_net_dev->send != NULL) {
        return current_net_dev->send(sockfd, buf, len, flags);
    }
    return -1;
}

int recv(int sockfd, void *buf, size_t len, int flags)
{
    if (current_net_dev != NULL && current_net_dev->recv != NULL) {
        return current_net_dev->recv(sockfd, buf, len, flags);
    }
    return -1;
}

int close(int sockfd)
{
    if (current_net_dev != NULL && current_net_dev->close != NULL) {
        return current_net_dev->close(sockfd);
    }
    return -1;
}
uint8_t socket_status(int sockfd)
{
	return current_net_dev->socket_status(sockfd);
}

Net_Device esp8266_net_device = {
    .name    		= "ESP8266_WIFI",
	.init 	 		= esp8266_init,
    .socket  		= esp8266_socket,
    .connect 		= esp8266_connect,
    .send    		= esp8266_send,
    .recv    		= esp8266_recv,
    .close   		= esp8266_close,
	.socket_status	= esp8266_state,
};