#include <stdio.h>
#include <algorithm>
#include <stdlib.h>
#include <sstream>
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
#include <vector>
#include "structs.h"

using namespace std;

#define SERVICES_BEGIN_INDEX 5
#define COMMA ','
#define TWO_POINTS ':'
#define WHITE_SPACE ' '

// Error definitions
#define SYNTAX_ERROR "Syntax Error. Try again"
#define MESSAGE_ERROR "Message Error. Try Again"
#define OPEN_SHARED_MEM_ERROR "Error in shm_open"
#define OPEN_SEMAPHORE_ERROR "Error in sem_open"
#define SERVICE_NOT_INITIALIZED_ERR "Service not initialized"
#define ZERO_SERVICES_ACTIVE_ERR "At least one service must be started"
#define SET_DEFAULT_SIZE_ERR "You must send a default queue size"
#define SET_BACKEND_QUEUE_ERR "You may pass backend queue size"
#define UNRECOGNIZED_OPERATION_ERR "Unrecognized operation"
#define DEFAULT_QUEUE_SIZE_ERR "Send a correct default queue size"
#define CONVERSION_EXCEPTION "Error. There is a number too big to cast"

string semaphorePrefix;
string memSharedPrefix;

const int SERVICES_NUMBER = 10;
const string emptySemSufix = "0";
const string fullSemSufix = "1";
const string mutexSemSufix = "2";
const string memorySemPrefix = "ShdMemory";


vector<string> splitMessage(string str, char separator) {

  for (int i=0; i<str.length(); i++){

    if (str[i] == separator){
      str[i] = WHITE_SPACE;
    }

  }

  vector<string> array;
  stringstream ss(str);
  string temp;
  while (ss >> temp){
    array.push_back(temp);
  }

  return array;

}

// Method took from http://www.cplusplus.com/forum/beginner/31141/
bool isNumber(string & s, bool canBeNegative) {
  /*
    if s[i] is between '0' and '9' or if it's a whitespace
    (there may be some before and after the number) it's ok.
    otherwise it isn't a number.
  */
  for(int i = 0; i < s.length(); i++) {

    if (canBeNegative) {

      /* If the number is negative, the minus must be in the first position*/
      if (s[i] == '-' && i != 0) return false;

      if(! (s[i] >= '0' && s[i] <= '9' || s[i] == ' '
        || s[i] == '-') ) return false;

    } else{
      if(! (s[i] >= '0' && s[i] <= '9' || s[i] == ' ') ) return false;
    }

  }

  return true;

}

bool allIntegersInVector(vector<string> s, bool validateRange) {

  for (int i = 0; i < s.size(); i++) {
    if (!isNumber(s.at(i) , false)) {
      return false;
    }

    if (validateRange) {
      if (!(atoi(s.at(i).c_str()) >= 0 && atoi(s.at(i).c_str()) < 10 )) {
        return false;
      }
    }

  }

  return true;

}

bool messageValidations(vector<string> messageParsed) {

  /* It is mandatory to send 5 parameters
    - Service Id
    - Services to consume
    - Parameter 1
    - Parameter 2
    - Delays
  */
  if (messageParsed.size() != 5) {
    cerr << MESSAGE_ERROR << endl;
    return false;
  }

  string serviceId = messageParsed.at(0);

  /* Service Id must not have any special character*/
  if (serviceId.find(COMMA) < serviceId.length()) {
    cerr << MESSAGE_ERROR << endl;
    return false;
  }

  /* Service id must be a number*/
  if (!isNumber(serviceId, false)) {
    cerr << MESSAGE_ERROR << endl;
    return false;
  }

  string servicesToConsume = messageParsed.at(1);
  vector<string> servicesSplitted = splitMessage(servicesToConsume, COMMA);

  /* At least on service must be consumed. Also, each service must be a
  positive integer */
  if (servicesSplitted.size() == 0 ||
     !allIntegersInVector(servicesSplitted, true)) {
    cerr << MESSAGE_ERROR << endl;
    return false;
  }

  string parameter1 = messageParsed.at(2);
  string parameter2 = messageParsed.at(3);

  /* Parameters 1 and 2 must be numbers*/
  if (!isNumber(parameter1, true) || !isNumber(parameter2, true)) {
    cerr << MESSAGE_ERROR << endl;
    return false;
  }

  try {

    string::size_type sz = 0;   // alias of size_t
    long long longParameter1 = stoll(parameter1,&sz,0);
    long long longParameter2 = stoll(parameter2,&sz,0);

  } catch (out_of_range) {
    cerr << CONVERSION_EXCEPTION << endl;
    return false;
  }

  string delays = messageParsed.at(4);
  vector<string> delaysSplitted = splitMessage(delays, COMMA);

  /* All delays must me a positive number*/
  if (!allIntegersInVector(delaysSplitted, false)) {
    cerr << MESSAGE_ERROR << endl;
    return false;
  }

  return true;

}

