#include <iostream>
#include <fstream>
#include <queue>
#include <stack>
#include <vector>
#include <unistd.h>
#include <stdio.h>  /* fgets */
#include <stdlib.h>  /* atoi */
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <algorithm>

using namespace std;

// defining the global variable
int* randvals;

int getEventID() {
    static int id = 0;
    return id++;
}
// STEP 1: read in input file, create Process objects
// in each line -- a separate process specification
// AT TC CB IO
// AT: arrival time; TC: total CPU time; CB: CPU burst; IO: IO burst
int myrandom(int burst) {
    static int ofs = 1;
    int randS = randvals[0];
    int ret;
    // increase ofs with each invocation and 
    // wrap around when you run out of numbers in the file/array
    if (ofs <= randS) {
        ret = 1 + (randvals[ofs] % burst);
        ofs++;
    } else {
        ofs = 1;
        ret = 1 + (randvals[ofs] % burst);        
    }
    return ret;
}

class Event;
enum State {CREATED, READY, RUNNING, BLOCKED, DONE};
enum PreemptedBy {NOT, Q_EXPIRED, OTHER_PROCESS};
class Process
{
public:
    int at;  // arrival itme
    int tc;  // total CPU time
    int cb;  // CPU burst
    int io;  // IO burst
    int cpuburst;  // random CPU burst [1, cb]
    int ioburst;  // random IO burst [1, io]
    State currState; // current state
    int timeRemaining; // remaining time
    int timeInPrevState;
    int state_ts; // time when the process starts current state
    int finishingTime; // finishing time
    int ioTime; // time in blocked state
    int cpuWaitingTime; // time in Ready state
    int static_priority;
    PreemptedBy preemptedTag;
    bool canPreemptOthers;
    Event * pendingEvt;
    int timeAdded; // time when the process is added to the runqueue
    
    // With every quantum expiration the dynamic priority decreases by one.
    // When “-1” is reached the prio is reset to (static_priority-1).
    int dynamic_priority;
    bool dpReset;

    Process(int a, int t, int c, int i)
    {        
        at = a;
        tc = t;
        cb = c;
        io = i;
        
        currState = CREATED;
        timeInPrevState = 0;
        timeRemaining = tc;
        cpuWaitingTime = 0;
        ioTime = 0;
        state_ts = at;
        static_priority = myrandom(4);
        preemptedTag = NOT;
        pendingEvt = nullptr;
        canPreemptOthers = false;
        dynamic_priority = static_priority - 1;
        dpReset = false;
        
        //cpuburst = min(myrandom(cb), timeRemaining);
        //ioburst = myrandom(io);
    }

    void updateTimeRemaining() {
        if (timeInPrevState == 0) {
            timeRemaining = tc;
        } else {
            timeRemaining -= timeInPrevState;
        }
    }

    void updateCPUBurst() {
        cpuburst = min(myrandom(cb), timeRemaining);
    }
};


// STEP 2: a generic DES layer: create events that take the timestamp
// when it is supposed to fire, a pointer to the Process, and the state
// you want to transition to.
enum State_trans {TRANS_TO_READY, TRANS_TO_RUN, TRANS_TO_BLOCK, TRANS_TO_DONE, TRANS_TO_PREEMPT};

class Event
{
public:
    int evtTimeStamp;
    Process * evtProcess;
    State_trans transition;
    bool deleted;
    int timePut;
    int eventID;

    Event(int timestamp, Process *proc, State_trans trans, int add, int eventID): eventID(eventID) {
        evtTimeStamp = timestamp;
        evtProcess = proc;
        transition = trans;
        timePut = add;
        deleted = false;
    }   
};


class Compare
{
public:
    bool operator() (Event* e1, Event* e2) {
        if (e1->evtTimeStamp > e2->evtTimeStamp) {
            return true;
        } else if (e1->evtTimeStamp < e2->evtTimeStamp) {
            return false;
        } else {
            if (e1->timePut > e2->timePut) {
                return true;
            } else if (e1->timePut < e2->timePut) {
                return false;
            } else {
                return e1->eventID > e2->eventID;
            }        
        }
    }
};

class DES
{
public:

    priority_queue<Event*, vector<Event*>, Compare> evtQueue; // time-ordered event queue   

