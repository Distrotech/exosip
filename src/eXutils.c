/*
  eXosip - This is the eXtended osip library.
  Copyright (C) 2002, 2003  Aymeric MOIZARD  - jack@atosc.org
  
  eXosip is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  eXosip is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#ifdef ENABLE_MPATROL
#include <mpatrol.h>
#endif

#include <osipparser2/osip_port.h>
#include "eXosip2.h"


#if defined(WIN32) || defined(_WIN32_WCE)
#include <WinDNS.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif

#ifdef HAVE_RESOLV8_COMPAT_H
#include <nameser8_compat.h>
#include <resolv8_compat.h>
#elif defined(HAVE_RESOLV_H) || defined(OpenBSD) || defined(FreeBSD) || defined(NetBSD)
#include <resolv.h>
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#endif


extern eXosip_t eXosip;

extern int ipv6_enable;

#if defined(WIN32) || defined(_WIN32_WCE)

/* You need the Platform SDK to compile this. */
#include <Windows.h>
#include <Iphlpapi.h>

int
eXosip_dns_get_local_fqdn (char **servername, char **serverip,
                        char **netmask, unsigned int WIN32_interface)
{
  unsigned int pos;

  *servername = NULL;           /* no name on win32? */
  *serverip = NULL;
  *netmask = NULL;

  /* First, try to get the interface where we should listen */
  {
    DWORD size_of_iptable = 0;
    PMIB_IPADDRTABLE ipt;
    PMIB_IFROW ifrow;

    if (GetIpAddrTable (NULL, &size_of_iptable, TRUE) == ERROR_INSUFFICIENT_BUFFER)
      {
        ifrow = (PMIB_IFROW) _alloca (sizeof (MIB_IFROW));
        ipt = (PMIB_IPADDRTABLE) _alloca (size_of_iptable);
        if (ifrow == NULL || ipt == NULL)
          {
            /* not very usefull to continue */
            OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO4, NULL,
                                    "ERROR alloca failed\r\n"));
            return -1;
          }

        if (!GetIpAddrTable (ipt, &size_of_iptable, TRUE))
          {
            /* look for the best public interface */

            for (pos = 0; pos < ipt->dwNumEntries && *netmask == NULL; ++pos)
              {
                /* index is */
                struct in_addr addr;
                struct in_addr mask;

                ifrow->dwIndex = ipt->table[pos].dwIndex;
                if (GetIfEntry (ifrow) == NO_ERROR)
                  {
                    switch (ifrow->dwType)
                      {
                        case MIB_IF_TYPE_LOOPBACK:
                          /*    break; */
                        case MIB_IF_TYPE_ETHERNET:
                        default:
                          addr.s_addr = ipt->table[pos].dwAddr;
                          mask.s_addr = ipt->table[pos].dwMask;
                          if (ipt->table[pos].dwIndex == WIN32_interface)
                            {
                              *servername = NULL;       /* no name on win32? */
                              *serverip = osip_strdup (inet_ntoa (addr));
                              *netmask = osip_strdup (inet_ntoa (mask));
                              OSIP_TRACE (osip_trace
                                          (__FILE__, __LINE__, OSIP_INFO4, NULL,
                                           "Interface ethernet: %s/%s\r\n",
                                           *serverip, *netmask));
                              break;
                            }
                      }
                  }
              }
          }
      }
  }

  if (*serverip == NULL || *netmask == NULL)
    {
      OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_WARNING, NULL,
                              "ERROR No network interface found\r\n"));
      return -1;
    }

  return 0;
}

#if 1

