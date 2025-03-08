/*
# Copyright 2025 University of Kentucky
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
*/

/* 
Please specify the group members here

# Student #1: Thomas Kennett
# Student #2: Connor Tallent
# Student #3: 

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <pthread.h>

#define MAX_EVENTS 64
#define MESSAGE_SIZE 16
#define DEFAULT_CLIENT_THREADS 4

char *server_ip = "127.0.0.1";
int server_port = 12345;
int num_client_threads = DEFAULT_CLIENT_THREADS;
int num_requests = 1000000;


/*
 * This structure is used to store per-thread data in the client
 */
typedef struct {
    int epoll_fd;        /* File descriptor for the epoll instance, used for monitoring events on the socket. */
    int socket_fd;       /* File descriptor for the client socket connected to the server. */
    long long total_rtt; /* Accumulated Round-Trip Time (RTT) for all messages sent and received (in microseconds). */
    long total_messages; /* Total number of messages sent and received. */
    float request_rate;  /* Computed request rate (requests per second) based on RTT and total messages. */
} client_thread_data_t;

/*
 * This function runs in a separate client thread to handle communication with the server
 */
void *client_thread_func(void *arg) {
    client_thread_data_t *data = (client_thread_data_t *)arg;
    struct epoll_event event, events[MAX_EVENTS];
    char send_buf[MESSAGE_SIZE] = "ABCDEFGHIJKMLNOP"; /* Send 16-Bytes message every time */
    char recv_buf[MESSAGE_SIZE];
    struct timeval start, end;
    //RTT is declared as a long long in run client function, se needs to be long long in client thread
    long long totalThreadRTT = 0;
    int threadMessagesSent = 0;
    //register the "connected" clientThread's socket in the its epoll instance
    //epoll event must listen for data to be read, so use EPOLLIN
    event.events = EPOLLIN;
    event.data.fd = data->socket_fd;
    if(epoll_ctl(data->epoll_fd, EPOLL_CTL_ADD, data->socket_fd, &event) < 0)
    {
        perror("singular thread epoll_ctl: ADD");
        exit(1);
    }

    for(int i = 0; i < num_requests; i++)
    {
        //start time measurement
        gettimeofday(&start,NULL);
        //send message and wait for response
        if (send(data->socket_fd, send_buf, MESSAGE_SIZE, 0) < 0)
        {
            perror("single client thread send");
            break;
        }

        //now lets wait for response
        int nfds = epoll_wait(data->epoll_fd, events, MAX_EVENTS, -1);
        if(nfds < 0)
        {
            perror("single client thread epoll_wait");
            break;
        }
        //iterate through ready file descriptors
        for(int j = 0; j<nfds; j++)
        {
            //check if our event corresponds to clients socket (basically if the event has recived data)
            if(events[j].data.fd == data->socket_fd)
            {
                int recivedStatus = recv(data->socket_fd, recv_buf, MESSAGE_SIZE, 0);
                if(recivedStatus <= 0)
                {
                    perror("single thread recv");
                    break;
                }
            }
            //end the time measurement and get calculations
            gettimeofday(&end,NULL);
            long long rtt_microseconds = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
            totalThreadRTT += rtt_microseconds;
            threadMessagesSent++;
        }
    }
    // Update global metrics (make sure you lock and unlock mutex to avoid race conditions here)
    data->total_rtt = totalThreadRTT;
    data->total_messages = threadMessagesSent;
    data->request_rate = (float)threadMessagesSent / (totalThreadRTT / 1000000.0);

    //close the sockets now
    close(data->socket_fd);
    close(data->epoll_fd);


    // Hint 2: use gettimeofday() and "struct timeval start, end" to record timestamp, which can be used to calculated RTT.
    // calculate RTT


    /* TODO:
     * It sends messages to the server, waits for a response using epoll,
     * and measures the round-trip time (RTT) of this request-response.
     */
 
    /* TODO:
     * The function exits after sending and receiving a predefined number of messages (num_requests). 
     * It calculates the request rate based on total messages and RTT
     */

    return NULL;
}