    // Define get_event(), put_event(), rm_event()
    Event* get_event() {
        if (evtQueue.empty()) {
            return NULL;
        }
        return evtQueue.top();
    }

    void put_event(Event *evt) {
        evtQueue.push(evt);
    }

    void rm_event() {
        evtQueue.pop();
    }

    bool queue_empty() {
        return evtQueue.empty();
    }
};

//typedef bool (*) (Process*, Process*) comparator;
// STEP 3: implement one scheduler
// the Simulation() should not know any detail about the specific
// scheduler itself, so all has to be accomplished through virtual funcitons.
// Treat non-preemptive scheduler as preemptive with very large quantum
// (10K is good for our simulation) that will never fire.
class Scheduler
{
public:
    // priority_queue <Process*, vector<Process*>, comparator> runQueue;
    
    // Scheduler(comparator compare): runQueue(compare) {}
    int quantum;
    string schedulerType;
    Scheduler() {
        quantum = 10000;
    }

    // Define add_process, get_next_process
    virtual void add_process(Process *proc) {}

    virtual Process* get_next_process() {return NULL;}

    virtual void update_dynamic_prio(Process *proc) {
        // if quantum expires
        proc->dynamic_priority -= 1;
        if (proc->dynamic_priority == -1) {
            proc->dynamic_priority = proc->static_priority - 1;
            proc->dpReset = true;
        } else {
            proc->dpReset = false;
        }  
    }
};


// FCFS: first come, first served
// LCFS: last come, first served
// SRTF: shortest remaining time next
// RR (Round Robin): only regenerate a new CPU burst when the current one has expired
// PRIO (Priority scheduler): RR + has 4 priority levels [0..3]
// Preemptive PRIO (E): a variant of PRIO where processes that become active will preempt a process of lower priority.
// runqueue under PRIO is the combination of active and expired.

/*
class FCFS_compare
{
public:
    bool operator() (Process* p1, Process* p2) {
        return p1->timeAdded > p2->timeAdded;
        
    }
};
*/
class FCFS: public Scheduler
{
public:
    FCFS() {
        schedulerType = "FCFS";
    }
    //priority_queue<Process*, vector<Process*>, FCFS_compare> runQueue;
    queue<Process*> runQueue;

    void add_process(Process *proc) {
        runQueue.push(proc);
    }
    Process* get_next_process() {
        if (runQueue.empty()) {
            return nullptr;
        }
        Process * tmp = runQueue.front();
        runQueue.pop();
        return tmp;
    }
};

/*
class LCFS_compare
{
public:
    bool operator() (Process* p1, Process* p2) {
        return p1->timeAdded < p2->timeAdded;
    }
};
*/
class LCFS: public Scheduler
{
public:
    LCFS() {
        schedulerType = "LCFS";
    }
    //priority_queue<Process*, vector<Process*>, LCFS_compare> runQueue;
    stack<Process*> runQueue;
    void add_process(Process *proc) {
        runQueue.push(proc);
    }
    Process* get_next_process() {
        if (runQueue.empty()) {
            return nullptr;
        }
        Process * tmp = runQueue.top();
        runQueue.pop();
        return tmp;
    }
};


class SRTF_compare
{
public:
    bool operator() (Process* p1, Process* p2) {
        if (p1->timeRemaining > p2->timeRemaining) {
            return true;
        } else if (p1->timeRemaining < p2->timeRemaining) {
            return false;
        } else {
            if (p1->timeAdded > p2->timeAdded) {
                return true;
            } else if (p1->timeAdded < p2->timeAdded) {
                return false;
            } else {
                return (p1 - p2) >0;
            }
        }
    }
};
class SRTF: public Scheduler
{ // non-preemptive version
public:
    SRTF() {
        schedulerType = "SRTF";
    }
    priority_queue<Process*, vector<Process*>, SRTF_compare> runQueue;

    void add_process(Process *proc) {
        runQueue.push(proc);
    }
    Process* get_next_process() {
        if (runQueue.empty()) {
            return nullptr;
        }
        Process * tmp = runQueue.top();
        runQueue.pop();
        return tmp;
    }
};

class RR: public Scheduler
{
public:
    queue <Process*> runQueue;
    RR(int quan) {
        quantum = quan;
        schedulerType = "RR";
    }

