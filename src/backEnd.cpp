#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <sched.h>
#include <semaphore.h>
#include "structs.h"

using namespace std;

// Error definitions
#define OPEN_SHARED_MEM_ERROR "Error in shm_open"
#define OPEN_SEMAPHORE_ERROR "Error in sem_open"
#define UNRECOGNIZED_OPERATION_ERR "Unrecognized operation"

string semaphorePrefix;
string memSharedPrefix;
off_t memorySize = 1024;

const string emptySemSufix = "0";
const string fullSemSufix = "1";
const string mutexSemSufix = "2";
const string backEndPrefix = "10";
const string memorySemPrefix = "ShdMemory";

void consume () {

  int shm_fd;
  int size;

  string emptySemName = semaphorePrefix + backEndPrefix + emptySemSufix;
  string fullSemName = semaphorePrefix + backEndPrefix + fullSemSufix;
  string mutexSemName = semaphorePrefix + backEndPrefix + mutexSemSufix;
  string sharedMemSemName = memorySemPrefix + backEndPrefix;
  string sharedMemName = memSharedPrefix + backEndPrefix;

  sem_t * empty = sem_open(emptySemName.c_str(), 0);

  sem_t * full = sem_open(fullSemName.c_str(), 0);

  sem_t * mutex = sem_open(mutexSemName.c_str(), 0);

  sem_t * memoryMutex = sem_open(sharedMemSemName.c_str(), 0);

  if (empty == SEM_FAILED || full == SEM_FAILED
      || mutex == SEM_FAILED || memoryMutex == SEM_FAILED) {

    cerr << OPEN_SEMAPHORE_ERROR << endl;

  } else {

    if ((shm_fd = shm_open(sharedMemName.c_str(),
         O_EXCL | O_RDWR , 0666)) < 0) {
      cerr << OPEN_SHARED_MEM_ERROR << endl;
    } else {

      ftruncate(shm_fd, 1024);
      int * shared_memory = (int *) mmap(NULL, 1024,
      PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

      while(true) {

        sem_wait (full);
        sem_wait(mutex);

        HeaderShm * header = (HeaderShm *) shared_memory;
        BackEndService * service =
                    (BackEndService *) shared_memory;

        int sequence = service->items[service->out].sequence;
        short serviceId = service->items[service->out].service;
        long long result = service->items[service->out].result;
        cout << sequence << ":" << serviceId << ":" << result << endl;
        service->out = ((service->out) + 1) % size;

        sem_post(mutex);
        sem_post(empty);

      }

    }

  }

}

/*
  The parameters that the main is going to reveive
  are as follows:
    -m
    <sharedMemoryPrefix>
    -p
    <semaphoresPrefix>
*/
int
main (int argc, char * argv[]) {

  if (argc != 4) {

    memSharedPrefix = argv[2];
    semaphorePrefix = argv[4];

    cout << "";
    consume();

  }

  return 0;

}