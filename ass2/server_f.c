/*
 * Rooshil Patel
 * CMPUT 379
 * Fork server
 * This code is largely adapted from Bob Beck's server.c program
 * as well as from the lab examples.
 * Additional coding resources used from stackoverflow and other
 * online entites. Used code will be commented with a url from
 * where the code was taken from.
 */
 

/*
 * Copyright (c) 2008 Bob Beck <beck@obtuse.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>

#define BUFFER_SIZE 4096

char docdir[80];
char logdir[80];

static void usage();
static void kidhandler(int);
int write_client(int, char *);
void write_log(char *, char *, char *, FILE *, char *);
void client_request(int, char *, char *);
int read_request(int, char *);
int is_http_get(char *, char *, char *, char *);
void getlinec(char *, char *);
void getdir(char *, char *, char *);
int ok(int, char *, FILE *, char *);
void bad_request(int, char *);
void forbidden(int, char *);
void file_not_found(int, char *);
void internal_server_error(int, char *);


static void usage()
{
	extern char * __progname;
	fprintf(stderr, "usage: %s portnumber filedirectory logfile\n", __progname);
	exit(1);
}

static void kidhandler(int signum) 
{
	/* signal handler for SIGCHLD */
	waitpid(WAIT_ANY, NULL, WNOHANG);
}


int main(int argc,  char *argv[])
{
	struct sockaddr_in sockname, client;
	struct sigaction sa;
	struct tm* time_struct;
	FILE * logfile;
	socklen_t clientlen;
        int sd;
	char timec[80];
	char dircopy[80];
	char *ep;
	time_t rawtime;
	u_short port;
	pid_t pid;
	u_long p;


	/*
	 * first, figure out what port we will listen on - it should
	 * be our first parameter.
	 */

	if (argc != 4) {
		usage();
		errno = 0;
	}
        p = strtoul(argv[1], &ep, 10);
        if (*argv[1] == '\0' || *ep != '\0') {
		/* parameter wasn't a number, or was empty */
		fprintf(stderr, "%s - not a number\n", argv[1]);
		usage();
	}
        if ((errno == ERANGE && p == ULONG_MAX) || (p > USHRT_MAX)) {
		/* It's a number, but it either can't fit in an unsigned
		 * long, or is too big for an unsigned short
		 */
		fprintf(stderr, "%s - value out of range\n", argv[1]);
		usage();
	}
	
	strncpy(dircopy, argv[2], sizeof(dircopy));
	
	/* Checking if the directory exists.
	 * adapted from
	 * http://stackoverflow.com/a/12510903
	 */
	if (opendir(dircopy) == NULL) {
		fprintf(stderr, "Directory does not exist.\n");
		usage();
	}

	/* 
	 * Check to make sure logfile can open and does exist 
	 */
	logfile = fopen(argv[3], "a");

	if (logfile == NULL) {
		fprintf(stderr, "Error opening logfile.\n");
		usage();
	}
	fclose(logfile);

	/* used strncpy as I found out assignment is not possible */

	strncpy(docdir, argv[2], sizeof(docdir));
	strncpy(logdir, argv[3], sizeof(logdir));

	/* now safe to do this */

	if (daemon(1, 0) == -1) {
		fprintf(stderr, "Could not daemonize.\n");
		usage();
	}

	port = p;

	memset(&sockname, 0, sizeof(sockname));
	sockname.sin_family = AF_INET;
	sockname.sin_port = htons(port);
	sockname.sin_addr.s_addr = htonl(INADDR_ANY);
	sd=socket(AF_INET,SOCK_STREAM,0);
	if ( sd == -1)
		err(1, "socket failed");

	if (bind(sd, (struct sockaddr *) &sockname, sizeof(sockname)) == -1)
		err(1, "bind failed");

	if (listen(sd,3) == -1)
		err(1, "listen failed");

	/*
	 * we're now bound, and listening for connections on "sd" -
	 * each call to "accept" will return us a descriptor talking to
	 * a connected client
	 */


	/*
	 * first, let's make sure we can have children without leaving
	 * zombies around when they die - we can do this by catching
	 * SIGCHLD.
	 */
	sa.sa_handler = kidhandler;
        sigemptyset(&sa.sa_mask);
	/*
	 * we want to allow system calls like accept to be restarted if they
	 * get interrupted by a SIGCHLD
	 */
        sa.sa_flags = SA_RESTART;
        if (sigaction(SIGCHLD, &sa, NULL) == -1)
                err(1, "sigaction failed");

	/*
	 * finally - the main loop.  accept connections and deal with 'em
	 */
	for(;;) {
		int clientsd;
		clientlen = sizeof(&client);
		clientsd = accept(sd, (struct sockaddr *)&client, &clientlen);
		if (clientsd == -1)
			err(1, "accept failed");
		/*
		 * We fork child to deal with each connection, this way more
		 * than one client can connect to us and get served at any one
		 * time.
		 */

		pid = fork();
		if (pid == -1)
		     err(1, "fork failed");

		if(pid == 0) 
		{
			/* 
			 * Using INET to determine client ip 
			 * code adapted from
			 * http://stackoverflow.com/a/3060988
			 */
			char clientip[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &(client.sin_addr), clientip, INET_ADDRSTRLEN);

			/*
			 * Getting current time using time library
			 * code adapted from
			 * http://stackoverflow.com/a/10332099
			 */
			time(&rawtime);
			time_struct = localtime(&rawtime);
			strftime(timec, 80, "%a, %d %b %Y %X %Z", time_struct);
			client_request(clientsd, timec, clientip);
			exit(0);
		}
		close(clientsd);
	}
}