    void add_process(Process *proc) {
        // if preempted, no new cpu burst
        
        // from arrival (transition 1) 

        //finish the cpu burst, then go to blocked, from blocked(transition 4)
        // new cpuburst
        //if (proc->currState == BLOCKED) {
          //  proc->updateCPUBurst();
        //}
        runQueue.push(proc);
        
    }
    Process* get_next_process() {
        if (runQueue.empty()) {
            return nullptr;
        }
        Process * tmp = runQueue.front();
        runQueue.pop();
        return tmp;
    }
};

class PRIO: public Scheduler
{
public:
    queue<Process*> activeQueue[4];
    queue<Process*> expiredQueue[4];
    PRIO(int quan) {
        quantum = quan;
        schedulerType = "PRIO";        
    }

    void add_process(Process *proc) { 
        if (proc->currState == BLOCKED) {
            activeQueue[proc->dynamic_priority].push(proc);    
        } else if (proc->currState == RUNNING) {
            // quantum expires
            if (!proc->dpReset) {
                activeQueue[proc->dynamic_priority].push(proc);
            } else {
                expiredQueue[proc->dynamic_priority].push(proc);
            }
            proc->dpReset = false;
            exchange_active_expired();
        } else if (proc->currState == CREATED) {
            activeQueue[proc->dynamic_priority].push(proc);
        }
        
    }

    void exchange_active_expired() {
        if (activeQueue[0].empty() && activeQueue[1].empty() && activeQueue[2].empty()
            && activeQueue[3].empty()) {
            queue<Process*> tmp[4];
            for (int i = 0; i < 4; i++) {
                tmp[i] = activeQueue[i];
                activeQueue[i] = expiredQueue[i];
                expiredQueue[i] = tmp[i];
            }
        }
    }
    Process* get_next_process() {
        // when the active queue is empty, switch active and expired
        exchange_active_expired();
        int highest_prio = 3;
        while (highest_prio >= 0) {
            //cout << highest_prio << "\n";
            if (activeQueue[highest_prio].size() != 0) {
                Process* tmp0 = activeQueue[highest_prio].front();
                activeQueue[highest_prio].pop();
                //exchange_active_expired();
                return tmp0;
            } else {
                highest_prio -= 1;
            }
        }
        return NULL;      
    }
};

class PREPRIO: public Scheduler
{
public:
    queue<Process*> activeQueue[4];
    queue<Process*> expiredQueue[4];

    PREPRIO(int quan) {
        quantum = quan;
        schedulerType = "PREPRIO";
    }

    void add_process(Process *proc) { 
        if (proc->currState == BLOCKED) {
            activeQueue[proc->dynamic_priority].push(proc);    
        } else if (proc->currState == RUNNING) {
            // quantum expires
            if (!proc->dpReset) {
                activeQueue[proc->dynamic_priority].push(proc);
            } else {
                expiredQueue[proc->dynamic_priority].push(proc);
            }
            proc->dpReset = false;
            exchange_active_expired();
        } else if (proc->currState == CREATED) {
            activeQueue[proc->dynamic_priority].push(proc);
        }     
    }

    void exchange_active_expired() {
        if (activeQueue[0].empty() && activeQueue[1].empty() && activeQueue[2].empty()
            && activeQueue[3].empty()) {
            queue<Process*> tmp[4];
            for (int i = 0; i < 4; i++) {
                tmp[i] = activeQueue[i];
                activeQueue[i] = expiredQueue[i];
                expiredQueue[i] = tmp[i];
            }
        }
    }

    Process* get_next_process() {
        // when the active queue is empty, switch active and expired
        exchange_active_expired();
        int highest_prio = 3;
        while (highest_prio >= 0) {
            //cout << highest_prio << "\n";
            if (activeQueue[highest_prio].size() != 0) {
                Process* tmp0 = activeQueue[highest_prio].front();
                activeQueue[highest_prio].pop();
                //exchange_active_expired();
                return tmp0;
            } else {
                highest_prio -= 1;
            }
        }
        return NULL;     
    }
};

