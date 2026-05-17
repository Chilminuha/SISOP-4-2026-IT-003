#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 9000

int main(int argc, char const *argv[]) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};
    char input[1024];

    // Membuat socket TCP
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Mengubah alamat IP ke format biner
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    // Menghubungkan ke Server Database di dalam Docker
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    printf("Connected to DB Server on port %d\n", PORT);
    printf("Type HELP for available commands\n");
    printf("Type EXIT to quit\n\n");

    // Looping untuk menerima input user dan mengirimnya ke server
    while(1) {
        printf("db > ");
        fgets(input, 1024, stdin);
        input[strcspn(input, "\n")] = 0;

        if(strcmp(input, "EXIT") == 0) {
            break;
        }

        if(strlen(input) == 0) continue;

        send(sock, input, strlen(input), 0);

        memset(buffer, 0, 1024);
        int valread = read(sock, buffer, 1024);

        if (valread > 0) {
            printf("%s\n\n", buffer);
        }
    }

    close(sock);
    return 0;
}