/*
 * write code adapted from Bob Beck's orignal writing method
 */
int write_client(int clientsd, char *clientmsg)
{
	ssize_t written, w;
	/*
	 * write the message to the client, being sure to
	 * handle a short write, or being interrupted by
	 * a signal before we could write anything.
	 */
	w = 0;
	written = 0;
	while (written < strlen(clientmsg)) {
		w = write(clientsd, clientmsg + written,
			strlen(clientmsg) - written);
		if (w == -1) {
			if (errno != EINTR)
				err(1, "write failed");
		}
		else
			written += w;
	}
	return written;
}

void write_log(char *timec, char *line, char *response, FILE *logfile, char *clientip) 
{
	fprintf(logfile, "%s\t%s\t%s\t%s\n", timec, clientip, line, response);
	fclose(logfile);
}

void client_request(int clientsd, char * timec, char * clientip) 
{
	FILE * requestfile;
	FILE * logfile;
	char read_output[BUFFER_SIZE] = {0};
	char method[80];
	char dir[80];
	char http[80];
	char line[100];
	char finaldir[BUFFER_SIZE] = {0};
	struct stat buf;
	int httpint;
	int readint;
	int totalsize;
	char totalsizec[20];
	int totalwrite;
	char totalwritec[20];
	char okstr[80];

	logfile = fopen(logdir, "a");

	readint = read_request(clientsd, read_output);
	
	if (readint == -1)
	{
		internal_server_error(clientsd, timec);
		return;
	}

	getlinec(read_output, line);
	httpint = is_http_get(line, method, dir, http);
	
	if (httpint == -1)
	{
		bad_request(clientsd, timec);
		write_log(timec, line, "400 Bad Request", logfile, clientip);
		return;
	}
	
	getdir(docdir, dir, finaldir);

	requestfile = fopen(finaldir, "r");
	
	if (requestfile == NULL)
	{
		file_not_found(clientsd, timec);
		write_log(timec, line, "404 Not Found", logfile, clientip);
		return;
	}

	if (errno = EACCES)
	{
		forbidden(clientsd, timec);
		write_log(timec, line, "403 Forbidden", logfile, clientip);
		return;
	}
	/*
	 * Gets file size from buffer file to be used as total write size
	 * for ok response
	 *
	 * Code adapted from
	 * http://stackoverflow.com/a/238644
	 */
	fstat(requestfile, &buf);
	totalsize = buf.st_size;
	/*
	 * Converts int to char
	 *
	 * Code adapted from
	 * http://stackoverflow.com/a/1114752
	 */
	sprintf(totalsizec, "%d", totalsize);
	totalwrite = ok(clientsd, timec, requestfile, totalsizec);
	sprintf(totalwritec, "%d", totalwrite);
	strncat(okstr, "200 OK ", sizeof(okstr));	
	strncat(okstr, totalwritec, sizeof(okstr));
	strncat(okstr, "/", sizeof(okstr));
	strncat(okstr, totalsizec, sizeof(okstr));
	write_log(timec, line, okstr, logfile, clientip);
	return;

}

