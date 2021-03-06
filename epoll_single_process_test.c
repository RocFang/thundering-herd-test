#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

static int
create_and_bind (char *port)
{
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(atoi(port));
    bind(fd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
    return fd;
}

static int
make_socket_non_blocking (int sfd)
{
    int flags, s;
 
    flags = fcntl (sfd, F_GETFL, 0);
    if (flags == -1)
    {
        perror ("fcntl");
        return -1;
    }
 
    flags |= O_NONBLOCK;
    s = fcntl (sfd, F_SETFL, flags);
    if (s == -1)
    {
        perror ("fcntl");
        return -1;
    }
 
    return 0;
}

#define MAXEVENTS 64
 
int
main (int argc, char *argv[])
{
    int sfd, s;
    int efd;
    struct epoll_event event;
    struct epoll_event *events;
 
    sfd = create_and_bind("1234");
    if (sfd == -1)
        abort ();
 
    s = make_socket_non_blocking (sfd);
    if (s == -1)
        abort ();
 
    s = listen(sfd, SOMAXCONN);
    if (s == -1)
    {
        perror ("listen");
        abort ();
    }
 
    efd = epoll_create(MAXEVENTS);
    if (efd == -1)
    {
        perror ("epoll_create");
        abort ();
    }
 
    event.data.fd = sfd;
    event.events = EPOLLIN | EPOLLET;
    s = epoll_ctl (efd, EPOLL_CTL_ADD, sfd, &event);
    if (s == -1)
    {
        perror ("epoll_ctl");
        abort ();
    }
 
    /* Buffer where events are returned */
    events = calloc(MAXEVENTS, sizeof event);
 
    /* The event loop */
    while (1)
    {
        int n, i;
 
        n = epoll_wait (efd, events, MAXEVENTS, -1);
        for (i = 0; i < n; i++)
        {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN)))
            {
                /* An error has occured on this fd, or the socket is not
                ready for reading (why were we notified then?) */
                fprintf (stderr, "epoll error\n");
                close (events[i].data.fd);
                continue;
            }
            else if (sfd == events[i].data.fd)
            {
                /* We have a notification on the listening socket, which
                means one or more incoming connections. */
                while (1)
                {
                    struct sockaddr in_addr;
                    socklen_t in_len;
                    int infd;
                    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
 
                    in_len = sizeof in_addr;
                    infd = accept (sfd, &in_addr, &in_len);
                    if (infd == -1)
                    {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                        {
                            /* We have processed all incoming connections. */
                            break;
                        }
                        else
                        {
                            perror ("accept");
                            break;
                        }
                    }
 
                    s = getnameinfo (&in_addr, in_len, hbuf, sizeof hbuf, sbuf, sizeof sbuf,
                                      NI_NUMERICHOST | NI_NUMERICSERV);
                    if (s == 0)
                    {
                      printf("Accepted connection on descriptor %d "
                             "(host=%s, port=%s)\n", infd, hbuf, sbuf);
                    }
 
                    /* Make the incoming socket non-blocking and add it to the
                    list of fds to monitor. */
                    s = make_socket_non_blocking (infd);
                    if (s == -1)
                        abort ();
 
                    event.data.fd = infd;
                    event.events = EPOLLIN | EPOLLET;
                    s = epoll_ctl (efd, EPOLL_CTL_ADD, infd, &event);
                    if (s == -1)
                    {
                        perror ("epoll_ctl");
                        abort ();
                    }
                }
                continue;
            }
            else
            {
                /* We have data on the fd waiting to be read. Read and
                 display it. We must read whatever data is available
                 completely, as we are running in edge-triggered mode
                 and won't get a notification again for the same
                 data. */
                int done = 0;
 
                while (1)
                {
                    ssize_t count;
                    char buf[512];
 
                    count = read (events[i].data.fd, buf, sizeof buf);
                    if (count == -1)
                    {
                        /* If errno == EAGAIN, that means we have read all
                         data. So go back to the main loop. */
                        if (errno != EAGAIN)
                        {
                            perror ("read");
                            done = 1;
                        }
                        break;
                    }
                    else if (count == 0)
                    {
                        /* End of file. The remote has closed the
                         connection. */
                        done = 1;
                        break;
                    }
 
                    /* Write the buffer to standard output */
                    s = write (1, buf, count);
                    if (s == -1)
                    {
                        perror ("write");
                        abort ();
                    }
                }
 
                if (done)
                {
                    printf ("Closed connection on descriptor %d\n",
                        events[i].data.fd);
 
                    /* Closing the descriptor will make epoll remove it
                     from the set of descriptors which are monitored. */
                    close (events[i].data.fd);
                }
            }
        }
    }
 
    free (events);
    close (sfd);
 
    return EXIT_SUCCESS;
}
