//SERVER
//DOR MOOLAY - 205870637
//SNIR MOSCOVICH - 206293128
//=========================================
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <time.h>

//==========================================
struct ip_mreq mreq;
int port = 6000;
int Row[5], Col[5];
char multicast_addr[10];
int client_socket;
int multicast_socket;
struct sockaddr_in serverAddr;
socklen_t addr_size;
int Table[5][5];
int ID;
int close_flag = 0; // turn 1 if need to close socket.
int bingo_flag = 0; // turn 1 if have a bingo in table.
int state = 1; // 1- wait for start game msg or send ack for start game.
               // 2- wait for number msg or send ack for number.
               // 3- wait for (your own) victory declaration or send ack for victory msg.
               // you can get victory declaration in any state beside state 1.
//==========================================
void send_to_server(int fd, char* msg);
int find_bingo(int row[5], int col[5]);
int client_check_msg(char msg[256]);
int find_num_in_table(int num, int table[5][5]);
void setup_client();
void get_table(int fd);
void open_multicast_socket();
void run();
void print_table(int table[][5]);
void handle_error_x(int x, char* msg);
//==========================================
void setup_client(){
    int i;
    int check;
    for (i = 0; i < 5; ++i) {
        Col[i] = 0;
        Row[i] = 0;
    }
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket==-1){perror("Error Openning socket\n");}

    serverAddr.sin_family = AF_INET;                                 //Address family = Internet
    serverAddr.sin_port = htons(1234);                      //set port using htons
    serverAddr.sin_addr.s_addr = inet_addr("192.12.5.1");                         //set ip address
    memset(serverAddr.sin_zero, '\0', sizeof (serverAddr.sin_zero));   // Set all bits of the padding field to 0
    addr_size = sizeof serverAddr;
    check =connect(client_socket, (struct sockaddr *) &serverAddr, addr_size); //Connect the socket to the server using the address struct
    if(check == -1) {
        close(client_socket);
        perror("Error: Connection refused");
        exit(EXIT_FAILURE);
    }
    return;
}

void get_table(int fd){
    int i,j;
    char check;
    int check1, k=2;
    char buffer1[36];
    recv(fd, &buffer1, sizeof(buffer1), 0);
    check = buffer1[0];
    check1 = (int)check;
    if(check1 != 0){
        close_flag = 1;
        close(fd);
        return;
    }
    check = buffer1[1];
    ID = (int)check;
    printf("ID = %d\n",ID);

    for (i = 0; i < 5; ++i) {
        for (j = 0; j < 5; ++j) {
            Table[i][j] = (int)buffer1[k];
            k++;
        }
    }
    print_table(Table);
    k = 0;
    for (i = 27; i < 36; ++i) {
        multicast_addr[k] = buffer1[i];
        k++;
    }
    multicast_addr[k] = '\n';
    printf("mulricast address to conect: %s\n",multicast_addr);
}