int
eXosip_guess_ip_for_via (int family, char *address, int size)
{
  SOCKET sock;
  SOCKADDR_STORAGE local_addr;
  int local_addr_len;
  struct addrinfo *addrf;
  
  address[0] = '\0';
  sock = socket(family, SOCK_DGRAM, 0);
  
  if(family == AF_INET)
    {
      getaddrinfo("217.12.3.11",NULL,NULL,&addrf);
    }
  else if(family == AF_INET6)
    {
      getaddrinfo("2001:238:202::1",NULL,NULL,&addrf);
    }
  
  if (addrf==NULL)
  {
      closesocket(sock);
      freeaddrinfo(addrf);
      snprintf(address, size, (family == AF_INET) ? "127.0.0.1" : "::1" );
      return -1;
  }

  if(WSAIoctl(sock,SIO_ROUTING_INTERFACE_QUERY, addrf->ai_addr, addrf->ai_addrlen,
	      &local_addr, sizeof(local_addr), &local_addr_len, NULL, NULL) != 0)
    {
      closesocket(sock);
      freeaddrinfo(addrf);
      snprintf(address, size, (family == AF_INET) ? "127.0.0.1" : "::1" );
      return -1;
    }
  
  closesocket(sock);
  freeaddrinfo(addrf);
  
  if(getnameinfo((const struct sockaddr*)&local_addr,
		 local_addr_len,address, size, NULL, 0, NI_NUMERICHOST))
    {
      snprintf(address, size, (family == AF_INET) ? "127.0.0.1" : "::1" );
      return -1;
    }
  
  return 0;
}

#else

int
eXosip_guess_ip_for_via (int family, char *address, int size)
{
  /* w2000 and W95/98 */
  unsigned long best_interface_index;
  DWORD hr;

  /* NT4 (sp4 only?) */
  PMIB_IPFORWARDTABLE ipfwdt;
  DWORD siz_ipfwd_table = 0;
  unsigned int ipf_cnt;

  address[0] = '\0';
  best_interface_index = -1;
  /* w2000 and W95/98 only */

  hr = GetBestInterface (inet_addr ("217.12.3.11"), &best_interface_index);
  if (hr)
    {
      LPVOID lpMsgBuf;

      FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER |
                     FORMAT_MESSAGE_FROM_SYSTEM |
                     FORMAT_MESSAGE_IGNORE_INSERTS,
                     NULL,
                     hr,
                     MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
                     (LPSTR) & lpMsgBuf, 0, NULL);

      OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO4, NULL,
                              "GetBestInterface: %s\r\n", lpMsgBuf));
      best_interface_index = -1;
    }

  if (best_interface_index != -1)
    {                           /* probably W2000 or W95/W98 */
      char *servername;
      char *serverip;
      char *netmask;

      OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO4, NULL,
                              "Default Interface found %i\r\n",
                              best_interface_index));

      if (0 == eXosip_dns_get_local_fqdn (&servername, &serverip, &netmask,
                                       best_interface_index))
        {
          osip_strncpy (address, serverip, size - 1);
          osip_free (servername);
          osip_free (serverip);
          osip_free (netmask);
          return 0;
        }
      return -1;
    }


  if (!GetIpForwardTable (NULL, &siz_ipfwd_table, FALSE) ==
      ERROR_INSUFFICIENT_BUFFER
      || !(ipfwdt = (PMIB_IPFORWARDTABLE) alloca (siz_ipfwd_table)))
    {
      OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO4, NULL,
                              "Allocation error\r\n"));
      return -1;
    }


  /* NT4 (sp4 support only?) */
  if (!GetIpForwardTable (ipfwdt, &siz_ipfwd_table, FALSE))
    {
      for (ipf_cnt = 0; ipf_cnt < ipfwdt->dwNumEntries; ++ipf_cnt)
        {
          if (ipfwdt->table[ipf_cnt].dwForwardDest == 0)
            {                   /* default gateway found */
              char *servername;
              char *serverip;
              char *netmask;

              OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO4, NULL,
                                      "Default Interface found %i\r\n",
                                      ipfwdt->table[ipf_cnt].dwForwardIfIndex));

              if (0 == eXosip_dns_get_local_fqdn (&servername,
                                               &serverip,
                                               &netmask,
                                               ipfwdt->table[ipf_cnt].
                                               dwForwardIfIndex))
                {
                  osip_strncpy (address, serverip, size);
                  osip_free (servername);
                  osip_free (serverip);
                  osip_free (netmask);
                  return 0;
                }
              return -1;
            }
        }

    }

  OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_WARNING, NULL,
                          "Please define a default network interface.\r\n"));
