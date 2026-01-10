#ifndef _ROUTE_H_
#define _ROUTE_H_

#include <net/socket.h>
#include <lib/list.h>

#define	RTF_UP			0x0001		/* route usable		  	  */
#define	RTF_GATEWAY		0x0002		/* destination is a gateway	  */
#define	RTF_HOST		0x0004		/* host entry (net otherwise)	  */
#define RTF_REINSTATE		0x0008		/* reinstate route after tmout	  */
#define	RTF_DYNAMIC		0x0010		/* created dyn. (by redirect)	  */
#define	RTF_MODIFIED		0x0020		/* modified dyn. (by redirect)	  */
#define RTF_MSS			0x0040		/* specific MSS for this route	  */
#define RTF_WINDOW		0x0080		/* per route window clamping	  */
#define RTF_IRTT		0x0100		/* Initial round trip time	  */
#define RTF_REJECT		0x0200		/* Reject route			  */

/*
 * Route entry.
 */
struct rtentry {
	unsigned long		rt_pad1;
	struct sockaddr		rt_dst;		/* target address		*/
	struct sockaddr		rt_gateway;	/* gateway addr (RTF_GATEWAY)	*/
	struct sockaddr		rt_genmask;	/* target network mask (IP)	*/
	unsigned short		rt_flags;
	short			rt_pad2;
	unsigned long		rt_pad3;
	void *			rt_pad4;
	short			rt_metric;	/* +1 for binary compatibility!	*/
	char *			rt_dev;		/* forcing the device at add	*/
	unsigned long		rt_mtu;		/* per route MTU/Window 	*/
#define rt_mss	rt_mtu				/* Compatibility :-(            */
	unsigned long		rt_window;	/* Window clamping 		*/
	unsigned short		rt_irtt;	/* Initial RTT			*/
};

/*
 * Kernel route entry.
 */
struct route {
	uint32_t		rt_dst;
	uint16_t		rt_metric;
	uint32_t		rt_use;
	uint32_t		rt_gateway;
	uint32_t		rt_mask;
	struct net_device *	rt_dev;
	int			rt_refcnt;
	uint32_t		rt_window;
	uint16_t		rt_flags;
	uint16_t		rt_mtu;
	uint16_t		rt_irtt;
	struct list_head	rt_list;
};

struct route *ip_rt_route(uint32_t daddr);
int ip_rt_ioctl(int cmd, void *arg);
int route_read_proc(char *page, char **start, off_t off, size_t count, int *eof);

#endif