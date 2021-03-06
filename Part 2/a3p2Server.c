#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>

//Flag for when alarm has gone off and it is time to exit
int ALRM_EXIT = 0;

//Log file descriptor
int LOG_FD;

//Mutex for threads to share the file descriptor without interferring with each other
pthread_mutex_t lock;

//Struct for processing
struct serverParm {
           int connectionDesc;
};

//Handler function for signals
void ALARMhandler(int sig){
  //Ignore the signal generated by the alarm
  signal(SIGALRM, SIG_IGN);

  //Set the exit flag to true
  ALRM_EXIT = 1;

  //Reset this function as the handler just in case
  signal(SIGALRM, ALARMhandler);
}

//Function each worker thread is passed to
void *serverThread(void *parmPtr) {

    #define PARMPTR ((struct serverParm *) parmPtr)
    
    //Length of the message in bytes
    int recievedMsgLen;

    //Size of the buffer
    int BUF_SIZE = 1025;

    //Buffer for messages
    char messageBuf[BUF_SIZE];

    //The id of this current worker thread
    int threadId = pthread_self();

    /* Server thread code to deal with message processing */
    printf("DEBUG: connection made, connectionDesc=%d\n",
            PARMPTR->connectionDesc);
    if (PARMPTR->connectionDesc < 0) {
        printf("Accept failed\n");
        return(0);    /* Exit thread */
    }
    
    /* Receive messages from sender... */
    while ((recievedMsgLen=
            read(PARMPTR->connectionDesc,messageBuf,sizeof(messageBuf)-1)) > 0) 
    {
        recievedMsgLen[messageBuf] = '\0';

        //Create a stringe to hold our log data for this iteration
	char log[BUF_SIZE];

	//Build our log string
	strcpy(log, "[Thread ");

	//Convert the thread id to a c-string, then continue building the string
	char s_TID[100];
	sprintf(s_TID, "%d", threadId);
	strcat(log, s_TID);
	strcat(log, ": Received Command]> ");
	strcat(log, messageBuf);
	strcat(log, "\n");

        //Get the current time, and add it to our string
	time_t t;
	struct tm *tmp;
	char date[64];
	time(&t);
	tmp = localtime(&t);
	strftime(date, 64, "[%A, %B %d, %Y, %r]\n\n", tmp);
	strcat(log, date);
        
	//Lock the mutex during write, ensure consistency of data
	pthread_mutex_lock(&lock);

	//Print our log to the command line, and write it to the server log
	printf("\n%s", log);
	write(LOG_FD, log, strlen(log));

	//Unlock our mutex so that other threads may proceed
	pthread_mutex_unlock(&lock);

	//Initialize a pipe
	int pip[2];
	pipe(pip);

	//Fork the current thread
	if(fork()==0){
		//If the child, set the standard output and error to the write end of the pipe, then close the pipe
		close(pip[0]);
		dup2(pip[1], 1);
		dup2(pip[1], 2);
		close(pip[1]);
		
		//Perform command as given
		system(messageBuf);

		//Replace this current child process with an insignificant process to exit gracefully
		execlp("echo", "echo" "");
	}

	//Wait for child process to finish executing
	wait(NULL);
	
	//Initialize a buffer
	char buf[BUF_SIZE];

	//Read the output of the child process into the buffer, storing its size in receivedMsgLen.
	//Close the pip after
	close(pip[1]);
	recievedMsgLen = read(pip[0], buf, sizeof(buf)-1);
	close(pip[0]);
	recievedMsgLen[buf] = '\0';

	//Try to write the output of the child process in the buffer to the socket
	if (write(PARMPTR->connectionDesc, buf, BUF_SIZE) < 0) {
               perror("Server: write error");
               return(0);
        }

    }

    //Once we're done, clean up the process
    close(PARMPTR->connectionDesc);  /* Avoid descriptor leaks */
    free(PARMPTR);                   /* And memory leaks */
    return(0);                       /* Exit thread */
}

int main (int argc, char** argv) {
    //Initialize socket and thread variables
    int listenDesc;
    struct sockaddr_in myAddr;
    struct serverParm *parmPtr;
    int connectionDesc;
    pthread_t threadID;

    //If the arg count is too low, send an error
    if(argc < 2){
	perror("Usage: TCPServer <PORT NUMBER>");
	exit(1);
    }

    //Initialize our alarm handler, and set up a backup alarm for 3 minutes just to be safe
    signal(SIGALRM, ALARMhandler);
    alarm(120);

    /* Create socket from which to read */
    if ((listenDesc = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("open error on socket");
        exit(1);
    }

    /* Create "name" of socket */
    myAddr.sin_family = AF_INET;
    myAddr.sin_addr.s_addr = INADDR_ANY;
    myAddr.sin_port = htons((int) strtol(argv[1], (char **)NULL, 10));
    
    //If binding the socket result in an error, throw an error and exit
    if (bind(listenDesc, (struct sockaddr *) &myAddr, sizeof(myAddr)) < 0) {
        perror("bind error");
        exit(1);
    }

    /* Start accepting connections.... */
    /* Up to 5 requests for connections can be queued... */
    listen(listenDesc,5);

    //Try to open the log file - if something goes wrong, throw an error
    if((LOG_FD = open("a3p2ServerLog.txt", O_WRONLY | O_APPEND)) < 0){
        perror("Could not open a3p2ServerLog.txt");
	exit(1);
    }

    //Try to initialize our mutex - if something goes wrong, throw and error
    if(pthread_mutex_init(&lock, NULL) != 0){
        perror("Mutex could not be instantiated");
	exit(1);
    }

    //While the alarm hasn't gone off, don't exit
    while (!ALRM_EXIT) {
        /* Wait for a client connection */
        connectionDesc = accept(listenDesc, NULL, NULL);

        /* Create a thread to actually handle this client */
        parmPtr = (struct serverParm *)malloc(sizeof(struct serverParm));
        parmPtr->connectionDesc = connectionDesc;

	//Try to create a worker thread to handle this request - if something goes wrong, throw and error
        if (pthread_create(&threadID, NULL, serverThread, (void *)parmPtr) 
              != 0) {
            perror("Thread create error");
            close(connectionDesc);
            close(listenDesc);
            exit(1);
        }

        //Ready for a new connection
        printf("Parent ready for another connection\n");
    }

    //Close the log file, and destroy the mutex
    close(LOG_FD);
    pthread_mutex_destroy(&lock);
    return 0;
}
