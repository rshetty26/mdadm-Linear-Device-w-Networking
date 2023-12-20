#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache.h"
#include "jbod.h"
#include "mdadm.h"

#include "net.h"

/*Use your mdadm code*/

static int mountCheck = 0; //saves whether it's mounted or not

void translate(uint32_t lin_address, int *diskid, int *blockid, int *offset) { //takes the linear address and splits it up
  *diskid = lin_address / JBOD_DISK_SIZE;
  *blockid = (lin_address % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
  *offset = (lin_address % JBOD_DISK_SIZE) % JBOD_BLOCK_SIZE;
}

uint32_t encode_operation(jbod_cmd_t command, int diskid, int blockid) { //encodes the operation
  uint32_t operation = 0x0;
  operation = command << 12 | diskid << 0 | blockid << 4;
  return operation;
}

int mdadm_mount(void) { // checks whether it's mounted or not and mounts
  if (mountCheck == 0) { //if it is not mounted
    jbod_client_operation(encode_operation(JBOD_MOUNT, 0, 0), NULL);
    mountCheck = 1;
    return 1;
  } 
  return -1;
}

int mdadm_unmount(void) { //checks whether it's mounted or not and unmounts
  if (mountCheck == 1) { //if it is mounted
    jbod_client_operation(encode_operation(JBOD_UNMOUNT, 0, 0), NULL);
    mountCheck = 0;
    return 1;
  }
  return -1;
}

int seek(int diskid, int blockid) {
  int disk = jbod_client_operation(encode_operation(JBOD_SEEK_TO_DISK, diskid, 0), NULL);
  int block = jbod_client_operation(encode_operation(JBOD_SEEK_TO_BLOCK, 0, blockid), NULL);
  return (disk || block) ? -1 : 0; //if disk or block, return error
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
  if ((mountCheck != 0) && (buf != 0 || len == 0) && (addr >= 0) && (len <= 0x400) && (addr + len <= 0x100000)) { //check if it's valid
    uint8_t buffer[JBOD_BLOCK_SIZE];
    int remain = len;
    int used = 0;
    int diskid;
    int blockid;
    int offset;

    while (remain > 0) { // while not overflowed
      translate(addr + used, &diskid, &blockid, &offset);
      
      if (cache_lookup(diskid, blockid, buffer) == -1) { // if cache miss
        seek(diskid, blockid);
        jbod_cmd_t operation = encode_operation(JBOD_READ_BLOCK, 0, 0);
        jbod_client_operation(operation, buffer);
        cache_insert(diskid, blockid, buffer);
      }

      int copy = JBOD_BLOCK_SIZE - offset;
      copy = (copy > remain) ? remain : copy;
      memcpy(used+buf, buffer+offset, copy);
      offset = 0;
      used += copy;
      remain -= copy;
    }
    return len;
  } 
  return -1; //if not valid
}

int mdadm_write_permission(void) {
  if (jbod_client_operation(encode_operation(JBOD_WRITE_PERMISSION, 0, 0), NULL) == 0) {
    return 0;
  }
  return -1; //if not valid
}

int mdadm_revoke_write_permission(void) {
  if (jbod_client_operation(encode_operation(JBOD_REVOKE_WRITE_PERMISSION, 0, 0), NULL) == 0) {
    return 0;
  }
  return -1; //if not valid
}

int mdadm_write(uint32_t start_addr, uint32_t write_len, const uint8_t *write_buf) {
  if ((mountCheck != 0) && (start_addr >= 0) && (write_len <= 0x400) && (start_addr + write_len <= 0x100000) && (write_buf != 0 || write_len == 0)) { //makes sure that the address is within range of the length and it is mounted
    int diskid; // initialize all the variables maintained
    int blockid;
    int offset;
    int len = write_len;
    int bytesDone = 0;
    int currentAddress = start_addr;

    uint8_t buffer[JBOD_BLOCK_SIZE]; // temporary buffer

    while (currentAddress < (write_len + start_addr)) { // loop through buffer
      translate(currentAddress, &diskid, &blockid, &offset);
      seek(diskid, blockid);  // find the needed block after conversion

      if (cache_lookup(diskid, blockid, buffer) == -1) { // if cache miss
        jbod_client_operation(encode_operation(JBOD_READ_BLOCK, 0, 0), buffer); 
        cache_insert(diskid, blockid, buffer); // add to cache
      }
      
      int toWrite = (JBOD_BLOCK_SIZE - offset > len) ? len : (JBOD_BLOCK_SIZE - offset);
      memcpy(buffer + offset, write_buf + bytesDone, toWrite);
      seek(diskid, blockid);
      jbod_client_operation(encode_operation(JBOD_WRITE_BLOCK, 0, 0), buffer);
      cache_update(diskid, blockid, buffer); // update cache

      len -= toWrite;
      bytesDone += toWrite;
      currentAddress += toWrite;
    }

    return write_len; 
  } else {
    return -1;
  }
}
