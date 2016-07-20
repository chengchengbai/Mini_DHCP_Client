/*
	dhcp.h
	definition of dhcp
*/

#pragma once

#include<windows.h>
#include<inaddr.h>
#include<time.h>

typedef unsigned char u_int8_t;
typedef signed char int8_t;
typedef unsigned short u_int16_t;
typedef signed short int16_t;
typedef unsigned int u_int32_t;
typedef signed int int32_t;

#define DHCP_UDP_OVERHEAD	(14 + /* Ethernet header */		\
	20 + /* IP header */			\
	8)   /* UDP header */
#define DHCP_SNAME_LEN		64
#define DHCP_FILE_LEN		128
#define DHCP_FIXED_NON_UDP	236
#define DHCP_FIXED_LEN		(DHCP_FIXED_NON_UDP + DHCP_UDP_OVERHEAD)
/* Everything but options. */
#define DHCP_MTU_MAX		1500
#define DHCP_OPTION_LEN		(DHCP_MTU_MAX - DHCP_FIXED_LEN)

#define BOOTP_MIN_LEN		300


typedef struct dhcp_packet {
	u_int8_t  op;		/* Message opcode/type */
	u_int8_t  htype;	/* Hardware addr type (see net/if_types.h) */
	u_int8_t  hlen;		/* Hardware addr length */
	u_int8_t  hops;		/* Number of relay agent hops from client */
	u_int32_t xid;		/* Transaction ID */
	u_int16_t secs;		/* Seconds since client started looking */
	u_int16_t flags;	/* Flag bits */
	struct in_addr ciaddr;	/* Client IP address (if already in use) */
	struct in_addr yiaddr;	/* Client IP address */
	struct in_addr siaddr;	/* IP address of next server to talk to */
	struct in_addr giaddr;	/* DHCP relay agent IP address */
	unsigned char chaddr[16];	/* Client hardware address */
	char sname[DHCP_SNAME_LEN];	/* Server name */
	char file[DHCP_FILE_LEN];	/* Boot filename */
	u_int8_t cookie[4];
	unsigned char options[308];
	/* Optional parameters
	(actual length dependent on MTU). */
}dhcp_packet;

/* DHCP message types. */
#define DHCPDISCOVER	1
#define DHCPOFFER	2
#define DHCPREQUEST	3
#define DHCPDECLINE	4
#define DHCPACK		5
#define DHCPNAK		6
#define DHCPRELEASE	7
#define DHCPINFORM	8

/* REQUEST Type */
#define OFFERING 0
#define RENEWING 1
#define REBINGDING  2

#define BROADCAST 0
#define UNBROADCAST 1

#define MAXBUFSIZE 1024

void dhcp_init();
void dhcp_reboot();
void dhcp_selecting();
void dhcp_discover();
void dhcp_request(int type);
void dhcp_bound();
void dhcp_renewing(time_t cnt_time);
void dhcp_rebinding(time_t cnt_time);


void load_dhcp_packet(dhcp_packet *p);
void load_discover_packet(dhcp_packet *p);
void load_request_packet(dhcp_packet *p);
int send_request(int type);
int recv_request(char *recv_buf);
int udp_broadcast_send(char *data, int len);
void clean_dhcp_info();
void select_net_info(u_int8_t * buffer, int len);
int is_recved_offer();
int analyse_dhcp_packet(char *recv_buf);
void show_dhcp_packet_details(char *recv_buf);
int look_up_cache();
void write_cache();