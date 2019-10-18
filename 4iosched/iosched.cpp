#include <iostream>
#include <fstream>
#include <unistd.h>  /* getopt */
#include <queue>
#include <vector>
#include <stdio.h>   /* printf */
#include <stdlib.h>  /* abs */
#include <limits.h>
#include <string>
#include <sstream>
#include <string.h>  /* strcmp */


using namespace std;

int head = 0;             // head of the disk, at track 0 at time 0
int head_direction = 1;   // 1 = up, -1 = down

typedef struct
{
    int io_op_number;
    int arrival_time;   // the time at which the IO operation is issued
    int track_number;   // the track to access
    int start_time;     // disk start time
    int end_time;       // disk end time
    int where_at_wl;    // the location in the waiting list
} iorequest;

vector<iorequest*> io_wl;
queue<iorequest*> input_queue;

class Scheduler {
public:
    virtual iorequest* select_io() {
        return NULL;
    }
};

class FIFO: public Scheduler
{
public:
    iorequest* select_io() {
        iorequest* res = io_wl.front();
        res->where_at_wl = 0;
        if (res->track_number > head) head_direction = 1;
        else head_direction = -1;
        io_wl.erase(io_wl.begin() + res->where_at_wl);
        return res;
    }
};

class SSTF: public Scheduler
{
public:
    iorequest* select_io() {
        iorequest* res = io_wl.front();
        int dis = res->track_number - head;
        int sst = abs(dis);
        int counter = 0;
        head_direction = dis >= 0 ? 1 : -1;
        for (vector<iorequest*>::iterator it = io_wl.begin(); it != io_wl.end(); ++it) {
            dis = (*it)->track_number - head;
            (*it)->where_at_wl = counter;
            if (sst > abs(dis)) {
                sst = abs(dis);
                res = *it;
                head_direction = dis >= 0 ? 1 : -1;
            }
            counter++;
        }
        //cout << "res is at " << res->where_at_wl << "\n";
        io_wl.erase(io_wl.begin() + res->where_at_wl);
        return res;
    }
};

class LOOK: public Scheduler
{
public:
    iorequest* select_io() {
        iorequest* res = NULL;
        int dis;
        int sst = INT_MAX;
        int counter = 0;
        for (vector<iorequest*>::iterator it = io_wl.begin(); it != io_wl.end(); ++it) {
            dis = (*it)->track_number - head;
            (*it)->where_at_wl = counter;
            if (dis * head_direction >= 0) {
                if (sst > abs(dis)) {
                    sst = abs(dis);
                    res = *it;
                }
            }
            counter++;
        }
        if (res) {
            io_wl.erase(io_wl.begin() + res->where_at_wl);
            return res;
        }
        // if no more io request in this direction, change the direction
        else {
            head_direction = 0 - head_direction;
            return res = select_io();
        }
    }
};

// back to the smallest
class CLOOK: public Scheduler
{
public:
    iorequest* select_io() {
        iorequest* res = NULL;
        iorequest* smallest;
        int dis;
        int smallest_track = INT_MAX;
        int sst = INT_MAX;
        int counter = 0;
        if (head_direction == 1) {
            for (vector<iorequest*>::iterator it = io_wl.begin(); it != io_wl.end(); ++it) {
                if ((*it)->track_number < smallest_track) {
                    smallest_track = (*it)->track_number;
                    smallest = *it;
                    smallest->where_at_wl = counter;
                }
                dis = (*it)->track_number - head;
                (*it)->where_at_wl = counter;
                if (dis >= 0) {
                    if (sst > abs(dis)) {
                        sst = abs(dis);
                        res = *it;
                    }
                }
                counter++;
            }
            // if no more io request in UP direction, move back to smallest
            // after reaching the smallest, change direction to UP again
            if (res == NULL) {
                head_direction = -1;
                res = smallest;
            }
            io_wl.erase(io_wl.begin() + res->where_at_wl);
            
        } else { // reached the smallest, now turn back to UP
            head_direction = 1;
            res = select_io();
        }
        return res;   
    }
};

vector<iorequest*> active_queue;
class FLOOK: public Scheduler
{
public:    
    iorequest* select_io() {
        //static vector<iorequest*> add_queue (io_wl);
        //static vector<iorequest*> active_queue;
        static int switch_counter = 0;

        int counter = 0;
        if (active_queue.empty() and !io_wl.empty()) {
            active_queue.swap(io_wl);
            //cout << "swap\n";
            switch_counter++;
        }
        iorequest* res = NULL;
        int dis;
        int sst = INT_MAX;
        for (vector<iorequest*>::iterator it = active_queue.begin(); it != active_queue.end(); ++it) {
            dis = (*it)->track_number - head;
            (*it)->where_at_wl = counter;
            if (dis * head_direction >= 0) {
                if (sst > abs(dis)) {
                    sst = abs(dis);
                    res = *it;
                }
            }
            counter++;
        }
        if (res) {
            //cout << "select req " << res->io_op_number << "\n";
            active_queue.erase(active_queue.begin() + res->where_at_wl);
            return res;
        }
        // if no more io request in this direction, switch queues
        else if (res == NULL and !active_queue.empty()) {
            head_direction = 0 - head_direction;
            res = select_io();
        } else if (res == NULL and active_queue.empty() and !io_wl.empty()) {
            res = select_io();
        } else if (res == NULL and active_queue.empty() and io_wl.empty()) 
            res = NULL; 
        return res;       
    }
};

