#include <unistd.h>
#include <fstream>
#include <sstream>

using namespace std;

int head = 0; // the head is initially positioned at track = 0
int direction = 1; // 1 == up , -1 == down, start from up
int io_count = 0; // total number of operations

int current_time = 0; // start from time = 0
int total_time = 0; // total simulated time, until the last I/O request has completed
int tot_movement = 0; // total number of tracks the head had to be moved
int tot_turnaround = 0; // total turnaround time
double avg_turnaround = 0.0; // average turnaround time per operation from submission to completion
int tot_waittime = 0; // total wait time for all operations
double avg_waittime = 0.0; // average wait time per operation
int max_waittime = 0; // maximum wait time for any IO operation

bool isFLOOK = false;
bool isActive = false;
// bool IOtracing = false;

// IO request info
class IOrequest{
public:
    int op_num; // IO-op#
    int start_time; // disk service start time
    int end_time; // disk service end time
    int arrival_time; // its arrival time from input file
    int track_access; // the track that is accesses from input file

    IOrequest(int time, int track, int op){
        arrival_time = time;
        track_access = track;
        op_num = op;
    }
};

deque<IOrequest*> IOqueue; // Info from inputfile
deque<IOrequest*> Runqueue; // IO processing in scheduler
deque<IOrequest*> Printer; // For printing the summary

// For FLOOK
deque<IOrequest*> Activequeue;
deque<IOrequest*> Addqueue;

// basic type of scheduler
class Scheduler{
public:
    virtual IOrequest* selection() {
        return nullptr;
    }
};

// FIFO algorithm
class FIFO: public Scheduler{
    IOrequest* selection() override{
        IOrequest* result = nullptr;
        if (!Runqueue.empty()){
            result = Runqueue.front();
            Runqueue.pop_front();
        }
        return result;
    }
};

// SSTF algorithm
class SSTF: public Scheduler{
    IOrequest* selection() override{
        IOrequest* result = nullptr;
        int dis;
        int position;
        int SST;
        int counter = 0;
        if (!Runqueue.empty()){
            for (deque<IOrequest*>::iterator it = Runqueue.begin(); it != Runqueue.end(); ++it) {
                auto value = (*it)->track_access - head;
                value >= 0 ? dis = value : dis = (-1) * value;

                if (!result){
                    result = (*it);
                    SST = dis;
                    position = counter;
                }
                else {
                    if (dis < SST) {
                        result = (*it);
                        SST = dis;
                        position = counter;
                    }
                }
                counter++;
            }
            Runqueue.erase(Runqueue.begin() + position);
        }
        return result;
    }
};

// LOOK algorithm
class LOOK: public Scheduler{
    IOrequest* selection() override{
        IOrequest* result = nullptr;
        int dis;
        int position;
        int SST;
        int counter = 0;
        if (!Runqueue.empty()){
            for (deque<IOrequest*>::iterator it = Runqueue.begin(); it != Runqueue.end(); ++it) {
                dis = ((*it)->track_access - head) * direction;

                if (dis >= 0) {
                    if (!result) {
                        result = (*it);
                        SST = dis;
                        position = counter;
                    } else {
                        if (dis < SST) {
                            result = (*it);
                            SST = dis;
                            position = counter;
                        }
                    }
                }
                counter++;
            }

            if(result != nullptr){
                Runqueue.erase(Runqueue.begin() + position);
            }
            else{
                direction = (-1) * direction;
                return selection();
            }
        }
        return result;
    }
};

// CLOOK algorithm
class CLOOK: public Scheduler{
    IOrequest* selection() override{
        IOrequest* result = nullptr;
        int dis;
        int position;
        int SST;
        int counter = 0;
        if (!Runqueue.empty()){
            for (deque<IOrequest*>::iterator it = Runqueue.begin(); it != Runqueue.end(); ++it) {
                dis = (*it)->track_access - (head * direction);

                if (dis >= 0) {
                    if (!result) {
                        result = (*it);
                        SST = dis;
                        position = counter;
                    } else {
                        if (dis < SST) {
                            result = (*it);
                            SST = dis;
                            position = counter;
                        }
                    }
                }
                counter++;
            }

            if(result != nullptr){
                direction = 1;
                Runqueue.erase(Runqueue.begin() + position);
            }
            else{
                direction = 0;
                return selection();
            }
        }
        return result;
    }
};