#ifdef WANT_INTERFACE_ANYWAY


  /* NT4 (sp4 support only?) */
  if (!GetIpForwardTable (ipfwdt, &siz_ipfwd_table, FALSE))
    {
      for (ipf_cnt = 0; ipf_cnt < ipfwdt->dwNumEntries; ++ipf_cnt)
        {
          char *servername;
          char *serverip;
          char *netmask;

          OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO4, NULL,
                                  "Default Interface found %i\r\n",
                                  ipfwdt->table[ipf_cnt].dwForwardIfIndex));

          if (0 == eXosip_dns_get_local_fqdn (&servername,
                                           &serverip,
                                           &netmask,
                                           ipfwdt->table[ipf_cnt].
                                           dwForwardIfIndex))
            {
              /* search for public */
              if (eXosip_is_public_address (serverip) == 0)
                {
                  osip_strncpy (address, serverip, size);
                  osip_free (servername);
                  osip_free (serverip);
                  osip_free (netmask);
                  return 0;
                }
              osip_free (servername);
              osip_free (serverip);
              osip_free (netmask);
            }
        }
    }

  OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO2, NULL,
                          "No public interface found. searching private\r\n"));

  /* NT4 (sp4 support only?) */
  if (!GetIpForwardTable (ipfwdt, &siz_ipfwd_table, FALSE))
    {
      for (ipf_cnt = 0; ipf_cnt < ipfwdt->dwNumEntries; ++ipf_cnt)
        {
          char *servername;
          char *serverip;
          char *netmask;

          OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO4, NULL,
                                  "Default Interface found %i\r\n",
                                  ipfwdt->table[ipf_cnt].dwForwardIfIndex));

          if (0 == eXosip_dns_get_local_fqdn (&servername,
                                           &serverip,
                                           &netmask,
                                           ipfwdt->table[ipf_cnt].
                                           dwForwardIfIndex))
            {
              osip_strncpy (address, serverip, size);
              osip_free (servername);
              osip_free (serverip);
              osip_free (netmask);
              return 0;
            }
        }
    }

  {
    char *lo = osip_strdup ("127.0.0.1");

    osip_strncpy (address, lo, size);
    osip_free (lo);
    return 0;
  }

  OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_INFO2, NULL,
                          "No interface found. returning 127.0.0.1\r\n"));
  /* no default gateway interface found */
  return 0;
#else
  return -1;
#endif /* WANT_INTERFACE_ANYWAY */
}

#endif

#else /* sun, *BSD, linux, and other? */


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <sys/param.h>

#include <stdio.h>

static int ppl_dns_default_gateway_ipv4 (char *address, int size);
static int ppl_dns_default_gateway_ipv6 (char *address, int size);


int
eXosip_guess_ip_for_via (int family, char *address, int size)
{
  if (family == AF_INET6)
    {
      return ppl_dns_default_gateway_ipv6 (address, size);
  } else
    {
      return ppl_dns_default_gateway_ipv4 (address, size);
    }
}

/* This is a portable way to find the default gateway.
 * The ip of the default interface is returned.
 */