int main(int argc, char ** argv) {
	int c;
	char *algo = NULL;   // disk scheduling algorithm
    extern char *optarg;

	while ((c = getopt(argc, argv, "s:")) != -1) {
        algo = optarg;
	}

    Scheduler *THE_SCHEDULER;
    if (strcmp(algo, "i") == 0) THE_SCHEDULER = (Scheduler*) new FIFO();
    else if (strcmp(algo, "j") == 0) THE_SCHEDULER = (Scheduler*) new SSTF();
    else if (strcmp(algo, "s") == 0) THE_SCHEDULER = (Scheduler*) new LOOK();
    else if (strcmp(algo, "c") == 0) THE_SCHEDULER = (Scheduler*) new CLOOK();
    else if (strcmp(algo, "f") == 0) THE_SCHEDULER = (Scheduler*) new FLOOK();

    // read input file
    ifstream inputfile(argv[optind]);
    string line;
    int io_count = 0;  // count the number of io operations
    vector<iorequest*> io_vector; // used for printing results
    // read in io requests, push to input_queue
    while (getline(inputfile, line)) {
        if (line.compare(0, 1, "#") == 0) continue;
        else {
            istringstream iss(line);
            iorequest* req = new iorequest;
            req->io_op_number = io_count;
            iss >> req->arrival_time >> req->track_number;
            input_queue.push(req);
            io_vector.push_back(req);
            io_count++;
        }
    }
    
    // simulation
    int time_counter = 0;     // head moves one track in one time unit
    int tot_movement = 0;
    iorequest* current_io_req = NULL;

    while (!input_queue.empty() or !io_wl.empty() or current_io_req) {
        if (!input_queue.empty()) {
            iorequest* req = input_queue.front();    // the IO request that will arrive next
            // check if a new I/O request arrives, if so, add to IO queue
            if (req->arrival_time == time_counter) {
                input_queue.pop();
                io_wl.push_back(req);
                //cout << "req " << req->io_op_number << " arrives at " << time_counter << "\n";
            }
        }
        
        // if an io active and completes at this time
        // compute relevant info and store in io request for final summary
        if (current_io_req and head == current_io_req->track_number) {
            current_io_req->end_time = time_counter;
            // what to update?
            current_io_req = NULL;
        } 
        
        // if an io active, but not complete, move head in the direction it is going to
        else if (current_io_req and head != current_io_req->track_number) {
            // simulate seek            
            if (head_direction == 1) head++; // move up
            else head--;  // move down
            tot_movement++;
        }
        
        // if no io request active now, but pending io requests
        // fetch and start a new IO
        while (!current_io_req and !(io_wl.empty() and active_queue.empty())) {
            //cout << "head is at " << head << "\n";
            //cout << time_counter << " before waiting io are \n";
            //for (unsigned i=0; i<io_wl.size();++i) cout << " " << io_wl[i]->io_op_number ;
            //cout << "\n";
            current_io_req = THE_SCHEDULER->select_io();
            if (!current_io_req) break;
            current_io_req->start_time = time_counter;
            //cout << time_counter << " after waiting io are \n";
            //for (unsigned i=0; i<io_wl.size();++i) cout << " " << io_wl[i]->io_op_number ;
            //cout << "\n";
            if (head == current_io_req->track_number) {
                current_io_req->end_time = time_counter;
                // what to update?
                current_io_req = NULL;
            } else {
                if (head_direction == 1) head++; // move up
                else head--;  // move down
                tot_movement++;
            }
        }
        
        time_counter++;
        //if (current_io_req) cout << "beginning of time " << time_counter << " : req " << 
        //current_io_req->io_op_number << " : head at "<< head << "\n";
    }

    double avg_turnaround = 0; // don't use float
    double avg_waittime = 0;
    int max_waittime = 0;
    iorequest* request;

    for (int i = 0; i < io_count; ++i) {
        request = io_vector[i];
        printf("%5d: %5d %5d %5d\n", i, request->arrival_time, 
            request->start_time, request->end_time);
        avg_turnaround += request->end_time - request->arrival_time;
        avg_waittime += request->start_time - request->arrival_time;
        if (max_waittime < request->start_time - request->arrival_time) {
            max_waittime = request->start_time - request->arrival_time;
        }
        delete request;
    }

    avg_turnaround = avg_turnaround / io_count;
    avg_waittime = avg_waittime / io_count;
    int total_time = time_counter - 1;
    printf("SUM: %d %d %.2lf %.2lf %d\n", total_time, tot_movement, avg_turnaround,
        avg_waittime, max_waittime);
    delete THE_SCHEDULER;
}