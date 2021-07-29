#include <iostream>
#include <vector>
#include <cmath>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helpers.h"

using namespace std;

// Structura in care tin datele despre un topic.
struct topic {
    char name[51];
    int sf;
};

// Structura folosita la conversia bufferului venit de la clientul UDP.
struct udp {
    char topic[50];
    uint8_t  data_type;
    char content[1501];
};

// Structura in care tin datele despre un client.
struct client {
    char id[51];
    int socketClient;
    int connected;
    vector<topic> topics;
    vector<char*> recover;
};

// Functie in care parcurg vectorul de clienti si verific daca id-ul respectiv
// exista si clientul e inca conectat.
bool activeClient(vector< client >  clients, char* id) {
    for (int i = 0; i < clients.size(); i++) {
        if (strcmp(clients[i].id, id) == 0 && clients[i].connected == 1) {
            return true;
        }
    }
    return false;
}

// Functie in care parcurg vectorul de clienti si verific daca id-ul respectiv
// exista si clientul e este deconectat.
bool inactiveClient(vector< client >  clients, char* id) {
    for (int i = 0; i < clients.size(); i++) {
        if (strcmp(clients[i].id, id) == 0 && clients[i].connected == 2) {
            return true;
        }
    }
    return false;
}

// Funtie in care parcurg vectorul de topicuri al unui client si verific daca acesta este
// deja conectat la topicul respectiv.
bool activeTopic(vector< client >  clients, int index, char* name) {
    bool ok = false;
    for (int k = 0; k < clients[index].topics.size(); k++) {
        if (strcmp(name, clients[index].topics[k].name) == 0) {
            ok = true;
            break;
        }
    }
    return ok;
}

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
    udp *udpMessage;
    vector< client > clients;
    char buffer[BUFLEN], id[BUFLEN];
    DIE(argc < 2, "Numar insuficient de argumente pentru server.\n");
    int port_dorit = atoi(argv[1]);
    DIE(port_dorit == 0, "Portul e invalid.\n");

    fd_set readFds;
    fd_set auxFds;
    FD_ZERO(&readFds);
    FD_ZERO(&auxFds);

    int socketUdp, socketTcp, socketNew;
    struct sockaddr_in clientTCPAddr, clientUDPAddr;

    // Initializam socketurile de TCP si UDP.
    socketTcp = socket(AF_INET, SOCK_STREAM, 0);
    DIE(socketTcp < 0, "socket");

    socketUdp = socket(PF_INET, SOCK_DGRAM, 0);
    DIE(socketUdp < 0, "socket");

    // Completam strcturile sockaddr_in.
    memset((char *) &clientTCPAddr, 0, sizeof(clientTCPAddr));
    clientTCPAddr.sin_family = AF_INET;
    clientTCPAddr.sin_port = htons(port_dorit);
    clientTCPAddr.sin_addr.s_addr = INADDR_ANY;

    memset((char *) &clientUDPAddr, 0, sizeof(clientUDPAddr));
    clientUDPAddr.sin_family = AF_INET;
    clientUDPAddr.sin_port = htons(port_dorit);
    clientUDPAddr.sin_addr.s_addr = INADDR_ANY;

    // Legam la server cei doi socketii.
    cond = bind(socketTcp, (struct sockaddr *) &clientTCPAddr, sizeof(struct sockaddr));
    DIE(cond < 0, "bind");

    cond = bind(socketUdp, (struct sockaddr *) &clientUDPAddr, sizeof(struct sockaddr));
    DIE(cond < 0, "bind");

    cond = listen(socketTcp, SOMAXCONN);
    DIE(cond < 0, "listen");

    // Adaugam socketi de pe care se ascultam in multimea de citire.
    FD_SET(socketTcp, &readFds);
    FD_SET(socketUdp, &readFds);
    FD_SET(0 , &readFds);
    int fdmax = max(socketTcp, socketUdp);

    bool condition = true;
    while (condition) {
        // Salvam in multimea auxiliara de citire, socketi de unde se primeste informatia
        // la momentul actual.
        auxFds = readFds;
        cond = select(fdmax + 1, &auxFds, NULL, NULL, NULL);
        DIE(cond < 0, "select");

        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &auxFds)) {
                // Daca primim o comanda de la tastaura, citim comanda si in cazul
                // in care aceasta este exit trimitem un mesaj la toti clientii care
                // sunt conectati pentru a stii sa se inchida, golim multimile de citire
                // si inchidem socketul de TCP si UDP.
                if (i == 0) {
                    memset(buffer, 0, BUFLEN);
                    fgets(buffer, BUFLEN - 1, stdin);
                    if (strncmp(buffer, "exit", 4) == 0) {
                        condition = false;

                        for (int j = 0; j < clients.size(); j++) {
                            if (clients[j].connected == 1) {
                                cond = send(clients[j].socketClient, "Out", 3, 0);
                                DIE(cond < 0, "send");
                            }
                        }

                        FD_ZERO(&readFds);
                        FD_ZERO(&auxFds);
                        close(socketTcp);
                        close(socketUdp);
                        return 0;
                    }

                } else if (i == socketTcp) {
                    // Acceptam conexiunea.
                    socklen_t clilen = sizeof(struct sockaddr);
                    socketNew = accept(socketTcp, (struct sockaddr *) &clientTCPAddr, &clilen);
                    DIE(socketNew < 0, "accept");

                    //Dezactivam alogritmul lui Neagle.
                    int const c = 1;
                    int neagle = setsockopt(socketNew, IPPROTO_TCP, c, (char *)&(c), sizeof(int));
                    DIE(neagle != 0, "disable");

                    FD_SET(socketNew, &readFds);
                    if (socketNew > fdmax) {
                        fdmax = socketNew;
                    }

                    // Primim id-ul de la clientul care tocmai s-a conectat.
                    memset(buffer, 0, BUFLEN);
                    cond = recv(socketNew, buffer, sizeof(buffer), 0);
                    DIE(cond < 0, "recv");
                    strcpy(id, buffer);

                    // Daca clientul este activ (exista deja id-ul) , ii trimitem
                    // un mesaj pentru a stii sa inchidem socketul respectiv.
                    if (activeClient(clients, buffer)) {
                        cond = send(socketNew, "Used", 4, 0);
                        DIE(cond < 0, "send");

                        cout << "Client " << buffer << " already connected.\n";

                    // Daca clientul este inactiv, il facem din nou activ si ii transmitem toate
                    // mesajel pe care acesta trebuia sa le primeasca cat timp a fost inactiv.
                    } else if (inactiveClient(clients, buffer)) {
                        for (int j = 0; j < clients.size(); j++) {
                            if (strcmp(clients[j].id, buffer) == 0) {
                                clients[j].connected = 1;
                                cout << "New client " << clients[j].id
                                    << " connected from " << inet_ntoa(clientTCPAddr.sin_addr)
                                        << ":" << ntohs(clientTCPAddr.sin_port) << ".\n";
                           
                                for (int k = 0; k < clients[j].recover.size(); k++) {
                                    cond = send(socketNew, clients[j].recover[k], BUFLEN, 0);
                                    DIE(cond < 0, "send");
                                }
                                clients[j].recover.clear();

                           }
                        }

                    // Daaca clientul nu exista, creem un client nou si il adaugam la vectorul de
                    // clienti.
                    } else {
                        client newClient;
                        strcpy(newClient.id, buffer);
                        newClient.socketClient = socketNew;
                        newClient.connected = 1;
                        clients.push_back(newClient);

                        cout << "New client " << newClient.id
                            << " connected from " << inet_ntoa(clientTCPAddr.sin_addr)
                                << ":" << ntohs(clientTCPAddr.sin_port) << ".\n";
                    }

                } else if (i == socketUdp) {
                    socklen_t clilen = sizeof(clientUDPAddr);
                    char toRecv[1551];
                    memset(toRecv, 0, 1551);
                    cond = recvfrom(socketUdp, (char *)toRecv, 1551, 0, (struct sockaddr *) &clientUDPAddr, &clilen);
                    DIE(cond < 0, "recvfrom");

                    // Castam mesajul primit de la clientul UDP intr-o strctura udp.
                    // In functie de data_type, parsam mesajul.
                    char message[1581];
                    udpMessage = (udp*) toRecv;
                    if (udpMessage->data_type == 0) {
                        int sign = udpMessage->content[0];
                        uint32_t value = ntohl(*(uint32_t*)(udpMessage->content + 1));
                        if (sign == 1) {
                            value *= -1;
                        }
                        sprintf(message, "%s:%d - %s - INT - %d", inet_ntoa(clientUDPAddr.sin_addr),
                                ntohs(clientUDPAddr.sin_port), udpMessage->topic, value);
                    }

                    if (udpMessage->data_type  == 1) {
                        double value = ntohs(*(uint16_t*) (udpMessage->content));
                        sprintf(message, "%s:%d - %s - SHORT_REAL - %0.2f", inet_ntoa(clientUDPAddr.sin_addr),
                                ntohs(clientUDPAddr.sin_port), udpMessage->topic, value / 100);
                    }

                    if (udpMessage->data_type  == 2) {
                        int sign = udpMessage->content[0];
                        int value = ntohl(*(uint32_t*)(udpMessage->content + 1));
                        int power = udpMessage->content[5];
                        if (sign == 1) {
                            value *= -1;
                        }
                        sprintf(message, "%s:%d - %s - FLOAT - %.4f", inet_ntoa(clientUDPAddr.sin_addr),
                                ntohs(clientUDPAddr.sin_port), udpMessage->topic, (float) value / pow(10, power));
                    }

                    if (udpMessage->data_type  == 3) {
                        sprintf(message, "%s:%d - %s - STRING - %s", inet_ntoa(clientUDPAddr.sin_addr),
                                ntohs(clientUDPAddr.sin_port), udpMessage->topic, udpMessage->content);
                    }
                   
                    // Parcurgem vectorul clienti si pentru fiecare client verificam daca acesta este activ
                    // si este abonat la topicul respectiv caz in care ii trimitem mesajul, sau incativ si
                    // abonat la topic caz in care bagam mesajul in vectorul de topicuri pe care o sa le 
                    // preimeasca cand o sa se reconecteze.
                    for (int k = 0; k < clients.size(); k++) {
                        for (int j = 0; j < clients[k].topics.size(); j++) {
                            if (strcmp(clients[k].topics[j].name, udpMessage->topic) == 0) {
                                if(clients[k].connected == 2 && clients[k].topics[j].sf == 1) {
                                    clients[k].recover.push_back(message);   

                                } else if(clients[k].connected == 1) {
                                    cond = send(clients[k].socketClient, message, 1581, 0);
                                    DIE(cond < 0, "send");
                                }
                            }
                        }
                    }

                } else {
                    // Primi un mesaj de la un client.
                    memset(buffer, 0, BUFLEN);
                    cond = recv(i, buffer, sizeof(buffer), 0);
                    DIE(cond < 0, "recv");

                    // Daca am primit mesaajul disconnect insemna ca clientul tocmai ce s-a deconectat,
                    // cautam idul lui pentru a marca faptul ca a fost deconecat si inchiodem conexiunea.
                    for(int j = 0; j < clients.size(); j++) {
                        if(clients[j].socketClient == i) {
                            if(strncmp(buffer, "Disconnect", 10) == 0) {
                                clients[j].connected = 2;
                                cout << "Client " << id << " disconnected.\n";

                                close(i);
                                FD_CLR(i, &readFds);
                                break;

                            } else {
                                char aux[100];
                                strcpy(aux, buffer);
                                vector< char* > commandFromTCP;
                                commandFromTCP = convert(aux);

                                // Daca primim unsubscribe stergem din vectorul de topicuri topicul respectiv.
                                if (strncmp(buffer, "unsubscribe", 11) == 0) {
                                    for (int k = 0; k < clients[j].topics.size(); k++) {
                                        if (strncmp(commandFromTCP[1], clients[j].topics[k].name, strlen(clients[j].topics[k].name)) == 0) {
                                            clients[j].topics.erase(clients[j].topics.begin() + k);
                                            break;
                                        }
                                    }

                                // Daca am primit subscribe verficam daca clientul urmarea deja topicul respectiv,
                                // iar daca nu creem un nou topic si il adaugam in vector.
                                } else if (strncmp(buffer, "subscribe", 9) == 0) {
                                   if (activeTopic(clients, j, commandFromTCP[1])) {
                                        break;

                                    } else {
                                            topic newTopic;
                                            strcpy(newTopic.name, commandFromTCP[1]);
                                            newTopic.sf = atoi(commandFromTCP[2]);
                                            clients[j].topics.push_back(newTopic);
                                    }

                                } else {
                                    continue;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
