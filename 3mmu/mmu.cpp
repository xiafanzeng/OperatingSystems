#include <iostream>
#include <fstream>
#include <istream>  
#include <sstream>
#include <string>   /* getline */
#include <vector>
#include <queue>
#include <stdlib.h>  /* atoi */
#include <unistd.h>  /* getopt */
#include <string.h>  /* strcmp */
#include <stdio.h> 
#include <iomanip>

using namespace std;

#define NO_OF_PTE (64)  // a page table contains 64 PTE 
#define TAU_DELTA (50) 

int num_frames;        // number of frames
unsigned long instr_count;


class Process;

typedef struct {
    Process* proc;  // the process it belongs to
    int start_vpage;
    int end_vpage;
    unsigned write_protect;
    unsigned file_mapped; 
} vma;

typedef struct
{
    unsigned long unmaps;
    unsigned long maps;
    unsigned long ins;
    unsigned long outs;
    unsigned long fins;
    unsigned long fouts;
    unsigned long zeros;
    unsigned long segv;
    unsigned long segprot;
} pstatistics;

// page table entry
typedef struct {
    unsigned present:1;
    unsigned write_protect:1;
    unsigned modified:1;
    unsigned referenced:1;
    unsigned pageout:1;
    unsigned frameNumber:7;
    unsigned file_mapped:1;
    // unsigned proc_no:3;
    unsigned page_index:6;
    unsigned just_referenced:1;
    // still 9 bits left
} pte_t; // can only be total of 32-bit size !!!!
// all bits of pte should be iniitialized to 0 before the instruction simulation

class Process {
public:
    int num_vmas;
    vector<vma*> vma_vector;
    pte_t* page_table[NO_OF_PTE];
    unsigned proc_no;
    pstatistics *pstats;

    Process() {
        pstats = new pstatistics;
        pstats->unmaps = 0;
        pstats->maps = 0;
        pstats->ins = 0;
        pstats->outs = 0;
        pstats->fins = 0;
        pstats->fouts = 0;
        pstats->zeros = 0;
        pstats->segv = 0;
        pstats->segprot = 0;
    }
};

Process* proc_arr;

// frame table entry
typedef struct {
    bool free = true;
    unsigned frame_index:7;
    //pte_t *pte; // proc-id, vpage
    unsigned proc_no;
    unsigned page_index;
    unsigned age_counter = 0;
    //unsigned age;
    int tau;  // at which instruction the frame was last used
} frame_t;

// a global frame table, describe the usage of each of its physical
// frames and maintain reverse mappings to the process and the vpage 
// that maps a particular frame.
frame_t** frametable;
int victim_hand = 0;

// virtual base class
class Pager {
public:
	virtual frame_t* select_victim_frame() {
        return NULL;
    } 
};

class FIFO: public Pager
{
public:
    frame_t* select_victim_frame() {
        frame_t* res = frametable[victim_hand];
        victim_hand++;
        if (victim_hand == num_frames) victim_hand = 0;
        return res;
    }
};

int* randvals;
int myrandom() {
    static int ofs = 1;
    int randS = randvals[0];
    int ret;
    // increase ofs with each invocation and 
    // wrap around when you run out of numbers in the file/array
    if (ofs <= randS) {
        ret = randvals[ofs] % num_frames;
        ofs++;
    } else {
        ofs = 1;
        ret = randvals[ofs] % num_frames;        
    }
    return ret;
}

class Random: public Pager
{
public:
    frame_t* select_victim_frame() {
        return frametable[myrandom()];
    }
};

class Clock: public Pager
{
public:
    frame_t* select_victim_frame() {
        pte_t* pte = proc_arr[frametable[victim_hand]->proc_no].page_table[frametable[victim_hand]->page_index];
        while (pte->referenced == 1) {
            
            pte->referenced = 0;
            victim_hand++;
            if (victim_hand == num_frames) victim_hand = 0;
            pte = proc_arr[frametable[victim_hand]->proc_no].page_table[frametable[victim_hand]->page_index];
        }
        frame_t* res = frametable[victim_hand];
        victim_hand++;
        if (victim_hand == num_frames) victim_hand = 0;
        return res;
    }
};

