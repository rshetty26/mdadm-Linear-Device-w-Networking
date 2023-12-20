#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1; 

/* attempts to read n bytes from fd; returns true on success and false on
 * failure */
bool nread(int fd, int len, uint8_t *buf) { // returns true if it can read the n bytes from fd
  int readval = 0;
  while (readval < len) { 
    int out = read(fd, &buf[readval], len - readval); 
    if (out == -1) {
      return false; // if any errors, return false
    } else {
    readval += out; 
    }
  }
  return true; // return true if it runs through the whole fd
}

/* attempts to write n bytes to fd; returns true on success and false on
 * failure */
bool nwrite(int fd, int len, uint8_t *buf) { // returns true if it can write the n bytes to fd
  int writeval = 0;
  while (writeval < len) {
    int out = write(fd, &buf[writeval], len - writeval);
    if (out == -1) {
      return false; // if any errors, return false
    } else {
    writeval += out;
    }
  }
  return true; // return true if it runs through the whole fd
}

/* attempts to receive a packet from fd; returns true on success and false on
 * failure */
bool recv_packet(int fd, uint32_t *op, uint16_t *ret, uint8_t *block) { 
  uint8_t recpacket[HEADER_LEN + JBOD_BLOCK_SIZE]; 
  if (!nread(fd, HEADER_LEN, recpacket)) {        
    return false;
  } else {
    memcpy(op, recpacket, sizeof(*op));
    *op = ntohl(*op);
    memcpy(ret, recpacket + sizeof(*op), sizeof(*ret));

    return ((*ret & 0x02) && !nread(fd, JBOD_BLOCK_SIZE, block)) ? false : true; // Check if data block exists, return false if fails, else return true
  }
}

/* attempts to send a packet to sd; returns true on success and false on
 * failure */
bool send_packet(int sd, uint32_t op, uint8_t *block) {
  uint8_t sendpacket[HEADER_LEN + JBOD_BLOCK_SIZE];
  uint16_t len = HEADER_LEN;
  uint8_t info = 0x00;
  uint32_t rest = op >> 12 & 0x3f;

  if (rest == JBOD_WRITE_BLOCK) { // if it is in write block
    len += JBOD_BLOCK_SIZE;
    info = 0x02; // initialized as given
  }
  op = htonl(op); 
  memcpy(sendpacket, &op, sizeof(op));
  memcpy(sendpacket + sizeof(op), &info, sizeof(info));
  if (len > HEADER_LEN) { // if it exists then,
    memcpy(sendpacket + HEADER_LEN, block, JBOD_BLOCK_SIZE);
  }
  return nwrite(sd, len, sendpacket); 
}

/* connect to server and set the global client variable to the socket */
bool jbod_connect(const char *ip, uint16_t port) {
  struct sockaddr_in caddr;
  caddr.sin_family = AF_INET;
  caddr.sin_port = htons(port);

  if (!inet_aton(ip, &caddr.sin_addr)) { // if adress returns N/A return false
    return false;
  } else {
    cli_sd = socket(AF_INET, SOCK_STREAM, 0); 
    if (cli_sd == -1) { // if new socket is -1, return false
      return false;
    } else if (connect(cli_sd, (const struct sockaddr *)&caddr, sizeof(caddr)) == -1) { /
      close(cli_sd); // if the connection fails, close socket and return false
      cli_sd = -1;
      return false;
    }
    return true; // successful so return true
  }
}

void jbod_disconnect(void) {
  if (cli_sd != -1) { // disconnect is socket returns -1
    close(cli_sd);
    cli_sd = -1;
  }
}

int jbod_client_operation(uint32_t op, uint8_t *block) {
  uint16_t outReturn;
  send_packet(cli_sd, op, block);
  uint32_t temp;
  recv_packet(cli_sd, &temp, &outReturn, block);
  return (op != temp) ? -1 : outReturn;
}