static int
ppl_dns_default_gateway_ipv4 (char *address, int size)
{
#ifdef __APPLE_CC__
  int len;
#else
  unsigned int len;
#endif
  int sock_rt, on = 1;
  struct sockaddr_in iface_out;
  struct sockaddr_in remote;

  memset (&remote, 0, sizeof (struct sockaddr_in));

  remote.sin_family = AF_INET;
  remote.sin_addr.s_addr = inet_addr ("217.12.3.11");
  remote.sin_port = htons (11111);

  memset (&iface_out, 0, sizeof (iface_out));
  sock_rt = socket (AF_INET, SOCK_DGRAM, 0);

  if (setsockopt (sock_rt, SOL_SOCKET, SO_BROADCAST, &on, sizeof (on)) == -1)
    {
      perror ("DEBUG: [get_output_if] setsockopt(SOL_SOCKET, SO_BROADCAST");
      close (sock_rt);
      snprintf(address, size, "127.0.0.1");
      return -1;
    }

  if (connect
      (sock_rt, (struct sockaddr *) &remote, sizeof (struct sockaddr_in)) == -1)
    {
      perror ("DEBUG: [get_output_if] connect");
      close (sock_rt);
      snprintf(address, size, "127.0.0.1");
      return -1;
    }

  len = sizeof (iface_out);
  if (getsockname (sock_rt, (struct sockaddr *) &iface_out, &len) == -1)
    {
      perror ("DEBUG: [get_output_if] getsockname");
      close (sock_rt);
      snprintf(address, size, "127.0.0.1");
      return -1;
    }

  close (sock_rt);
  if (iface_out.sin_addr.s_addr == 0)
    {                           /* what is this case?? */
      snprintf(address, size, "127.0.0.1");
      return -1;
    }
  osip_strncpy (address, inet_ntoa (iface_out.sin_addr), size - 1);
  return 0;
}


/* This is a portable way to find the default gateway.
 * The ip of the default interface is returned.
 */
static int
ppl_dns_default_gateway_ipv6 (char *address, int size)
{
#ifdef __APPLE_CC__
  int len;
#else
  unsigned int len;
#endif
  int sock_rt, on = 1;
  struct sockaddr_in6 iface_out;
  struct sockaddr_in6 remote;

  memset (&remote, 0, sizeof (struct sockaddr_in6));

  remote.sin6_family = AF_INET6;
  inet_pton (AF_INET6, "2001:638:500:101:2e0:81ff:fe24:37c6", &remote.sin6_addr);
  remote.sin6_port = htons (11111);

  memset (&iface_out, 0, sizeof (iface_out));
  sock_rt = socket (AF_INET6, SOCK_DGRAM, 0);

  if (setsockopt (sock_rt, SOL_SOCKET, SO_BROADCAST, &on, sizeof (on)) == -1)
    {
      perror ("DEBUG: [get_output_if] setsockopt(SOL_SOCKET, SO_BROADCAST");
      close (sock_rt);
      return -1;
    }

  if (connect
      (sock_rt, (struct sockaddr *) &remote, sizeof (struct sockaddr_in6)) == -1)
    {
      perror ("DEBUG: [get_output_if] connect");
      close (sock_rt);
      return -1;
    }

  len = sizeof (iface_out);
  if (getsockname (sock_rt, (struct sockaddr *) &iface_out, &len) == -1)
    {
      perror ("DEBUG: [get_output_if] getsockname");
      close (sock_rt);
      return -1;
    }
  close (sock_rt);

  if (iface_out.sin6_addr.s6_addr == 0)
    {                           /* what is this case?? */
      return -1;
    }
  inet_ntop (AF_INET6, (const void *) &iface_out.sin6_addr, address, size - 1);
  return 0;
}

#endif

#ifdef SM