class NRU: public Pager
{
public:
    frame_t* select_victim_frame() {
        static int instr_count_last_reset = 0;  // total number of instructions when the R bits were reset last time
        int instr_number = instr_count - instr_count_last_reset + 1;
        int scan_pointer = victim_hand;
        int scan_count = 0;
        // remember the first frame that falls into each class
        int frame_class0 = -1;
        int frame_class1 = -1;
        int frame_class2 = -1;
        int frame_class3 = -1;
        pte_t* pte;
        
        while ((scan_count < num_frames) && (frame_class0 == -1)) {
            pte = proc_arr[frametable[scan_pointer]->proc_no].page_table[frametable[scan_pointer]->page_index];
            if (pte->referenced == 0 && pte->modified == 0) {
                if (instr_number >= TAU_DELTA) {
                    frame_class0 = scan_pointer;
                } else {
                    victim_hand = scan_pointer + 1;
                    if (victim_hand == num_frames) victim_hand = 0;
                    return frametable[scan_pointer];
                }
            } else if (pte->referenced == 0 && pte->modified == 1) {
                if (frame_class1 == -1) frame_class1 = scan_pointer;             
            } else if (pte->referenced == 1 && pte->modified == 0) {
                if (frame_class2 == -1) frame_class2 = scan_pointer;
            } else {
                if (frame_class3 == -1) frame_class3 = scan_pointer;
            }
            scan_pointer++;
            if (scan_pointer == num_frames) scan_pointer = 0;
            scan_count++;
        }

        if (instr_number >= TAU_DELTA) {
            // need to reset R bit for all frames after consider their class
            for (int i = 0; i < num_frames; ++i) {
                pte = proc_arr[frametable[i]->proc_no].page_table[frametable[i]->page_index];
                pte->referenced = 0;
            }
            instr_count_last_reset = instr_count + 1;
        }
        
        int result;
        if (frame_class0 != -1) {
            result = frame_class0;
        } else if (frame_class1 != -1) {
            result = frame_class1;
        } else if (frame_class2 != -1) {
            result = frame_class2;
        } else {
            result = frame_class3;
        } 
        victim_hand = result + 1;
        if (victim_hand == num_frames) victim_hand = 0;
        return frametable[result];         
    }
};


class Aging: public Pager
{
public:
    frame_t* select_victim_frame() {
        int scan_count = 0;
        int scan_pointer = victim_hand;
        int lowest_counter = victim_hand;
        unsigned ref_bit_shifted;
        pte_t* pte;

        while (scan_count < num_frames) {
            frametable[scan_pointer]->age_counter >>= 1;
            pte = proc_arr[frametable[scan_pointer]->proc_no].page_table[frametable[scan_pointer]->page_index];
            ref_bit_shifted = pte->referenced;
            ref_bit_shifted <<= 31;            
            frametable[scan_pointer]->age_counter += ref_bit_shifted;
            if (frametable[scan_pointer]->age_counter < 
                frametable[lowest_counter]->age_counter)
                lowest_counter = scan_pointer;
            scan_pointer++;
            //cout << "pointer: " << scan_pointer << "\n";
            if (scan_pointer == num_frames) scan_pointer = 0;
            scan_count++;
        }
        victim_hand = lowest_counter + 1;
        if (victim_hand == num_frames) victim_hand = 0;
        for (int i = 0; i < num_frames; ++i) {
            pte = proc_arr[frametable[i]->proc_no].page_table[frametable[i]->page_index];
            pte->referenced = 0;
        }
        
        return frametable[lowest_counter];
    }
};

