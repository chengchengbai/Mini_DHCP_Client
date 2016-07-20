/*
	dhcp.cpp
	This is the State Transducer of DHCP client
*/

#include<iostream>
#include<time.h>
#include<WinSock2.h>
#include "dhcp.h"

#pragma comment(lib,"ws2_32.lib")

using namespace std;

//configration info
double lease_time=10000;

int global_secs = 0;

extern unsigned char local_mac[16];
char last_ip[] = "127.0.0.1";

unsigned char recved_mac[16] = { 0 };

extern struct in_addr allocated_ip;		//被分配的IP
extern struct in_addr allocated_subnet_mask;	//被分配的子网掩码
extern struct in_addr server_ip;			//接收到的服务端的IP

extern struct in_addr  local_ip;			//本地的ip，从配置文件里头读取
extern struct in_addr  subnet_mask;			//子网掩码，从配置文件里头读取

void dhcp_init()
{
	clean_dhcp_info();//初始化
	dhcp_discover();            //广播
	dhcp_selecting();
}

void dhcp_reboot()			//进入REBOOT状态
{
	look_up_cache();
}

void dhcp_selecting()		//进入SELECTING状态
{
	int count = 0;
	int n;
	while (true)
	{
		n = is_recved_offer();
		if (n != 0)
		{
			break;
		}
		else
		{
			dhcp_discover();
			count++;
			if (count >= 10)
			{
				cout << "error:DHCP_DISCOVER　send unsuccessfully" << endl;
				break;
			}
		}
	}
}

void dhcp_discover()
{
	u_int8_t buffer[MAXBUFSIZE];
	u_int8_t *buffer_p = buffer;
	struct dhcp_packet *DHCP = (struct dhcp_packet *)buffer_p;

	load_discover_packet(DHCP);
	/*通过udp发送*/
	udp_broadcast_send((char*)buffer_p, sizeof(buffer));
	cout << "This is the client discover !" << endl;
}

void dhcp_request(int type)		//发送REQUEST报文
{
	int cnt_times = 1;
	while (true)
	{
		if (cnt_times >= 5)
		{
			cout << "dhcp request didn't receive any reply! " << endl;
			return;
		}
		if (send_request(type))
		{
			cout << "Client Request! " << endl;
			char *recv_buf;
			recv_buf = (char *)malloc(MAXBUFSIZE);
			int rn = recv_request(recv_buf);
			if (rn)
			{		
				switch (rn)
				{
				case DHCPACK:
					dhcp_bound();
					break;
				case DHCPNAK:
					dhcp_init();
					break;
				default:
					break;
				}
			}
			else
			{
				cnt_times++;
				continue;
			}
		}
		else
		{
			cout << "DHCP Request sends failed!" << endl;
			return;
		}
	}
}

void dhcp_bound()			//进入BOUDN状态
{
	time_t cnt_time = 0;
	time_t temp_time = time(NULL);

	while (true)
	{
		cnt_time = (time(NULL) - temp_time);
		global_secs = cnt_time;
		if (cnt_time >= lease_time*0.5)		//租约时间的判断，为了测试可以改小一点
		{
			dhcp_renewing(cnt_time);
			return;
		}
		else
		{
			Sleep(1000);	//in the BOUND state, we should wait and count the time
		}
	}
}

void dhcp_renewing(time_t cnt_time)		//进入RENEWING状态
{
	time_t temp_time;
	while (true)
	{
		temp_time = time(NULL);
		dhcp_request(DHCPREQUEST);
		char *recv_buf;
		recv_buf = (char *)malloc(MAXBUFSIZE);
		int rn = recv_request(recv_buf);
		if (rn)
		{
			switch (rn)
			{
			case DHCPACK:
				dhcp_bound();
				break;
			case DHCPNAK:
				dhcp_init();
				break;
			default:
				break;
			}
		}
		else
		{
			Sleep(10);		//leave some time for server's reply
			cnt_time += (time(NULL) - temp_time);
			global_secs = cnt_time;
			if (cnt_time >= lease_time*0.875)
			{
				dhcp_rebinding(cnt_time);
			}
		}
	}
}