// STEP 4: Simulation   
// Note: run queue/ready queue has nothing to do with the event queue.
void Simulation(char *stype, DES *desLayer, vector<pair<int, int> > &IOUse) {
    Scheduler * THE_SCHEDULER;
    int quantum;
    if (strcmp(stype, "F") == 0) {
        THE_SCHEDULER = (Scheduler*) new FCFS();
    } else if (strcmp(stype, "L") == 0) {
        THE_SCHEDULER = (Scheduler*) new LCFS();
    } else if (strcmp(stype, "S") == 0) {
        THE_SCHEDULER = (Scheduler*) new SRTF();
    } else if (*stype == 'R') {
        quantum = atoi(stype + 1);
        THE_SCHEDULER = new RR(quantum);
    } else if (*stype == 'P') {
        quantum = atoi(stype + 1);
        THE_SCHEDULER = new PRIO(quantum);
    } else if (*stype == 'E') {
        quantum = atoi(stype + 1);
        THE_SCHEDULER = new PREPRIO(quantum);
    }
    if ((*stype == 'R') || (*stype == 'P') || (*stype == 'E')) {
        cout << THE_SCHEDULER->schedulerType << " " << THE_SCHEDULER->quantum << "\n";
    } else {
        cout << THE_SCHEDULER->schedulerType << "\n";
    }
    
    Event* evt;
    bool CALL_SCHEDULER;
    Process * CURRENT_RUNNING_PROCESS = nullptr;
    //int testloop = 0;
    while ((evt = desLayer->get_event())) {
        if (evt->deleted == true) {
            desLayer->rm_event();
            continue;
        }
        // cout << "\ntestloop " << testloop <<"\n";
        // cout << "transition: "<< evt->transition << "\n"<< "timestamp: "<<evt->evtTimeStamp<< "\n";
        desLayer->rm_event();
        
        Process *proc = evt->evtProcess; // this is the process the event works on

        int CURRENT_TIME = evt->evtTimeStamp;
        proc->timeInPrevState = CURRENT_TIME - proc->state_ts;
        proc->state_ts = CURRENT_TIME;

        //cout << evt->evtTimeStamp << " " << proc << " " << proc->timeInPrevState << ": " 
          //   << proc->currState << "-> " << evt->transition ;
        //cout << " prio = " << proc->dynamic_priority ;
        Event *newEvt;
        
        switch(evt->transition) { //which state to transition to?
            case TRANS_TO_READY:
                // must come from BLOCKED or from PREEMPTION
                if (*stype == 'E' && (proc->currState == BLOCKED || proc->currState == CREATED)) {
                    proc->canPreemptOthers = true;
                }
                if (proc->currState == BLOCKED) {
                    //proc->ioburst = myrandom(proc->io);
                    proc->ioTime += proc->timeInPrevState;
                    proc->dynamic_priority = proc->static_priority-1;
                }
                // must add to run_queue
                THE_SCHEDULER->add_process(proc);                 
                proc->timeAdded = CURRENT_TIME;
                proc->currState = READY;
                CALL_SCHEDULER = true; // conditional on whether something is run
                break;
            case TRANS_TO_RUN:
                if (proc->preemptedTag == NOT) {
                    proc->cpuburst = min(myrandom(proc->cb), proc->timeRemaining);
                } 
                //else {
                //    proc->cpuburst -= quantum;
                //}
                proc->preemptedTag = NOT;
                
                CURRENT_RUNNING_PROCESS = proc;
                proc->currState = RUNNING;
                proc->cpuWaitingTime += proc->timeInPrevState;
                //cout << "reach here\n";
                // create event for done
                if (proc->timeRemaining <= min(proc->cpuburst, THE_SCHEDULER->quantum)) {
                    newEvt = new Event(CURRENT_TIME + proc->timeRemaining, proc, TRANS_TO_DONE, CURRENT_TIME, getEventID());
                    
                }
                // create event for either preemption or blocking
                else if (proc->cpuburst <= THE_SCHEDULER->quantum) {
                    // finish the cpu burst before expired
                    // for blocking

                    newEvt = new Event(CURRENT_TIME + proc->cpuburst, proc, TRANS_TO_BLOCK, CURRENT_TIME, getEventID());
                } else {
                    // for preemption:
                    newEvt = new Event(CURRENT_TIME + THE_SCHEDULER->quantum, proc, TRANS_TO_PREEMPT, CURRENT_TIME, getEventID());
                    proc->preemptedTag = Q_EXPIRED;
                }
                proc->pendingEvt = newEvt;
                desLayer->put_event(newEvt);
                break;
            case TRANS_TO_BLOCK:
                CURRENT_RUNNING_PROCESS = nullptr;
                proc->ioburst = myrandom(proc->io);
                
                proc->timeRemaining -= proc->timeInPrevState;
                proc->currState = BLOCKED;
                //proc->cpuburst = min(myrandom(proc->cb), proc->timeRemaining);

                IOUse.push_back(make_pair(CURRENT_TIME, CURRENT_TIME + proc->ioburst));
                // create an event for when process becomes READY again
                newEvt = new Event(CURRENT_TIME + proc->ioburst, proc, TRANS_TO_READY, CURRENT_TIME, getEventID());
                desLayer->put_event(newEvt);
                proc->pendingEvt = newEvt;
                CALL_SCHEDULER = true;
                break;
            case TRANS_TO_PREEMPT:
                // add to run_queue (no event is generated)
                proc->timeRemaining -= proc->timeInPrevState;
                CURRENT_RUNNING_PROCESS = nullptr;
                // if preempted because quantum expires
                if (proc->preemptedTag == Q_EXPIRED) {
                    proc->cpuburst -= quantum;
                } else if (proc->preemptedTag == OTHER_PROCESS) {
                    proc->cpuburst -= proc->timeInPrevState;
                }// if preempted because another process becomes ready                
                
                THE_SCHEDULER->update_dynamic_prio(proc);
                THE_SCHEDULER->add_process(proc);
                proc->timeAdded = CURRENT_TIME;
                
               // cout << "currState is " << proc->currState << "\n";
                proc->currState = READY;
                CALL_SCHEDULER = true;
                break;
            case TRANS_TO_DONE: 
                // from running
                proc->timeRemaining = 0;
                proc->currState = DONE;
                CURRENT_RUNNING_PROCESS = nullptr;
                CALL_SCHEDULER = true;
                break;
        }
        // remove current event object from Memory
        //cout << " rem=" << proc->timeRemaining;
        //cout << " cb=" << proc->cpuburst;
        //cout << " ib=" << proc->ioburst<<"\n";
        //cout << "time added to run queue: " << proc->timeAdded << "\n";
        delete evt;
        evt = nullptr;
        
        if (CALL_SCHEDULER) {
            if (proc->canPreemptOthers && CURRENT_RUNNING_PROCESS != nullptr &&
                proc->dynamic_priority > CURRENT_RUNNING_PROCESS->dynamic_priority &&
                CURRENT_RUNNING_PROCESS->pendingEvt->evtTimeStamp != CURRENT_TIME) {
                CURRENT_RUNNING_PROCESS->pendingEvt->deleted = true;
                newEvt = new Event(CURRENT_TIME, CURRENT_RUNNING_PROCESS, TRANS_TO_PREEMPT, CURRENT_TIME, getEventID());
                desLayer->put_event(newEvt);
                CURRENT_RUNNING_PROCESS->preemptedTag = OTHER_PROCESS;
                //CURRENT_RUNNING_PROCESS->pendingEvt = newEvt;
                continue;
            }

            if (!desLayer->queue_empty()) {
                if ((desLayer->get_event())->evtTimeStamp == CURRENT_TIME) {
                    continue;  // process next event from Event queue
                }
            }
            
            CALL_SCHEDULER = false; // reset global flag
            if (CURRENT_RUNNING_PROCESS == nullptr) {                    
                CURRENT_RUNNING_PROCESS = THE_SCHEDULER->get_next_process();
                
                //cout << "current process is " << CURRENT_RUNNING_PROCESS << "\n";
                if (CURRENT_RUNNING_PROCESS == nullptr) {
                    continue;
                } else if (CURRENT_RUNNING_PROCESS->currState == DONE || CURRENT_RUNNING_PROCESS->currState == BLOCKED) {
                    CURRENT_RUNNING_PROCESS = THE_SCHEDULER->get_next_process();
                    
                }

                // create event to make this process runnable for same time
                newEvt = new Event(CURRENT_TIME, CURRENT_RUNNING_PROCESS, TRANS_TO_RUN, CURRENT_TIME, getEventID());
                //cout << CURRENT_RUNNING_PROCESS << "!!!!!\n";
                desLayer->put_event(newEvt);
                proc->pendingEvt = newEvt;
            } //else if {// there is a running process and scheduler E
                // if 
                //if (proc->dynamic_priority > CURRENT_RUNNING_PROCESS->dynamic_priority) {

                //}
            //}
        }
        //testloop ++;
    }
    delete THE_SCHEDULER;
}

