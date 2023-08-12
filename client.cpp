#include <cbor.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <chrono>
#include <cstring>
#include <thread>
#include <cmath>
#include <iostream>

const int port = 6789;
const auto delay = std::chrono::milliseconds(200); // attente de 50ms entre chaque mise envoi
const double period = 1000;
const double pi = 4 * atan(1);

int main(int argc, char const *argv[]) {
    double val;
    double val2;
    char const *host;
    if (argc < 2)
        host = "localhost";
    else
        host = argv[1];

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (socket_fd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    int socket_info;
    struct addrinfo *server;
    // int found = getaddrinfo("localhost", "6789", &hints, &server);
    int found = getaddrinfo("127.0.0.1", "6789", &hints, &server);
    if (found < 0)
        perror("erreur recherche hôte");

    double time = 0;
    unsigned char *buffer;
    int iter = 0;
    while (iter < 10) {
        iter++;
        std::this_thread::sleep_for(delay);
        cbor_item_t *cbor_root = cbor_new_definite_map(2);
        time += 50;
        val = sin(2 * pi * time / period)+1;
        val2 = cos(2 * pi * time / period)+2;
        cbor_map_add(cbor_root, (struct cbor_pair) {
                .key = cbor_move(cbor_build_string("sun_x")),
                .value = cbor_move(cbor_build_float8(val))
        });
        cbor_map_add(cbor_root, (struct cbor_pair) {
                .key = cbor_move(cbor_build_string("sun_y")),
                .value = cbor_move(cbor_build_float8(val2))
        });
        size_t buffer_size,
                length = cbor_serialize_alloc(cbor_root, &buffer, &buffer_size);
        sendto(socket_fd, buffer, length, 0, server->ai_addr, server->ai_addrlen);

        free(buffer);
        cbor_decref(&cbor_root);
    }
    free(server);
}