/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   mini_serv.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: xmatute- <xmatute-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/07/04 16:56:50 by xmatute-          #+#    #+#             */
/*   Updated: 2024/07/13 13:50:24 by xmatute-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

// in main.c
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
//
#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>

//select variables
int nfds = 0; // number of file descriptors

fd_set readfds;

fd_set writefds;

fd_set fds; // all file descriptors

// id management
int nids = 0; // number of ids
int ids[FD_SETSIZE]; // pseudo map

void    send_message(char *msg, int fd)
{
    // printf("sending: %s?\n", msg);
    if (FD_ISSET(fd, &writefds))
        send(fd, msg, strlen(msg), 0);
}

void    broadcast_message(char *msg, int id)
{
    // printf("broadcasting: %s\n", msg);
    for (int fd = 0; fd <= nfds; fd++)
    {
        if (id != ids[fd])
            send_message(msg, fd);        
    }
    
}

void broadcast_entry_message(int id)
{
    char msg[42];

    sprintf(msg, "server: client %d just arrived\n", id);
    broadcast_message(msg, id);
}

void broadcast_exit_message(int id)
{
    char msg[42];

    sprintf(msg, "server: client %d just left\n", id);
    broadcast_message(msg, id);
}

int extract_message(char **buf, char **msg) // (from main.c) // similar to get_next_line extract the first line of the buffer and "return" it in msg, it actually return 1 if buf is not empty and 0 if it is empty
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add) // (from main.c) // similar to strjoin, it concatenate add to buf and return the new buffer (freeing the old one)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

char *recive_message(int fd)
{
    char    buf[64];
    char    *msg = 0;
    size_t  rb;

    bzero(buf, sizeof(buf));
    do
    {
        rb = recv(fd, buf, sizeof(buf), 0);
        msg = str_join(msg, buf);
        printf("msg: %s\n", msg);
    } while (rb > 0);
    return (msg);
}

void error(char *msg) // print the error message(followed by a new line) and exit with status 1
{
    write(2, msg, strlen(msg));
    write(2, "\n", 1);
    exit(1);
}

void argc_error() // "Wrong number of arguments"
{
    error("Wrong number of arguments");
}

void fatal_error() // "Fatal error"
{
    error("Fatal error");
}

int mini_serv(int port) // mainly main.c copypaste
{
	int sockfd, connfd;
    socklen_t   len; // needed because flags
	struct sockaddr_in servaddr, cli;

    char buf[424242];

	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) { 
		fatal_error();		
	}
	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	// servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
    servaddr.sin_addr.s_addr = htonl((10 << 24) | (13 << 16) | (5 << 8) | 4);
	servaddr.sin_port = htons(port); // PUT PORT HERE 
  
	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) { 
        fatal_error();
    }
	if (listen(sockfd, 10) != 0) {
        fatal_error();
	}

    //init select variables
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&fds);
    nfds = sockfd;
    FD_SET(sockfd, &fds);
    // tv dont needed

    // init ids
    memset(ids, -1, sizeof(ids));
    nids = 0;

    while (42)
    {
        // copy fds to readfds (needed because select modify the set (not cool))
        readfds = fds;
        // same for writefds
        writefds = fds;
        connfd = select(nfds + 1, &readfds, &writefds, 0, 0); //waitting for something to happen?
        if (connfd < 1) // error/timeout
            continue;
        for (int fd = 0; fd <= nfds; fd++) // check all monitoreable fd
        {
            if (!FD_ISSET(fd, &readfds)) // sknfd if not ready to read
                continue;
            if (fd == sockfd) // new client
            {
                // accept new client
                len = sizeof(cli);
                int nfd = accept(sockfd, (struct sockaddr *)&cli, &len);
                int id = nids++;
                if (nfd < 0) // error
                    continue;
                // add client
                FD_SET(nfd, &fds); // add to monitoreable fds
                if (nfd > nfds) // update nfds
                    nfds = nfd;
                ids[nfd] = id; // add to ids
                broadcast_entry_message(id);
                continue;
            }
            bzero(buf, sizeof(buf));
            if (!recv(fd, buf, sizeof(buf), 0)) // client disconnected
            {
                // remove client
                FD_CLR(fd, &fds); // remove from monitoreable fds
                close(fd); // close fd
                int id = ids[fd];
                broadcast_exit_message(id);
                ids[fd] = -1; // remove from ids
                continue;
            }
            else
            {
                // process message
                int     id = ids[fd];
                char    *msg = 0;
                char    pmsg[42];
                sprintf(pmsg, "client %d: ", id);
                char   *dbuf = str_join(0, buf);
                int  c = extract_message(&dbuf, &msg);
                while (c)
                {
                    broadcast_message(pmsg, id); // broadcast pre message
                    broadcast_message(msg, id); // broadcast message
                    free(msg); // free message
                    c = extract_message(&dbuf, &msg);
                }
            }
        }
        
    }

}

int main(int argc, char const *argv[]) // imput verification and process
{
    if (argc < 2)
        argc_error();
    return mini_serv(atoi(argv[1]));
}