class sortClass {
    public:
        bool operator() (pair<int, int>& p1, pair<int, int>& p2) {
            return p1.first < p2.first;
        }
};

int main(int argc, char ** argv) {
    int vflag = 0;  // verbose
    char *stype = NULL;  // scheduler specification
    int c;
    int index;

    extern char *optarg;
    
    while ((c = getopt(argc, argv, "vs:")) != -1) {
        switch(c)
        {
            case 'v':
                vflag = 1;
                break;
            case 's':
                stype = optarg;
                break;
        }
    }
    
    // read in randfile
    ifstream randfile(argv[optind + 1]);
    int number;
    randfile >> number;
    randvals = new int[number+1];
    randvals[0] = number;
    for (int a = 1; a < number; a++) {
        randfile >> randvals[a];
    }

    // create process objects
    // for each line of inputfile, generate corresponding processes
    // Process(at, tc, cb, io)
    FILE *inputfile;
    inputfile = fopen(argv[optind], "r");
    vector<Process> allProc;
    char line[LINE_MAX];
    
    while (fgets(line, INT_MAX, inputfile) != NULL) {
        if (strlen(line) == 0) {
            fgets(line, INT_MAX, inputfile);
        }
        
        char * token;        
        token = strtok(line, " \t\n");
        int procInfo[4];
        procInfo[0] = atoi(token);
        int i = 1;
        while (token != NULL && i <4) {
            token = strtok(NULL, " \t\n");
            procInfo[i] = atoi(token);
            i++;
        }
        Process process = Process(procInfo[0], procInfo[1], procInfo[2], procInfo[3]);
        allProc.push_back(process);
        
    }
    fclose(inputfile);
    
    
    // put initial events for processes' arraival into the event queue 
    DES des = DES();
    DES *desLayer = &des;
    Event *newEvt;
    
    for (vector<Process>::iterator it = allProc.begin(); it != allProc.end(); ++it) {
        newEvt = new Event(it->at, &(*it), TRANS_TO_READY, 0, getEventID());
        desLayer->put_event(newEvt);
        it->pendingEvt = newEvt;
        //it->cpuburst = min(myrandom(it->cb), it->timeRemaining);
        //it->ioburst = myrandom(it->io);
    }

    vector<pair<int, int> > IOUse;
    // start simulation
    Simulation(stype, desLayer, IOUse);

    int lastFT = 0, i = 0;
    double cpuUti, ioUti, avgTT, avgCW, throughput;
    // print out statistics
    for (vector<Process>::iterator it = allProc.begin(); it != allProc.end(); ++it) {
        it->finishingTime = it->at + it->tc + it->ioTime + it->cpuWaitingTime;
        printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n",
             i, it->at, it->tc, it->cb, it->io, it->static_priority, 
             it->finishingTime, 
             it->tc + it->ioTime + it->cpuWaitingTime, 
             it->ioTime, it->cpuWaitingTime);
        if (it->finishingTime > lastFT) {
            lastFT = it->finishingTime;
        }
        cpuUti += it->tc;
        ioUti += it->ioTime;
        avgTT += it->finishingTime - it->at;
        avgCW += it->cpuWaitingTime;
        i++;
    }
    cpuUti = cpuUti/lastFT*100;
    
    sortClass sortObj;
    sort(IOUse.begin(), IOUse.end(), sortObj);
    int n = IOUse.size();
    double totalIOUse = 0.0;
    if (!IOUse.empty()) {
        int start = IOUse[0].first, end = IOUse[0].second, n = IOUse.size();
        for(int i = 1; i < n; ++i) {
            if (end >= IOUse[i].first) {
                end = max(end, IOUse[i].second);
            }
            else {
                totalIOUse += end - start;
                start = IOUse[i].first;
                end = IOUse[i].second;
            }
        }
        totalIOUse += end - start;
    }
    ioUti = totalIOUse /lastFT*100;

    double count = allProc.size();
    //cout << "count " << count << "\n";
    avgTT = avgTT/count;
    avgCW = avgCW/count;
    throughput = count/lastFT*100;
    printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n", 
         lastFT, cpuUti, ioUti, avgTT, avgCW, throughput);
    return 0;
}