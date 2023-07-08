#ifndef _NET_IF_H_
#define _NET_IF_H_

#define	IFNAMSIZ			16

#define SIOCGIFCONF			0x8912
#define SIOCGIFFLAGS			0x8913
#define SIOCSIFFLAGS			0x8914
#define SIOCGIFADDR			0x8915
#define SIOCSIFADDR			0x8916
#define SIOCGIFDSTADDR			0x8917
#define SIOCGIFBRDADDR			0x8919
#define SIOCGIFNETMASK			0x891B
#define SIOCSIFNETMASK			0x891c
#define SIOCGIFMETRIC			0x891D
#define SIOCGIFMTU			0x8921
#define SIOCGIFHWADDR			0x8927
#define SIOCGIFINDEX			0x8933
#define SIOCGIFTXQLEN			0x8942
#define SIOCGIFMAP			0x8970

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