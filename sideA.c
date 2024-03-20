#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <linux/input.h>
#include <alsa/asoundlib.h>
#include <semaphore.h>


#define WRITE 5000
#define LISTEN 5001
#define AUD_PORT_RCV 5003
#define AUD_PORT_SND 5002

#define LED_FILE "/sys/class/leds/pca995x:blue4/brightness"

#define CHANNELS    2
#define FRAMES      768  

#define END_MESSAGE "1"

bool capture_audio = false;
int init_volume = 100;
volatile int send_flag = 0;
pthread_mutex_t send_flag_mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t mutex;

void reproduce_audio();

void * writter(void* arg)
{
    printf("Writting\n");
    struct sockaddr_in client;

    //Dominio internet, icp, protocolo automático
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    inet_pton(AF_INET, "192.168.1.10", &(client.sin_addr));
    client.sin_port = htons(WRITE);
    client.sin_family = AF_INET;

    int ret = bind(sock, (const struct sockaddr*)&client, sizeof(client));
    ret = listen(sock, 10);

    int client_send = accept(sock, (struct sockaddr*)NULL, NULL);
    printf("Creating socket\n");
    for (;;)
    {
        //sleep(1);
        char str[100];
        printf("B: ");
        fflush(STDIN_FILENO);
        char* gets = fgets(str, sizeof(str), stdin);
        if(strcmp(str, "exit\n") == 0){
            break;
        }
        ret = send(client_send, str, strlen(str), 0);
    }
    ret = close(client_send);
    ret = close(sock);
    return 0;
}

void * listener(void* arg)
{
    printf("Listening\n");
    char buff[100];

    int socket_connection = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));

    server.sin_addr.s_addr = inet_addr("192.168.1.20"); // Server IP address
    server.sin_port = htons(LISTEN);
    server.sin_family = AF_INET;

    int ret = connect(socket_connection, (struct  sockaddr*)&server, sizeof(server));
    while (ret != 0)
    {
        ret = connect(socket_connection, (struct  sockaddr*)&server, sizeof(server));
    }

    printf("Connected\n");

    for (;;)
    {
        ret = recv(socket_connection, (void*)buff, 100, 0);
        if (ret <= 0)
        {
            break;
        }        
        printf("\b\bA: %s\nB: ", buff);
    }
    printf("Connection closed\n");
    ret = close(socket_connection);
    return NULL;
}

void * audio_capture(void* arg){
    printf("Audio Capture\n");
    int ret;
    FILE* rec_file = fopen("send_audio.wav", "w");

    snd_pcm_t* handle;
    snd_pcm_hw_params_t* hw_params;

    ret = snd_pcm_open(&handle, "hw:0", SND_PCM_STREAM_CAPTURE, 0);

    snd_pcm_hw_params_alloca(&hw_params);
    ret = snd_pcm_hw_params_any(handle, hw_params);
    ret = snd_pcm_hw_params_set_access(handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_format_t format = SND_PCM_FORMAT_S32_LE;

    ret = snd_pcm_hw_params_set_format(handle, hw_params, format);

    int channels = CHANNELS;

    ret = snd_pcm_hw_params_set_channels(handle, hw_params, channels);

    int rate = 48000;
    ret = snd_pcm_hw_params_set_rate_near(handle, hw_params, &rate, 0);

    ret = snd_pcm_hw_params(handle, hw_params);
    int size = CHANNELS * FRAMES * sizeof(uint32_t);
    uint32_t* buffer = (uint32_t*)malloc(size);

    while (capture_audio)
    {
        snd_pcm_sframes_t frames = snd_pcm_readi(handle, buffer, FRAMES);
        int n_bytes = fwrite(buffer, 1, size, rec_file);
        fflush(rec_file);
    }

    snd_pcm_close(handle);
    fclose(rec_file);
}

static void set_volume(long volume)
{
    long min, max;
    volume = init_volume + volume;
    init_volume = volume;
    printf("Volume: %ld\n", volume);
    snd_mixer_t *handle;
    snd_mixer_selem_id_t *sid;
    const char *card = "hw:1";
    const char *selem_name = "Softmaster";

    snd_mixer_open(&handle, 0);
    snd_mixer_attach(handle, card);
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, selem_name);

    snd_mixer_elem_t* elem = snd_mixer_find_selem(handle, sid);
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    snd_mixer_selem_set_playback_volume_all(elem, volume * max / 100);

    snd_mixer_close(handle);
}

