// Version: 6.2.0

#include <iomanip>
#include <iostream>
#include <fstream>
#include <getopt.h>
#include <deque>
#include <cmath>

using namespace std;

int current_track = 0;
int current_time = 0;
int direction = 1; // going up
bool isProcessing = false; // only useful for FLOOK scheduler
bool traceFLOOK = false; // only useful for FLOOK scheduler

class IO {
public:
	int id;
	int arrival_time;
	int track;

	int start_time;
	int end_time;

	IO(int time, int arrival_track) { arrival_time = time; track = arrival_track; start_time = 0; end_time = 0; }
};

class Scheduler {
public:
	deque<IO*> IO_queue;

	virtual void add_request(IO* io) = 0;
	virtual IO* get_request() = 0;
};

class FIFO: public Scheduler {
public:
	void add_request(IO* io) {
		IO_queue.push_back(io);
	}

	IO* get_request() {
		IO* req = nullptr;

		if (!IO_queue.empty()) {
			req = IO_queue.front();
			IO_queue.pop_front();
		}

		return req;
	}
};

class SSTF: public Scheduler {
private:
	int min_seek, distance;
	int position_in_queue;
public:
	void add_request(IO* io) {
		IO_queue.push_back(io);
	}

	IO* get_request() {
		IO* req = nullptr;

		if (!IO_queue.empty()) {			
			for (int i=0; i < IO_queue.size(); i++) {
				distance = abs(IO_queue[i]->track - current_track);

				if (!req) {
					// point to the first IO request in the queue
					req = IO_queue[i];
					position_in_queue = i;
					min_seek = distance;
				} else {
					// find track with minimum seek distance
					if (distance < min_seek) {
						req = IO_queue[i];
						position_in_queue = i;
						min_seek = distance;
					}
				}
			}

			IO_queue.erase(IO_queue.begin() + position_in_queue);
		}

		return req;
	}
};

class LOOK: public Scheduler {
private:
	int min_seek, diff;
	int position_in_queue;
public:
	void add_request(IO* io) {
		IO_queue.push_back(io);
	}

	IO* get_request() {
		IO* req = nullptr;

		if (!IO_queue.empty()) {
			for (int i=0; i < IO_queue.size(); i++) {
				// only distance in the current seeking direction is positive
				diff = (IO_queue[i]->track - current_track) * direction;

				// only consider track in the current seeking direction
				if (diff >= 0) {
					// point to the first IO request in the current direction in the queue
					if (!req) {
						req = IO_queue[i];
						position_in_queue = i;
						min_seek = diff;
					} else {
						// find track with minimum seek distance
						if (diff < min_seek) {
							req = IO_queue[i];
							position_in_queue = i;
							min_seek = diff;
						}
					}
				}
			}

			if (!req) {
				// if no request is found in the seeking direction then reverse
				direction = direction * (-1);
				return get_request();
			} else {
				IO_queue.erase(IO_queue.begin() + position_in_queue);
			}
		}

		return req;
	}
};

class CLOOK: public Scheduler {
private:
	int min_seek, diff;
	int position_in_queue;
public:
	void add_request(IO* io) {
		IO_queue.push_back(io);
	}

	IO* get_request() {
		IO* req = nullptr;

		if (!IO_queue.empty()) {
			for (int i=0; i < IO_queue.size(); i++) {
				// only distance in the up direction is positive
				diff = (IO_queue[i]->track - current_track * direction);

				// only consider track in the current seeking direction
				if (diff >= 0) {
					// point to the first IO request in the queue
					if (!req) {
						req = IO_queue[i];
						position_in_queue = i;
						min_seek = diff;
					} else {
						// find the lowest track
						if (diff < min_seek) {
							req = IO_queue[i];
							position_in_queue = i;
							min_seek = diff;
						}
					}
				}
			}

			if (!req) {
				// "free fall" towards the lowest track requested
				direction = 0;
				return get_request();
			} else {
				// going up again after reaching the lowest track
				direction = 1;
				IO_queue.erase(IO_queue.begin() + position_in_queue);
			}
		}

		return req;
	}
};

class FLOOK: public Scheduler {
private:
	int min_seek, diff;
	int position_in_queue;
	int queue_idx = 0;
	deque<IO*> active_queue;
	deque<IO*> wait_queue;
public:
	void add_request(IO* io) {
		IO_queue.push_back(io);

		// if current active queue is being processed then request is added to the wait queue
		if ((active_queue.empty()) && (!isProcessing)) {
			active_queue.push_back(io);
		} else {
			wait_queue.push_back(io);
		}
	}

	IO* get_request() {
		IO* req = nullptr;

		// swap wait queue and active queue
		if (active_queue.empty()) {
			active_queue.swap(wait_queue);
			queue_idx = 1 - queue_idx;
		}

		// similar to LOOK but only consider requests in the active queue
		if (!IO_queue.empty()) {
			for (int i=0; i < active_queue.size(); i++) {
				// only distance in the current seeking direction is positive
				diff = (active_queue[i]->track - current_track) * direction;

				// only consider track in the current seeking direction
				if (diff >= 0) {
					// point to the first IO request in the current direction in the queue
					if (!req) {
						req = active_queue[i];
						position_in_queue = i;
						min_seek = diff;
					} else {
						// find track with minimum seek distance
						if (diff < min_seek) {
							req = active_queue[i];
							position_in_queue = i;
							min_seek = diff;
						}
					}
				}
			}

			if (!req) {
				// if no request is found in the seeking direction then reverse
				direction = direction * (-1);
				return get_request();
			} else {
				active_queue.erase(active_queue.begin() + position_in_queue);
				// need to reduce IO_queue size by one to match the total number of requests from both active and wait queue
				// doesn't matter which request b/c we only use request from the active queue
				IO_queue.pop_front();
			}
		}

		if ((traceFLOOK) && (req)) {
			printf("%d:       %d get Q=%d\n", current_time, req->id, queue_idx);
		}

		return req;
	}
};

