struct BufferInMiddleEnd {
  int sequence;
  long long number1;
  long long number2;
  unsigned int delay;
};

struct HeaderShm {
  unsigned int nroShm;
  unsigned int size;
};

struct Service {
  int in;
  int out;
  BufferInMiddleEnd items[];
};

struct BufferInBackEnd {
  int sequence;
  short service;
  long long result;
};

struct FrontEnd {
  int defaultQueueSize;
  int activeServices;
  bool backendQueueSent;
  bool defaultQueueSent;
};

struct BackEndService{
  int in;
  int out;
  BufferInBackEnd items[];
};

struct MemorySize {
  off_t size;
};