void * button_listener(void* arg){
    printf("Input Test\n");
    int input_fd = open("/dev/input/event0", O_RDWR);
    if (input_fd == -1)
        return NULL;



    for (;;)
    {
        struct input_event ev;
        int n_bytes = read(input_fd, &ev, sizeof(struct input_event));
        if (ev.type == EV_KEY && ev.code == 0x19C)
        {
            if (ev.value == 0x1)
            {
                printf("Button Pressed\n");
                capture_audio = true;
                pthread_t thread_id;
                pthread_create(&thread_id, NULL, audio_capture, NULL);
            }
            else
            {
                printf("Button Released\n");
                capture_audio = false;
                pthread_mutex_lock(&send_flag_mutex);
                send_flag = 1;
                pthread_mutex_unlock(&send_flag_mutex);
                printf("Flag: %d\n", send_flag);
                FILE* led = fopen(LED_FILE, "w");
                fputc('1', led);
                fclose(led);
            }
        }
        // reporoduce audio
        
        if (ev.type == EV_KEY && ev.code == 207)
        {
            if (ev.value == 0x1)
            {
                printf("Button Pressed\n");
                reproduce_audio();
                // turn off LED
                FILE* led = fopen(LED_FILE, "w");
                fputc('0', led);
                fclose(led);
            }
        }
        if( ev.type == EV_KEY && ev.code == 103)
        {
            if(ev.value == 0x1)
            {
                printf("Volume increased\n");
                set_volume(10);
            }
        }
        if( ev.type == EV_KEY && ev.code == 108)
        {
            if(ev.value == 0x1)
            {
                printf("Volume decreased\n");
                set_volume(-10);
            }
        }

    }

    close(input_fd);
    return NULL;
}