void dhcp_rebinding(time_t cnt_time)		//进入REBINGDING状态
{
	time_t temp_time;
	while (true)
	{
		temp_time = time(NULL);
		global_secs = cnt_time;
		dhcp_request(DHCPREQUEST);
		char *recv_buf;
		recv_buf = (char *)malloc(MAXBUFSIZE);
		int rn = recv_request(recv_buf);
		if (rn)
		{
			switch (rn)
			{
			case DHCPACK:
				dhcp_bound();
				break;
			case DHCPNAK:
				dhcp_init();
				break;
			default:
				break;
			}
		}
		else
		{
			Sleep(10);		//leave some time for server's reply, you can change this value to fit real situation
			cnt_time += (time(NULL) - temp_time);
			if (cnt_time >= lease_time)
			{
				dhcp_init();
			}
		}
	}
}

void load_dhcp_packet(dhcp_packet *p)
{
	p->op = 1;		//client to server
	p->htype = 1;	//ethernet
	p->hlen = 6;		// Hardware addr length
	p->hops = 0;		// Number of relay agent hops from client
	p->xid = rand();
	p->secs = global_secs;		//when client didn't obtain a ip address it would be initialized as 0
	p->flags = 0x8000;	//0x8000 means a broadcast

	/*client ip is initialized as 0.0.0.0 */
	p->ciaddr.S_un.S_un_b.s_b1 = 0x00;
	p->ciaddr.S_un.S_un_b.s_b2 = 0x00;
	p->ciaddr.S_un.S_un_b.s_b3 = 0x00;
	p->ciaddr.S_un.S_un_b.s_b4 = 0x00;

	//p->ciaddr.S_un.S_addr = inet_addr("0.0.0.0");

	//client ip address filled by server in dhcp_offer and dhcp_ack
	p->yiaddr.S_un.S_un_b.s_b1 = 0x00;		
	p->yiaddr.S_un.S_un_b.s_b2 = 0x00;
	p->yiaddr.S_un.S_un_b.s_b3 = 0x00;
	p->yiaddr.S_un.S_un_b.s_b4 = 0x00;

	//next DHCP server ip address
	p->siaddr.S_un.S_un_b.s_b1 = 0x00;
	p->siaddr.S_un.S_un_b.s_b1 = 0x00;
	p->siaddr.S_un.S_un_b.s_b1 = 0x00;
	p->siaddr.S_un.S_un_b.s_b1 = 0x00;

	//DHCP relay agent IP address
	p->giaddr.S_un.S_un_b.s_b1 = 0x00;
	p->giaddr.S_un.S_un_b.s_b1 = 0x00;
	p->giaddr.S_un.S_un_b.s_b1 = 0x00;
	p->giaddr.S_un.S_un_b.s_b1 = 0x00;

	//client physical address
	for (int i = 0; i < 16; i++)
	{
		p->chaddr[i] = local_mac[i];
	}
	//p->chaddr[0] = 0xa0;
	//p->chaddr[1] = 0x48;
	//p->chaddr[2] = 0x1c;
	//p->chaddr[3] = 0x0f;
	//p->chaddr[4] = 0xf8;
	//p->chaddr[5] = 0xa4;

	//Server name
	for (int i = 0; i < DHCP_SNAME_LEN; i++)
	{
		p->sname[i]=0;
	}

	//Boot filename
	for (int i = 0; i < DHCP_FILE_LEN; i++)
	{
		p->file[i] = 0;
	}

	//cookie
	p->cookie[0] = 0x63;
	p->cookie[1] = 0x82;
	p->cookie[2] = 0x53;
	p->cookie[3] = 0x63;

	//options
	for (int i = 0; i < 308; i++)
	{
		p->options[i] = 0;
	}

}

