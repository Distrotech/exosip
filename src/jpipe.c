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

#ifdef NEW_TIMER

#include "jpipe.h"

#ifndef WIN32

jpipe_t * jpipe ()
{
  jpipe_t *my_pipe = (jpipe_t *) malloc (sizeof (jpipe_t));

  if (0 != pipe (my_pipe->pipes))
    {
      free (my_pipe);
      return NULL;
    }
  return my_pipe;
}

int jpipe_close (jpipe_t * apipe)
{
  if (apipe == NULL)
    return -1;
  close (apipe->pipes[0]);
  close (apipe->pipes[1]);
  free (apipe);
  return 0;
}


/**
 * Write in a pipe.
 */
int
jpipe_write (jpipe_t * apipe, const void *buf, int count)
{
  if (apipe == NULL)
    return -1;
  return write (apipe->pipes[1], buf, count);
}

/**
 * Read in a pipe.
 */
int jpipe_read (jpipe_t * apipe, void *buf, int count)
{
  if (apipe == NULL)
    return -1;
  return read (apipe->pipes[0], buf, count);
}

/**
 * Get descriptor of reading pipe.
 */
int jpipe_get_read_descr (jpipe_t * apipe)
{
  if (apipe == NULL)
    return -1;
  return apipe->pipes[0];
}

#else

jpipe_t * jpipe ()
{
  jsocket_t s = 0;
  int timeout = 0;
  static int aport = 10500;
  struct sockaddr_in raddr;
  int j;

  jpipe_t *my_pipe = (jpipe_t *) malloc (sizeof (jpipe_t));
  if (my_pipe==NULL)
    return NULL;

  s = (int) socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (0 > s)
    {
      free (my_pipe);
      return NULL;
    }
  my_pipe->pipes[1] = (int) socket (PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (0 > my_pipe->pipes[1])
    {
      jsocket_close (s);
      free (my_pipe);
      return NULL;
    }

  raddr.sin_addr.s_addr = inet_addr ("127.0.0.1");
  raddr.sin_family = AF_INET;

  j = 50;
  while (aport++ && j-- > 0)
    {
      raddr.sin_port = htons ((short) aport);
      if (jsocket_bind
	  (s, (struct sockaddr *) &raddr, sizeof (raddr)) < 0)
	{
	  OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_WARNING, NULL,
				  "Failed to bind one local socket %i!\n",
				  aport));
	}
      else
	break;
    }

  if (j == 0)
    {
      OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_ERROR, NULL,
			      "Failed to bind a local socket, aborting!\n"));
      jsocket_close (s);
      jsocket_close (my_pipe->pipes[1]);
      free (my_pipe);
      exit (0);
    }

  j = listen(s,1);
  if (j != 0)
    {
      OSIP_TRACE (osip_trace (__FILE__, __LINE__, OSIP_ERROR, NULL,
			      "Failed to listen on a local socket, aborting!\n"));
      jsocket_close (s);
      jsocket_close (my_pipe->pipes[1]);
      free (my_pipe);
      exit (0);
    }

  j = setsockopt (my_pipe->pipes[1],
		  SOL_SOCKET,
		  SO_RCVTIMEO, (const char*) &timeout, sizeof (timeout));
  if (j != NO_ERROR)
    {
      /* failed for some reason... */
      OSIP_TRACE (osip_trace
		  (__FILE__, __LINE__, OSIP_ERROR, NULL,
		   "udp plugin; cannot set O_NONBLOCK to the file desciptor!\n"));
      jsocket_close (s);
      jsocket_close (my_pipe->pipes[1]);
      free (my_pipe);
      exit (0);
    }

  connect (my_pipe->pipes[1], (struct sockaddr *) &raddr, sizeof (raddr));

  my_pipe->pipes[0] = accept (s, NULL, NULL);

  if (my_pipe->pipes[0]<=0)
    {
      OSIP_TRACE (osip_trace
		  (__FILE__, __LINE__, OSIP_ERROR, NULL,
		   "udp plugin; Failed to call accept!\n"));
      jsocket_close (s);
      jsocket_close (my_pipe->pipes[1]);
      free (my_pipe);
      exit (0);
    }

  return my_pipe;
}

int jpipe_close (jpipe_t * apipe)
{
  if (apipe == NULL)
    return -1;
  jsocket_close (apipe->pipes[0]);
  jsocket_close (apipe->pipes[1]);
  free (apipe);
  return 0;
}


/**
 * Write in a pipe.
 */
int
jpipe_write (jpipe_t * apipe, const void *buf, size_t count)
{
  if (apipe == NULL)
    return -1;
  return send (apipe->pipes[1], buf, count, 0);
}

/**
 * Read in a pipe.
 */
int jpipe_read (jpipe_t * apipe, void *buf, size_t count)
{
  if (apipe == NULL)
    return -1;
  return recv (apipe->pipes[0], buf, count, 0 /* MSG_DONTWAIT */ );	/* BUG?? */
}

/**
 * Get descriptor of reading pipe.
 */
int jpipe_get_read_descr (jpipe_t * apipe)
{
  if (apipe == NULL)
    return -1;
  return apipe->pipes[0];
}

#endif

#endif