void reproduce_audio(){
    FILE* rec_file = fopen("audio_rcv.wav", "r");

    snd_pcm_t* handle;
    snd_pcm_hw_params_t* hw_params;


    int ret = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);

    snd_pcm_hw_params_alloca(&hw_params);
    ret = snd_pcm_hw_params_any(handle, hw_params);


    if ((ret = snd_pcm_hw_params_set_access(handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
    {
        printf("ERROR! Cannot set interleaved mode\n");
        return;
    }

    snd_pcm_format_t format = SND_PCM_FORMAT_S32_LE;
    if ((ret = snd_pcm_hw_params_set_format(handle, hw_params, format)) < 0)
    {
        printf("ERROR! Cannot set format\n");
        return;
    }


    int channels = 2;

    if ((ret = snd_pcm_hw_params_set_channels(handle, hw_params, channels)) < 0)
    {
        printf("ERROR! Cannot set Channels\n");
        return;
    }

    int rate = 48000;
    if ((ret = snd_pcm_hw_params_set_rate_near(handle, hw_params, &rate, 0)) < 0)
    {
        printf("ERROR! Cannot set Rate %d\n", rate);
        return;
    }


    if ((ret = snd_pcm_hw_params(handle, hw_params)) < 0)
    {
        printf("ERROR! Cannot set hw params\n");
        return;
    }

    int size = CHANNELS * FRAMES * sizeof(uint32_t);
    uint32_t* buffer = (uint32_t*)malloc(size);

    while (!feof(rec_file))
    {

        int n_bytes = fread(buffer, 1, size, rec_file);
        snd_pcm_sframes_t frames = snd_pcm_writei(handle, buffer, FRAMES);
    }

    ret = snd_pcm_drain(handle);
    ret = snd_pcm_close(handle);
    fclose(rec_file);

}

void * send_audio(){
    printf("Sending Audio\n");
    struct sockaddr_in client;

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    inet_pton(AF_INET, "192.168.1.10", &(client.sin_addr));
    client.sin_port = htons(AUD_PORT_SND);
    client.sin_family = AF_INET;

    uint32_t buffer[CHANNELS * FRAMES];
    size_t size;

    int ret = bind(sock, (const struct sockaddr*)&client, sizeof(client));
    ret = listen(sock, 10);

    int client_send = accept(sock, (struct sockaddr*)NULL, NULL);
    printf("Creating sending audio socket\n");
    for(;;){
        pthread_mutex_lock(&send_flag_mutex);
        while (send_flag == 0) {
            pthread_mutex_unlock(&send_flag_mutex);
            pthread_mutex_lock(&send_flag_mutex);
        }
        printf("Sending audio\n");
        FILE* wav_file = fopen("send_audio.wav", "rb");
        while ((size = fread(buffer, sizeof(uint32_t), CHANNELS * FRAMES, wav_file)) > 0) {
            ret = send(client_send, buffer, size * sizeof(uint32_t), 0);
            if (ret < 0) {
                perror("Error sending audio data");
                fclose(wav_file);
                close(client_send);
                close(sock);
                return 0;
            }
        }
        
        fclose(wav_file);
        printf("Audio sent successfully\n");

        send_flag = 0;
        pthread_mutex_unlock(&send_flag_mutex);
        bzero(buffer, sizeof(buffer));
    }
    // Close connections and file
    
    close(client_send);
    close(sock);
}

void *rcv_audio(void *arg) {
    printf("Receiving Audio\n");
    struct sockaddr_in server;
    int size = CHANNELS * FRAMES * sizeof(uint32_t);
    char buffer[size];

    int socket_audio = socket(AF_INET, SOCK_STREAM, 0);

    memset(&server, 0, sizeof(server));
    server.sin_addr.s_addr = inet_addr("192.168.1.20"); // Server IP address
    server.sin_port = htons(AUD_PORT_RCV);
    server.sin_family = AF_INET;

    int ret = connect(socket_audio, (struct sockaddr *)&server, sizeof(server));
    while (ret == -1) {
        ret = connect(socket_audio, (struct sockaddr *)&server, sizeof(server));
    }
    printf("Server audio listener connected\n");
    FILE* blue_led = fopen(LED_FILE, "w");
    for(;;){
            
        fputc('2', blue_led);
        fputc('5',blue_led);
        fputc('5',blue_led);
        fclose(blue_led);
        FILE *wav_file = fopen("audio_rcv.wav", "wb");
        ssize_t bytes_received;
        while ((bytes_received = recv(socket_audio, buffer, sizeof(buffer), 0)) > 0) {
            size_t items_written = fwrite(buffer, sizeof(char), bytes_received, wav_file);
            if (items_written < bytes_received) {
                perror("Error writing to audio file");
                fclose(wav_file);
                close(socket_audio);
                return NULL;
            }
        }
        fclose(wav_file);
        bzero(buffer, sizeof(buffer));
        printf("Audio received successfully\n");
    }

    // Close connections and file
    close(socket_audio);

    return NULL;
}



void main(int argc, char* argv[])
{
    pthread_t th_c, th_p, th_b, th_a, th_s;
    int ret;
    //sem_init(&mutex, 0, 1);

    printf("Creating Threads\n");

    ret = pthread_create(&th_c, NULL, writter, NULL);
    ret = pthread_create(&th_p, NULL, listener, NULL);
    ret = pthread_create(&th_b, NULL, button_listener, NULL);
    ret = pthread_create(&th_a, NULL, rcv_audio, NULL);
    ret = pthread_create(&th_s, NULL, send_audio, NULL);
    pthread_detach(th_b);

    ret = pthread_join(th_c, NULL);
    ret = pthread_join(th_p, NULL);
    ret = pthread_join(th_b, NULL);
    ret = pthread_join(th_a, NULL);
    ret = pthread_join(th_s, NULL);
    //sem_destroy(&mutex);
    pthread_mutex_destroy(&send_flag_mutex);

    return;
}