void load_discover_packet(dhcp_packet *p)
{
	load_dhcp_packet(p);
	int len = 0;
	p->options[len] = 53;
	len++;
	p->options[len] = 1;
	len++;
	p->options[len] = 1;
	len++;
	//client id
	p->options[len] = 61;
	len++;
	p->options[len] = 7;
	len++;
	p->options[len] = 1;
	len++;
	p->options[len] = 0xa0;
	len++;
	p->options[len] = 0x48;
	len++;
	p->options[len] = 0x1c;
	len++;
	p->options[len] = 0x0f;
	len++;
	p->options[len] = 0xf8;
	len++;
	p->options[len] = 0xa4;
	len++;
	//host name
	p->options[len] = 12;
	len++;
	p->options[len] = 8;
	len++;
	p->options[len] = 0x62;
	len++;
	p->options[len] = 0x61;
	len++;
	p->options[len] = 0x69;
	len++;
	p->options[len] = 0x63;
	len++;
	p->options[len] = 0x68;
	len++;
	p->options[len] = 0x65;
	len++;
	p->options[len] = 0x6e;
	len++;
	p->options[len] = 0x67;


}

void load_release_packet(dhcp_packet *p)
{
	load_dhcp_packet(p);

	p->ciaddr.S_un.S_un_b.s_b1 = allocated_ip.S_un.S_un_b.s_b1;
	p->ciaddr.S_un.S_un_b.s_b2 = allocated_ip.S_un.S_un_b.s_b2;
	p->ciaddr.S_un.S_un_b.s_b3 = allocated_ip.S_un.S_un_b.s_b3;
	p->ciaddr.S_un.S_un_b.s_b4 = allocated_ip.S_un.S_un_b.s_b4;
	
	int len = 0;
	p->options[len] = 53;
	len++;
	p->options[len] = 1;
	len++;
	p->options[len] = DHCPRELEASE;
}

void load_request_packet(dhcp_packet *p)		//构建即将发送给服务器的REQUEST报文，关键是options字段
{
	load_dhcp_packet(p);
	int len = 0;
	p->options[len] = 53;
	len++;
	p->options[len] = 1;
	len++;
	p->options[len] = 3;
	len++;
	//your mac address
	p->options[len] = 61;
	len++;
	p->options[len] = 7;
	len++;
	p->options[len] = 1;
	len++;
	p->options[len] = local_mac[0];
	len++;
	p->options[len] = local_mac[1];
	len++;
	p->options[len] = local_mac[2];
	len++;
	p->options[len] = local_mac[3];
	len++;
	p->options[len] = local_mac[4];
	len++;
	p->options[len] = local_mac[5];
	//allocated ip
	len++;
	p->options[len] = 0x32;
	len++;
	p->options[len] = 0x04;
	len++;
	p->options[len] = allocated_ip.S_un.S_un_b.s_b1;
	len++;
	p->options[len] = allocated_ip.S_un.S_un_b.s_b2;
	len++;
	p->options[len] = allocated_ip.S_un.S_un_b.s_b3;
	len++;
	p->options[len] = allocated_ip.S_un.S_un_b.s_b4;
	//server ip
	len++;
	p->options[len] = 0x36;
	len++;
	p->options[len] = 0x04;
	len++;
	p->options[len] = server_ip.S_un.S_un_b.s_b1;
	len++;
	p->options[len] = server_ip.S_un.S_un_b.s_b2;
	len++;
	p->options[len] = server_ip.S_un.S_un_b.s_b3;
	len++;
	p->options[len] = server_ip.S_un.S_un_b.s_b4;

}

void load_ack_packet(dhcp_packet *p)
{
	load_dhcp_packet(p);
	int len = 0;
	p->options[len] = 53;
	len++;
	p->options[len] = 1;
	len++;
	p->options[len] = 5;
}