int
eXosip_get_localip_for (const char *address_to_reach, char *loc, int size)
{
  int err, tmp;
  struct addrinfo hints;
  struct addrinfo *res = NULL;
  struct sockaddr_storage addr;
  int sock;

#ifdef __APPLE_CC__
  int s;
#else
  socklen_t s;
#endif
#ifdef MAXHOSTNAMELEN
  if (size > MAXHOSTNAMELEN)
    size = MAXHOSTNAMELEN;
#else
  if (size > 256)
    size = 256;
#endif
  if (eXosip.forced_localip)
    {
      if (size > sizeof (eXosip.net_interfaces[0].net_firewall_ip))
        size = sizeof (eXosip.net_interfaces[0].net_firewall_ip);
      strncpy (loc, eXosip.net_interfaces[0].net_firewall_ip, size);
      return 0;
    }

  strcpy (loc, "127.0.0.1");    /* always fallback to local loopback */

  memset (&hints, 0, sizeof (hints));
  if(ipv6_enable)
    hints.ai_family = PF_INET6;
  else
    hints.ai_family = PF_INET;    /* ipv4 only support */

  hints.ai_socktype = SOCK_DGRAM;
  /*hints.ai_flags=AI_NUMERICHOST|AI_CANONNAME; */
  err = getaddrinfo (address_to_reach, "5060", &hints, &res);
  if (err != 0)
    {
      eXosip_trace (OSIP_ERROR,
                    ("Error in getaddrinfo for %s: %s\n", address_to_reach,
                     gai_strerror (err)));
      return -1;
    }
  if (res == NULL)
    {
      eXosip_trace (OSIP_ERROR, ("getaddrinfo reported nothing !"));
      return -1;
    }
  sock = socket (res->ai_family, SOCK_DGRAM, 0);
  tmp = 1;
  err =
    setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, (const char *) &tmp, sizeof (int));
  if (err < 0)
    {
      eXosip_trace (OSIP_ERROR, ("Error in setsockopt: %s\n", strerror (errno)));
      freeaddrinfo (res);
      close (sock);
      return -1;
    }
  err = connect (sock, res->ai_addr, res->ai_addrlen);
  if (err < 0)
    {
      eXosip_trace (OSIP_ERROR, ("Error in connect: %s\n", strerror (errno)));
      freeaddrinfo (res);
      close (sock);
      return -1;
    }
  freeaddrinfo (res);
  res = NULL;
  s = sizeof (addr);
  err = getsockname (sock, (struct sockaddr *) &addr, &s);
  if (err != 0)
    {
      eXosip_trace (OSIP_ERROR, ("Error in getsockname: %s\n", strerror (errno)));
      close (sock);
      return -1;
    }

  err =
    getnameinfo ((struct sockaddr *) &addr, s, loc, size, NULL, 0, NI_NUMERICHOST);
  if (err != 0)
    {
      eXosip_trace (OSIP_ERROR, ("getnameinfo error:%s", strerror (errno)));
      close (sock);
      return -1;
    }
  close (sock);
  eXosip_trace (OSIP_INFO1,
                ("Outgoing interface to reach %s is %s.\n", address_to_reach,
                 loc));
  return 0;
}


void
eXosip_get_localip_from_via (osip_message_t * mesg, char *locip, int size)
{
  osip_via_t *via = NULL;
  char *host;

  via = (osip_via_t *) osip_list_get (mesg->vias, 0);
  if (via == NULL)
    {
      host = "15.128.128.93";
      eXosip_trace (OSIP_ERROR, ("Could not get via:%s"));
  } else
    host = via->host;
  eXosip_get_localip_for (host, locip, size);

}
#endif

char *
strdup_printf (const char *fmt, ...)
{
  /* Guess we need no more than 100 bytes. */
  int n, size = 100;
  char *p;
  va_list ap;

  if ((p = osip_malloc (size)) == NULL)
    return NULL;
  while (1)
    {
      /* Try to print in the allocated space. */
      va_start (ap, fmt);
#ifdef WIN32
      n = _vsnprintf (p, size, fmt, ap);
#else
      n = vsnprintf (p, size, fmt, ap);
#endif
      va_end (ap);
      /* If that worked, return the string. */
      if (n > -1 && n < size)
        return p;
      /* Else try again with more space. */
      if (n > -1)               /* glibc 2.1 */
        size = n + 1;           /* precisely what is needed */
      else                      /* glibc 2.0 */
        size *= 2;              /* twice the old size */
      if ((p = realloc (p, size)) == NULL)
        return NULL;
    }
}

