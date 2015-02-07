#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <signal.h>
#include <semaphore.h>
#include <sched.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "structs.h"
#include <vector>

#define SERVICES_BEGIN_INDEX 5
#define OPEN_SHARED_MEM_ERROR "Error in shm_open"
#define OPEN_SEMAPHORE_ERROR "Error in sem_open"
#define PARAMETER_MISSING_ERROR "One or more parameters missing"
#define COMMA ','
#define TWO_POINTS ':'
#define WHITE_SPACE ' '

using namespace std;

string semaphorePrefix;
string memSharedPrefix;
string FRONT_END_PATH =
        "/home/juanrivillas/MemSharedPractice/roadtothepodium/bin/frontEnd";
string MIDDLE_END_PATH =
       "/home/juanrivillas/MemSharedPractice/roadtothepodium/bin/middleEnd";
string BACK_END_PATH =
       "/home/juanrivillas/MemSharedPractice/roadtothepodium/bin/backEnd";
unsigned int defaultBackendSize = 1;
unsigned int defaultQueueSize = 1;
off_t memorySize = 0;

const string S = "-s";
const string C = "-c";
const string B = "-b";
const string M = "-m";
const string P = "-p";
const string emptySemSufix = "0";
const string fullSemSufix = "1";
const string mutexSemSufix = "2";
const string backEndPrefix = "10";
const string memorySemPrefix = "ShdMemory";

off_t setMemorySize (unsigned int service, unsigned int size) {
  return sizeof(HeaderShm) + size*sizeof(Service);
}

void createSemaphores (unsigned int service, unsigned int bufferSize) {

  string emptySemName = semaphorePrefix + to_string(service) + emptySemSufix;
  string fullSemName = semaphorePrefix + to_string(service) + fullSemSufix;
  string mutexSemName = semaphorePrefix + to_string(service) + mutexSemSufix;
  string shdMemSemName = memorySemPrefix + to_string(service);

  sem_t * emptySemaphore = sem_open(emptySemName.c_str(),
                                    O_CREAT | O_EXCL, 0777, bufferSize);

  sem_t * fullSemaphore = sem_open(fullSemName.c_str(),
                                    O_CREAT | O_EXCL, 0777, 0);

  sem_t * mutexSemaphore = sem_open(mutexSemName.c_str(),
                                    O_CREAT | O_EXCL, 0777, 1);

  sem_t * sharedMemSemaphore = sem_open(shdMemSemName.c_str(),
                                    O_CREAT | O_EXCL, 0777, 1);

  if (emptySemaphore == SEM_FAILED || fullSemaphore == SEM_FAILED
      || mutexSemaphore == SEM_FAILED || sharedMemSemaphore == SEM_FAILED) {

    cerr << OPEN_SEMAPHORE_ERROR << endl;
    exit(1);

  }

}

void createMiddleEnd(int service) {

  int status;
  char * newargv[] = { NULL , const_cast<char *> (M.c_str()) ,
                       const_cast<char *> (memSharedPrefix.c_str()),
                       const_cast<char *> (P.c_str()),
                       const_cast<char *> (semaphorePrefix.c_str()),
                       const_cast<char *> (to_string(service).c_str()), NULL };
  char * newenviron[] = { NULL };

  pid_t middle_end = fork();

  if (middle_end == 0) {

    newargv[0] = const_cast<char *> (MIDDLE_END_PATH.c_str());
    execve(MIDDLE_END_PATH.c_str(), newargv, newenviron);

  }

}