int udp_broadcast_send(char *data, int len)	
{
	WORD wVersionRequested = MAKEWORD(2, 2);
	WSADATA wsaData;
	if (0 != WSAStartup(wVersionRequested, &wsaData))
	{
		printf("WSAStartup failed with error: %d\n", GetLastError());
		return 0;
	}
	if (2 != HIBYTE(wsaData.wVersion) || 2 != LOBYTE(wsaData.wVersion))
	{
		printf("Socket version not supported.\n");
		WSACleanup();
		return 0;
	}
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (INVALID_SOCKET == sock)
	{
		printf("socket failed with error: %d\n", WSAGetLastError());
		WSACleanup();
		return 0;
	}
	SOCKADDR_IN addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.S_un.S_addr = htonl(INADDR_BROADCAST);
	addr.sin_port = htons(67);
	BOOL bBoardcast = TRUE;
	if (SOCKET_ERROR == setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&bBoardcast, sizeof(bBoardcast)))
	{
		printf("setsockopt failed with error code: %d\n", WSAGetLastError());
		if (INVALID_SOCKET != sock)
		{
			closesocket(sock);
			sock = INVALID_SOCKET;
		}
		WSACleanup();
	}

	dhcp_packet *pack;
	pack = (dhcp_packet *)data;
	if (SOCKET_ERROR == sendto(sock, data, len, 0, (LPSOCKADDR)&addr, sizeof(addr)))
	{
		printf("sendto failed with error: %d\n", WSAGetLastError());
	}
	else
	{
		closesocket(sock);
		WSACleanup();
		return 1;
	}

	closesocket(sock);
	WSACleanup();
	return 0;
}


int send_request(int send_type)
{
	u_int8_t buffer[MAXBUFSIZE];
	u_int8_t *buffer_p = buffer;
	dhcp_packet *client_packet = (dhcp_packet *)buffer_p;

	load_dhcp_packet(client_packet);


	int data_len = sizeof(buffer);
	int n = sizeof(dhcp_packet);

	switch (send_type)
	{
	case DHCPDISCOVER:
		load_discover_packet(client_packet);
		break;
	case DHCPREQUEST:
		load_request_packet(client_packet);
		break;
	case DHCPRELEASE:
		client_packet->options[0] = 53;
		client_packet->options[1] = 1;
		client_packet->options[2] = DHCPRELEASE;
		break;
	default:
		break;
	}
	client_packet->flags = 0;
	if (!udp_broadcast_send((char *)buffer, data_len))
	{
		cout << "DHCP_Request send failed!" << endl;
		return 0;
	}
	else
		return 1;
}