/*
 * This function orchestrates multiple client threads to send requests to a server,
 * collect performance data of each threads, and compute aggregated metrics of all threads.
 */
void run_client() {
    pthread_t threads[num_client_threads];
    client_thread_data_t thread_data[num_client_threads];
    struct sockaddr_in server_addr;
    //need to store data for total_rtt, totalMessages, and totalRequestRate
    //total_rtt is in microseconds, might need to use long long to play it safe
    long long totalRTT = 0;
    long totalMessages = 0;
    float totalRequestRate = 0.0;
    /* TODO:
     * Create sockets and epoll instances for client threads
     * and connect these sockets of client threads to the server
     */
    //bind socket to server
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton");
        exit(1);
    }
    //start by creating sockets and epoll instances for all threads:
    for(int i = 0; i < num_client_threads; i++)
    {
        //create the socket for this thread
        //AF_INET to specify IPV4, SOCK_STREAM to specify TCP sockets
        int clientSocketFD = socket(AF_INET, SOCK_STREAM, 0);
        if(clientSocketFD < 0)
        {
            perror("Client socket creation");
            exit(1);
        }
        if(connect(clientSocketFD, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            perror("Client connect");
            exit(1);
        }
        //Create an epoll instance for the thread
        int epollFD = epoll_create1(0);
        if(epollFD < 0)
        {
            perror("client epoll_create");
            exit(1);
        }
        //Register the socket with the epoll instance
        struct epoll_event event;
        //EPOLLIN is used here because we want to be notified when there is data to read
        event.events = EPOLLIN;
        //set the event file descriptor to our socket. This registers the client socket to the epoll instance
        event.data.fd = clientSocketFD;
        if(epoll_ctl(epollFD, EPOLL_CTL_ADD, clientSocketFD, &event))
        {
            perror("client epoll_ctl: add");
            exit(1);
        }
        // use thread_data to save the created socket and epoll instance for each thread
        thread_data[i].socket_fd = clientSocketFD;
        thread_data[i].epoll_fd = epollFD;
        thread_data[i].total_rtt = 0;
        thread_data[i].total_messages = 0;
        thread_data[i].request_rate = 0.0;
    }
    
    // You will pass the thread_data to pthread_create() as below
    for (int i = 0; i < num_client_threads; i++) {
        pthread_create(&threads[i], NULL, client_thread_func, &thread_data[i]);
    }

    // Wait for client threads to complete and aggregate metrics of all client threads
    for (int i = 0; i < num_client_threads; i++)
    {
        pthread_join(threads[i], NULL);
        totalRTT += thread_data[i].total_rtt;
        totalMessages += thread_data[i].total_messages;
        totalRequestRate += thread_data[i].request_rate;
        //may need to come back and close the socket and epoll instance for the threads
    }

    if(totalMessages>0) printf("Average RTT: %lld us\n", totalRTT / totalMessages);
    printf("Total Request Rate: %f messages/s\n", totalRequestRate);
}