int main(int argc, char **argv) {
	int c;
	char* cptr;
	Scheduler* sched;

	bool trace = false;

	// proper way to parse arguments
    while ((c = getopt(argc,argv,"vqfs:")) != -1 ) {
    	switch (c) {
    		case 'v':
    			printf("TRACE\n");
    			trace = true;
    			break;
    		case 'q':
    			break;
    		case 'f':
    			traceFLOOK = true;
    			break;
    		case 's':
    			cptr = optarg;
    			switch (*cptr) {
    				case 'i':
    					sched = new FIFO();
    					traceFLOOK = false;
    					break;
    				case 'j':
    					sched = new SSTF();
    					traceFLOOK = false;
    					break;
    				case 's':
    					sched = new LOOK();
    					traceFLOOK = false;
    					break;
    				case 'c':
    					sched = new CLOOK();
    					traceFLOOK = false;
    					break;
    				case 'f':
    					sched = new FLOOK();
    					break;
    			}
    			break;
    	}
    }

    string inputFileName = argv[optind];

    // process inputFile
    ifstream inputFile;
    string fline;
    deque<IO*> IO_requests;
    int time, track;

    inputFile.open(inputFileName);

    while (getline(inputFile, fline)) {
		// skip # lines
		if (fline[0] == '#') {
			continue;
		}

    	istringstream iss(fline);
        int num[2];
        if(!(iss >> num[0] >> num[1])) {
            break;
        }
        int time = num[0];  // read time&track from inputfile
        int track = num[1];
        IO* new_IO = new IO(time, track);
    	IO_requests.push_back(new_IO);    	
    }

    inputFile.close();

    for (int i=0; i < IO_requests.size(); i++) {
    	IO_requests[i]->id = i;
    }

    int idx = 0;
    IO* active_request = nullptr;

    int total_time;
    int tot_movement = 0;
    long long tot_turnaround = 0;
    double avg_turnaround;
    long long tot_waittime = 0;
	double avg_waittime;
	int max_waittime = 0;

	// keep running if there is an active request or pending request to be added to the queue
    while ((idx < IO_requests.size()) || (active_request)) {
    	current_time++;

    	// add request to the queue
    	if (idx < IO_requests.size()) {
	    	if (current_time == IO_requests[idx]->arrival_time) {
	    		sched->add_request(IO_requests[idx]);
	    		if (trace) {
	    			printf("%d:     %d add %d\n", current_time, IO_requests[idx]->id, IO_requests[idx]->track);
	    		}
	    		idx++;
	    	}	
    	}

    	// get new request if there is no active request but there are pending requests in the queue
    	if ((!active_request) && (!sched->IO_queue.empty())) {
    		active_request = sched->get_request();
    		active_request->start_time = current_time;
    		if (trace) {
    			printf("%d:     %d issue %d %d\n", current_time, active_request->id, active_request->track, current_track);
    		}
    		isProcessing = true;

    		if (max_waittime < active_request->start_time - active_request->arrival_time) {
    			max_waittime = active_request->start_time - active_request->arrival_time;
    		}
    	}

    	// move the head if request is not finished or get new request if finished
    	while (active_request) {
    		if (current_track == active_request->track) {
	    		active_request->end_time = current_time;
	    		if (trace) {
	    			printf("%d:     %d finish %d\n", current_time, active_request->id, current_time - active_request->arrival_time);
	    		}
	    		isProcessing = false;
	    		active_request = nullptr;

	    		// immediately start new request if there are pending requests in the queue
	    		if (!sched->IO_queue.empty()) {
	    			active_request = sched->get_request();
	    			active_request->start_time = current_time;
	    			if (trace) {
		    			printf("%d:     %d issue %d %d\n", current_time, active_request->id, active_request->track, current_track);
		    		}
	    			isProcessing = true;

	    			if (max_waittime < active_request->start_time - active_request->arrival_time) {
		    			max_waittime = active_request->start_time - active_request->arrival_time;
		    		}
	    		}
	    	} else if (current_track < active_request->track) {
				current_track++;
				tot_movement++;
				break;
			} else {
				current_track--;
				tot_movement++;
				break;
			}
    	}
    }

    total_time = current_time;

    for (int i=0; i < IO_requests.size(); i++) {
    	tot_turnaround = tot_turnaround + (IO_requests[i]->end_time - IO_requests[i]->arrival_time);
    }

    avg_turnaround = double(tot_turnaround) / IO_requests.size();

    for (int i=0; i < IO_requests.size(); i++) {
    	tot_waittime = tot_waittime + (IO_requests[i]->start_time - IO_requests[i]->arrival_time);
    }

    avg_waittime = double(tot_waittime) / IO_requests.size();

    // print each IO request info line
    for (int i=0; i < IO_requests.size(); i++) {
		printf("%5d: %5d %5d %5d\n", i, IO_requests[i]->arrival_time, IO_requests[i]->start_time, IO_requests[i]->end_time);
	}

	// print summary
    printf("SUM: %d %d %.2lf %.2lf %d\n", total_time, tot_movement, avg_turnaround, avg_waittime, max_waittime);

	return 0;
}
