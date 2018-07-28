#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>

#define MAXLINE 4096 /*max text line length*/

//Keeps track of when to exit the application, based on an alarm
int ALRM_EXIT = 0;

//Handler function for when SIGALRM is thrown.
void ALARMhandler(int sig){
  //Ignore the alarm
  signal(SIGALRM, SIG_IGN);

  //Signal to exit
  ALRM_EXIT = 1;

  //Replace our handler back just in case
  signal(SIGALRM, ALARMhandler);
}

//Main function of application
int main(int argc, char **argv) {
 //Socket descriptor
 int sockfd;

 //File descriptor for log
 int logfd;

 //Address of server
 struct sockaddr_in servaddr;

 //Buffer to hold messages sent through socket
 char sendline[MAXLINE];

 //Initialize our signal handler, and an alarm to go off in 30 seconds
 signal(SIGALRM, ALARMhandler);
 alarm(30);
	
 //basic check of the arguments
 //additional checks can be inserted
 if (argc !=3) {
  perror("Usage: TCPClient <Server IP> <Server Port>"); 
  exit(1);
 }
	
 //Create a socket for the client
 //If sockfd<0 there was an error in the creation of the socket
 if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) <0) {
  perror("Problem in creating the socket");
  exit(2);
 }
	
 //Creation of the socket
 memset(&servaddr, 0, sizeof(servaddr));
 servaddr.sin_family = AF_INET;
 servaddr.sin_addr.s_addr= inet_addr(argv[1]);
 servaddr.sin_port =  htons((int) strtol(argv[2], (char **)NULL, 10)); 
	
 //Connection of the client to the socket 
 if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr))<0) {
  perror("Problem in connecting to the server");
  exit(3);
 }

 //Open the log file for this session
 if((logfd = open("a3p3Client3Log.txt", O_WRONLY | O_APPEND)) < 0){
  perror("Problem opening log file");
  exit(3);
 }

 //Get the process id
 int pid = getpid();

 //Get the current date/time
 time_t t;
 struct tm *tmp;
 char date[64];
 time(&t);
 tmp = localtime(&t);

 ///Put the current date/time into our buffer
 strftime(date, 64, "%A, %B %d, %Y, %r\n", tmp);

 //Build our Session Initialization string message, then print it to both the output and the log file
 char sessionInfo[84] = "[Session Initiated] ";
 strcat(sessionInfo, date);
 printf("\n%s\n", sessionInfo);
 write(logfd, (void*)sessionInfo, strlen(sessionInfo));

 //Command we will execute on the remote server
 char* comm = "date; ls -l junk.txt; uname -a \0";

 //Display connection message
 printf("[CONNECTED!]\n The server will now execute the following commands: %s\n\n", comm);

 //While the alarm hasn't gone off yet, keep looping
 while (!ALRM_EXIT){
  //Buffer for what the server sends back
  char recvline[MAXLINE];

  //Put the command into the buffer
  strcpy(sendline, comm);

  //Build the output string for executing the command
  char msg[86] = "Waking up....\nAwake! ";
  //Build the date portion of the string
  time(&t);
  tmp = localtime(&t);
  strftime(date, 64, "[%a, %B %d, %Y, %r]\n", tmp);

  //Compile the date and the wake up message together
  strcat(msg, date);

  //Print the header for the output
  printf("%s[Process Id]: %i\n", msg, pid);

  //Send the command to the server
  send(sockfd, sendline, strlen(sendline), 0);
		
  //Read the response from the server into the buffer
  if (recv(sockfd, recvline, MAXLINE,0) == 0){
   //error: server terminated prematurely
   perror("The server terminated prematurely"); 
   exit(4);
  }
  
  //Build formatted string from the server's output
  char buf[MAXLINE] = "\n[Command]: ";
  strcat(buf, sendline);
  strcat(buf, "\n-----------------------\n[Response Begin:]\n");
  strcat(buf, recvline);
  strcat(buf, "[:Response End]\n\n");

  //Print the formatted response
  printf("%s", buf);

  //Write the formatted response to the log file
  write(logfd, (void*)buf, strlen(buf));

  //Print the sleep message, and sleep for 3 seconds
  printf("%s\n\n", "Sleeping...");
  sleep(1);
 }
 
 //Build the date/time string like before - we can reuse the same variables
 time(&t);
 tmp = localtime(&t);
 strftime(date, 65, "%A, %B %d, %Y, %r\n", tmp);

 //Build the end session string
 strcpy(sessionInfo, "[Session Ended] ");
 strcat(sessionInfo, date);

 //Write the end session string to the log and close the file
 write(logfd, (void*)sessionInfo, strlen(sessionInfo));
 close(logfd);

 //Print the success message
 printf("\n[Session termination: SUCCESSFUL!]\n[Process ID: %i]\n%s\n", pid, date);

 //Exit
 exit(0);
}
