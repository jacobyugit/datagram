/* 
 * A simple datagram/UDP client in the Internet domain.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
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
#define QUIT				(0)

/*
 * Messages
 */
#define READ_TEMP_RESULT	(3)
#define CITY_NOT_FOUND		(4)
#define ERROR_IN_INPUT		(9)

#define	MAX_TEMP			(150)

struct addrinfo				*rptr;
int							sock_fd;

pthread_t					client_send_thread, client_receive_thread;

struct message {
	long	message_id;
	char	city [100];
	char	temperature [16];   // Temperature degrees in Celcius 
};	// message

/*
 * Error messasge handling (& exit)
 */
void
err_msg( char *msg )
{
	perror( msg );
	exit( EXIT_FAILURE );
}	/* err_msg */


/*
 * Handle user options
 */
int
get_option( char *city, char *temperature )
{
	double	temp;
	int		option;
	char	buffer[10];

	while ( 1 ) {
		// Display menu
		printf( "1: Store temperature\n" );
		printf( "2: Read temperature\n" );
		printf( "0: Quit\n\n" );

		// Get user request
		printf( "Your option: " );
		if ( fgets(buffer, 10, stdin) == NULL ) err_msg( "ERROR on fgets in get_option" );
		sscanf( buffer, "%d", &option );

		// Process user request
		if ( option == 0 ) return( option );	// Quit
		else if ( option == 1 ) {				// Store temperature
			// Get city, temperature
			printf( "City: " );
			if ( fgets(city, 100, stdin) == NULL ) 
				err_msg( "ERROR on fgets in get_option" );

			int len = strlen( city );
			if ( city[len - 1] == '\n' ) city[len - 1] = '\0';

			printf( "Temperature: " );
			if ( fgets(buffer, 10, stdin) == NULL ) err_msg( "ERROR on fgets in get_option" );
 			sscanf( buffer, "%lf", &temp );
			if ( temp > MAX_TEMP ) {
				printf( "\nError in temperature, try again\n\n" );
				continue;
			}	/* if */
 			sprintf( temperature, "%4.1lf", temp );

			return( option );
		}	/* else if */
        else if ( option == 2 ) {
			// Get city
			printf( "City: " );
			if ( fgets(city, 100, stdin) == NULL ) err_msg( "ERROR on fgets in get_option" );

			// Add EOL
			int	len = strlen( city );
 			if ( city[len - 1] == '\n' ) city[len - 1] = '\0';

			return( option );
		}	/* 2nd else if */
		else	printf( "\nInvalid Option, try again\n\n" );
	}	/* while */
}	/* get_option */


/*
 * Client sending thread
 */
void *
client_send( void *arg )
{
	int				option;
	struct message	message;

	/*
	 * Forever loop
	 */
	while ( 1 ) {
		// Get user option
		option = get_option( message.city, message.temperature );

		// Process user option
		if ( option == QUIT )	break;
		else if ( option == STORE_TEMP ) {
			message.message_id = htonl( STORE_TEMP );
			if ( sendto(sock_fd, &message, sizeof(struct message), 0, rptr->ai_addr, rptr->ai_addrlen) == -1 )
				err_msg( "ERROR on sendto in client_send thread" );
		}	/* else if */
		else if ( option == READ_TEMP ) {
			message.message_id = htonl(READ_TEMP);
			if ( sendto(sock_fd, &message, sizeof(struct message), 0, rptr->ai_addr, rptr->ai_addrlen) == -1 )
				err_msg( "ERROR on sendto in client_send thread" );
		}	/* 2nd else if */
	}	/* while */

	if ( pthread_cancel(client_receive_thread) != 0 ) err_msg( "ERROR on pthread_cancel in client_send thread" );

	return( NULL );
}	/* client_send */


/*
 * Client receiving thread
 */
void *
client_receive( void *arg )
{
	struct	message	message;
	ssize_t			numbytes;

	/*
	 * Forever loop
	 */
	while ( 1 ) {
		// Recieve response from the datagram server
		if ( (numbytes = recvfrom(sock_fd, &message, sizeof(struct message), 0, NULL, NULL)) == -1 )
			err_msg( "ERROR on recvfrom in client_receive thread" );

		// Process the response
		switch ( ntohl(message.message_id) ) {
			case READ_TEMP_RESULT :
				printf( "\n(Receive): City: %s Temperature: %s\n\n", message.city, message.temperature );
 				break;

			case CITY_NOT_FOUND :
				printf( "\n(Receive): City: %s, not found.\n\n", message.city );
				break;

			case ERROR_IN_INPUT :
				printf( "\n(Receive): Error in message sent to the server\n\n" );
				break;

			default :
				fprintf( stderr, "\n(Receive): Unexpected message\n\n" );
		}	/* switch */
	}	/* while */
}	/* client_receive */


int
main ( int argc, char **argv )
{
	int					s; 
	struct addrinfo		hints;
	struct addrinfo *	result;

	/*
	 * Mandate server's "hostname" parameter
	 */
	if ( argc != 2 ) {
		fprintf( stderr, "Usage: %s hostname\n", argv[0] );
		exit( EXIT_FAILURE );
	}	/* if */

	/*
	 * Set up host and make socket
	 */

	// Retrieve desired addrinfo structure(s) for socket connection
	memset( &hints, 0, sizeof(struct addrinfo) );

	hints.ai_family		= AF_UNSPEC;	// Allow IPv4 or IPv6
	hints.ai_socktype	= SOCK_DGRAM;	// Use Datagram socket

	if ( (s = getaddrinfo(argv[1], SERVER_PORT, &hints, &result)) != 0 ) {
		fprintf( stderr, "getaddrinfo: %s\n", gai_strerror(s) );
		exit( EXIT_FAILURE );
	}	/* if */

	// Scan through the list of address structures returned by "getaddrinfo". Stop when the the
	// socket and "connect" calls are successful.
	for ( rptr = result ; rptr != NULL ; rptr = rptr->ai_next ) {
		if ( (sock_fd = socket(rptr->ai_family, rptr->ai_socktype, rptr->ai_protocol)) == -1 ) continue;

		break;
	}	/* for */

	if ( rptr == NULL ) {
		fprintf( stderr, "Not able to make socket\n" );
		exit( EXIT_FAILURE );
	}	/* if */

	freeaddrinfo( result );

    /*
	 * Prepare client's pthreads for sending and receiving
	 */
	if ( pthread_create(&client_receive_thread, NULL, client_receive, NULL) != 0 )
		err_msg( "ERROR on pthread_create: client_receive_thread" );

	if ( pthread_create(&client_send_thread, NULL, client_send, NULL) != 0 )
		err_msg( "ERROR on pthread_create: client_send_thread" );

    // Wait for threads to complete
	if ( pthread_join(client_receive_thread, NULL) != 0 )	err_msg( "ERROR on pthread_join: client_receive_thread" );
	if ( pthread_join(client_send_thread, NULL) != 0 )		err_msg( "ERROR on pthread_join: client_send_thread" );

	/*
	 * Clean up and exit
	 */
	close( sock_fd );

	exit( EXIT_SUCCESS );
}	/* main */