void startProducer (BufferInMiddleEnd actualBuffer, int serviceId) {

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

      HeaderShm * header = (HeaderShm *) shared_memory;
      Service * serviceInMem = (Service *) shared_memory + sizeof(HeaderShm);

      sem_wait(empty);
      sem_wait(mutex);
      int actualPosition = serviceInMem->in;
      int newPosition = (actualPosition+1) % size;
      serviceInMem->items[serviceInMem->in] = actualBuffer;
      serviceInMem->in = newPosition;
      sem_post(mutex);
      sem_post(full);

    }
  }
}

void setProducer (vector<string> messages) {

  string sequence = messages.at(0);
  /* We have right now the certainty that all parameters are ok so we can
  use them directly*/
  string parameter1 = messages.at(2);
  string parameter2 = messages.at(3);

  string servicesToConsume = messages.at(1);
  vector<string> servicesSplitted = splitMessage(servicesToConsume, COMMA);

  string delays = messages.at(4);
  vector<string> delaysSplitted = splitMessage(delays, COMMA);

  int servicesSize = servicesSplitted.size();
  int delaysSize = delaysSplitted.size();
  int diff = 0;

  /*
  Validating the length of delays and servicesToConsume.
  There are two heuristics:
    - If delays are greater than services array, then delays have to be omitted
    - If services are greater than delays array, then delays have to be
        repeated
  */
  if (delaysSize > servicesSize) {

    diff = delaysSize - servicesSize;
    for (int i = 0; i < diff; i++) {
      delaysSplitted.pop_back();
    }

  } else if (servicesSize > delaysSize){

    diff = servicesSize - delaysSize;
    for (int i = 0; i < diff; i++) {
      delaysSplitted.push_back(delaysSplitted.at(delaysSize-1));
    }

  }

  for (int i = 0; i < servicesSplitted.size(); i++) {

    cout << "";

    string::size_type sz = 0;   // alias of size_t

    //Create the items that are going to be produced
    BufferInMiddleEnd itemMiddleEnd;
    itemMiddleEnd.sequence = atoi(sequence.c_str());
    itemMiddleEnd.number1 = stoll(parameter1,&sz,0);
    itemMiddleEnd.number2 = stoll(parameter2,&sz,0);
    itemMiddleEnd.delay = atoi(delaysSplitted.at(i).c_str());

    //Get the service and produce the item for it
    int actualService = atoi(servicesSplitted.at(i).c_str());

    startProducer(itemMiddleEnd, actualService);

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

  string input;
  bool isMessageCorrect = false;

  if (argc != 4) {

    memSharedPrefix = argv[2];
    semaphorePrefix = argv[4];

    while (getline(cin, input)) {

      if (input == "0") {
        break;
      }

      /* If input is empty then wait again for an user input */
      if (input.empty()) {
        continue;
      }

      if (count(input.begin(), input.end(), ':') != 4) {
        cerr << SYNTAX_ERROR << endl;
        continue;
      }

      vector<string> messageParsed = splitMessage(input, TWO_POINTS);
      isMessageCorrect = messageValidations(messageParsed);

      if (isMessageCorrect) {
        setProducer(messageParsed);
      }

    }

  }

  return 0;

}