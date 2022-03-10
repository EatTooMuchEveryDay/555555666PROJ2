#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>

// TODO TO BE REMOVED
#define MSG_CONFIRM 0

#define RECV_TIMEOUT 60

/*
| name       | length      |
| ---------- | ----------- |
| endflag    | 1byte       |
| seqno      | 2byte       |
| packetsize | 2byte       |
| filepath   | 60byte      |
| data       | 0~30000byte |
| crc        | 4byte       |
*/
#define FILE_PATH_SIZE 60
#define HEADER_SIZE sizeof(char) + 2 * sizeof(short) + FILE_PATH_SIZE
#define DATA_SIZE 30000
#define PKT_SIZE HEADER_SIZE + DATA_SIZE + CRC_SIZE
#define CRC_SIZE 4

#define ACK_SIZE 2

// TODO
unsigned int crc32b(char *message, long msg_len)
{
    int i, j;
    unsigned int byte, crc, mask;

    i = 0;
    crc = 0xFFFFFFFF;
    // while (message[i] != 0) {
    while (i < msg_len)
    {
        if (message[i] == 0)
        {
            i = i + 1;
            continue;
        }
        byte = message[i]; // Get next byte.
        crc = crc ^ byte;
        for (j = 7; j >= 0; j--)
        { // Do eight times.
            mask = -(crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
        i = i + 1;
    }
    return ~crc;
}

int main(int argc, char *argv[])
{
    // Check if arguments are valid
    if (argc != 3 || (strcmp(argv[1], "-p") && strcmp(argv[1], "--p")))
    {
        printf("Usage: ./recvfile -p <recv_port>\n");
        return -1;
    }

    // Check if assigned port is valid
    int port = atoi(argv[2]);
    if (port < 0 || port > INT16_MAX)
    {
        printf("Sorry %d is not a valid input for port value.\n", port);
        return -1;
    }
    else if (port < 18000 || port > 18200)
    {
        printf("Warning: port value is only valid from 18000 to 18200 on clear server.\n");
    }

    // Setup socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Create Socket Failed\n");
        return -1;
    }

    // Setup socket address information
    socklen_t addrlen = sizeof(struct sockaddr_in);
    struct sockaddr_in local_sin, client_sin;

    memset(&local_sin, 0, sizeof(local_sin));
    local_sin.sin_family = AF_INET;
    local_sin.sin_port = htons(port);
    local_sin.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to local port
    if (bind(sockfd, (struct sockaddr *)&local_sin, sizeof(local_sin)) < 0)
    {
        printf("Unable to bind the socket. Check if the port is in use.\n");
        return -1;
    }

    // Initialize timeout value
    struct timeval timeout;
    timeout.tv_sec = RECV_TIMEOUT;
    timeout.tv_usec = 0;

    // Receive and send buffer
    char *recv_buf = malloc(PKT_SIZE);
    char *ack_buf = malloc(ACK_SIZE);

    char *file_option = "w";

    char complete_flag = 0;
    short cur_seq = 0;
    int bytes_received = 0;

    while (1)
    {
        // Initialize the receiving buffer
        memset(recv_buf, 0, PKT_SIZE);

        // Retrieve the next packet
        int count = recvfrom(sockfd, recv_buf, PKT_SIZE, MSG_WAITALL, (struct sockaddr *)&client_sin, &addrlen);
        if (count < 0)
        {
            if (errno == EAGAIN)
            {
                // EAGAIN error, we should try again
                continue;
            }
            else
            {
                // Unexpected error thrown while retrieving packet
                break;
            }
        }

        // Check CRC
        if (crc32b(recv_buf, PKT_SIZE - CRC_SIZE) != *(unsigned *)(recv_buf + PKT_SIZE - CRC_SIZE))
        {
            // CRC check failed
            printf("[recv corrupt packet]\n");

            // TODO REMOVE THIS
            printf("send ack_buf (crc check failed): cur_seq=%d\n", (int)cur_seq);
        }
        else
        {
            // CRC check passed
            char end_flag = *recv_buf;
            if (end_flag)
            {
                // Complete transmission
                complete_flag = 1;
                break;
            }

            short recvID = (short)ntohs(*(short *)(recv_buf + 1));
            short data_size = (short)ntohs(*(short *)(recv_buf + 3));

            if (recvID <= cur_seq)
            {
                // Duplicated previous packets that should be ignored
                printf("[recv data] %d %u IGNORED\n", (bytes_received - data_size), data_size);

                // TODO REMOVE THIS
                printf("send ack_buf(duplicated): cur_seq=%d\n", (int)cur_seq);
            }
            else
            {
                // This is the next pkt we want
                // Set socket timeout option
                if (cur_seq == 0)
                    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout)); // Set the timeout value

                cur_seq = recvID;
                bytes_received += data_size;
                printf("[recv data] %d %u ACCEPTED\n", (bytes_received - data_size), data_size);

                // Write the data into specific file
                char *data = malloc(data_size);
                memcpy(data, recv_buf + HEADER_SIZE, data_size);
                char filepath[65];
                strncpy(filepath, recv_buf + 5, 60);
                int dirlen = -1;
                for (int idx = 0; filepath[idx] != '\0'; idx++)
                {
                    if (filepath[idx] == '/')
                    {
                        dirlen = idx;
                    }
                }
                dirlen++;
                if (dirlen > 0)
                {
                    char dir[60];
                    strncpy(dir, filepath, dirlen);
                    if (access(dir, F_OK) < 0 && mkdir(dir, S_IRWXU) < 0)
                    {
                        printf("Unable to create file or directory.\n");
                        break;
                    }
                }
                strcat(filepath, ".recv");

                FILE *fp = fopen(filepath, file_option);
                if (file_option[0] == 'w')
                {
                    file_option = "a";
                }

                if (!fp || fwrite(data, 1, sizeof(data), fp) != sizeof(data))
                {
                    printf("Failed to open or create the given file.\n");
                    free(data);
                    break;
                }
                fclose(fp);
                free(data);

                printf("send ack_buf(everything good): cur_seq=%d\n", (int)cur_seq);
            }
        }

        // Send ACK with current max sequence number
        *((short *)ack_buf) = (short)htons(cur_seq);
        count = sendto(sockfd, (const char *)ack_buf, ACK_SIZE, MSG_CONFIRM, (const struct sockaddr *)&client_sin, addrlen);
        if (count <= 0)
        {
            printf("Send ACK Error (errorno: %d)\n", errno);
            break;
        }
    }

    // Release the memory for receiving buffer
    free(recv_buf);
    free(ack_buf);

    // Check whether the transmission is complete
    if (complete_flag)
    {
        printf("[completed]\n");
    }

    return 0;
}
