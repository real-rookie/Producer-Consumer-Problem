#include <pthread.h>
#include <semaphore.h>
#include <chrono>
#include <iostream>
#include <cassert>
#include <queue>
#include <string>
#include <unordered_map>
#include <sstream>

void Trans( int n );
void Sleep( int n );
enum Actions{ASK, RECEIVE, WORK, SLEEP, COMPLETE, END};

int log_id;
std::queue<int> buffer;
std::unordered_map<pthread_t, int> threads;
decltype(std::chrono::steady_clock::now()) begin;

pthread_mutex_t mutex_buffer = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_log = PTHREAD_MUTEX_INITIALIZER;
sem_t empty;  // empty slots in the buffer
sem_t full;  // full slots in the buffer
bool producer_done = false;

// TODO time elapsed
void log(pthread_t t_id, Actions action, int n){
    pthread_mutex_lock(&mutex_log);
    auto end = std::chrono::steady_clock::now();
    std::ostringstream oss;
    oss << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << '\t';
    oss << "ID= " << threads[t_id] << '\t';
    if(action == Actions::RECEIVE || action == Actions::WORK){
        oss << "Q= " << buffer.size() << '\t';
    }else{
        oss << "\t";
    }
    switch(action){
        case ASK:
            oss << "Ask"      << "\t\t"; break;
        case RECEIVE:
            oss << "Receive"  << "\t\t"; break;
        case WORK:
            oss << "Work"     << "\t\t"; break;
        case SLEEP:
            oss << "Sleep"    << "\t\t"; break;
        case COMPLETE:
            oss << "Complete" << "\t"  ; break;
        case END:
            oss << "End"     << "\t\t"; break;
    }
    if(n != -1){
        oss << n;
    }
    std::cout << oss.str() << std::endl;
    pthread_mutex_unlock(&mutex_log);
}

void *producer(void* arg){
    std::string cmd;
    int n;
    while(std::cin >> cmd){
        n = atoi(cmd.substr(1).c_str());
        if(cmd[0] == 'T'){
            assert((sem_wait(&empty) == 0));
            assert((pthread_mutex_lock(&mutex_buffer) == 0));
                buffer.push(n);
                log(pthread_self(), Actions::WORK, n);
            assert((pthread_mutex_unlock(&mutex_buffer) == 0));
            assert((sem_post(&full) == 0));
        }else if(cmd[0] == 'S'){
            log(pthread_self(), Actions::SLEEP, n);
            Sleep(n);
        }
    }
    log(pthread_self(), Actions::END, -1);
    producer_done = true;
}

void *consumer(void* arg){
    while(!producer_done || buffer.size()){
        log(pthread_self(), Actions::ASK, -1);
        assert((sem_wait(&full) == 0));
        assert((pthread_mutex_lock(&mutex_buffer) == 0));
            int n = buffer.front();
            buffer.pop();
            log(pthread_self(), Actions::RECEIVE, n);
        assert((pthread_mutex_unlock(&mutex_buffer) == 0));
        Trans(n);
        log(pthread_self(), Actions::COMPLETE, n);
        assert((sem_post(&empty) == 0));
    }
}

int main(int argc, char *argv[]){
    assert((argc == 2 || argc == 3));
    begin = std::chrono::steady_clock::now();
    int n_consumers = atoi(argv[1]);
    int buffer_size = n_consumers * 2;
    log_id = 0;
    if(argc == 3){
        log_id = atoi(argv[2]);
    }

    assert((sem_init(&empty, 0, buffer_size) == 0));  // initially all slots are empty
    assert((sem_init(&full, 0, 0) == 0));  // initially no slots are full

    pthread_t t_id;
    assert((pthread_create(&t_id, nullptr, producer, nullptr) == 0));
    threads[t_id] = 0;
    for(int i = 1; i <= n_consumers; ++i){
        assert((pthread_create(&t_id, nullptr, consumer, nullptr) == 0));
        threads[t_id] = i;
    }

    for(auto thread : threads){
        assert((pthread_join(thread.first, nullptr) == 0)); // can be used to retrieve consumer stat
    }
    
}