 /* CSCI-4061 Fall 2022
* Group Member #1: Michael Vang vang2891
* Group Member #2: Vaibhav Jain jain0232
* Group Member #3: Matin Horri horri031
*/

#include "server.h"
#define PERM 0644

//Global Variables [Values Set in main()]
int queue_len           = INVALID;                              //Global integer to indicate the length of the queue
int cache_len           = INVALID;                              //Global integer to indicate the length or # of entries in the cache        
int num_worker          = INVALID;                              //Global integer to indicate the number of worker threads
int num_dispatcher      = INVALID;                              //Global integer to indicate the number of dispatcher threads      
FILE *logfile;                                                  //Global file pointer for writing to log file in worker


/* ************************ Global Hints **********************************/

int cacheIndex = 0;                            //[Cache]           --> When using cache, how will you track which cache entry to evict from array?
int workerIndex = 0;                            //[worker()]        --> How will you track which index in the request queue to remove next?
int dispatcherIndex = 0;                        //[dispatcher()]    --> How will you know where to insert the next request received into the request queue?
int curequest= 0;                               //[multiple funct]  --> How will you update and utilize the current number of requests in the request queue?
int dispatcherId=0;
int workerId =0;

pthread_t worker_thread[MAX_THREADS];           //[multiple funct]  --> How will you track the p_thread's that you create for workers?
pthread_t dispatcher_thread[MAX_THREADS];       //[multiple funct]  --> How will you track the p_thread's that you create for dispatchers?
int threadID[MAX_THREADS];                      //[multiple funct]  --> Might be helpful to track the ID's of your threads in a global array

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;        //What kind of locks will you need to make everything thread safe? [Hint you need multiple]
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t req_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dispatch_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t some_content = PTHREAD_COND_INITIALIZER;  //What kind of CVs will you need  (i.e. queue full, queue empty) [Hint you need multiple]
pthread_cond_t free_space = PTHREAD_COND_INITIALIZER;
pthread_cond_t notempty_request = PTHREAD_COND_INITIALIZER;  //IMPLEMENTED for notempty & empty requests
pthread_cond_t empty_request = PTHREAD_COND_INITIALIZER;
pthread_cond_t full_request = PTHREAD_COND_INITIALIZER;  //IMPLEMENTED full request
pthread_cond_t notfull_request = PTHREAD_COND_INITIALIZER;
request_t req_entries[MAX_QUEUE_LEN];                    //How will you track the requests globally between threads? How will you ensure this is thread safe?
cache_entry_t* cache_entries;                                  //[Cache]  --> How will you read from, add to, etc. the cache? Likely want this to be global

/**********************************************************************************/


/*
  THE CODE STRUCTURE GIVEN BELOW IS JUST A SUGGESTION. FEEL FREE TO MODIFY AS NEEDED
*/


/* ******************************** Cache Code  ***********************************/

// Function to check whether the given request is present in cache
int getCacheIndex(char *request){
  /* TODO (GET CACHE INDEX)
  *    Description:      return the index if the request is present in the cache otherwise return INVALID
  */
  int i;
  int cache_hit = 0;
  
  for (i=0; i<cache_len; i++){
    if(cache_entries[i].request == NULL) {
      continue;
    }
    else if (strcmp(cache_entries[i].request, request) == 0){
      cache_hit = i;
      return cache_hit;
    } 
  }
  return INVALID;
}

// Function to add the request and its file content into the cache
void addIntoCache(char *mybuf, char *memory , int memory_size){
  /* TODO (ADD CACHE)
  *    Description:      It should add the request at an index according to the cache replacement policy
  *                      Make sure to allocate/free memory when adding or replacing cache entries
  */
  //If it already exist in the cache, no need to readd it.
  if(getCacheIndex(mybuf) != -1) {
    return;
  }

  if(cacheIndex == cache_len-1) {
    cacheIndex = 0;
  }

  char* temp_req = (char*)malloc(strlen(mybuf)+1);
  char* temp_memory = (char*)malloc(memory_size+1);
  
  strcpy(temp_memory,memory);
  strcpy(temp_req, mybuf);

  cache_entry_t temp_cache;
  temp_cache.request = temp_req;
  temp_cache.content = temp_memory; 
  temp_cache.len = memory_size;
  

  //Check to see if there is an existing cache entry. NULL is used
  //because of initCache() initalization of NULL values. 
  if(cache_entries[cacheIndex].request != NULL) { 
    free(cache_entries[cacheIndex].request); // FREE C & D
  }
  
  cache_entries[cacheIndex] = temp_cache;
  cacheIndex++;
}