int
eXosip_get_addrinfo (struct addrinfo **addrinfo, const char *hostname,
                     int service, int protocol)
{
  struct addrinfo hints;
  int error;
  char portbuf[10];

  if (hostname == NULL)
    return -1;

  if (service != -1)            /* -1 for SRV record */
    snprintf (portbuf, sizeof (portbuf), "%i", service);

  memset (&hints, 0, sizeof (hints));

  hints.ai_flags = 0;

  if(ipv6_enable)
    hints.ai_family = PF_INET6;
  else
    hints.ai_family = PF_INET;    /* ipv4 only support */

  if (protocol == IPPROTO_UDP)
    hints.ai_socktype = SOCK_DGRAM;
  else
    hints.ai_socktype = SOCK_STREAM;

  hints.ai_protocol = protocol; /* IPPROTO_UDP or IPPROTO_TCP */
  if (service == -1)            /* -1 for SRV record */
    {
      error = getaddrinfo (hostname, "sip", &hints, addrinfo);
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_INFO2, NULL,
                   "SRV resolution with udp-sip-%s\n", hostname));
  } else
    {
      error = getaddrinfo (hostname, portbuf, &hints, addrinfo);
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_INFO2, NULL,
                   "DNS resolution with %s:%i\n", hostname, service));
    }
  if (error || *addrinfo == NULL)
    {
      OSIP_TRACE (osip_trace
                  (__FILE__, __LINE__, OSIP_INFO2, NULL,
                   "getaddrinfo failure. %s:%s (%s)\n", hostname, portbuf,
                   gai_strerror (error)));
      return -1;
    }

  return 0;
}

int
_eXosip_srv_lookup(osip_transaction_t * tr, osip_message_t * sip, struct osip_srv_record *record)
{
	int use_srv=1;
	int port;
	char *host;
	osip_via_t *via;

	via = (osip_via_t *) osip_list_get (&sip->vias, 0);
	if (via == NULL || via->protocol == NULL)
		return -1;

	if (MSG_IS_REQUEST(sip))
	{
	    osip_route_t *route;

	    osip_message_get_route (sip, 0, &route);
	    if (route != NULL)
	    {
			osip_uri_param_t *lr_param = NULL;
			
			osip_uri_uparam_get_byname (route->url, "lr", &lr_param);
			if (lr_param == NULL)
				route = NULL;
		}
	    
	    if (route != NULL)
	    {
			port = 5060;
			if (route->url->port != NULL)
			{
				port = osip_atoi (route->url->port);
				use_srv=0;
			}
			host = route->url->host;
	    }
	    else
	    {
			port = 5060;
			if (sip->req_uri->port != NULL)
			{
				port = osip_atoi (sip->req_uri->port);
				use_srv=0;
			}
			host = sip->req_uri->host;
	    }
	}
	else
	{
		osip_generic_param_t *maddr;
		osip_generic_param_t *received;
		osip_generic_param_t *rport;

		osip_via_param_get_byname (via, "maddr", &maddr);
		osip_via_param_get_byname (via, "received", &received);
		osip_via_param_get_byname (via, "rport", &rport);
		if (maddr != NULL)
			host = maddr->gvalue;
		else if (received != NULL)
			host = received->gvalue;
		else
			host = via->host;

		if (via->port == NULL)
			use_srv=0;
		if (rport == NULL || rport->gvalue == NULL)
		{
			if (via->port != NULL)
				port = osip_atoi (via->port);
			else
				port = 5060;
		} else
			port = osip_atoi (rport->gvalue);
	}

	if (use_srv==1)
	{
		int i;
		i = _eXosip_get_srv_record(record, host, via->protocol);
	}
	return 0;
}


#ifdef WIN32

