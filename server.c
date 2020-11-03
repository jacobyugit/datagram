/* 
 * A simple datagram/UDP server in the Internet domain.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define SERVER_PORT			"4357"

/*
 * User queries
 */
#define STORE_TEMP			(1)
#define READ_TEMP			(2)

/*
 * Messages
 */
#define READ_TEMP_RESULT	(3)
#define CITY_NOT_FOUND		(4)
#define ERROR_IN_INPUT		(9)

#define	MAX_TEMP			(150)

int							sock_fd;
socklen_t					client_addrlen;
struct sockaddr_storage		client_addr;

struct message {
	long	message_id;
	char	city [100];
	char	temperature [16];	// Temperature degrees in Celcius 
};	// message

struct message	recv_message, send_message;

struct tnode {
	char	*city;
	double	temperature;      	// Temperature degrees in Celcius
	struct	tnode *left;
	struct	tnode *right;
};

/*
 * Error message handling (& exit)
 */
void
err_msg( char *msg )
{
    perror( msg );
	exit( EXIT_FAILURE );
}	/* err_msg */


/*
 * Record temperature of a city in a tree structure
 */
struct tnode *
add_to_tree( struct tnode *p, char *city, double temperature )
{	
    int res;

    if ( p == NULL ) {								// new entry
        if ( (p = (struct tnode *)malloc(sizeof(struct tnode))) == NULL )
			err_msg( "ERROR on malloc" );

        p->city				= strdup(city);
        p->temperature		= temperature;
        p->left = p->right	= NULL;
    }
    else if ( (res = strcmp(city, p->city)) == 0 )	// entry exists
        p->temperature		= temperature;
    else if ( res < 0 )								// less than city for this node, put in left subtree
        p->left				= add_to_tree( p->left, city, temperature );
    else											// greater than city for this node, put in right subtree
        p->right			= add_to_tree( p->right, city, temperature );

	return( p );
}	/* add_to_tree */


/*
 * Find node from binary tree for the city whose temperature is queried
 */
struct tnode *
find_city_rec( struct tnode *p, char *city )
{
    int	res;

    if ( p == NULL ) {
        send_message.message_id		= htonl (CITY_NOT_FOUND);
        strcpy( send_message.city, city );
        send_message.temperature[0]	= '\0';

        if ( sendto(sock_fd, &send_message.message_id, sizeof(struct message), 0,
				(struct sockaddr *)&client_addr, client_addrlen) == -1 )
			err_msg( "ERROR on sendto" );

        return( NULL );
    }	/* if */
    else if ( (res = strcmp(city, p->city)) == 0 ) {	// entry exists
        send_message.message_id	= htonl( READ_TEMP_RESULT );
        strcpy( send_message.city, p->city );
        sprintf( send_message.temperature, "%4.1lf", p->temperature );

        if ( sendto(sock_fd, &send_message, sizeof(struct message), 0,
				(struct sockaddr *)&client_addr, client_addrlen) == -1 )
			err_msg("ERROR on sendto" );

        return( p );
    }	/* else if */
    else if ( res < 0 )		// less than city for this node, search left subtree
        p->left		= find_city_rec( p->left, city );
    else					// greater than city for this node, search right subtree
        p->right	= find_city_rec( p->right, city );
}	/* find_city_rec */


/*
 * Print the tree (in-order traversal)
 */
void
print_tree( struct tnode *p )
{
    if ( p != NULL ) {
        print_tree( p->left );
        printf( "%s: %4.1lf\n\n", p->city, p->temperature );
        print_tree( p->right );
    }
}	/* print_tree */


int
main( int argc, char **argv )
{
    int					s;
    ssize_t				numbytes;
    struct tnode		*root = NULL;
    struct addrinfo		*result, *rptr, hints;
    const char * const	ident = "temperature-server";

	// Set up system logger - identity header, system console, caller's PID and stderr
    openlog( ident, LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER );
    syslog( LOG_USER | LOG_INFO, "%s", "Hello world!" );
    
	/*
	 * Set up host and proper socket for binding
	 */

	// Retrieve desired addrinfo structure(s) for socket binding
    memset( &hints, 0, sizeof(struct addrinfo) );

    hints.ai_family		= AF_UNSPEC;	// Allow IPv4 or IPv6
    hints.ai_socktype	= SOCK_DGRAM;	// Use Datagram socket
    hints.ai_flags		= AI_PASSIVE;	// For wildcard IP address

    if ( (s = getaddrinfo(NULL, SERVER_PORT, &hints, &result)) != 0 ) {
        fprintf( stderr, "getaddrinfo: %s\n", gai_strerror(s) );
        exit( EXIT_FAILURE );
    }	/* if */

    // Scan through the list of address structures returned by "getaddrinfo".  Stop when the socket
	// and "bind" calls are successful.
    for ( rptr = result ; rptr != NULL ; rptr = rptr->ai_next ) {
        if ( (sock_fd = socket(rptr->ai_family, rptr->ai_socktype, rptr->ai_protocol)) == -1 ) continue;

        if ( bind(sock_fd, rptr->ai_addr, rptr->ai_addrlen) == 0 ) break;	// Success

        if ( close(sock_fd) == -1 ) err_msg( "ERROR on close" );
    }	/* for */

    if ( rptr == NULL ) {               // Not successful with any address
        fprintf( stderr, "Not able to bind\n" );
        exit( EXIT_FAILURE );
    }

    freeaddrinfo( result );

	/*
	 * Enter server loop ...
	 */
    while ( 1 ) {
         double temperature;

         client_addrlen = sizeof( struct sockaddr_storage );
         if ( (numbytes = recvfrom(sock_fd, &recv_message, sizeof(struct message), 0,
                        (struct sockaddr *) &client_addr, &client_addrlen)) == -1 )
             err_msg( "ERROR on recvfrom" );

         switch( ntohl(recv_message.message_id) ) {
             case STORE_TEMP :
				sscanf( recv_message.temperature, "%lf", &temperature );
				if ( strlen(recv_message.city) > 99 || temperature > MAX_TEMP ) {
					send_message.message_id		= htonl( ERROR_IN_INPUT );
					send_message.city[0]		= '\0';
					send_message.temperature[0]	= '\0';
					if ( sendto(sock_fd, &send_message.message_id, sizeof(struct message), 0,
							(struct sockaddr *)&client_addr, client_addrlen) == -1 )
						err_msg( "ERROR on sendto" );
				}

				root = add_to_tree( root, recv_message.city, temperature );

                break;

			case READ_TEMP :
				if ( strlen(recv_message.city) > 99 ) {
					send_message.message_id		= htonl( ERROR_IN_INPUT );
					send_message.city [0]		= '\0';
					send_message.temperature[0]	= '\0';
					if ( sendto(sock_fd, &send_message.message_id, sizeof(struct message), 0,
							(struct sockaddr *)&client_addr, client_addrlen) == -1 )
						err_msg( "ERROR on sendto" );
				}	/* if */

				struct tnode *tmp = find_city_rec( root, recv_message.city );

				break;

             default:
				fprintf( stderr, "Unknown message type\n" );
         }	/* switch */
    }	/* while */

    syslog( LOG_USER | LOG_INFO, "%s", "Bye." );
    closelog();

    exit( EXIT_SUCCESS );
}	/* main */
