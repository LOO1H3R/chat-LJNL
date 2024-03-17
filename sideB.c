#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <linux/input.h>

#include <alsa/asoundlib.h>

#define WRITE 5001
#define LISTEN 5000

#define CHANNELS    2
#define FRAMES      768  



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

    FILE * rec_file = fopen("Recorded.wav", "w");

    snd_pcm_t * handle;
    snd_pcm_hw_params_t * hw_params; 


    for (;;) {
        struct input_event ev;
        int n_bytes = read(input_fd, &ev, sizeof(struct input_event));
        if (ev.type == EV_KEY && ev.code == 0x19C) {
            if(ev.value == 0x1){
                printf("Record Button Pressed\n");
                int ret = snd_pcm_open(&handle, "hw:0", SND_PCM_STREAM_CAPTURE, 0);
                snd_pcm_hw_params_alloca(&hw_params);
                ret = snd_pcm_hw_params_any(handle, hw_params);
                if( (ret = snd_pcm_hw_params_set_access(handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
                    printf("ERROR! Cannot set interleaved mode\n");
                    return ret;
                }

                snd_pcm_format_t format = SND_PCM_FORMAT_S32_LE;

                if( (ret = snd_pcm_hw_params_set_format(handle, hw_params, format)) < 0) {
                    printf("ERROR! Cannot set format\n");
                    return ret;
                }

                int channels = CHANNELS;

                if( (ret = snd_pcm_hw_params_set_channels(handle, hw_params, channels)) < 0) {
                    printf("ERROR! Cannot set Channels\n");
                    return ret;
                }

                int rate = 48000;
                if( (ret = snd_pcm_hw_params_set_rate_near(handle, hw_params, &rate, 0)) < 0) {
                    printf("ERROR! Cannot set Rate %d\n", rate);
                    return ret;
                }

                
                if( (ret = snd_pcm_hw_params(handle, hw_params)) < 0) {
                    printf("ERROR! Cannot set hw params\n");
                    return ret;
                }

                int size = CHANNELS * FRAMES * sizeof(uint32_t);
                uint32_t * buffer = (uint32_t * )malloc(size);


                for (;;) {
                    snd_pcm_sframes_t frames = snd_pcm_readi(handle, buffer, FRAMES);
                    int n_bytes = fwrite(buffer, 1, size, rec_file);
                    fflush(rec_file);
                }
                ret = snd_pcm_close(handle);
                fclose(rec_file);
                printf("Recording Finished\n");

                //insmod snd-soc-tfa98xx.ko

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