int
_eXosip_get_srv_record (struct osip_srv_record *record, char *domain, char *protocol)
{
	char zone[1024];
	PDNS_RECORD answer, tmp;      /* answer buffer from nameserver */
	int n;

	memset(record, 0, sizeof(struct osip_srv_record));
	if (strlen(domain)+strlen(protocol)>1000)
		return -1;

	osip_tolower(protocol);
	snprintf(zone, 1024, "_sip._%s.%s", protocol, domain);

	OSIP_TRACE (osip_trace
				(__FILE__, __LINE__, OSIP_INFO2, NULL,
				"About to ask for '%s IN SRV'\n", zone));

	if (DnsQuery (zone, DNS_TYPE_SRV, DNS_QUERY_STANDARD, NULL, &answer, NULL) != 0)
	{
		return -1;
    }

	n = 0;
	for (tmp = answer; tmp != NULL; tmp = tmp->pNext)
	{
		struct osip_srv_entry *srventry;
		DNS_SRV_DATA * data;
		if (tmp->wType != DNS_TYPE_SRV)
			continue;
		srventry = &record->srventry[n];
		data=&tmp->Data.SRV;
		snprintf(srventry->srv, sizeof(srventry->srv), "%s", data->pNameTarget);

		srventry->priority = data->wPriority;
		srventry->weight = data->wWeight;
		if (srventry->weight)
			srventry->rweight = 1 + rand () % (10000 * srventry->weight);
		else
			srventry->rweight = 0;
		srventry->port = data->wPort;

		OSIP_TRACE (osip_trace
					(__FILE__, __LINE__, OSIP_INFO2, NULL,
					"SRV record %s IN SRV -> %s:%i/%i/%i/%i\n",
					zone, srventry->srv, srventry->port, srventry->priority,
					srventry->weight, srventry->rweight));
		n++;
	}

	if (n==0)
		return -1;

	snprintf(record->name, sizeof(record->name), "%s", domain);
	return 0;
}

#elif defined(xxx)

/* the biggest packet we'll send and receive */
#if PACKETSZ > 1024
#define	MAXPACKET PACKETSZ
#else
#define	MAXPACKET 1024
#endif

/* and what we send and receive */
  typedef union
  {
    HEADER hdr;
    u_char buf[MAXPACKET];
  }
  querybuf;

#ifndef T_SRV
#define T_SRV		33
#endif

int
_eXosip_get_srv_record (struct osip_srv_record *record, char *domain, char *protocol)
{
	querybuf answer;              /* answer buffer from nameserver */
	int n;
	char *zone;
	int ancount, qdcount;         /* answer count and query count */
	HEADER *hp;                   /* answer buffer header */
	char hostbuf[256];
	unsigned char *msg, *eom, *cp;        /* answer buffer positions */
	int dlen, type, aclass, pref, weight, port;
	long ttl;
	int answerno;

	memset(record, 0, sizeof(struct osip_srv_record));
	if (strlen(domain)+strlen(protocol)>1000)
		return -1;

	osip_tolower(protocol);
	snprintf(zone, 1024, "_sip._%s.%s", protocol, domain);

	OSIP_TRACE (osip_trace
				(__FILE__, __LINE__, OSIP_INFO2, NULL,
				"About to ask for '%s IN SRV'\n", zone));

	n = res_query (zone, C_IN, T_SRV, (unsigned char *) &answer, sizeof (answer));

	if (n < (int) sizeof (HEADER))
	{
		return -1;
	}

	/* browse message and search for DNS answers part */
	hp = (HEADER *) answer;
	qdcount = ntohs (hp->qdcount);
	ancount = ntohs (hp->ancount);

	msg = (unsigned char *) answer;
	eom = (unsigned char *) answer + n;
	cp = (unsigned char *) answer + sizeof (HEADER);

	while (qdcount-- > 0 && cp < eom)
	{
		n = dn_expand (msg, eom, cp, (char *) hostbuf, 256);
		if (n < 0)
		{
			OSIP_TRACE (osip_trace
						(__FILE__, __LINE__, OSIP_ERROR, NULL,
						"Invalid SRV record answer for '%s': bad format\n", zone));
			return -1;
		}
		cp += n + QFIXEDSZ;
	}


	/* browse DNS answers */
	answerno = 0;

	/* loop through the answer buffer and extract SRV records */
	while (ancount-- > 0 && cp < eom)
	{
		struct osip_srv_entry *srventry;

		n = dn_expand (msg, eom, cp, (char *) hostbuf, 256);
		if (n < 0)
		{
			OSIP_TRACE (osip_trace
						(__FILE__, __LINE__, OSIP_ERROR, NULL,
						"Invalid SRV record answer for '%s': bad format\n", zone));
			return -1;
		}

		cp += n;


#if defined(__NetBSD__) || defined(__OpenBSD__) ||\
defined(OLD_NAMESER) || defined(__FreeBSD__)
		type = _get_short (cp);
		cp += sizeof (u_short);
#elif defined(__APPLE_CC__)
		GETSHORT (type, cp);
#else
		NS_GET16 (type, cp);
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__) ||\
defined(OLD_NAMESER) || defined(__FreeBSD__)
		aclass = _get_short (cp);
		cp += sizeof (u_short);
#elif defined(__APPLE_CC__)
		GETSHORT (aclass, cp);
#else
		NS_GET16 (aclass, cp);
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__) ||\
defined(OLD_NAMESER) || defined(__FreeBSD__)
		ttl = _get_long (cp);
		cp += sizeof (u_long);
