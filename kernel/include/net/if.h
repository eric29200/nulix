#ifndef _NET_IF_H_
#define _NET_IF_H_

#define	IFNAMSIZ			16

#define SIOCGIFCONF			0x8912
#define SIOCGIFINDEX			0x8933

/*
 * Device mapping.
 */
struct ifmap  {
	unsigned long			mem_start;
	unsigned long			mem_end;
	unsigned short			base_addr; 
	unsigned char			irq;
	unsigned char			dma;
	unsigned char			port;
};

/*
 * Interface request.
 */
struct ifreq {
	union {
		char			ifrn_name[IFNAMSIZ];
	} ifr_ifrn;
	union {
		struct sockaddr		ifru_addr;
		struct sockaddr		ifru_dstaddr;
		struct sockaddr		ifru_broadaddr;
		struct sockaddr		ifru_netmask;
		struct sockaddr		ifru_hwaddr;
		short			ifru_flags;
		int			ifru_ivalue;
		int			ifru_mtu;
		struct ifmap		ifru_map;
		char			ifru_slave[IFNAMSIZ];
		char			ifru_newname[IFNAMSIZ];
		char *			ifru_data;
	} ifr_ifru;
};

/*
 * Interface configuration.
 */
struct ifconf {
	int				ifc_len;
	union {
		char *			ifcu_buf;
		struct ifreq *		ifcu_req;
	} ifc_ifcu;
};

#endif