// Function to clear the memory allocated to the cache
void deleteCache(){
  /* TODO (CACHE)
  *    Description:      De-allocate/free the cache memory
  */
  for(int i = 0; i < cache_len; i++) {
    free(cache_entries[i].request); //FREE C
  }
  free(cache_entries);
}

// Function to initialize the cache
void initCache(){
  /* TODO (CACHE)
  *    Description:      Allocate and initialize an array of cache entries of length cache size
  */
  cache_entries = (cache_entry_t*) malloc(cache_len * sizeof(cache_entry_t));
  for(int i = 0; i< cache_len; i++){
      cache_entries[i].len = 0;
      cache_entries[i].request = NULL;
      cache_entries[i].content = NULL;
  }
}

/**********************************************************************************/

/* ************************************ Utilities ********************************/
// Function to get the content type from the request
char* getContentType(char *mybuf) {

  /* TODO (Get Content Type)
  *    Description:      Should return the coinitntent type based on the file type in the request
  *                      (See Section 5 in Project description for more details)
  *    Hint:             Need to check the end of the string passed in to check for .html, .jpg, .gif, etc.
  */
      char *type;
      if((type = strstr(mybuf, ".html")) != NULL) {
          return "text/html";
      }
      else if((type = strstr(mybuf, ".jpg")) != NULL){
          return "image/jpeg";
      }
      else if((type = strstr(mybuf, ".gif")) != NULL){
          return "image/gif";
      }
      else{
        return "text/plain";
      }
  }


// Function to open and read the file from the disk into the memory. Add necessary arguments as needed
// Hint: caller must malloc the memory space
int readFromDisk(char *mybuf, void **memory) {
  //    Description: Try and open requested file, return INVALID if you cannot meaning error
  /* TODO 
  *    Description:      Find the size of the file you need to read, read all of the contents into a memory location and return the file size
  *    Hint:             Using fstat or fseek could be helpful here
  *                      What do we do with files after we open them?
  */
  FILE *ptr;
  if((ptr = fopen(mybuf, "r")) == NULL){
    fprintf (stderr, "ERROR: Fail to open the file.\n");
    return INVALID;
  }


  fseek(ptr, 0, SEEK_END);
  int filesize = ftell(ptr);
  rewind(ptr);

  *memory = malloc(filesize + 1);
  fread(memory[0], filesize, 1, ptr);

  fclose(ptr);
  return filesize;
}

/**********************************************************************************/

