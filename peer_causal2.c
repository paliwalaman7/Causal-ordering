#include <stdio.h>
#include <stdlib.h>

//next 2 gives socket definitions and api
#include <sys/types.h>
#include <sys/socket.h> 

// to store ip address type data
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>


#ifndef PORT
int PORT=8080;
#endif


#ifndef NODES
int max_nodes=3;
#endif

char name[20];
int my_id,flag_to_delay=1;
pthread_mutex_t lock;

struct msg_queue_node{
    char msg[2000];
    struct msg_queue_node *next;
};


struct msg_queue_node *head=NULL;
struct msg_queue_node *tail=NULL;

int local_vector_clock[]={0,0,0};
char msg_buffer[10][2044];
int msg_buffer_pointer=0;
char ip[][10]={ "10.0.0.1","10.0.0.2","10.0.0.3","10.0.0.4","10.0.0.5",
                "10.0.0.6","10.0.0.7","10.0.0.8","10.0.0.9","10.0.0.10"};

void sending();
void receiving(int );
void *receive_thread(void *);
void delay(int);
char *getStringClock();
int *extractMsgVc(char *);
int getSenderId(char *);
int check_order(char *);
void print_msg(char*);
void push(char *);
int printAnySatisfyingOrder();
char * getStringClock();
void delay(int);

void * delayAgent(void *);
void deliveryHandler(char *);
void printClock();