void run_server() {

    /* TODO:
     * Server creates listening socket and epoll instance.
     * Server registers the listening socket to epoll
     */
    //create listening socket + epoll instance
    int listeningSocketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (listeningSocketFD < 0) 
    {
        perror("listening socket creation");
        exit(1);
    }
    int epollFD = epoll_create1(0);
    if (epollFD < 0) 
    {
        perror("server epoll_create");
        exit(1);
    }

    //register the listening socket to epoll instance
    //declare a struct for epoll event (declared in sys/epoll.h)
    struct epoll_event event;
    /*struct in header file for reference:
    typedef union epoll_data {
        void    *ptr;
        int      fd;
        uint32_t u32;
        uint64_t u64;
    } epoll_data_t;

    struct epoll_event {
        uint32_t     events;  Epoll events, like EPOLLIN, EPOLLOUT 
        epoll_data_t data;    User data, which can store an integer. definition above. 
    };*/
    //EPOLLIN is used here because we want to be notified when there is data to read
    event.events = EPOLLIN;
    //set the event file descriptor to our socket. This registers the listening socket to the epoll instance
    event.data.fd = listeningSocketFD;
    //Tell epoll to monitor the fd we placed in event.data.fd (the listening socket)
    if (epoll_ctl(epollFD, EPOLL_CTL_ADD, listeningSocketFD, &event) < 0) 
    {
        perror("epoll_ctl: add");
        exit(1);
    }
    /* Server's run-to-completion event loop */
    // Bind and listen
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = INADDR_ANY; // Bind to all interfaces

    if (bind(listeningSocketFD, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("server bind");
        exit(1);
    }

    if (listen(listeningSocketFD, SOMAXCONN) < 0)
    {
        perror("server listen");
        exit(1);
    }

    struct epoll_event events[MAX_EVENTS];
    printf("Server is currently listening on %s:%d\n", server_ip, server_port);
    //printf("Server is currently listening on %s:%d\n",server_ip,server_port);
    while (1) {
        /* TODO:
         * Server uses epoll to handle connection establishment with clients
         * or receive the message from clients and echo the message back
         */
        //num of file descriptors in epoll_fd
        int nfds = epoll_wait(epollFD, events, MAX_EVENTS, -1);
        if (nfds == -1)
        {
            perror("Server Socket epoll_wait");
            exit(1);
        }
        for (int i = 0; i < nfds; i++)
        {
            //if the current event is associated with the listening socket
            if(events[i].data.fd==listeningSocketFD)
            {
                //we need to create a new connection for our listening socket
                //use sock_addrin
                struct sockaddr_in clientaddr;
                socklen_t client_len = sizeof(clientaddr);
                int clientSocketFD = accept(listeningSocketFD, (struct sockaddr *)&clientaddr, &client_len);
                if (clientSocketFD < 0)
                {
                    perror("Server socket accept");
                    exit(1);
                }
                //register the new client socket to our epoll instance
                event.events = EPOLLIN;
                event.data.fd = clientSocketFD;
                //tell epoll to start monitoring the client socket
                if (epoll_ctl(epollFD, EPOLL_CTL_ADD, clientSocketFD, &event) < 0)
                {
                    perror("server socket epoll_ctl: add");
                    exit(1);
                }
            }
            else
            {
                //if the current event is not assosiated with a listening socket, it must associated iwth a client socket
                int clientSocketFD = events[i].data.fd;
                char buffer[MESSAGE_SIZE];
                int receivedMessageSize = recv(clientSocketFD, buffer, MESSAGE_SIZE, 0);
                if (receivedMessageSize < 0)
                {
                    perror("recieved message size under 0");
                }
                else if (receivedMessageSize == 0)
                {
                    //recived message is 0 in length, so we need to close the conection here
                    epoll_ctl(epollFD, EPOLL_CTL_DEL, clientSocketFD, NULL);
                    close(clientSocketFD);
                }
                else
                {
                    //recived message came through, send the message back to the client to confirm
                    if(send(clientSocketFD, buffer, receivedMessageSize, 0) < 0)
                    {
                        perror("send");
                        //exit(1);
                    }
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "server") == 0) {
        if (argc > 2) server_ip = argv[2];
        if (argc > 3) server_port = atoi(argv[3]);

        run_server();
    } else if (argc > 1 && strcmp(argv[1], "client") == 0) {
        if (argc > 2) server_ip = argv[2];
        if (argc > 3) server_port = atoi(argv[3]);
        if (argc > 4) num_client_threads = atoi(argv[4]);
        if (argc > 5) num_requests = atoi(argv[5]);

        run_client();
    } else {
        printf("Usage: %s <server|client> [server_ip server_port num_client_threads num_requests]\n", argv[0]);
    }

    return 0;
}
