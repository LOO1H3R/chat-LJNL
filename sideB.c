#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <linux/input.h>

#define WRITE 5001
#define LISTEN 5000



void * writter(void * arg) {
    printf("Writting\n");
    struct sockaddr_in client;
    
    //Dominio internet, icp, protocolo automático
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    inet_pton(AF_INET, "192.168.1.20", &(client.sin_addr));
    client.sin_port = htons(WRITE);
    client.sin_family = AF_INET;

    int ret = bind(sock, (const struct sockaddr *)&client, sizeof(client));
    ret = listen(sock, 10);
    
    int client_send = accept(sock, (struct sockaddr *) NULL, NULL);
    printf("Creating socket\n");
    for(;;){
        sleep(1);
        char str[100]; 
        printf("B: ");
        fflush(STDIN_FILENO); 
        fgets(str,sizeof(str),stdin);   
        ret = send(client_send, str, strlen(str), 0);         
    }
    ret = close(sock);
    return 0;
}

void * listener(void * arg) {
    printf("Listening\n");
    char buff[100];

    int socket_connection = socket(AF_INET, SOCK_STREAM, 0);
    
    struct sockaddr_in server;
    memset(&server, 0 , sizeof(server));
    
    server.sin_addr.s_addr = inet_addr("192.168.1.10"); // Server IP address
    server.sin_port = htons(LISTEN);
    server.sin_family = AF_INET;
    
    int ret = connect(socket_connection, (struct  sockaddr *)&server, sizeof(server) );
    while(ret != 0){
        ret = connect(socket_connection, (struct  sockaddr *)&server, sizeof(server) );
    }

    printf("Connected\n");

    for(;;){
        ret = recv(socket_connection, (void *)buff, 100, 0);
        //printf("Rx: %d\n", ret);
        printf("A: %s\n", buff);
    }

    ret = close(socket_connection);
    return NULL;
}

void * button_listener(void * arg) {
    printf("Input Test\n");
    int input_fd = open("/dev/input/event0", O_RDWR);
    if (input_fd == -1)
        return NULL;

    for (;;) {
        struct input_event ev;
        int n_bytes = read(input_fd, &ev, sizeof(struct input_event));
        if (ev.type == EV_KEY && ev.code == 0x19C) {
            if(ev.value == 0x1){
                printf("Button Pressed\n");
            }else{
                printf("Button Released\n");
            }
        }
    }

    close(input_fd);
    return NULL;
}

void main(int argc, char * argv[]) {
    pthread_t th_c, th_p, th_b;
    int ret;

    printf("Creating Threads\n");

    ret = pthread_create(&th_c, NULL, writter, NULL);
    ret = pthread_create(&th_p, NULL, listener, NULL);
    ret = pthread_create(&th_b, NULL, button_listener, NULL);
    
    ret = pthread_join(th_c, NULL);
    ret = pthread_join(th_p, NULL);
    ret = pthread_join(th_b, NULL);

    return;
}