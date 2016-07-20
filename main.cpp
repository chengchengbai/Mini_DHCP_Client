/*****************************/
/*       本程序根据			*/
/*      DHCP程序流程图书写		*/
/*****************************/


#include <stdio.h>
#include <iostream>
#include "dhcp.h"

using namespace std;


bool first_time_to_login = true;
struct in_addr  local_ip;
struct in_addr  subnet_mask;
struct in_addr allocated_ip;
struct in_addr allocated_subnet_mask;
struct in_addr server_ip;

unsigned char local_mac[16] = { 0 };

void test()
{
	//udp_broadcast_send();
}

void initial_local_mac()			//当在不同客户端上使用时，需要修改手动这里的mac地址
{
	local_mac[0] = 0xa0;
	local_mac[1] = 0x48;
	local_mac[2] = 0x1c;
	local_mac[3] = 0x0f;
	local_mac[4] = 0xf8;
	local_mac[5] = 0xa4;
}

int main()
{

	initial_local_mac();	//获得本地mac地址

	first_time_to_login = look_up_cache();		//读取ip配置缓存文件，即当前目录下的"ip.coi"文件
	
	if (first_time_to_login)	//如果是第一次登陆，则发送DHCPDISCOVER
	{

		dhcp_init();
	}
	else
	{
		dhcp_reboot();			//如果不是第一次登陆，则发送DHCPREQUEST
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
			}
		}
	}
	system("pause");
	return 0;
}