// Function to receive the path)request from the client and add to the queue
void * dispatch(void *arg) {

  /********************* DO NOT REMOVE SECTION - TOP     *********************/


  /* TODO (B.I)
  *    Description:      Get the id as an input argument from arg, set it to ID
  */
  dispatcherId = *(int *) arg;
  while (1) {

    /* TODO (FOR INTERMEDIATE SUBMISSION)
    *    Description:      Receive a single request and print the conents of that request
    *                      The TODO's below are for the full submission, you do not have to use a 
    *                      buffer to receive a single request 
    *    Hint:             Helpful Functions: int accept_connection(void) | int get_request(int fd, char *filename
    *                      Recommend using the request_t structure from server.h to store the request. (Refer section 15 on the project write up)
    */

    /* TODO (B.II)
    *    Description:      Accept client connection
    *    Utility Function: int accept_connection(void) //utils.h => Line 24
    */
    int fd_connect;
    if((fd_connect=accept_connection()) > INVALID) {
        //Process the request

        char file_name[BUFF_SIZE];
        /* TODO (B.III)
         *    Description:      Get request from the client
         *    Utility Function: int get_request(int fd, char *filename); //utils.h => Line 41
         */
        if( get_request(fd_connect, file_name) == 0) {
         /* TODO (B.IV)
          *    Description:      Add the request into the queue
          */
          
            //(1) Copy the filename from get_request into allocated memory to put on request queue     
            char *temp_name = (char*)malloc(strlen(file_name)+1);   //MALLOC #A
            request_t tempreq;

            memset(temp_name, '\0', strlen(file_name)+1);
            sprintf(temp_name, "%s", file_name);
            tempreq.fd = fd_connect;
            tempreq.request = temp_name;

            //(2) Request thread safe access to the request queue
            pthread_mutex_lock(&dispatch_queue_mutex);
            
            //(3) Check for a full queue... wait for an empty one which is signaled from req_queue_notfull
            while(curequest > queue_len) {
              pthread_cond_wait(&notfull_request, &dispatch_queue_mutex);
            }
            //(4) Insert the request into the queue
            req_entries[dispatcherIndex] = tempreq;
            //(5) Update the queue index in a circular fashion
            curequest++;
            
            if(dispatcherIndex == queue_len-1) {
              dispatcherIndex = 0;
            }
            else {
              dispatcherIndex++;
            }
            
            //(6) Release the lock on the request queue and signal that the queue is not empty anymore
            pthread_cond_signal(&notempty_request);
            pthread_mutex_unlock(&dispatch_queue_mutex);
            fprintf(stderr, "Dispatcher Received Request: fd[%d] request[%s]\n", tempreq.fd, tempreq.request);
        }
    }
  }
  return NULL;
}

/**********************************************************************************/
// Function to retrieve the request from the queue, process it and then return a result to the client
void * worker(void *arg) {
  /********************* DO NOT REMOVE SECTION - BOTTOM      *********************/

  // Helpful/Suggested Declarations
  int num_request = 0;                                    //Integer for tracking each request for printing into the log
  bool cache_hit  = false;                                //Boolean flag for tracking cache hits or misses if doing 
  int file_size    = 0;                                    //Integer for holding the file size returned from readFromDisk or the cache
  void *memory    = NULL;                                 //memory pointer where contents being requested are read and stored
  int fd          = INVALID;                              //Integer to hold the file descriptor of incoming request
  char mybuf[BUFF_SIZE];                                  //String to hold the file path from the request
  char type[BUFF_SIZE];
  char error[BUFF_SIZE];

  /* TODO (C.I)
  *    Description:      Get the id as an input argument from arg, set it to ID
  *     This is an int from 0 to num_worker -1 indicating the thread index of a request handling worker
  */
  workerId = *(int *) arg;


  while (1) {
    /* TODO (C.II)
    *    Description:      Get the request from the queue and do as follows
    */
    //(1) Request thread safe access to the request queue by getting the req_queue_mutex lock
    pthread_mutex_lock(&req_queue_mutex);

    //(2) While the request queue is empty conditionally wait for the request queue lock once the not empty signal is raised
    while(curequest == 0) {
      pthread_cond_wait(&notempty_request, &req_queue_mutex); //Wait till there's some requests in request
    }

    //(3) Now that you have the lock AND the queue is not empty, read from the request queue
    request_t tempreq;
    tempreq = req_entries[workerIndex];
    strcpy(mybuf, tempreq.request); //Put file path to mybuf
    free(tempreq.request); 
    num_request = workerIndex;
    
    fd = tempreq.fd;
    tempreq.fd = 0;

    //(4) Update the request queue & remove index in a circular fashion
    curequest--;
    
    if(workerIndex == queue_len-1) {
      workerIndex = 0;
    } else {
      workerIndex++;
    }
    
    //(5) Check for a path with only a "/" if that is the case add index.html to it
    if(strcmp(mybuf,"/") == 0) {
      strcpy(mybuf, "/index.html");
    }

    //(6) Fire the request queue not full signal to indicate the queue has a slot opened up and release the request queue lock
    pthread_cond_signal(&notfull_request);
    pthread_mutex_unlock(&req_queue_mutex);
    
          
    /* TODO (C.III)
    *    Description:      Get the data from the disk or the cache 
    *    Local Function:   int readFromDisk(//necessary arguments//);
    *                      int getCacheIndex(char *request);  
    *                      void addIntoCache(char *mybuf, char *memory , int memory_size);  
    */
    int cache_hit_index = getCacheIndex(mybuf);

    //Move mybuf by 1 to remove / so it can be read
    char* mybuffer = mybuf+1;

    //If was found in Cache.
    if(cache_hit_index == -1){

      file_size = readFromDisk(mybuffer, &memory);

      //If sucessful, add to cache.
      if(file_size != -1) {
        addIntoCache(mybuf, memory, file_size);
        cache_hit = false;
      } else{
        sprintf(error, "%s", mybuf);            
        printf("Error opening: %s\n", mybuf);
      }
    } else {
      cache_hit = true;
      file_size = cache_entries[cache_hit_index].len;
      memory = cache_entries[cache_hit_index].content;
    }
    /* TODO (C.IV)
    *    Description:      Log the request into the file and terminal
    *    Utility Function: LogPrettyPrint(FILE* to_write, int threadId, int requestNumber, int file_descriptor, char* request_str, int num_bytes_or_error, bool cache_hit);
    *    Hint:             Call LogPrettyPrint with to_write = NULL which will print to the terminal
    *                      You will need to lock and unlock the logfile to write to it in a thread safe manor
    */
    pthread_mutex_lock(&log_lock);
    LogPrettyPrint(logfile, workerId, num_request, fd, mybuf, file_size, cache_hit);
    LogPrettyPrint(NULL, workerId, num_request, fd, mybuf, file_size, cache_hit);
    pthread_mutex_unlock(&log_lock);

    /* TODO (C.V)
    *    Description:      Get the content type and return the result or error
    *    Utility Function: (1) int return_result(int fd, char *content_type, char *buf, int numbytes); //look in utils.h 
    *                      (2) int return_error(int fd, char *buf); //look in utils.h 
    */
    
    if(file_size > 0) {
      strcpy(type, getContentType(mybuf));
    }

    if(return_result(fd, type, memory, file_size) != 0) {
      return_error(fd, error);
    }
    free(memory);
  }
  return NULL;
}