class WorkingSet: public Pager
{
public:
    frame_t* select_victim_frame() {
        int smallest_tau_index = -1;
        int smallest_tau;
        int scan_count = 0;
        int scan_pointer = victim_hand;
        int res = -1;
        int reset_count = 0; 
        int smallest_tau_index_1 = -1;
        int smallest_tau_1;
        int stop_count;
        pte_t* pte;

        while (scan_count < num_frames) {
            pte = proc_arr[frametable[scan_pointer]->proc_no].page_table[frametable[scan_pointer]->page_index];
            if (pte->referenced == 0) {
                if ((instr_count - frametable[scan_pointer]->tau) >= TAU_DELTA) {
                    res = scan_pointer;
                    stop_count = scan_count;
                    break;
                } else {
                    if (smallest_tau_index == -1) {
                        smallest_tau_index = scan_pointer;
                        smallest_tau = frametable[scan_pointer]->tau;
                    }
                }
            } else {
                reset_count++;
                if (smallest_tau_index_1 == -1) {
                    smallest_tau_index_1 = scan_pointer;
                    smallest_tau_1 = frametable[scan_pointer]->tau;
                }
            }
            scan_pointer++;
            if (scan_pointer == num_frames) scan_pointer = 0;
            scan_count++;
        }

        scan_count = 0;
        scan_pointer = victim_hand;
        if (res != -1) {
            // reset those before res with ref = 1
            while (scan_count < stop_count) {
                pte = proc_arr[frametable[scan_pointer]->proc_no].page_table[frametable[scan_pointer]->page_index];
                if (pte->referenced == 1) {
                    pte->referenced = 0;
                    frametable[scan_pointer]->tau = instr_count;
                }
            scan_pointer++;
            if (scan_pointer == num_frames) scan_pointer = 0;
            scan_count++;    
            }
        } else if (res == -1 and reset_count != num_frames) {    
            while (scan_count < num_frames) {
                pte = proc_arr[frametable[scan_pointer]->proc_no].page_table[frametable[scan_pointer]->page_index];
                if (pte->referenced == 1) {
                    pte->referenced = 0;
                    frametable[scan_pointer]->tau = instr_count;
                } else {
                    if (frametable[scan_pointer]->tau < smallest_tau) {
                        smallest_tau_index = scan_pointer;
                        smallest_tau = frametable[scan_pointer]->tau;
                    }
                }
                scan_pointer++;
                if (scan_pointer == num_frames) scan_pointer = 0;
                scan_count++;
            }
            res = smallest_tau_index;
        } else if (res == -1 and reset_count == num_frames) { // all frames were referenced
            while (scan_count < num_frames) {
                pte = proc_arr[frametable[scan_pointer]->proc_no].page_table[frametable[scan_pointer]->page_index];
                pte->referenced = 0;
                if (frametable[scan_pointer]->tau < smallest_tau_1) {
                    smallest_tau_index_1 = scan_pointer;
                    smallest_tau_1 = frametable[scan_pointer]->tau;
                }
                frametable[scan_pointer]->tau = instr_count;
                scan_pointer++;
                if (scan_pointer == num_frames) scan_pointer = 0;
                scan_count++;
            }
            res = smallest_tau_index_1;
        }
        victim_hand = res + 1;
        if (victim_hand == num_frames) victim_hand = 0;
        return frametable[res];
    }
};

queue<frame_t*> freeframelist;
frame_t* allocate_frame_from_free_list() {
    frame_t* res;
    if (!freeframelist.empty()) {
        res = freeframelist.front();
        freeframelist.pop();
        return res;
    } else return NULL;
}

Pager *THE_PAGER;
// all frames are initially in a free list. once run out of free frames,
// then must implement paging
frame_t *get_frame() {
    frame_t *frame = allocate_frame_from_free_list();   
    if (frame == NULL) frame = THE_PAGER->select_victim_frame();
    return frame;
}

