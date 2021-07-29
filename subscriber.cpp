#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"

using namespace std;

// Functie pentru despartirea unui sir de cuvinte in cuvinte.
vector< char* > convert(char *buffer) {
    vector < char* > result;
    char *token;
    token = strtok(buffer, " ");
    while (token != NULL) {
        result.push_back(token);
        token = strtok(NULL, " ");
    }
    return result;
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    int cond;
    char buffer[BUFLEN];
    DIE(argc < 4, "Numar insuficient de argumente pentru server.\n");
    int port_dorit = atoi(argv[3]);
    DIE(port_dorit == 0, "Portul invalid.\n");

    fd_set readFds;
    fd_set auxFds;
    FD_ZERO(&readFds);
    FD_ZERO(&auxFds);

    // Initializam socketul clientului.
    int socketClient;
    socketClient = socket(AF_INET, SOCK_STREAM, 0);
    DIE(socketClient < 0, "socket");

    // Completam structura de sockaddr.
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port_dorit);
    cond = inet_aton(argv[2], &serverAddr.sin_addr);
    DIE(cond == 0, "Adresa ip a serverului nu este corecta.\n");

    // Conectam clientul la server si trimitem serverului idul acestuia.
    cond = connect(socketClient, (struct sockaddr*) &serverAddr, sizeof(serverAddr));
    DIE(cond < 0, "connect");

    cond = send(socketClient, argv[1], strlen(argv[1]), 0);
    DIE(cond < 0, "send");

    // Adaugam socketi de pe care ascultam in multimea de citire.
    int fdmax;
    FD_SET(socketClient, &readFds);
    FD_SET(0, &readFds);
    fdmax = socketClient;


    while (true) {
        // Salvam in multimea auxiliara de citire socketi de unde se primeste informatia
        // la momentul actual.
        auxFds = readFds;
        cond = select(fdmax + 1, &auxFds, NULL, NULL, NULL);
        DIE(cond < 0, "select");

        if (FD_ISSET(0, &auxFds)) {
            memset(buffer, 0, BUFLEN);
            fgets(buffer, BUFLEN - 1, stdin);
            char aux[1000];
            strcpy(aux, buffer);
            vector< char* > command = convert(aux);
            DIE(command.size() < 1 || command.size() > 3, "Wrong command");

            // Daca primim comanda de exit de la tastatura, transimtem un mesaj vectorului
            // pentru a stii ca s-a deconectat clientul.
            if (command.size() == 1 && strncmp(command[0], "exit",4) == 0) {
                cond = send(socketClient, "Disconnect", 10, 0);
                DIE(cond < 0, "send");
                close(socketClient);
                break;

            } else {
                // Daca primim un buffer in care primul cuvant este subscribe / unsubscribe
                // trimitem bufferul serverului.
                if (command.size() == 3 && strncmp(command[0], "subscribe", 9) == 0) {
                    cond = send(socketClient, buffer, strlen(buffer), 0);
                    DIE(cond < 0, "send");
                    printf("Subscribed to topic.\n");
                }

                if (command.size() == 2 && strncmp(command[0], "unsubscribe", 11) == 0) {
                    cond = send(socketClient, buffer, strlen(buffer), 0);
                    DIE(cond < 0, "send");
                    printf("Unsubscribed from topic.\n");
                }
            }

        } else {
            memset(buffer, 0, 1581);
            cond = recv(socketClient, buffer, 1581, 0);
            DIE(cond < 0, "recv");

            // Daca primim de la server comanda Out insemna ca serverul s-a inchis
            // si trebuie sa inchidem si clientul.
            if (strncmp(buffer, "Out", 3) == 0) {
                close(socketClient);
                break;
            }

            // Daca primim de la server comanda Used insemna ca exista deja in server
            // idul respectiv si trebuie sa inchidem acest client.
            if (strncmp(buffer, "Used", 4) == 0) {
                close(socketClient);
                break;
            }

            // Afisam mesajuele primite de la server.
            cout << buffer << endl;
        }
    }

    return 0;
}
