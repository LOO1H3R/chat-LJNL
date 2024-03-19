#include <stdio.h>

int main() {
    FILE *wav_file;
    char byte;
    
    // Open the WAV file in binary mode
    wav_file = fopen("recording.wav", "rb");

    // Check if the file opened successfully
    if (wav_file == NULL) {
        printf("Error opening file.");
        return 1;
    }

    // Read each byte from the WAV file and print its hexadecimal representation
    while (fread(&byte, sizeof(char), 1, wav_file) == 1) {
        printf("%02X ", (unsigned char)byte);
    }

    // Close the file
    fclose(wav_file);

    return 0;
}