int read_request(int clientsd, char * read_output) 
{
	int reader, readcount, still_reading, i;
	reader = 0;
	readcount = 0;
	still_reading = 1;
	
	/*
	 * Traverses the information from clientsd and appends it into a buffer
	 * until there is no additional information to be written or a newline
	 * character is found
	 *
	 * Intially checked for 2 new line characters but the buffer would not
	 * get all the information sent to it. Now with checking for only
	 * one newline, it seems to work fine.
	 *
	 * Function returns a code based on the value of reader in order to be
	 * used to check for internal server errors
	 */

	while ((readcount < BUFFER_SIZE -1) && still_reading)
	{
		reader = read(clientsd, read_output + readcount, (BUFFER_SIZE - 1) - readcount);
		for (i = 0; i < strlen(read_output); i++)
		{
			if (read_output[i] == '\n')
			{
				still_reading = 0;
				break;
			}
		}
		if (reader == -1)
		{
			if (errno != EINTR)
				return reader;
		}
		else
			readcount += reader;
	}
	reader = 1;
	read_output[readcount] = '\0';
	return reader;
}

/*
 * Splits input into three outputs: method, directory and http
 */
int is_http_get(char *read_output, char *method, char *dir, char *http)
{
	sscanf(read_output, "%s %s %s", method, dir, http);
	if (strcmp(method, "GET"))
		return -1;
	if (strcmp(http, "HTTP/1.1"))
		return -1;
	return 1;
}
/* 
 * copies the first line from read_ouput into line
 */
void getlinec(char *read_output, char *line)
{
	int i;
	for (i = 0; i < strlen(read_output); i++)
	{
		if (read_output[i] == "\n")
			break;
		line[i] = read_output[i];
	}
}

void getdir(char * docdir, char * dir, char * finaldir)
{
	dir++;
	strncpy(finaldir, docdir, sizeof(finaldir));
	strncat(finaldir, dir, sizeof(finaldir));
}

/*
 * Returns okay response and outputs file to client
 */
int ok(int clientsd, char *timec, FILE *requestfile, char * totalsizec)
{
	ssize_t totalwrite, buf;
	char buffer[BUFFER_SIZE] = {0};
	totalwrite = 0;
	buf = 0;

	write_client(clientsd, "HTTP/1.1 200 OK\n");
	write_client(clientsd, "Date: ");
	write_client(clientsd, timec);
	write_client(clientsd, "\nContent-Type: text/html\n");
	write_client(clientsd, "Content-Length: ");
	write_client(clientsd, totalsizec);
	write_client(clientsd, "\n\n");

	while (fgets(buffer, BUFFER_SIZE, requestfile) != NULL)
	{
		buf = write_client(clientsd, buffer);
		if (buf == -1)
		{
			return totalwrite;
		}
		totalwrite += buf;
	}
	return totalwrite;
}

void bad_request(int clientsd, char *timec)
{
	write_client(clientsd, "HTTP/1.1 400 Bad Request\n");
	write_client(clientsd, "Date: ");
	write_client(clientsd, timec);
	write_client(clientsd, "\nContent-Type: text/html\n");
	write_client(clientsd, "Content-Length: 107\n\n");
	write_client(clientsd, "<html><body>\n<h2>Malformed Request</h2>\n");
	write_client(clientsd, "Your broswer sent a request I could not understand.\n");
	write_client(clientsd, "</body></html>");

}

void forbidden(int clientsd, char *timec)
{
	write_client(clientsd, "HTTP/1.1 403 Forbidden\n");
	write_client(clientsd, "Date: ");
	write_client(clientsd, timec);
	write_client(clientsd, "\nContent-Type: text/html\n");
	write_client(clientsd, "Content-Length: 130\n\n");
	write_client(clientsd, "<html><body>\n<h2>Permission Denied</h2>\n");
	write_client(clientsd, "You asked for a document you are not permitted to see. It sucks to be you.\n");
	write_client(clientsd, "</body></html>");
}

void file_not_found(int clientsd, char *timec)
{
	write_client(clientsd, "HTTP/1.1 404 Not Found\n");
	write_client(clientsd, "Date: ");
	write_client(clientsd, timec);
	write_client(clientsd, "\nContent-Type: text/html\n");
	write_client(clientsd, "Content-Length: 117\n\n");
	write_client(clientsd, "<html><body>\n<h2>Document not found</h2>\n");
	write_client(clientsd, "You asked for a document that doesn't exist. That is so sad.\n");
	write_client(clientsd, "</body></html>");
}

void internal_server_error(int clientsd, char *timec)
{
	write_client(clientsd, "HTTP/1.1 500 Internal Server Error\n");
	write_client(clientsd, "Date: ");
	write_client(clientsd, timec);
	write_client(clientsd, "\nContent-Type: text/html\n");
	write_client(clientsd, "Content-Length: 131\n\n");
	write_client(clientsd, "<html><body>\n<h2>That Didn't work</h2>\n");
	write_client(clientsd, "I had some sort of problem dealing with your request. Sorry, I'm lame.\n");
	write_client(clientsd, "</body></html>");
}

