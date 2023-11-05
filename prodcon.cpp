#include <pthread.h>
#include <semaphore.h>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <queue>
#include <vector>
#include <string>
#include <unordered_map>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits>

struct stat_producer{
    int work;
    int sleep;
};

struct stat_consumer{
    int ask;
    int receive;
    int complete;
};

void Trans( int n );
void Sleep( int n );
enum Actions{ASK, RECEIVE, WORK, SLEEP, COMPLETE, END, EXIT};
const int POSION_PILL = std::numeric_limits<int>::min();

std::queue<int> buffer;
std::unordered_map<pthread_t, int> threads;
decltype(std::chrono::steady_clock::now()) begin;

pthread_mutex_t mutex_buffer = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_log = PTHREAD_MUTEX_INITIALIZER;
sem_t empty;  // empty slots in the buffer
sem_t full;  // full slots in the buffer

// TODO time elapsed
void log(pthread_t t_id, Actions action, int n){
    using namespace std::chrono;
    assert((pthread_mutex_lock(&mutex_log) == 0));
    auto end = std::chrono::steady_clock::now();
    std::ostringstream oss;
    oss << std::setprecision(3) << std::fixed
        << duration_cast<microseconds>(end - begin).count() / double(1e6)
        << '\t';
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
        case EXIT:
            oss << "Exit"     << "\t\t"; break;
    }
    if(n != -1){
        oss << n;
    }
    std::cout << oss.str() << std::endl;
    assert((pthread_mutex_unlock(&mutex_log) == 0));
}

void summary(stat_producer &stat_p, std::vector<stat_consumer> &stat_c){
    using namespace std::chrono;
    auto end = std::chrono::steady_clock::now();
    int ask = 0;
    int receive = 0;
    int complete = 0;
    for(auto stat : stat_c){
        ask += stat.ask;
        receive += stat.receive;
        complete += stat.complete;
    }
    double trans_per_sec = (stat_p.work - 1) / (duration_cast<microseconds>(end - begin).count() / double(1e6));
    std::ostringstream oss;
    oss << "Summary:" << std::endl;
    oss << "\tWork\t\t" << stat_p.work - 1 << std::endl;
    oss << "\tAsk\t\t" << ask << std::endl;
    oss << "\tReceive\t\t" << receive << std::endl;
    oss << "\tComplete\t" << complete << std::endl;
    oss << "\tSleep\t\t" << stat_p.sleep << std::endl;
    int thread_i = 0;
    for(auto stat : stat_c){
        oss << "\tThread\t" << ++thread_i << '\t' << stat.complete << std::endl;
    }

    oss << "Transactions per second:\t"
        << std::setprecision(2) << std::fixed
        << trans_per_sec
        << std::endl;
    std::cout << oss.str() << std::endl;
    }

void *producer(void *arg){
    stat_producer *stat = (stat_producer*)arg;
    std::string cmd;
    int n;
    bool input_eof = false;
    while(true){
        std::cin >> cmd;
        if(std::cin.eof()){
            input_eof = true;
        }
        n = input_eof ? POSION_PILL : atoi(cmd.substr(1).c_str());
        if(input_eof || cmd[0] == 'T'){
            assert((sem_wait(&empty) == 0));
            assert((pthread_mutex_lock(&mutex_buffer) == 0));
                buffer.push(n);
                log(pthread_self(), Actions::WORK, n);
                ++stat->work;
            assert((pthread_mutex_unlock(&mutex_buffer) == 0));
            assert((sem_post(&full) == 0));
        }else if(cmd[0] == 'S'){
            log(pthread_self(), Actions::SLEEP, n);
            Sleep(n);
            ++stat->sleep;
        }
        if(input_eof){
            log(pthread_self(), Actions::END, -1);
            pthread_exit(nullptr);
        }
    }
}

void *consumer(void *arg){
    stat_consumer *stat = (stat_consumer*)arg;
    while(true){
        log(pthread_self(), Actions::ASK, -1);
        ++stat->ask;
        assert((sem_wait(&full) == 0));
        assert((pthread_mutex_lock(&mutex_buffer) == 0));
            int n = buffer.front();
            if(n != POSION_PILL){
                buffer.pop();
            }else{
                sem_post(&full);
                log(pthread_self(), Actions::EXIT, n);
                assert((pthread_mutex_unlock(&mutex_buffer) == 0));
                pthread_exit(nullptr);
            }
            log(pthread_self(), Actions::RECEIVE, n);
            ++stat->receive;
        assert((pthread_mutex_unlock(&mutex_buffer) == 0));
        Trans(n);
        log(pthread_self(), Actions::COMPLETE, n);
        ++stat->complete;
        assert((sem_post(&empty) == 0));
    }
}

int main(int argc, char *argv[]){
    assert((argc == 2 || argc == 3));
    begin = std::chrono::steady_clock::now();
    int n_consumers = atoi(argv[1]);
    int buffer_size = n_consumers * 2;
    //write logs to file
    const char *log_id = "0";
    if(argc == 3){
        log_id = argv[2];
    }
    std::string file_path = "prodcon." + std::string(log_id) + ".log";
    int fd = creat(file_path.c_str(), S_IRWXU);
    assert((fd != -1));
    assert((dup2(fd, STDOUT_FILENO) != -1));
    assert((close(fd)) == 0);

    assert((sem_init(&empty, 0, buffer_size) == 0));  // initially all slots are empty
    assert((sem_init(&full, 0, 0) == 0));  // initially no slots are full

    stat_producer stat_p{0, 0};
    std::vector<stat_consumer> stat_c(n_consumers, stat_consumer{0, 0, 0});

    pthread_t t_id;
    assert((pthread_create(&t_id, nullptr, producer, (void*)&stat_p) == 0));
    threads[t_id] = 0;
    for(int i = 1; i <= n_consumers; ++i){
        assert((pthread_create(&t_id, nullptr, consumer, (void*)&(stat_c[i-1])) == 0));
        threads[t_id] = i;
    }

    for(auto thread : threads){
        assert((pthread_join(thread.first, nullptr) == 0)); // can be used to retrieve consumer stat
    }

    summary(stat_p, stat_c);
}