/**********************************************************************************/

//Executes the web server by taking in the necessary arguments and initializing the cache, workers, and dispatchers
int main(int argc, char **argv) {

  /********************* Dreturn resulfO NOT REMOVE SECTION - TOP     *********************/
  // Error check on number of arguments
  if(argc != 7){
    printf("usage: %s port path num_dispatcher num_workers queue_length cache_size\n", argv[0]);
    return -1;
  }

  int port            = -1;
  char path[PATH_MAX] = "no path set\0";
  num_dispatcher      = -1;                               //global variable
  num_worker          = -1;                               //global variable
  queue_len           = -1;                               //global variable
  cache_len           = -1;                               //global variable


  /********************* DO NOT REMOVE SECTION - BOTTOM  *********************/
  /* TODO (A.I)
  *    Description:      Get the input args --> (1) port (2) path (3) num_dispatcher (4) num_workers  (5) queue_length (6) cache_size
  */
  port = atoi(argv[1]);
  strcpy(path, argv[2]);
  num_dispatcher = atoi(argv[3]);
  num_worker = atoi(argv[4]);
  queue_len = atoi(argv[5]);
  cache_len = atoi(argv[6]);


  /* TODO (A.II)
  *    Description:     Perform error checks on the input arguments
  *    Hints:           (1) port: {Should be >= MIN_PORT and <= MAX_PORT} | (2) path: {Consider checking if path exists (or will be caught later)}
  *                     (3) num_dispatcher: {Should be >= 1 and <= MAX_THREADS} | (4) num_workers: {Should be >= 1 and <= MAX_THREADS}
  *                     (5) queue_length: {Should be >= 1 and <= MAX_QUEUE_LEN} | (6) cache_size: {Should be >= 1 and <= MAX_CE}
  */

  if(port > MAX_PORT || port < MIN_PORT) {
    printf("Port ERROR\n");
    return -1;
  }

  DIR* dir = opendir(path);
  if(dir) {
    closedir(dir);
  } else {
    printf("Path error\n");
    return -1;
  }

  if(num_dispatcher < 1 || num_dispatcher > MAX_THREADS) {
    printf("Dispatcher error\n");
    return -1;
  }

  if(num_worker < 1 || num_worker > MAX_THREADS) {
    printf("Workers error \n");
    return -1;
  }

  if (queue_len < 1 || queue_len > MAX_THREADS) {
    printf("queue len exceeded error \n");
    return -1;
  }

  if (cache_len < 1 || cache_len > MAX_CE) {
    printf("Cache sizing error \n");
    return -1;
  }
 
  /********************* DO NOT REMOVE SECTION - TOP    *********************/
  printf("Arguments Verified:\n\
    Port:           [%d]\n\
    Path:           [%s]\n\
    num_dispatcher: [%d]\n\
    num_workers:    [%d]\n\
    queue_length:   [%d]\n\
    cache_size:     [%d]\n\n", port, path, num_dispatcher, num_worker, queue_len, cache_len);
  /********************* DO NOT REMOVE SECTION - BOTTOM  *********************/


  /* TODO (A.III)
  *    Description:      Open log file
  *    Hint:             Use Global "File* logfile", use "web_server_log" as the name, what open flags do you want?
  */

  logfile = fopen("./web_server_log", "a");

  /* TODO (A.IV)
  *    Description:      Change the current working directory to server root directory
  *    Hint:             Check for error!
  */
  if(chdir(path)==INVALID) {
    perror("Path Invalid!");
    return -1;
 }

  char cwd[1024];
   if (getcwd(cwd, sizeof(cwd)) != NULL) {
       printf("Current working dir: %s\n", cwd);
   } else {
       perror("getcwd() error");
       return 1;
   }


  /* TODO (A.V)
  *    Description:      Initialize cache  
  *    Local Function:   void    initCache();
  */
  initCache();

  /* TODO (A.VI)
  *    Description:      Start the server
  *    Utility Function: void init(int port); //look in utils.h 
  */
  init(port);

  /* TODO (A.VII)
  *    Description:      Create dispatcher and worker threads 
  *    Hints:            Use pthread_create, you will want to store pthread's globally
  *                      You will want to initialize some kind of global array to pass in thread ID's
  *                      How should you track this p_thread so you can terminate it later? [global]
  */

  for(int i=0; i<num_worker; i++) {
    threadID[i] = i;
  }

  //DISPATCHER
  for(int i = 0; i < num_dispatcher; i++) {
    if(pthread_create(&(dispatcher_thread[i]), NULL, dispatch, (void *) &threadID[0]) != 0) {
      printf("Thread %d failed to create\n", i);
    }
    printf("Dispatcher                    [  %d] Started\n", i);
    threadID[i] = dispatcher_thread[i];
  }

  //WORKER
  for(int i = 0; i < num_worker; i++) {
    if(pthread_create(&(worker_thread[i]), NULL, worker, (void *) &threadID[0]) != 0) {
      printf("Thread %d failed to create \n", i);
    }
    printf("Worker                        [  %d] Started\n", i);
  }

  // Wait for each of the threads to complete their work
  // Threads (if created) will not exit (see while loop), but this keeps main from exiting
  int i;
  for(i = 0; i < num_worker; i++){
    fprintf(stderr, "JOINING WORKER %d \n",i);
    if((pthread_join(worker_thread[i], NULL)) != 0){
      printf("ERROR : Fail to join worker thread %d.\n", i);
    }
  }
  for(i = 0; i < num_dispatcher; i++){
    fprintf(stderr, "JOINING DISPATCHER %d \n",i);
    if((pthread_join(dispatcher_thread[i], NULL)) != 0){
      printf("ERROR : Fail to join dispatcher thread %d.\n", i);
    }
  }
  deleteCache();
  fprintf(stderr, "SERVER DONE \n");
}