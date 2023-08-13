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
u_char ttl = 16;
int omers_test;
int wait_time = 10;
time_t TIME_send[3], TIME1[3];
struct timeval TO[3];
struct  sockaddr_in server_multi_Addr[3];
int serverSocket;
int index1[3];
int multi_socket[3];
char multi_addr1[] = "239.0.0.1";
char multi_addr2[] = "239.0.0.2";
char multi_addr3[] = "239.0.0.3";
char multi_addr[3][9];
int player_count = 0;
pthread_mutex_t mutex_player_count;
int game_count = 0;
pthread_t games[3];
int num_of_game = 1;
int client_list[3][3];
int ack_table[3][3]; // check ack for ack msg(start, number)
int ack_victory_table[3][3];// check ack for victory msg
int winner[3];
int cur_num_play[3]; // number of player current playing.
int state[3];  // 1- send start msg or wait ack for start game.
               // 2- send number msg or wait ack for get number.
               // 3- send victory declaration
               // 4- wait ack for game over.
//==========================================
void setup_server();
_Noreturn void * run_game(void * client_list);
char* itoa(int value, char* result, int base);
void send_to_cli(int fd, char* msg);
void make_table(int table[][5]);
void send_table (int fd, int num, int num_of_game);
int number_to_send(int ball_array[101]);
int server_check_msg(char msg[2], int game_number, int socket);
void get_players(int lis_socket);
void game_over(int num);
int check_ack_table(int num, int table[3][3]);
void send_to_cli_multicast(int game_num, char buffer1[2]);
//==========================================
void setup_server(){
    int i,j;
    for (i = 0; i < 3; ++i) {
    	index1[i] = i+1;
        state[i] = 1;
        TO[i].tv_sec = wait_time;
        TO[i].tv_usec = 0;
        winner[i] = -1;
        games[i] = -1;
        cur_num_play[i] = 0;
        for (j = 0; j < 3; ++j) {
            ack_table[i][j] = 0;
            ack_victory_table[i][j] = 0;
        }
    }
    // init multicast address table
    //char *p = multi_addr;
    strncpy(multi_addr[0], multi_addr1, 9);
    strncpy(multi_addr[1], multi_addr2, 9);
    strncpy(multi_addr[2], multi_addr3, 9);

    // Initialize variables
    struct sockaddr_in serverAddr;
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(serverSocket == -1){perror("error on open socket!");}
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(1234);
    printf("check1\n");

    //multicast socket 1
    multi_socket[0] = socket(AF_INET, SOCK_DGRAM, 0);
    if(multi_socket[0] == -1){perror("error on open multicast socket!");}
    server_multi_Addr[0].sin_family = AF_INET;
    server_multi_Addr[0].sin_addr.s_addr = inet_addr("239.0.0.1");
    server_multi_Addr[0].sin_port = htons(6000);
    memset(server_multi_Addr[0].sin_zero, '\0', sizeof server_multi_Addr[0].sin_zero);  // Set all bits of the padding field to 0
    setsockopt(multi_socket[0], IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

    //multicast socket 2
    multi_socket[1] = socket(AF_INET, SOCK_DGRAM, 0);
    if(multi_socket[1] == -1){perror("error on open multicast socket!");}
    server_multi_Addr[1].sin_family = AF_INET;
    server_multi_Addr[1].sin_addr.s_addr = inet_addr("239.0.0.2");
    server_multi_Addr[1].sin_port = htons(6000);
    //memset(server_multi_Addr[1].sin_zero, '\0', sizeof server_multi_Addr[1].sin_zero);  // Set all bits of the padding field to 0
    setsockopt(multi_socket[1], IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

    //multicast socket 3
    multi_socket[2] = socket(AF_INET, SOCK_DGRAM, 0);
    if(multi_socket[2] == -1){perror("error on open multicast socket!");}
    server_multi_Addr[2].sin_family = AF_INET;
    server_multi_Addr[2].sin_addr.s_addr = inet_addr("239.0.0.3");
    server_multi_Addr[2].sin_port = htons(6000);
    //memset(server_multi_Addr[2].sin_zero, '\0', sizeof server_multi_Addr[2].sin_zero);  // Set all bits of the padding field to 0
    setsockopt(multi_socket[2], IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

    // Bind the socket to the
    // address and port number.
    if(bind(serverSocket,(struct sockaddr*)&serverAddr,sizeof(serverAddr))<0){
        perror("ERROR binding listener socket.");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }
    return;
}

void get_players(int lis_socket){
    int i,j;
    int check, check1, num_of_conc = 0; //, count = 0;
    fd_set fdset,fdset2;
    socklen_t addr_size;
    struct sockaddr_in cli_addr;
    FD_ZERO(&fdset); // clear the set

    listen(lis_socket, 100);
    //add welcome socket and stdin to fdset:
    FD_SET(lis_socket, &fdset);
    FD_SET(fileno(stdin),&fdset);
    while (num_of_game < 4) {
        while (1) {
            fdset2 = fdset;
            check = select(FD_SETSIZE, &fdset2, NULL, NULL, NULL);     //block until user input
            if (check < 0) {
                close(serverSocket);
                perror("Select error");
                return;
            }
            else {
                for (j = 0; j < FD_SETSIZE; ++j) {
                    if (FD_ISSET(j, &fdset2)) {    //found the socket who made the interrupt
                        if (j == lis_socket) {      //socket is welcome socket- new connection
                            client_list[num_of_game-1][num_of_conc] = accept(lis_socket, (struct sockaddr *) &cli_addr, &addr_size);
                            if(client_list[num_of_game-1][num_of_conc] == -1){
                                close(serverSocket);
                                perror("error in accept!");
                                exit(EXIT_FAILURE);
                            }
                            FD_SET(client_list[num_of_game-1][num_of_conc], &fdset);    //add new socket to fdset
                            send_table(client_list[num_of_game-1][num_of_conc], num_of_conc, num_of_game);
                            num_of_conc++;
                            pthread_mutex_lock(&mutex_player_count);
                            player_count++;
                            printf("Total number of player is %d now.\n", player_count);
                            pthread_mutex_unlock(&mutex_player_count);
                        }
                    }
                }
            }

            if (num_of_conc == 3) {
                num_of_conc = 0;
                check1 = pthread_create(&games[game_count], NULL, run_game, (void *)&index1[num_of_game-1]);
                if (check1 != 0) {
                    perror("error in pthread_create");
                }
                printf("num_of_game =  %d.\n", num_of_game);
                game_count++;
                num_of_game++;

                FD_ZERO(&fdset);
                FD_SET(lis_socket, &fdset);
                FD_SET(fileno(stdin), &fdset);
                while(num_of_game == 4){}
            }
        }
    }
}

_Noreturn void * run_game(void * num){
    int i,j;
    int ball_array[101];
    for (i = 0; i < 101; ++i) {
        ball_array[i] = i;
    }
    int check_state, check, number_to_send1, main_flag = 0; // 0-send msg, 1- recv mag.
    char buffer1[2], tmp;
    fd_set cur, ready;
    FD_ZERO(&cur);
    int game_number = *((int *)num);
    cur_num_play[game_number-1] = 3;
    printf("game_number = %d !!!!!!!!!!\n", game_number);
    for (i = 0; i < 3; ++i) {FD_SET(client_list[game_number-1][i], &cur);}
    while (1) {
        TO[game_number-1].tv_sec = wait_time;
        TO[game_number-1].tv_usec = 0;
        ready = cur;
        if (main_flag) {
            // waiting to msg
            if(!cur_num_play[game_number-1]){
            	printf("close game number %d, no player left!.\n", game_number);
            	game_over(game_number);
            }
            check = select(FD_SETSIZE, &ready, NULL, NULL, &TO[game_number-1]);     //block until user input  &TO[game_number-1]
            //======================================================================
            //TIME1[game_number-1] = time(NULL);
            //TIME1[game_number-1] = TIME1[game_number-1] - TIME_send[game_number-1];
            //TO[game_number-1].tv_sec = wait_time - TIME1[game_number-1];
            //TO[game_number-1].tv_usec = 0;
            //======================================================================
            if (check < 0) {
                perror("Select error");
                exit(EXIT_FAILURE);

            }
                // timeout expire for some client and we need to close his socket
            else if (check == 0){
                for (i = 0; i < 3; ++i) {
                    if(ack_table[game_number-1][i] != 1){
                    	FD_CLR(client_list[game_number-1][i], &cur);
                        close(client_list[game_number-1][i]);
                        client_list[game_number-1][i] = -1;
                        cur_num_play[game_number-1] = cur_num_play[game_number-1] - 1;
                        if(!cur_num_play[game_number-1]){
                        	printf("close game number %d, no player left!.\n", game_number);
                        	game_over(game_number);
                        }
                        pthread_mutex_lock(&mutex_player_count);
                        player_count--;
                        printf("Total number of player is %d now.\n", player_count);
                        pthread_mutex_unlock(&mutex_player_count);
                    }
                }
            }
                //have msg to read.
            else{
                for (j = 0; j < FD_SETSIZE; ++j){
                    if(FD_ISSET(j, &ready)){    //found the socket who made the interrupt
                        recv(j,buffer1, sizeof(buffer1), 0);
                        tmp = buffer1[0];
                        check_state = (int)tmp;
                        if ((check_state != state[game_number-1] && check_state != 3) || (((state[game_number-1] == 1)||(state[game_number-1] == 4)) && check_state == 3)){
                        	if(check_state==2 && state[game_number-1]==4){
                        		continue;
                        	}
                        	else{
                        		 printf("got msg doesnt match with state!\n");
                                // remove client from game
                                // need to remove if msg type != state unless we got a bingo declaration,
                                // or if we wait to ack for start but we got bingo declaration.
                                for (i = 0; i < 3; ++i) {
                                    if(client_list[game_number-1][i] == j){client_list[game_number-1][i] = -1;}
                                }
                                cur_num_play[game_number-1] = cur_num_play[game_number-1] - 1;
                                FD_CLR(j, &cur);
                                close(j);
                        	}
                        }
                        else {
                            if(server_check_msg(buffer1, game_number, j)){ // gor ack from all player.
                                sleep(3); // sleep 3 sec and then send new msg.
                                main_flag = 0; // need to send a msg.
                            }
                        }
                    }
                }
            }
        }
        else {
            // need to send msg
            switch (state[game_number-1]) {
                case 1: // send start game msg
                    printf("send start game msg!\n");
                    buffer1[0] = 1;
                    buffer1[1] = 97;
                    main_flag = 1; // to change flag to wait msg
                    sleep(1);
                    send_to_cli_multicast(game_number, buffer1);// send msg++++++++++++++++++++++++++++++
                    break;
                case 2: // send number
                    printf("send new number!\n");
                    buffer1[0] = state[game_number-1];
                    number_to_send1 = number_to_send(ball_array);
                    buffer1[1] = number_to_send1;
                    main_flag = 1; // to change flag to wait msg
                    printf("number to send = %d\n", buffer1[1]);
                    send_to_cli_multicast(game_number, buffer1);// send msg++++++++++++++++++++++++++++++
                    break;
                case 3: // send victory declaration
                    printf("send victory declaration!\n");
                    buffer1[0] = 3;
                    buffer1[1] = winner[game_number-1];
                    state[game_number-1] = 4; // to wait for ack for victory msg
                    printf("state array:DDDDDDDDDD  ");
                    for(i=0; i<3; i++){printf("%d",state[i]);}
                    printf("\n");
                    main_flag = 1;  // to change flag to wait msg
                    send_to_cli_multicast(game_number, buffer1);// send msg++++++++++++++++++++++++++++++
                    break;
            }
        }
    }
}

int server_check_msg(char msg[2], int game_number, int socket){
    char check;
    int check1, id, temp = 0;
    check = msg[0];
    check1 = (int)check;
    check = msg[1];
    id = (int)check;

    switch (check1) {
        case 1:
            // got Ack for "start game" msg and get multicast group name and port.
            printf("got Ack for 'start game' msg from player %d.\n", id);
            ack_table[game_number-1][id] = 1;
            if(check_ack_table(game_number, ack_table)){
                state[game_number-1] = 2;
                return 1; // to change flag to send msg
            } else{
                return 0; // wait for more ack
            }
            break;
        case 2:
            // got Ack for get the number.
            printf("got Ack for getting number from player %d.\n", id);
            ack_table[game_number-1][id] = 1;
            if(check_ack_table(game_number, ack_table)){
                return 1; // to change flag to send msg
            } else{
                return 0; // wait for more ack
            }

        case 3:
            // got BINGOOO msg.
            printf("got victory declaration from player %d.\n", id);
            state[game_number-1] = 3;                  // to change state to send victory declaration
            winner[game_number-1] = id; // mark winner
            return 1;                   // to change flag to send msg

        case 4:
            //got ack for victory declaration.
        	printf("got Ack for victory declaration msg from player %d.\n", id);
        	ack_victory_table[game_number-1][id] = 1;
            if(check_ack_table(game_number, ack_victory_table)){
                game_over(game_number); // all ack arrive, close socket and finish game.
            } else{
                return 0;                              // wait for more ack
            }
            break;
    }
}

void send_to_cli_multicast(int game_num, char buffer1[2]){
    //
    int check;
    if(game_num == 2){
    	printf("send start game msg for gaame number 2!\n");
    }
    check = sendto(multi_socket[game_num-1], buffer1, 2 , 0, (struct sockaddr*)&server_multi_Addr[game_num-1], sizeof(server_multi_Addr[game_num-1]));
    if(check == -1){
        close(multi_socket[game_num-1]);
        perror("error in send multicast msg!");
        exit(EXIT_FAILURE);
    }
    //TO[game_num-1].tv_sec = wait_time;
    //TO[game_num-1].tv_usec = 0;
    //TIME_send[game_num-1] = time(NULL);
    //==========================================================================
}

// create table for player with no repetition;
void make_table(int table[][5]){
    int i,j;
    int arr[101], num;
    int flag = 1;
    for (i = 0; i < 101; ++i) {
        arr[i] = i;
    }
    for (i = 0; i < 5; ++i) {
        for (j = 0; j < 5; ++j) {
            while (flag) {
                num = (rand() % 101);
                if(arr[num] != -1){
                    flag = 0;
                    table[i][j] = num;
                    arr[num] = -1;
                }
            }
            flag = 1;
        }
    }
}

// send table and multicast address to client
void send_table (int fd, int num, int num_of_game){
    int i,j;
    char *p = multi_addr[num_of_game-1]; //+ (9*(num_of_game-1));
    int table[5][5];
    make_table(table);
    char msg[36];
    int k = 2;
    msg[0] = 0;
    msg[1] = num;
    for (i = 0; i < 5; ++i) {
        for (j = 0; j < 5; ++j) {
            msg[k] = (char)table[i][j];
            k++;
        }
    }
    strncpy(msg+27, multi_addr[num_of_game-1], 9);
    int n = send(fd, msg, sizeof(msg), 0);
    if(n<0){ perror("ERROR IN SENDING MSG TO CLIENT SOCKET!");}
    return;
}

//choose random to send without repetition
int number_to_send(int ball_array[101]) {
    int flag = 1, num;
    while (flag) {
        num = (rand() % 101);
        if (ball_array[num] != -1) {
            flag = 0;
            ball_array[num] = -1;
            return num;
        }
    }
}

int check_ack_table(int num, int table[3][3]){
    int i;
    int count = 0;
    for (i = 0; i < 3; ++i) {
        if(table[num-1][i] == 1){count++;}
    }
    if(count == cur_num_play[num-1]){
        for (i = 0; i < 3; ++i) {
            table[num-1][i] = 0;
        }
        return 1;
    }
    return 0;
}

void game_over(int num){
    int i;
    printf("game number %d is finish, close in 3,2,1...\n", num);
    for (i = 0; i < 3; ++i) {
        if(client_list[num-1][i] != -1){
            close(client_list[num-1][i]);
        }
    }
    pthread_mutex_lock(&mutex_player_count);
    player_count = player_count - cur_num_play[num-1];
    printf("Total number of player is %d now.\n", player_count);
    pthread_mutex_unlock(&mutex_player_count);
    cur_num_play[num-1] = 0;
    //close(multi_socket[num-1]);
    printf("check16\n");
    pthread_exit(NULL);
}

int main() {
    srand(time(NULL));
    setup_server();
    get_players(serverSocket);

    return 0;
}