int main(int argc, char const *argv[])
{
    //setting up this host
    char my_ip[30];
    strcpy(name,argv[2]);
    my_id = atoi(argv[1]);
    strcpy(my_ip,ip[my_id]);

    srand(time(0));

    int curr_host_fd, new_socket, valread;
    struct sockaddr_in my_add_details; // this stucture is defined in inet header and allows to store ip and port no.
    int k = 0, p;

    
    /*
    Creating socket file descriptor
    AF_INET sets the domain of the socket to internet, sock_stream means its a tcp socket, 
    next param is for the protocol 
    */
    if ((curr_host_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    // Forcefully attaching socket to the port

    my_add_details.sin_family = AF_INET; //tell what type of ip we are working with 
    my_add_details.sin_addr.s_addr = inet_addr(my_ip); //setting ip add 
    my_add_details.sin_port = htons(PORT); // setting port that we need to connect to and htons just converts int to actual format of port in inet

    //Printed the current host socket addr and port
    printf("IP address is: %s\n", inet_ntoa(my_add_details.sin_addr));
    printf("port is: %d\n", (int)ntohs(my_add_details.sin_port));

    //binding to socket 
    if (bind(curr_host_fd, (struct sockaddr *)&my_add_details, sizeof(my_add_details)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    //listening
    if (listen(curr_host_fd, 5) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    //Creating thread to keep receiving message in real time
    pthread_t tid;
    pthread_create(&tid, NULL, &receive_thread, &curr_host_fd); 
    
    int ch;
    printf("\n*****At any point in time press the following:*****\n1.Send message\n2.check local clock\n0.Quit\n");
    
    do
    {
        printf("\nEnter choice\n:");
        scanf("%d", &ch);
        switch (ch)
        {

        case 2:
            printClock();
            break;
        case 1:
            sending();
            break;
        case 0:
            printf("\nLeaving\n");
            break;
        default:
            printf("\nWrong choice\n");
        }
    } while (ch>0);

    close(curr_host_fd);

    return 0;
}


void sending()
{

    
    
    int sock = 0, valread, cur_node=0;
    struct sockaddr_in recvr_host_details;
    char msg[2000] = {0};

    char dummy;
    printf("Enter your message:");
    scanf("%c", &dummy); //next line character capture
    scanf("%[^\n]s", msg);

    local_vector_clock[my_id]++; //increment local clock before sending

    // build the message to be sent by embedding the local vc and id
    char *string_clock = getStringClock();
    char buffer[2044] = {0};
    sprintf(buffer, "%s says: %s | %s !%d", name, msg, string_clock,my_id);

    while(cur_node<max_nodes){
        
        if(cur_node==my_id){
            cur_node++;
            continue;
        }   
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            printf("\n Socket creation error \n");
            return;
        }

        //setting  host details
        char recvr_addr[35];
        strcpy(recvr_addr,ip[cur_node]);

        recvr_host_details.sin_family = AF_INET;
        recvr_host_details.sin_addr.s_addr = inet_addr(recvr_addr); //INADDR_ANY always gives an IP of 0.0.0.0
        recvr_host_details.sin_port = htons(PORT);


        /*
        connect() will connect us to the recieving host
        connect() first param -> socket | second -> pointer to struct type sockaddr,
        so we are casting because recvr_host_details is of type sockaddr_in
        third -> size of the address
        connect returns an int which ==0 then connection success || if ==-1 then connection fail
        */
        if (connect(sock, (struct sockaddr *)&recvr_host_details, sizeof(recvr_host_details)) < 0)
        {
            printf("\ninside sending function and trying to connect but");
            printf("\nConnection Failed \n");
            return;
        }
        
        send(sock, buffer, sizeof(buffer), 0);
        printf("\nMessage sent\n");
        close(sock);
        cur_node++;

    }
}

//Calling receiving every 2 seconds
void *receive_thread(void *server_fd)
{
    int s_fd = *((int *)server_fd);
    while (1)
    {
        sleep(2);
        receiving(s_fd);
    }
}


void receiving(int server_fd)
{
    struct sockaddr_in address;
    int valread;
    
    int addrlen = sizeof(address);
    fd_set current_sockets, ready_sockets;
    
    //initialize my current set
    FD_ZERO(&current_sockets);
    FD_SET(server_fd, &current_sockets);
    int k=0;
    while(1)
    {
        k++;
        ready_sockets = current_sockets;

        if(select(FD_SETSIZE, &ready_sockets, NULL, NULL, NULL) < 0)
        {
            perror("Error");
            exit(EXIT_FAILURE);
        }

        for(int i=0;i< FD_SETSIZE;i++)
        {
            if(FD_ISSET(i, &ready_sockets))
            {
                if(i == server_fd)
                {
                    int client_socket;

                    if((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
                    {
                        perror("accept");
                        exit(EXIT_FAILURE);
                    }
                    FD_SET(client_socket, &current_sockets);
                }
                else
                {
                    
                    valread = recv(i, msg_buffer[msg_buffer_pointer], sizeof(msg_buffer[msg_buffer_pointer]), 0);
                    
                    pthread_t da_id;
                    // printf("\ncreating da thread and handing over msg ==> %s\n",msg_buffer[msg_buffer_pointer]);
                    // printf("add of buffer that thread will refer to %p\n",msg_buffer[msg_buffer_pointer]);
                    pthread_create(&da_id,NULL,&delayAgent,msg_buffer[msg_buffer_pointer]);
                    msg_buffer_pointer=(msg_buffer_pointer+1)%10;
                    
                    FD_CLR(i, &current_sockets);
                }
            }

        }

        if(k == (FD_SETSIZE * 2))
            break;

    }
}

void delay(int number_of_seconds){
    // Converting time into milli_seconds
    int milli_seconds = 500000 * number_of_seconds;
  
    // Storing start time
    clock_t start_time = clock();
  
    // looping till required time is not achieved
    while (clock() < start_time + milli_seconds);
}

char * getStringClock(){
    char *s;
    s= (char *) malloc(sizeof(char)*50);

    int pos=0;
    s[pos++]='[';
    for(int i=0;i<max_nodes;i++){
        char temp[10];
        sprintf(temp,"%d",local_vector_clock[i]);

        for(int j=0; temp[j]!='\0';j++){
            s[pos++]=temp[j];
        }

        if(i<max_nodes-1)
        s[pos++]=',';
    }
    s[pos++]=']';
    return s;

}
                                                                         
int *extractMsgVc(char *msg){
    int *arr;
    arr=(int *) malloc(sizeof(int)*max_nodes);

    int count=0;
    int i=0;

    while(msg[i++]!='|'); // move to | in the msg
    
    int temp=0;
    while(msg[i]!='!'){
        if(msg[i]>=48 && msg[i]<=57){
            temp= temp*10 + msg[i]-48; //build the number
        }

        if(msg[i]==','|| msg[i]==']'){
            //printf("%d\n",temp);
            arr[count++]=temp;
            temp=0;
        }
        i++;
    }
    
    return arr;    
}

int getSenderId(char *msg){

    int i=0;

    while(msg[i++]!='!'); // move to ! in the msg
    
    int temp=0;
    while(msg[i]!='\0'){
        temp= temp*10 + msg[i]-48; //build the number
        i++;
    }

    return temp;
}


int check_order(char *msg){

    int *msg_vc= extractMsgVc(msg);
    int sender_id = getSenderId(msg);

    if(local_vector_clock[sender_id]+1 != msg_vc[sender_id])
        return 0;

    for(int i=0;i<max_nodes;i++){
        if(i!=sender_id && local_vector_clock[i]<msg_vc[i]){
            return 0;
        }
    }

    local_vector_clock[sender_id]++;
    return 1;
}

void print_msg(char *msg){
    int i=0;
    printf("\ndelivering msg:");
    while(msg[i]!='|'){
        printf("%c",msg[i++]);
    }
    printf("\n");
}

void printClock(){

    printf("\n[");
    for(int i=0;i<max_nodes;i++){
        printf(" %d",local_vector_clock[i]);

        if(i<max_nodes-1)
            printf(",");
    }
    printf("]\n");
}

void push(char *temp_msg){
    struct msg_queue_node *temp = malloc(sizeof(struct msg_queue_node));

    strcpy(temp->msg,temp_msg);
    temp->next=NULL;

    if(head==NULL){
        head=temp;
        tail=temp;
    }

    else{
        tail->next=temp;
        tail=temp;   
    }
}

int printAnySatisfyingOrder(){

    if(head==NULL)
        return 0;


    struct msg_queue_node *temp=head;
    struct msg_queue_node *prev=NULL;

    while(temp!=NULL){
        if(check_order(temp->msg)){
            print_msg(temp->msg);

            if(temp==head){
                head=temp->next;
            }


            if(temp==tail){
                tail=prev;
            }

            if(head==NULL)
                tail==NULL;
            
            if(prev!=NULL)
                prev->next=temp->next;

            free(temp);
            return 1;
        }
        
        else{
            prev=temp;
            temp=temp->next;
        }
    }

    return 0;

    
}

void *delayAgent(void *buffer){


    if(my_id==2 && flag_to_delay){//intentionally introducing delay for node C
        flag_to_delay--;
        delay(rand()%100);
           
    }

    else
    delay(rand()%10);   


    char *msg= (char *) buffer;

    // pthread_t id = pthread_self();
    // printf("current da thread ruinning is %ld\n",id);
    // printf("and the message in its buffer ==> %s\n",(char *)buffer);

    int sender_id = 65+getSenderId(buffer);
    printf("recieved message from id:%c\n",(char)sender_id);
    deliveryHandler(msg);


}


void deliveryHandler(char *buffer){

    // printf("inside delivery handler\n");
    // printf("current dh thread ruinning is %ld\n",pthread_self());
    
    if(check_order(buffer)){ // if the recieved message is in order then deliver it immediately
        //printf("printing from first if\n");
        print_msg(buffer);

        pthread_mutex_lock(&lock);
        while(printAnySatisfyingOrder());
        pthread_mutex_unlock(&lock);
    }
    

    else{ 
        int result=0,flag=0;

        if(head==NULL){ //nothing in buffer
            printf("pushing to buffer ==> %s\n",buffer);
            pthread_mutex_lock(&lock);
            push(buffer);
            pthread_mutex_unlock(&lock);
        }

        
        else{ 

            pthread_mutex_lock(&lock);
            do{ 
                
                result = printAnySatisfyingOrder();

                if(result==1){//if there is one valid message in buffer print it
                    
                    if(flag==0 && check_order(buffer)){ //check validity of current message again if not delivered already
                        
                        print_msg(buffer);
                        flag=1;

                    }

                }

            }while(result==1);
            pthread_mutex_unlock(&lock);

            if(flag==0){ //after delivering all satisfying msg, current one is still out of order

                printf("not in order pushin into buffer\n");
                pthread_mutex_lock(&lock);
                push(buffer);
                pthread_mutex_unlock(&lock);
            }
        }
    }
}