void open_multicast_socket(){
    int check;
    struct sockaddr_in client_multi_Addr;
    multicast_socket = socket(AF_INET, SOCK_DGRAM, 0);
    client_multi_Addr.sin_family = AF_INET;
    client_multi_Addr.sin_addr.s_addr = htonl(INADDR_ANY);
    client_multi_Addr.sin_port = htons(port);
    check = bind(multicast_socket, (struct sockaddr*)&client_multi_Addr, sizeof(client_multi_Addr));
    if(check == -1){
        close(client_socket);
        perror("Error: bind!");
        exit(EXIT_FAILURE);
    }
    mreq.imr_multiaddr.s_addr = inet_addr(multicast_addr); //"239.0.0.1"
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(multicast_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    sleep(3);
}

void run(){
    int i,j;
    int main_flag = 1, check, check_state;
    char buffer1[2], tmp;
    fd_set cur, ready;
    FD_ZERO(&cur);
    FD_SET(multicast_socket, &cur);
    struct timeval TO;
    TO.tv_sec = 15;
    TO.tv_usec = 0;

    while (1){
    	ready = cur;
        TO.tv_sec = 15;
        TO.tv_usec = 0;

        if(bingo_flag){ //send victory msg
            printf("have a bingo! send msg to server.\n");
            buffer1[0] = 3;
            buffer1[1] = ID;
            send_to_server(client_socket, buffer1);
            state = 3;
            main_flag = 1;
            bingo_flag = 0;
        }

        //if (ID == 0){bingo_flag = 1;}//=====================================================

        if(close_flag){
            printf("close connection\n");
            sleep(5);
            close(client_socket);
            close(multicast_socket);
            return;
        }

        if (main_flag){

            check = select(FD_SETSIZE, &ready, NULL, NULL, &TO);   //block until user input
            if (check < 0) {
                handle_error_x(check, "error in select! close connection.\n");
            }
            else if(check == 0){
                handle_error_x(check, "server didnt send msg for a long time, exit game and close connection\n");
            }
            else{
                for (j = 0; j < FD_SETSIZE; ++j) {
                    if(FD_ISSET(j, &ready)){    //found the socket who made the interrupt
                        recv(j,buffer1, sizeof(buffer1), 0);
                        tmp = buffer1[0];
                        check_state = (int)tmp;
                        //printf("state(%d) =? check_state(%d)\n", state, check_state);
                        if ((check_state != state && check_state!=3) || (check_state==3 && state==1)){
                        	if((check_state==2 && state==3)){
                        		continue;
                        	}else{
                                // exit from game and close socket
                                // need to remove if msg type != state unless we got a bingo declaration,
                                // or if we wait to ack for start but we got bingo declaration.
                                handle_error_x(check_state, "got msg that doesnt match the state, close connection!");
                        	}
                        }
                        else {
                            if(client_check_msg(buffer1)){ // gor ack from all player.
                                main_flag = 0; // need to send a msg.
                            }
                        }
                    }
                }
            }
        }
        else{
            // need to send msg
            switch (state) {
                case 1: // send ack for start game msg
                    buffer1[0] = 1;
                    buffer1[1] = ID;
                    send_to_server(client_socket, buffer1);
                    state = 2;
                    main_flag = 1;
                    break;
                case 2: // send ack for number
                    buffer1[0] = 2;
                    buffer1[1] = ID;
                    send_to_server(client_socket, buffer1);
                    main_flag = 1;
                    break;
                case 3: //got victory msg and need to send ack.
                    buffer1[0] = 4;
                    buffer1[1] = ID;
                    send_to_server(client_socket, buffer1);
                    printf("send ack for victory declaration!\n");
                    main_flag = 1;
                    close_flag = 1; // for close al connection and finish game
                    break;
            }
        }
    }
}

void send_to_server(int fd, char* msg){
    int n = send(fd, msg, 2 , 0);
    if(n<0){ handle_error_x(n, "error in send function!");}
}

int client_check_msg(char msg[2]){
    char bit1, bit2;
    int type, num_or_id, temp = 0;
    bit1 = msg[0];
    type = (int)bit1;
    bit2 = msg[1];
    num_or_id = (int)bit2;
    switch (type) {
        case 1:
            // got "start game" msg.
            printf("got \"start game\" msg!\n");
            printf("press enter for approval\n");
            while(getchar() != '\n');
            printf("wait for start game!\n");
            return 1;

        case 2:
            // got a number.
            if ((num_or_id>100) || (num_or_id<0)){
                handle_error_x(num_or_id, "got number out of range, close connection!");
                return 0;
            }
            printf("got new number: %d\n", num_or_id);
            bingo_flag = find_num_in_table(num_or_id, Table); // return 1 if have bingo, 0 otherwise.
            print_table(Table);
            printf("================================\n");
            printf("wait...\n");
            printf("================================\n");
            return 1;

        case 3:
            // got winner declaration.
            if ((num_or_id>3) || (num_or_id<0)){
                handle_error_x(num_or_id, "got ID out of range, close connection!");
                return 0;
            }
            printf("got winner declaration msg!\n");
            if (num_or_id == ID){
                printf("i won!\n");
            }
            else{
                printf("player number %d won!\n", num_or_id);
            }
            printf("game finish!!\n");
            state = 3;
            bingo_flag = 0;
            return 1;
    }
}

//check if number in bingo table and mark it (-1), return 1 if have bingo, 0 otherwise.
int find_num_in_table(int num, int table[5][5]){
    int i,j;
    int flag1 = 0;
    for (i = 0; i < 5; ++i) {
        for (j = 0; j < 5; ++j) {
            if(table[i][j] == num){
            	printf("have number!, mark it(-1).\n");
                table[i][j] = -1;
                Row[i]++;
                Col[j]++;
                flag1 = find_bingo(Row,Col);
                return flag1;
            }
        }
    }
    printf("dont have number!\n");
    return 0;
}
//check if have bingo in table.
int find_bingo(int row[5], int col[5]){
    int i;
    for (i = 0; i < 5; ++i) {
        if(row[i] == 5){
            return 1;
        }
        if(col[i] == 5){
            return 1;
        }
    }
    return 0;
}

void print_table(int table[][5]){
    int i,j;
    for (i = 0; i < 5; ++i) {
        for (j = 0; j < 5; ++j) {
            printf("%d, ", table[i][j]);
        }
        printf("\n");
    }
}

void handle_error_x(int x, char* msg){
    errno = x;
    perror(msg);
    close(client_socket);
    close(multicast_socket);
    exit(EXIT_FAILURE);
}

int main (){
    setup_client();
    get_table(client_socket);
    open_multicast_socket();
    run();
    return 0;
}