int main(int argc, char ** argv) {
    char *algo = NULL;     // paging algorithm
    char *options = NULL;   // options
    
    int c;
    int index;

    extern char *optarg;
    
    while ((c = getopt(argc, argv, "a:o:f:")) != -1) {
        switch(c)
        {
            case 'a':
                algo = optarg;
                break;
            case 'o':
                options = optarg;
                break;
            case 'f':
                num_frames = atoi(optarg);
                break;
        }
    }

    frametable = new frame_t*[num_frames];
    // all frames initially are free
    for (int i = 0; i < num_frames; ++i) {
        frametable[i] = new frame_t;
        frametable[i]->frame_index = i;
        frametable[i]->free = true;
        freeframelist.push(frametable[i]); // add to the free frame list
    }
    
    //Pager *THE_PAGER;
    if (strcmp(algo, "f") == 0) THE_PAGER = (Pager*) new FIFO();
    else if (strcmp(algo, "r") == 0) THE_PAGER = (Pager*) new Random();
    else if (strcmp(algo, "c") == 0) THE_PAGER = (Pager*) new Clock();
    else if (strcmp(algo, "e") == 0) THE_PAGER = (Pager*) new NRU();
    else if (strcmp(algo, "a") == 0) THE_PAGER = (Pager*) new Aging();
    else if (strcmp(algo, "w") == 0) THE_PAGER = (Pager*) new WorkingSet();

    
    char* option = options;
    bool option_O = false, option_P = false, option_F = false, option_S = false, 
        option_x = false, option_y = false, option_f = false, option_a = false;
    while (strcmp(option, "\0")) {
        if (strncmp(option, "O", 1) == 0) option_O = true;
        else if (strncmp(option, "P", 1) == 0) option_P = true;
        else if (strncmp(option, "F", 1) == 0) option_F = true;
        else if (strncmp(option, "S", 1) == 0) option_S = true;
        else if (strncmp(option, "x", 1) == 0) option_x = true;
        else if (strncmp(option, "y", 1) == 0) option_y = true;
        else if (strncmp(option, "f", 1) == 0) option_f = true;
        else if (strncmp(option, "a", 1) == 0) option_a = true;
        option++;
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

    
    // read input file
    ifstream inputfile(argv[optind]);
    string line;
    int num_procs = 0;  // number of processes
    //Process* proc_arr;
    
    int count_procs = 0;
    int num_vmas;
    int count_vmas = -1;
    // vmas for each process
    while (getline(inputfile, line)) {
        // cout << line << "\n";
        if (line.compare(0, 1, "#") == 0) continue;
        else {
            if (num_procs == 0) {
                istringstream iss(line);
                iss >> num_procs;
                proc_arr = new Process[num_procs];
                for (int i = 0; i < num_procs; ++i) {
                    proc_arr[i] = Process();
                    proc_arr[i].proc_no = i;
                }
                continue;
            }

            if ((count_procs != num_procs) && (count_vmas == -1)) {
                istringstream iss(line);                
                iss >> num_vmas;
                proc_arr[count_procs].num_vmas = num_vmas;
                proc_arr[count_procs].proc_no = count_procs;
                count_vmas = 0;
                continue;
            }
            if ((count_procs != num_procs) && (count_vmas != num_vmas)) {
                istringstream iss(line);
                vma *vma_new = new vma;
                proc_arr[count_procs].vma_vector.push_back(vma_new);
                iss >> vma_new->start_vpage >> vma_new->end_vpage
                >> vma_new->write_protect >> vma_new->file_mapped;
                count_vmas++;
                if (count_vmas == num_vmas) {
                    count_procs++;
                    num_vmas = 0;
                    count_vmas = -1;
                    if (count_procs == num_procs) break;
                    //continue;
                }
            }
        }        
    }
    
    for (int j = 0; j < num_procs; ++j) {
        for (int i = 0; i < NO_OF_PTE; ++i) {
            proc_arr[j].page_table[i] = new pte_t;
            //proc_arr[j].page_table[i]->proc_no = j;
            proc_arr[j].page_table[i]->present = 0;
            proc_arr[j].page_table[i]->referenced = 0;
            proc_arr[j].page_table[i]->modified = 0;
            proc_arr[j].page_table[i]->pageout = 0;
            for (vector<vma*>::iterator it = proc_arr[j].vma_vector.begin(); 
                it != proc_arr[j].vma_vector.end(); ++it) {
                if (i >= (*it)->start_vpage && i <= (*it)->end_vpage) {
                    proc_arr[j].page_table[i]->write_protect = (*it)->write_protect;
                    proc_arr[j].page_table[i]->file_mapped = (*it)->file_mapped;
                }
            }
        }        
    }
    
    instr_count = 0;
    unsigned long ctx_switches = 0;
    unsigned long process_exits = 0;
    unsigned long long cost = 0;
    char instr;
    unsigned procid_or_vpage;
    Process* current_process = nullptr;

    // instructions
    while (getline(inputfile, line)) {
        if (line.compare(0, 1, "#") == 0) continue;
        else {
            //cout << line <<"\n";
            istringstream iss(line);
            
            iss >> instr >> procid_or_vpage;
            if (option_O) cout << instr_count << ": ==> " << instr << " " << procid_or_vpage << "\n";
            
            
            switch(instr) {
                case 'c':
                    current_process = &(proc_arr[procid_or_vpage]);
                    //current_process->proc_no = procid_or_vpage;
                    
                    // a context switch to process procid is to be performed
                    ctx_switches++;
                    cost += 121;
                    break;
                case 'e':
                    // procid_or_vpage is procid
                    // current prcoess exits
                    process_exits++;
                    cost += 175;
                    cout << "EXIT current process " << current_process->proc_no << "\n";
                    for (int i = 0; i < NO_OF_PTE; ++i) {
                        current_process->page_table[i]->pageout = 0;
                        if (current_process->page_table[i]->present) {
                            current_process->page_table[i]->present = 0;
                            if (option_O) cout << " UNMAP " << current_process->proc_no 
                                << ":" << i << "\n";
                            current_process->pstats->unmaps++;
                            cost += 400;
                            if (current_process->page_table[i]->file_mapped and 
                                current_process->page_table[i]->modified) {
                                if (option_O) cout << "FOUT\n";
                                current_process->pstats->fouts++;
                                cost += 2500;
                            }
                            frametable[current_process->page_table[i]->frameNumber]->free = true;
                            freeframelist.push(frametable[current_process->page_table[i]->frameNumber]);
                            // free frame
                        }
                    }
                    current_process = nullptr;
                    break;
                case 'r':
                case 'w':
                    // procid_or_vpage is vpage
                    unsigned vpage = procid_or_vpage;
                    cost += 1;
                    // 'r': a load/read operation is performed on vpage of the 
                    // currently running process
                    // 'w': a store/write operation is performed on vpage of the
                    // currently running process
                    
                    pte_t *pte = current_process->page_table[vpage];
                    pte->page_index = vpage;
                    if (!pte->present) {                       
                        // page fault
                        // first determine the vpage is part of one of the VMAs
                        bool in_vmas = false;
                        for (vector<vma*>::iterator it = current_process->vma_vector.begin(); 
                            it != current_process->vma_vector.end(); ++it) {
                            if (vpage >= (*it)->start_vpage && vpage <= (*it)->end_vpage) {
                                in_vmas = true;
                            }
                        }
                        // if not, SEGV, and move on to the next instruction
                        if (in_vmas == false) {
                            if (option_O) cout << " SEGV\n";
                            current_process->pstats->segv++;
                            cost += 240;
                            instr_count++;
                            continue;
                        }
                        // if yes, a frame must be allocated, assigned to the PTE
                        // belonging to the vpage of this instruction
                        // then populated with the proper content, which depends on
                        // whether this page was previously paged out.
                        else {

                            frame_t *newframe = get_frame();
                            //cout << newframe->frame_index;
                            pte->frameNumber = newframe->frame_index;
                            // unmap the victim frame
                            // if not free frame
                            if (!newframe->free) {
                                pte_t * prev_pte = proc_arr[newframe->proc_no].page_table[newframe->page_index];
                                if (option_O) cout << " UNMAP " << newframe->proc_no << ":" << prev_pte->page_index << "\n";
                                proc_arr[newframe->proc_no].pstats->unmaps++;
                                cost += 400;
                                
                                if (prev_pte->file_mapped && prev_pte->modified) {
                                    if (option_O) cout << " FOUT\n";
                                    proc_arr[newframe->proc_no].pstats->fouts++;
                                    cost += 2500;
                                } else if (prev_pte->modified) {
                                    if (option_O) cout << " OUT\n";
                                    proc_arr[newframe->proc_no].pstats->outs++;
                                    cost += 3000;
                                    prev_pte->pageout = 1;
                                }
                                // reset the PTE
                                prev_pte->present = 0;
                                prev_pte->modified = 0;
                                //prev_pte->frameNumber = ??;
                                // what else to reset?
                            }
                            if (pte->file_mapped) {
                                if (option_O) cout << " FIN\n";
                                current_process->pstats->fins++;
                                cost += 2500;
                            } else if (pte->pageout) {
                                if (option_O) cout << " IN\n";
                                current_process->pstats->ins++;
                                cost += 3000;
                            } else {
                                if (option_O) cout << " ZERO\n";
                                current_process->pstats->zeros++;
                                cost += 150;
                            }
                            newframe->free = false;
                            newframe->proc_no = current_process->proc_no;
                            newframe->page_index = vpage;
                            if (option_O) cout << " MAP " << newframe->frame_index << "\n";
                            newframe->age_counter = 0; // reset age to 0
                            newframe->tau = instr_count;
                            current_process->pstats->maps++;
                            cost += 400;
                            // now reuse the frame
                            pte->frameNumber = newframe->frame_index;
                            pte->present = 1;
                            
                        }     
                    }
                    if (instr == 'r') {
                        pte->referenced = 1;
                    } else if (instr == 'w') {
                        pte->referenced = 1;
                        pte->modified = 1;
                        if (pte->write_protect == 1) {
                            if (option_O) cout << " SEGPROT\n";
                            current_process->pstats->segprot++;
                            cost += 300;
                            pte->modified = 0;
                        }
                    }
                    break;
            }
            instr_count++;

            if (option_x) {
                cout << "PT[" << current_process->proc_no << "]: ";
                for (int j = 0; j < NO_OF_PTE; ++j) {
                    pte_t* pte = current_process->page_table[j];
                    if (!pte->present) {
                        if (pte->pageout) cout << "#"; else cout << "*";
                    } else {
                        cout << j << ":";
                        if (pte->referenced) cout << "R"; else cout << "-";
                        if (pte->modified) cout << "M"; else cout << "-";
                        if (pte->pageout) cout << "S"; else cout << "-";
                    }
                    cout << " ";
                }
                cout << "\n";
            }
            if (option_y) {
                for (int i = 0; i < num_procs; ++i) {
                    cout << "PT[" << i << "]: ";
                    for (int j = 0; i < NO_OF_PTE; ++j) {
                        pte_t* pte = proc_arr[i].page_table[j];
                        if (pte->present) {
                            if (pte->pageout) cout << "#"; else cout << "*";
                        } else {
                            cout << j << ":";
                            if (pte->referenced) cout << "R"; else cout << "-";
                            if (pte->modified) cout << "M"; else cout << "-";
                            if (pte->pageout) cout << "S"; else cout << "-";
                        }
                        cout << " "; 
                    }
                cout << "\n";
                }
            }
            if (option_f) {
                cout << "FT: ";
                for (int i = 0; i < num_frames; ++i) {
                    if (frametable[i]->free) cout << "*";
                    else cout << frametable[i]->proc_no << ":" 
                        << frametable[i]->page_index;
                    cout << " ";
                }
                cout << "\n";
            }               
        }
    }
    
    inputfile.close();
    randfile.close();

    if (option_P) {
        for (int i = 0; i < num_procs; ++i) {
            cout << "PT[" << i << "]: ";
            for (int j = 0; j < NO_OF_PTE; ++j) {
                pte_t* pte = proc_arr[i].page_table[j];
                if (!pte->present) {
                    if (pte->pageout) cout << "#"; else cout << "*";
                } else {
                    cout << j << ":";
                    if (pte->referenced) cout << "R"; else cout << "-";
                    if (pte->modified) cout << "M"; else cout << "-";
                    if (pte->pageout) cout << "S"; else cout << "-";
                }
                cout << " "; 
            }
            cout << "\n";
        }   
    }

    if (option_F) {
        cout << "FT: ";
        for (int i = 0; i < num_frames; ++i) {
            if (frametable[i]->free) cout << "*";
            else cout << frametable[i]->proc_no << ":" 
                << frametable[i]->page_index;
            cout << " ";
        }
        cout << "\n";
    }

    if (option_S) {
        for (int i = 0; i < num_procs; ++i) {
            Process *proc = &proc_arr[i];
            pstatistics *pstats = proc->pstats;
            printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
                proc->proc_no,
                pstats->unmaps, pstats->maps, pstats->ins, pstats->outs,
                pstats->fins, pstats->fouts, pstats->zeros,
                pstats->segv, pstats->segprot);
        }
        printf("TOTALCOST %lu %lu %lu %llu\n", instr_count, ctx_switches, process_exits, cost);
    }
    delete THE_PAGER;
    delete frametable;
    for (int j = 0; j < num_procs; ++j) {
        for (int i = 0; i < NO_OF_PTE; ++i) {
            delete proc_arr[j].page_table[i];
        }
        delete proc_arr[j].pstats;
        while (!proc_arr[j].vma_vector.empty()) {
            delete proc_arr[j].vma_vector.back();
            proc_arr[j].vma_vector.pop_back();
        }
    }
    //cout << "frames:" << frametable << "\n";
    //delete frametable;
    delete randvals;
    //cout << "array:" << proc_arr << "\n";
    //delete proc_arr;
}