// FLOOK algorithm (active queue is Activequeue, add queue is Runqueue)
class FLOOK: public Scheduler{
public:
    IOrequest* selection() override {
        if (Activequeue.empty()){
            Activequeue.swap(Addqueue);
        }

        IOrequest* result = nullptr;
        int dis;
        int position;
        int SST;
        int counter = 0;

        if (!Runqueue.empty()){
            for (deque<IOrequest*>::iterator it = Activequeue.begin(); it != Activequeue.end(); ++it) {
                dis = ((*it)->track_access - head) * direction;
                if (dis >= 0) {
                    if (!result) {
                        result = (*it);
                        SST = dis;
                        position = counter;
                    }
                    else {
                        if (dis < SST) {
                            result = (*it);
                            SST = dis;
                            position = counter;
                        }
                    }
                }
                counter++;
            }
            if(result != nullptr){
                Activequeue.erase(Activequeue.begin() + position);
                Runqueue.pop_front();
            }
            else{
                direction = (-1) * direction;
                return selection();
            }
        }
        return result;
    }
};

void readFile(string fileName){
    ifstream inputfile(fileName);
    string line;
    // read IO requests, maintain an IO-queue
    while(getline(inputfile, line)) {
        if(line[0] == '#'){
            continue;
        }
        istringstream iss(line);
        int num[2];
        if(!(iss >> num[0] >> num[1])) {
            break;
        }
        int time = num[0];  // read time&track from inputfile
        int track = num[1];
        IOrequest * req = new IOrequest(time, track, io_count);
        IOqueue.push_back(req);
        Printer.push_back(req);
        io_count++;
    }
    inputfile.close();
}

Scheduler* parseCommand(int argc, char ** argv, Scheduler* sched){
    int c;
    char *schedalgo; // disk scheduling algorithm
    while ((c = getopt(argc, argv, "s:")) != -1) {
        schedalgo = optarg;
    }
    switch (*schedalgo) {
        case 'i':
            sched = new FIFO();
            break;
        case 'j':
            sched = new SSTF();
            break;
        case 's':
            sched = new LOOK();
            break;
        case 'c':
            sched = new CLOOK();
            break;
        case 'f':
            sched = new FLOOK();
            isFLOOK = true;
            break;
    }
    return sched;
}

void Simulation(Scheduler* sched){
    IOrequest* current_req = nullptr;
    // traverse the input IOqueue for simulation
    while ((current_req) || !IOqueue.empty() ) {
        current_time++;
        // add request to Runqueue
        if (!IOqueue.empty()){
            // take the first one of IOqueue
            IOrequest* temp = IOqueue.front();

            if (temp->arrival_time == current_time){
                IOqueue.pop_front();
                Runqueue.push_back(temp);
                if (isFLOOK){
                    if ((Activequeue.empty()) && (!isActive))
                        Activequeue.push_back(temp);
                    else
                        Addqueue.push_back(temp);
                }
            }
        }

        // start processing from Runqueue
        if (!Runqueue.empty() && !(current_req)){
            current_req = sched->selection();
            current_req->start_time = current_time;

            int waittime = current_req->start_time - current_req->arrival_time;
            if (max_waittime < waittime) max_waittime = waittime;

            isActive = true;
        }

        while(current_req) {
            // if finds the track, finish the request
            if (head == current_req->track_access){
                current_req->end_time = current_time;

                // reset
                current_req = nullptr;
                isActive = false;
                // take another request from Runqueue
                if (!Runqueue.empty()){
                    current_req = sched->selection();
                    current_req->start_time = current_time;

                    int waittime = current_req->start_time - current_req->arrival_time;
                    if (max_waittime < waittime) max_waittime = waittime;

                    isActive = true;
                }

            }
            else if (head < current_req->track_access){
                head++;
                tot_movement++;
                break;
            }
            else {
                head--;
                tot_movement++;
                break;
            }
        }
    }
}

void printSummary(Scheduler* sched){
    total_time = current_time;
    IOrequest* request;
    for (int i = 0; i < io_count; ++i) {
        request = Printer[i];
        printf("%5d: %5d %5d %5d\n", i, request->arrival_time, request->start_time, request->end_time);
        tot_turnaround += request->end_time - request->arrival_time;
        tot_waittime += request->start_time - request->arrival_time;
        delete request;
    }

    avg_turnaround = double(tot_turnaround) / io_count;
    avg_waittime = double(tot_waittime) / io_count;
    printf("SUM: %d %d %.2lf %.2lf %d\n", total_time, tot_movement, avg_turnaround, avg_waittime, max_waittime);
    delete sched;
}

int main(int argc, char ** argv) {
    Scheduler* sched;

    // parse input command
    sched = parseCommand(argc, argv, sched);

    // read input file
    string fileName = argv[optind];
    readFile(fileName);

    // start simulation
    Simulation(sched);

    // print summary
    printSummary(sched);

    return 0;
}


