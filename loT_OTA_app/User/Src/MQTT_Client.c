#include "main.h"
#include "Socket.h"
#include "MQTT_Client.h"
#include <string.h>
#include <stdio.h>
static MQTT_List* mqtt_list=NULL;

static MQTT_List mqtt_null_node[MQTT_MAX_LIST_NODE];
static uint8_t NodeCount = 0;
/*   */
static int MQTT_EncodeLength(uint8_t *buf, uint32_t len)
{
    int count = 0;
    do{
        uint8_t encodedByte = len % 128;
        len = len / 128;
        if (len > 0)
		{
            encodedByte |= 128;
        }
        buf[count++] = encodedByte;
    } while (len > 0);
    return count;
}
/* 解析不定长度 */
static int MQTT_DecodeLength(const uint8_t *buf, uint32_t *value)
{
    uint32_t multiplier = 1;
    uint32_t len = 0;
    int count = 0;
    uint8_t encodedByte;
    do {
        encodedByte = buf[count++];
        len += (encodedByte & 127) * multiplier;
        multiplier *= 128;
    } while ((encodedByte & 128) != 0&& count < 4);
    
    *value = len;
    return count;
}

int MQTT_Connect(int fd, const char* client_id,const char* username,const char* password)
{
    static uint8_t buf[128];
    int index = 0;
	/* 1. 固定头部 */
    uint16_t client_id_len = strlen(client_id);
	uint16_t username_len = strlen(username);
	uint16_t password_len = strlen(password);
	/* 0x10 表示 CONNECT 报文类型 */
    buf[index++] = 0x10;
    /* 剩余长度 */	/* 变量报头 + payload_len(client_id username passward) */
    uint32_t remain_len = 10 + (2+client_id_len) + (2+username_len) + (2+password_len);
    index += MQTT_EncodeLength(&buf[index], remain_len);
    /* 2. 可变报头 */
    /* 协议名长度 (0x0004) 和 协议名 ("MQTT") */
    buf[index++] = 0x00;
    buf[index++] = 0x04;
    buf[index++] = 'M';
    buf[index++] = 'Q';
    buf[index++] = 'T';
    buf[index++] = 'T';
    /* 协议级别 0x04代表MQTT v3.1.1 */
    buf[index++] = 0x04;
    /* 连接标志 0xC2:清理会话 username password */
    buf[index++] = 0xC2;
    /* Keep Alive 60秒 */
    buf[index++] = 0x00;
    buf[index++] = 0x3C;
	/* 3. 有效载荷 */
    /* client_ID */
    buf[index++] = (client_id_len >> 8) & 0xFF;
    buf[index++] = client_id_len & 0xFF;
    memcpy(&buf[index], client_id, client_id_len);
    index += client_id_len;
	/* username */
    buf[index++] = (username_len >> 8) & 0xFF;
    buf[index++] = username_len & 0xFF;
    memcpy(&buf[index], username, username_len);
    index += username_len;
	/* password */
    buf[index++] = (password_len >> 8) & 0xFF;
    buf[index++] = password_len & 0xFF;
    memcpy(&buf[index], password, password_len);
    index += password_len;
	
    return send(fd, buf, index, 0);
}
int MQTT_Subscribe(const char* topic_name,MQTT_Callback_t handler,int sockfy)
{
	if (topic_name == NULL) return -1;
	
	MQTT_List* current = mqtt_list;
    uint8_t is_exist = 0;
    while(current != NULL)
	{
		if (strcmp(current->mqtt_sub.topic_name, topic_name) == 0)
        {
            current->mqtt_sub.sockfy = sockfy;
            current->mqtt_sub.handler = handler;
            is_exist = 1;
            break;
        }
        current = current->Next;
	}
	if (!is_exist)
	{
		if(NodeCount >= MQTT_MAX_LIST_NODE) return -1;
		MQTT_List* mqtt_new_node=&mqtt_null_node[NodeCount++];
		mqtt_new_node->mqtt_sub.handler=handler;
		strcpy(mqtt_new_node->mqtt_sub.topic_name,topic_name);
		mqtt_new_node->mqtt_sub.sockfy=sockfy;
		mqtt_new_node->Next=mqtt_list;
		mqtt_list=mqtt_new_node;
	}
	uint8_t buf[128];
    int index = 0;
    uint16_t topic_len = strlen(topic_name);
	/* 固定报头：0x82 代表 SUBSCRIBE 动作 */
	buf[index++] = 0x82;
	/* 剩余长度：报文ID(2) + 主题长度(2) + 字符串长度 + QoS(1) */
	uint32_t remain_len = 2 + 2 + topic_len + 1; 
    index += MQTT_EncodeLength(&buf[index], remain_len);
	/* 报文标识符 */
	buf[index++] = 0x00;
    buf[index++] = 0x01;
	/* 载荷长度 */
	buf[index++] = (topic_len >> 8) & 0xFF;
    buf[index++] = topic_len & 0xFF;
	/* 主题字符串 */
	memcpy(&buf[index], topic_name, topic_len);
    index += topic_len;
	/* 请求的 QoS 级别 */
	buf[index++] = 0x00;
	return send(sockfy, buf, index, 0);
}
int MQTT_Publish(int fd, const char* topic, const char* payload)
{
    static uint8_t buf[512];
    uint16_t topic_len = strlen(topic);
    uint16_t payload_len = strlen(payload);
    int index = 0;
    /* 固定报头 0x30代表PUBLISH */
    buf[index++] = 0x30;
    /* 剩余长度 */
	uint32_t remain_len = 2 + topic_len + payload_len;
	index += MQTT_EncodeLength(&buf[index], remain_len);
    /* Topic 长度 */
    buf[index++] = (topic_len >> 8) & 0xFF;
    buf[index++] = topic_len & 0xFF;
    /* Topic 字符串内容 */
    memcpy(&buf[index], topic, topic_len);
    index += topic_len;
    /* Payload 数据内容 */ 
    memcpy(&buf[index], payload, payload_len);
    index += payload_len;
    return send(fd, buf, index, 0);
}
volatile uint8_t g_mqtt_ping_waiting = 0;
int Subscribe_Callback(void* rx_buf, uint16_t size)
{
	if(size < 2) return -1;
	uint8_t* buf=(uint8_t*)rx_buf;
	if (buf[0] == 0xD0 && buf[1] == 0x00)
    {
		/* recv ping */
        g_mqtt_ping_waiting = 0;
        LOG_DEBUG("Recv PINGRESP");
        return 0;
    }
	if((buf[0]>>4)!=3) return -1;
	uint8_t qos =(buf[0]&0x06)>>1;
	/* 剩余长度 */
	uint32_t remain_len = 0;
    int len_bytes = MQTT_DecodeLength(&buf[1], &remain_len);
	int offset = 1 + len_bytes;
	if (offset + 2 > size) return -1;
	/* 主题的长度 */
	uint16_t topic_len = (buf[offset] << 8) | buf[offset + 1];
    offset += 2;
	if (offset + topic_len > size) return -1;
	char* topic_ptr = (char*)&buf[offset]; /* 主题起始帧 */
	
	offset += topic_len;
	if (qos > 0) /* QoS>0 报文中会多出2个字节的 Packet ID */
	{
        offset += 2;
    }
	if (offset > size) return -1;
	char* payload_ptr=(char*)&buf[offset];/* 数据起始帧 */
	uint32_t header_size = offset - (1 + len_bytes);
	if (remain_len < header_size) return -1;
	uint16_t payload_len =remain_len - header_size; /* 数据长度 */
	if (offset + payload_len > size) return -1;
	MQTT_List* current_node = mqtt_list;
	while(current_node != NULL)
	{
		int sub_len = strlen(current_node->mqtt_sub.topic_name);
		int match = 0;
		if (current_node->mqtt_sub.topic_name[sub_len - 1] == '#')
		{
			if(strncmp(current_node->mqtt_sub.topic_name,topic_ptr,sub_len-1)==0)
			{
				match=1;
			}
		}
		else
		{
			if (sub_len == topic_len && 
				strncmp(current_node->mqtt_sub.topic_name, topic_ptr, topic_len) == 0)
			{
				match=1;
			}
		}
		if(match==1)
		{
			if (current_node->mqtt_sub.handler != NULL)
			{
				static char current_topic[256];
				uint8_t cp_len=(topic_len<sizeof(current_topic))?topic_len:(sizeof(current_topic)-1);
				memcpy(current_topic,topic_ptr,cp_len);
				current_topic[cp_len]='\0';
				current_node->mqtt_sub.handler(current_topic,payload_ptr, payload_len);
			}	
			return 0;
		}
		current_node = current_node->Next;
	}
	return -1;
}
int MQTT_PingReq(int fd)
{
    uint8_t buf[2];
    buf[0] = 0xC0;
    buf[1] = 0x00;
    return send(fd, buf, 2, 0);
}

uint32_t Get_TotalPacket_Len(void* rx_buf,uint16_t len)
{
	if (len < 2) return 0;
	uint8_t* buf=(uint8_t*)rx_buf;
	uint32_t remain_len = 0;
    int len_bytes =MQTT_DecodeLength(&buf[1], &remain_len);
	return 1 + len_bytes + remain_len;
}