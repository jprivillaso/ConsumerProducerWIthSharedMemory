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

//Service definitions
#define SUM 0
#define SUB 1
#define MULT 2
#define DIV 3
#define MOD 4
#define AND 5
#define OR 6
#define XOR 7
#define NAND 8
#define NOR 9

string semaphorePrefix;
string memSharedPrefix;
off_t memorySize = 1024;

const int SERVICES_NUMBER = 10;
const string emptySemSufix = "0";
const string fullSemSufix = "1";
const string mutexSemSufix = "2";
const string backEndPrefix = "10";
const string memorySemPrefix = "ShdMemory";

off_t setMemorySize (unsigned int service, unsigned int size) {
  return sizeof(HeaderShm) + size*sizeof(Service);
}


int calculate (int type, int number1, int number2) {

  int result;

  switch(type){
    case SUM:
      result = number1 + number2;
      break;
    case SUB:
      result = number1 - number2;
      break;
    case MULT:
      result = number1 * number2;
      break;
    case DIV:
      result = number1 / number2;
      break;
    case MOD:
      result = number1 % number2;
      break;
    case AND:
      result = number1 & number2;
      break;
    case OR:
      result = number1 | number2;
      break;
    case XOR:
      result = number1 ^ number2;
      break;
    case NAND:
      result = ~(number1 & number2);
      break;
    case NOR:
      result = ~(number1 | number2);
      break;
    default:
      cout << UNRECOGNIZED_OPERATION_ERR << endl;
      break;
  }

  return result;

}

void produceBackEnd (short service, int sequence, long long result) {

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

      BufferInBackEnd buffer;
      buffer.sequence = sequence;
      buffer.service = service;
      buffer.result = result;

      ftruncate(shm_fd, 1024);
      int * shared_memory = (int *) mmap(NULL, 1024,
      PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

      sem_wait(empty);
      sem_wait(mutex);

      BackEndService * serviceInMem = (BackEndService *) shared_memory;
      int actualPosition = serviceInMem->in;
      int newPosition = (actualPosition+1) % size;
      serviceInMem->items[serviceInMem->in] = buffer;
      serviceInMem->in = newPosition;

      sem_post(mutex);
      sem_post(full);

    }
  }
}

void consume (int serviceId) {

  int shm_fd;
  int size;

  string emptySemName = semaphorePrefix + to_string(serviceId) + emptySemSufix;
  string fullSemName = semaphorePrefix + to_string(serviceId) + fullSemSufix;
  string mutexSemName = semaphorePrefix + to_string(serviceId) + mutexSemSufix;
  string sharedMemSemName = memorySemPrefix + to_string(serviceId);
  string sharedMemName = memSharedPrefix + to_string(serviceId);

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
        Service * service = (Service *) shared_memory + sizeof(HeaderShm);

        int delay = service->items[service->out].delay;
        int number1 = service->items[service->out].number1;
        int number2 = service->items[service->out].number2;
        int sequence = service->items[service->out].sequence;
        long long result = calculate(serviceId, number1, number2);
        usleep(delay*1000);
        produceBackEnd(serviceId, sequence, result);
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

  if (argc != 5) {

    memSharedPrefix = argv[2];
    semaphorePrefix = argv[4];
    string service = argv[5];

    consume(atoi(service.c_str()));

  }

  return 0;

}