void createSharedMemories (int argc, char ** argv, int index) {

  //   Parameters:
  //     PATH
  //     LENGTH
  //     PROTECTION FLAGS
  //     FLAGS
  //     FILE DESCRIPTOR
  //     OFFSET
  //     See also man mmap 2

  unsigned int service = atoi(argv[index + 1]);
  unsigned int size = atoi(argv[index + 2]);
  string sizeStr = argv[index + 2];

  bool indexS = sizeStr.find(S) < sizeStr.length();
  bool indexC = sizeStr.find(C) < sizeStr.length();
  bool indexB = sizeStr.find(B) < sizeStr.length();

  if (indexS || indexC || indexB) {
    size = defaultQueueSize;
  }

  string memoryPrefix = memSharedPrefix + to_string(service);
  const char * memoryName = memoryPrefix.c_str();
  memorySize = setMemorySize(service, size);

  // Create semaphores per service
  createSemaphores(service, size);

  int shm_fd =
            shm_open(memoryName, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);

  if (shm_fd < 0){
    cerr << OPEN_SHARED_MEM_ERROR << endl;
  } else {

    ftruncate(shm_fd, 1024);

    int * shared_memory =
              (int *) mmap(NULL, 1024, PROT_READ | PROT_WRITE,
               MAP_SHARED, shm_fd, 0);

    BufferInMiddleEnd buffer;
    buffer.sequence = 214;
    buffer.number1 = 8;
    buffer.number2 = 19;
    buffer.delay = 2848;

    HeaderShm * header = (HeaderShm *) shared_memory;
    header->size = size;
    header->nroShm = service;

    Service * serviceInMem = (Service *) shared_memory + sizeof(HeaderShm);
    serviceInMem->in = 0;
    serviceInMem->out = 0;
    serviceInMem->items[size];

    createMiddleEnd(service);

  }

};

void setDefaultQueueSize (char ** argv, int i) {

  string size = argv[i+1];
  defaultQueueSize = atoi(size.c_str());

}

void setBackEndQueueSize (char ** argv, int i) {

  string size = argv[i+1];
  defaultBackendSize = atoi(size.c_str());

}

void setBackEndEnvironment () {

  string emptySemName = semaphorePrefix + backEndPrefix + emptySemSufix;
  string fullSemName = semaphorePrefix + backEndPrefix + fullSemSufix;
  string mutexSemName = semaphorePrefix + backEndPrefix + mutexSemSufix;
  string shdMemSemName = memorySemPrefix + backEndPrefix;
  string memoryNameStr = memSharedPrefix + backEndPrefix;
  char * memoryName = const_cast<char*> (memoryNameStr.c_str());

  sem_t * emptySemaphore = sem_open(emptySemName.c_str(),
                                  O_CREAT | O_EXCL, 0777, defaultBackendSize);

  sem_t * fullSemaphore = sem_open(fullSemName.c_str(),
                                    O_CREAT | O_EXCL, 0777, 0);

  sem_t * mutexSemaphore = sem_open(mutexSemName.c_str(),
                                    O_CREAT | O_EXCL, 0777, 1);

  sem_t * sharedMemSemaphore = sem_open(shdMemSemName.c_str(),
                                    O_CREAT | O_EXCL, 0777, 1);

  if (emptySemaphore == SEM_FAILED || fullSemaphore == SEM_FAILED
      || mutexSemaphore == SEM_FAILED || sharedMemSemaphore == SEM_FAILED) {

    cerr << OPEN_SEMAPHORE_ERROR << endl;
    exit(1);

  } else {

    int shm_fd =
            shm_open(memoryName, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);

    if (shm_fd < 0){
      cerr << OPEN_SHARED_MEM_ERROR << endl;
    } else {

      ftruncate(shm_fd, 1024);

      int * shared_memory =
              (int *) mmap(NULL, 1024, PROT_READ | PROT_WRITE,
               MAP_SHARED, shm_fd, 0);

      BackEndService * serviceInMem = (BackEndService *) shared_memory;
      serviceInMem->in = 0;
      serviceInMem->out = 0;
      serviceInMem->items[defaultBackendSize];

    }

  }

}

