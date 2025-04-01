/*
 * network.c
 * ----------------------------------------
 * Network Initialization and Server Launch
 *
 * Created on: Apr 2023
 * Modified by: Antonio Andara Lara | Mar 19, 2024; Mar 15, 2025
 *
 * Description:
 * This file manages the initialization of the lwIP networking stack and the
 * setup of the Ethernet interface. It creates the required FreeRTOS threads
 * for lwIP operation and launches the HTTP server application.
 *
 * Components:
 * - main_thread(): Initializes lwIP, configures static IP settings, and starts
 *                  the server thread.
 * - network_thread(): Adds and configures the network interface.
 * - print_ip_setup(): Prints IP, subnet mask, and gateway info to the console.
 */


#include "network.h"
#include "server.h"


static struct netif server_netif;


int main_thread()
{
	// initialize lwIP before calling sys_thread_new
    lwip_init();

    // any thread using lwIP should be created using sys_thread_new
    sys_thread_new( "net_t"
    			  , network_thread
				  , NULL
				  , THREAD_STACKSIZE
				  , DEFAULT_THREAD_PRIO
				  );

    vTaskDelay(START_DELAY);

	// Set the IP address, netmask, and gateway for the server netif
	IP4_ADDR( &(server_netif.ip_addr)
			, ADDR1
			, ADDR2
			, ADDR3
			, ADDR4
			);

	IP4_ADDR( &(server_netif.netmask)
			, NETMASK1
			, NETMASK2
			, NETMASK3
			, NETMASK4
			);

	IP4_ADDR( &(server_netif.gw)
			, GW1
			, GW2
			, GW3
			, GW4
			);

	// Print the IP setup
	print_ip_setup( &(server_netif.ip_addr)
				  , &(server_netif.netmask)
				  , &(server_netif.gw)
				  );

	// print all application headers
	xil_printf("\r\n");

	xil_printf( "%20s %6s %s\r\n"
			  , "Server"
			  , "Port"
			  , "Connect With.."
			  );

	xil_printf( "%20s %6s %s\r\n"
			  , "--------------------"
			  , "------"
			  , "--------------------"
			  );

	xil_printf("\r\n");

	sys_thread_new( "server_app"
				  , server_application_thread
				  , 0
				  , THREAD_STACKSIZE * 2
				  , DEFAULT_THREAD_PRIO
				  );

	vTaskDelete(NULL);

	return 0;
}


void network_thread(void *p) // Thread for setting up the network interface
{
    struct netif *netif;
    // the mac address of the board. this should be unique per board
    unsigned char mac_ethernet_address[] = { 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };
    ip_addr_t ipaddr, netmask, gw;

    netif = &server_netif;

    xil_printf("\r\n\r\n");
    xil_printf("----- lwIP Socket Application ------\r\n");

	ipaddr.addr = 0;
	gw.addr = 0;
	netmask.addr = 0;

    // Add network interface to the netif_list, and set it as default
    if (!xemac_add( netif
    			  , &ipaddr
				  , &netmask
				  , &gw
				  , mac_ethernet_address
				  , PLATFORM_EMAC_BASEADDR
				  )){
		xil_printf("Error adding N/W interface\r\n");
		return;
    }

    netif_set_default(netif);

    netif_set_up(netif); // Specify that the network interface is up

    // start packet receive thread - required for lwIP operation
    sys_thread_new( "xemacif_input_thread"
    			  , (void(*)(void*))xemacif_input_thread
				  , netif
				  , THREAD_STACKSIZE
				  , DEFAULT_THREAD_PRIO
				  );

    vTaskDelete(NULL);
}


void print_ip_setup(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw)
{
	xil_printf("\nIP setup finished:\n");
	PRINT_IP("\tBoard IP:\t", ip)
	PRINT_IP("\tNetmask:\t", mask)
	PRINT_IP("\tGateway:\t", gw)
}