#elif defined(__APPLE_CC__)
		GETLONG (ttl, cp);
#else
		NS_GET32 (ttl, cp);
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__) ||\
defined(OLD_NAMESER) || defined(__FreeBSD__)
		dlen = _get_short (cp);
		cp += sizeof (u_short);
#elif defined(__APPLE_CC__)
		GETSHORT (dlen, cp);
#else
		NS_GET16 (dlen, cp);
#endif

		if (type != T_SRV)
		{
			cp += dlen;
			continue;
		}
#if defined(__NetBSD__) || defined(__OpenBSD__) ||\
defined(OLD_NAMESER) || defined(__FreeBSD__)
		pref = _get_short (cp);
		cp += sizeof (u_short);
#elif defined(__APPLE_CC__)
		GETSHORT (pref, cp);
#else
		NS_GET16 (pref, cp);
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__) ||\
defined(OLD_NAMESER) || defined(__FreeBSD__)
		weight = _get_short (cp);
		cp += sizeof (u_short);
#elif defined(__APPLE_CC__)
		GETSHORT (weight, cp);
#else
		NS_GET16 (weight, cp);
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__) ||\
defined(OLD_NAMESER) || defined(__FreeBSD__)
		port = _get_short (cp);
		cp += sizeof (u_short);
#elif defined(__APPLE_CC__)
		GETSHORT (port, cp);
#else
		NS_GET16 (port, cp);
#endif

		n = dn_expand (msg, eom, cp, (char *) hostbuf, 256);
		if (n < 0)
			break;
		cp += n;

		srventry = &record->srventry[answerno];
		snprintf(srventry->srv, sizeof(srventry->srv), "%s", hostbuf);

		srventry->priority = pref;
		srventry->weight = weight;
		if (weight)
			srventry->rweight = 1 + random () % (10000 * srventry->weight);
		else
			srventry->rweight = 0;
		srventry->port = port;

		OSIP_TRACE (osip_trace
					(__FILE__, __LINE__, OSIP_INFO2, NULL,
					"SRV record %s IN SRV -> %s:%i/%i/%i/%i\n",
					zone, srventry->srv, srventry->port, srventry->priority,
					srventry->weight, srventry->rweight));

		answerno++;
	}

	if (answerno == 0)
		return -1;

	snprintf(record->name, sizeof(record->name), "%s", domain);
	return 0;
}

#else

int
_eXosip_get_srv_record (struct osip_srv_record *record, char *domain, char *protocol)
{
	return -1;
}

#endif