void initServices (int argc, char * argv[]) {

    /* Going to extract only the services from the chain. At this point we
       know that the headers of memory shared and semaphores were extracted
       so we don't have to validate anymore
    */

  int index = argc - SERVICES_BEGIN_INDEX;

  for (int i = argc - 1; i >= SERVICES_BEGIN_INDEX; i--) {

    string parameter = argv[i];
    /* Check special characters
    -s: Service
    -c: Default Queue
    -b: Backend Queue
    */
    if (parameter.find(S) < parameter.length()) {
      createSharedMemories(argc, argv, i);
    } else if (parameter.find(C) < parameter.length()) {
      setDefaultQueueSize(argv, i);
    } else if (parameter.find(B) < parameter.length()) {
      setBackEndQueueSize(argv, i);
    }

  }

  setBackEndEnvironment();

}

void createBackEnd () {

  char * newargv[] = { NULL };
  char * newenviron[] = { NULL };
  pid_t back_end = fork();

  if (back_end == 0) {

    const char * memoryPrefixConst = memSharedPrefix.c_str();
    const char * semaphorePrefixConst = semaphorePrefix.c_str();

    newargv[0] = const_cast<char *> (BACK_END_PATH.c_str());
    newargv[1] = const_cast<char *> (M.c_str());
    newargv[2] = const_cast<char *> (memSharedPrefix.c_str());
    newargv[3] = const_cast<char *> (P.c_str());
    newargv[4] = const_cast<char *> (semaphorePrefix.c_str());
    newargv[5] = NULL;

    // Call Front End process and change its image in order not to come back
    execve(BACK_END_PATH.c_str(), newargv, newenviron);

  }

}

void createMemoryEnvironment (int argc, char * argv[]) {

  /* Extract memory and semaphore prefixes to create de shared memory */
  for (int i = 0; i < argc; i++) {

    string parameter = argv[i];

    if (parameter.find(M) < parameter.length()) {
      memSharedPrefix = argv[i+1];
    } else if (parameter.find(P) < parameter.length()) {
      semaphorePrefix = argv[i+1];
    }

  }

  if (memSharedPrefix.empty() || semaphorePrefix.empty()) {
    cout << PARAMETER_MISSING_ERROR << endl;
    exit(0);
  }

  initServices(argc, argv );

}

void createFrontEnd(int argc, char * argv[]) {

  pid_t front_end_parent;
  int status;
  char * newargv[] = { NULL };
  char * newenviron[] = { NULL };
  pid_t front_end = fork();

  if (front_end == 0) {

    const char * memoryPrefixConst = memSharedPrefix.c_str();
    const char * semaphorePrefixConst = semaphorePrefix.c_str();

    newargv[0] = const_cast<char *> (FRONT_END_PATH.c_str());
    newargv[1] = const_cast<char *> (M.c_str());
    newargv[2] = const_cast<char *> (memSharedPrefix.c_str());
    newargv[3] = const_cast<char *> (P.c_str());
    newargv[4] = const_cast<char *> (semaphorePrefix.c_str());
    newargv[5] = NULL;

    // Call Front End process and change its image in order not to come back
    execve(FRONT_END_PATH.c_str(), newargv, newenviron);

  } else {

    createBackEnd();
    front_end_parent = wait(&status);

  }

}

void cleanMemory () {

  /*
  Going to delete 3 semaphores and one shared memory per service and also
  it is going to delete the shared memory semaphore that is controlling
  the access to it
  */

  for (int i = 0; i <= 10; ++i) {

    string index = memSharedPrefix + to_string(i);
    string emptySemName = semaphorePrefix + to_string(i) + emptySemSufix;
    string fullSemName = semaphorePrefix + to_string(i) + fullSemSufix;
    string mutexSemName = semaphorePrefix + to_string(i) + mutexSemSufix;
    string shdMemSemName = memorySemPrefix + to_string(i);

    sem_unlink(emptySemName.c_str());
    sem_unlink(fullSemName.c_str());
    sem_unlink(mutexSemName.c_str());
    shm_unlink(index.c_str());
    sem_unlink(shdMemSemName.c_str());

  }

}

int main(int argc, char *argv[]) {

  createMemoryEnvironment(argc, argv);
  createFrontEnd(argc, argv);
  sleep(10);
  cleanMemory();
  exit(EXIT_FAILURE);

}