int recv_request(char *recv_buf)
{
	DWORD ver;
	WSADATA wsaData;
	ver = MAKEWORD(2, 2);
	WSAStartup(ver, &wsaData);
	SOCKET st = socket(AF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in addr;

	memset(&addr, 0, sizeof(addr));   // 初始化结构 addr
	addr.sin_family = AF_INET; // 代表要使用一个 TCP/IP 的地址

	addr.sin_port = htons(67); //host to net short

	addr.sin_addr.s_addr = htonl(INADDR_ANY); // 做为接收方，不需要指定具体的 IP 地址，接收的主机是什么 IP ，我就在什么 IP 上收数据
	DWORD TimeOut = 2000 * 5;//设置发送超时10秒
	int rc = 0;
	if (bind(st, (struct sockaddr *)&addr, sizeof(addr)) > -1)

	{

		//	char buf[100000] = { 0 };
		struct sockaddr_in sendaddr;
		memset(&sendaddr, 0, sizeof(sendaddr));
		int len = sizeof(sendaddr);
		if (::setsockopt(st, SOL_SOCKET, SO_RCVTIMEO, (char *)&TimeOut, sizeof(TimeOut)) == SOCKET_ERROR)
		{
			cout << "设置失败" << endl;
		}
		while (1)
		{
			// 接收 udp 数据
			rc = recvfrom(st, recv_buf, strlen(recv_buf), 0, NULL, NULL);
			if (rc < 0){
				printf("recvfrom failed with error %d\n", WSAGetLastError());
				closesocket(st); // 使用完 socket 要将其关闭
				WSACleanup();    // 释放 win_socket 内部的相关资源
				return 0;
			}

			int an = analyse_dhcp_packet(recv_buf);
			if (an == DHCPACK || an == DHCPNAK || an == DHCPOFFER)
			{
				printf("接收到 服务器回应报文\n");
				show_dhcp_packet_details(recv_buf);

				closesocket(st); // 使用完 socket 要将其关闭
				WSACleanup();    // 释放 win_socket 内部的相关资源
				return an;
			}
			else
				continue;
		}

	}
	else {
		printf("bind failed with error %d\n", WSAGetLastError());
		closesocket(st); // 使用完 socket 要将其关闭
		WSACleanup();    // 释放 win_socket 内部的相关资源
		return 0;
	}
	return 0;
}


void show_dhcp_packet_details(char *recv_buf)		//将收到的DHCP包详细信息打印出来
{
	dhcp_packet *p;
	p = (dhcp_packet *)malloc(sizeof(dhcp_packet));
	p = (dhcp_packet *)recv_buf;

	cout << "Message Reply type: " << (int)p->op << " Boot reply" << endl;
	cout << "Hardware type: " << (int)p->htype << " Ehhernet " << endl;
	cout << "Hops :" << p->hops << endl;
	cout << "Transation ID : " <<(int) p->xid << endl;
	cout << "Seconds elapsed : " <<(int) p->secs << endl;

	unsigned short ch1, ch2, ch3, ch4;
	ch1 = p->ciaddr.S_un.S_un_b.s_b1;
	ch2 = p->ciaddr.S_un.S_un_b.s_b2;
	ch3 = p->ciaddr.S_un.S_un_b.s_b3;
	ch4 = p->ciaddr.S_un.S_un_b.s_b4;
	cout << "Client IP address : " << ch1 << "." << ch2 << "." << ch3 << "." << ch4 << endl;

	ch1 = p->yiaddr.S_un.S_un_b.s_b1;
	ch2 = p->yiaddr.S_un.S_un_b.s_b2;
	ch3 = p->yiaddr.S_un.S_un_b.s_b3;
	ch4 = p->yiaddr.S_un.S_un_b.s_b4;
	cout << "Your (Client) IP address : " << ch1 << "." << ch2 << "." << ch3 << "." << ch4 << endl;

	if (p->options[0] == 53)
	{
		cout << "DHCP option Message type : ";
		switch (p->options[2])
		{
		case DHCPOFFER:
			cout << "DHCPOFFER" << endl;
			if (p->options[9] != 51)
				cout << "error lease time!" << endl;
			else
			{
				time_t time;
				time = lease_time;
				cout << "lease time : " << time << endl;
			}
			break;
		case DHCPACK:
			cout << "DHCPACK" << endl;
			if (p->options[9] != 51)
				cout << "error lease time!" << endl;
			else
			{
				time_t time;
				time = lease_time;
				cout << "lease time : " << time << endl;
			}
			break;
		case DHCPNAK:
			cout << "DHCPNAK" << endl;
			break;
		default:
			break;
		}
	}
}

int analyse_dhcp_packet(char *recv_buf)
{
	dhcp_packet *dhcp_buf;
	dhcp_buf = (dhcp_packet*)malloc(strlen(recv_buf));
	dhcp_buf = (dhcp_packet*)recv_buf;
	int msg_type = 0;

	int xid = dhcp_buf->xid;
	int secs = dhcp_buf->secs;
	char mac[6];
	for (int i = 1; i < 6; i++)
	{
		mac[i] = dhcp_buf->chaddr[i];
	}

	allocated_ip.S_un.S_un_b.s_b1 = dhcp_buf->yiaddr.S_un.S_un_b.s_b1;
	allocated_ip.S_un.S_un_b.s_b2 = dhcp_buf->yiaddr.S_un.S_un_b.s_b2;
	allocated_ip.S_un.S_un_b.s_b3 = dhcp_buf->yiaddr.S_un.S_un_b.s_b3;
	allocated_ip.S_un.S_un_b.s_b4 = dhcp_buf->yiaddr.S_un.S_un_b.s_b4;

	for (int i = 0; i < 6; i++)
	{
		recved_mac[i] = dhcp_buf->chaddr[i];
		if (recved_mac[i] != local_mac[i])
			return 0;
	}

	if (dhcp_buf->options[0] == 53)		//dhcp message type
	{
		msg_type = dhcp_buf->options[2];
		if (msg_type == DHCPOFFER || msg_type == DHCPACK)
		{
			if (dhcp_buf->options[9] == 51)
			{
				u_int32_t t1, t2, t3, t4;
				t1 = 0, t2 = 0, t3 = 0, t4 = 0;
				t1 = dhcp_buf->options[11];
				t1 = t1 << 24;
				t2 = dhcp_buf->options[12];
				t2 = t2 << 16;
				t3 = dhcp_buf->options[13];
				t3 = t3 << 8;
				t4 = dhcp_buf->options[14];
				u_int32_t time = 0;
				time |= t1;
				time |= t2;
				time |= t3;
				time |= t4;
				lease_time = time;
			}
			
			if (dhcp_buf->options[4] == 54)
			{
				server_ip.S_un.S_un_b.s_b1 = dhcp_buf->options[6];
				server_ip.S_un.S_un_b.s_b2 = dhcp_buf->options[7];
				server_ip.S_un.S_un_b.s_b3 = dhcp_buf->options[8];
				server_ip.S_un.S_un_b.s_b4 = dhcp_buf->options[9];
			}
			if (dhcp_buf->options[15] == 1)
			{
				allocated_subnet_mask.S_un.S_un_b.s_b1 = dhcp_buf->options[17];
				allocated_subnet_mask.S_un.S_un_b.s_b2 = dhcp_buf->options[18];
				allocated_subnet_mask.S_un.S_un_b.s_b3 = dhcp_buf->options[19];
				allocated_subnet_mask.S_un.S_un_b.s_b4 = dhcp_buf->options[20];
			}
			if (msg_type == DHCPACK)
			{
				cout << "DHCPACK" << endl;
				write_cache();
			}
			else
			{
				cout << "DHCPOFFER" << endl;
			}
		}
		if (msg_type == DHCPNAK)
		{
			allocated_ip.S_un.S_un_b.s_b1 = 0;
			allocated_ip.S_un.S_un_b.s_b2 = 0;
			allocated_ip.S_un.S_un_b.s_b3 = 0;
			allocated_ip.S_un.S_un_b.s_b4 = 0;

			allocated_subnet_mask.S_un.S_un_b.s_b1 = 0;
			allocated_subnet_mask.S_un.S_un_b.s_b2 = 0;
			allocated_subnet_mask.S_un.S_un_b.s_b3 = 0;
			allocated_subnet_mask.S_un.S_un_b.s_b4 = 0;

			cout << "DHCPNAK";
		}
	}
	else
	{
		cout << "error dhcp data!" << endl;
	}
	return msg_type;
}

int is_recved_offer()			//判断是否收到来自服务器端的DHCPOFFER
{
	char *recv_buf;
	recv_buf = (char *)malloc(MAXBUFSIZE);
	int n = recv_request(recv_buf);
	if (n != 0)
	{
		if (n==DHCPOFFER)
			select_net_info((u_int8_t *)recv_buf, MAXBUFSIZE);
		else
		{
			cout << "didn't recieve offer" << endl;
			return 0;
		}
	}
	return n;
}

void clean_dhcp_info()
{
	//DhcpCApiCleanup();
}

void select_net_info(u_int8_t * buffer, int len)
{
	struct dhcp_packet *p = (struct dhcp_packet *)buffer;
	load_request_packet(p);
	dhcp_request(DHCPREQUEST);
}

int look_up_cache()
{
	FILE *fp;

	if (!(fp = fopen("ip.coi", "rb")))
	{
		cout << "cannot read the ip configration cache" << endl;
		return 0;
	}
	int i = 0;

	while (!feof(fp))
	{
		unsigned char ch;
		ch = fgetc(fp);
		switch (i)
		{
		case 0:
			local_ip.S_un.S_un_b.s_b1 = ch;
			allocated_ip.S_un.S_un_b.s_b1 = ch;
			break;
		case 1:
			local_ip.S_un.S_un_b.s_b2 = ch;
			allocated_ip.S_un.S_un_b.s_b2 = ch;
			break;
		case 2:
			local_ip.S_un.S_un_b.s_b3 = ch;
			allocated_ip.S_un.S_un_b.s_b3 = ch;
			break;
		case 3:
			local_ip.S_un.S_un_b.s_b4 = ch;
			allocated_ip.S_un.S_un_b.s_b4 = ch;
			break;
		case 4:
			subnet_mask.S_un.S_un_b.s_b1 = ch;
			allocated_subnet_mask.S_un.S_un_b.s_b1 = ch;
			break;
		case 5:
			subnet_mask.S_un.S_un_b.s_b2 = ch;
			allocated_subnet_mask.S_un.S_un_b.s_b2 = ch;
			break;
		case 6:
			subnet_mask.S_un.S_un_b.s_b3 = ch;
			allocated_subnet_mask.S_un.S_un_b.s_b3 = ch;
			break;
		case 7:
			subnet_mask.S_un.S_un_b.s_b4 = ch;
			allocated_subnet_mask.S_un.S_un_b.s_b4 = ch;
			break;
		default:
			break;
		}
		i++;
		if (i == 8)
		{
			cout << "read cache succeeded! " << endl;
			break;
		}
	}
	return 1;
	fclose(fp);
}

void write_cache()
{
	FILE *fp;

	if (!(fp = fopen("ip.coi", "wb")))
	{
		cout << "cannot read the ip configration cache" << endl;
		return;
	}
	int i = 0;
	while (1)
	{
		unsigned char ch;
		switch (i)
		{
		case 0:
			local_ip.S_un.S_un_b.s_b1 = allocated_ip.S_un.S_un_b.s_b1;
			ch = local_ip.S_un.S_un_b.s_b1;
			fputc(ch, fp);
			break;
		case 1:
			local_ip.S_un.S_un_b.s_b2 = allocated_ip.S_un.S_un_b.s_b2;
			ch = local_ip.S_un.S_un_b.s_b2;
			fputc(ch, fp);
			break;
		case 2:
			local_ip.S_un.S_un_b.s_b3 = allocated_ip.S_un.S_un_b.s_b3;
			ch = local_ip.S_un.S_un_b.s_b3;
			fputc(ch, fp);
			break;
		case 3:
			local_ip.S_un.S_un_b.s_b4 = allocated_ip.S_un.S_un_b.s_b4;
			ch = local_ip.S_un.S_un_b.s_b4;
			fputc(ch, fp);
			break;
		case 4:
			subnet_mask.S_un.S_un_b.s_b1 = allocated_subnet_mask.S_un.S_un_b.s_b1;
			ch = subnet_mask.S_un.S_un_b.s_b1;
			fputc(ch, fp);
			break;
		case 5:
			subnet_mask.S_un.S_un_b.s_b2 = allocated_subnet_mask.S_un.S_un_b.s_b2;
			ch = subnet_mask.S_un.S_un_b.s_b2;
			fputc(ch, fp);
			break;
		case 6:
			subnet_mask.S_un.S_un_b.s_b3 = allocated_subnet_mask.S_un.S_un_b.s_b3;
			ch = subnet_mask.S_un.S_un_b.s_b3;
			fputc(ch, fp);
			break;
		case 7:
			subnet_mask.S_un.S_un_b.s_b4 = allocated_subnet_mask.S_un.S_un_b.s_b4;
			ch = subnet_mask.S_un.S_un_b.s_b4;
			fputc(ch, fp);
			break;
		default:
			break;
		}
		i++;
		if (i == 8)
		{
			cout << "write cache succeeded!" << endl;
			break;
		}
	}
	fclose(fp);
}