/*::USED LIBRARIES::*/
#include<iostream> //used for input/output
#include<string.h> //used for strings
#include <unistd.h> //used for sleep
#include <iomanip> //used for set precision
#include <pthread.h>//used to spawn parallel threads
#include <vector>//used to store Process objects
#include <queue> //Used for queue
#include <math.h> //Used for round
#include <chrono>//Used for thread waits
#include <thread>//used for thread waits
#include <sys/time.h>//used for time calculations
#include <mutex>//used to lock threads
#include <algorithm>//used to sort process in rdyQ for shortest first

using namespace std;

/*::CLASSES/STRUCTS::*/

/*A struct that we used as threads arguments when we create them
  ->Helps on indicating which cores is which when we assign processes
  to each core
 */
struct cores_struct
{
    int core_num;
};

/*A struct used to get time information*/
struct timeval tim;

/*
The class process is used to create objects that resembles our processes. It has all the attributes
defined in our assignment paper along with other attributes that was necessary to create this simulation
*/
class Process
{
public:
    /*The process attributes: below is explanation for what each is used*/
    int PID, priority, io_burst, cpu_burst,ctxsw;
    double start_time, cpu_time,io_time, wait_time,remain_time,total_cpu_time,total_time;
    string state,core;
    //A copy constructor used to store temp Process object (when it's in the front of the ready Queue
    Process(const Process &ps)
    {
        this->PID = ps.PID; //PID: stores a unique process id upon creation starting from 1024
        this->priority = ps.priority; //priority: used to indicate priority for preemptive priority scheduler
        this->io_burst = ps.io_burst; //io_burst: copies the number of io bursts
        this->cpu_burst = ps.cpu_burst;//cpu_burst: copies the number of cpu bursts
        this->start_time = ps.start_time;//start_time: copies the time of the process creation
        this->cpu_time = ps.cpu_time;//cpu_time: copies the random ms that a process will utilize cpu for(1000,6000)ms
        this->io_time = ps.io_time;//io_time: copies the random ms that a process will be in a ready Queue for (1000,6000)ms
        this->state = ps.state;//state: copies the state of the process (ready,running,i/o,or terminated)
        this->core = ps.core;//core: copies the core on which a process is running
        this->remain_time = ps.remain_time; //remain_time: copies the remaining time for each process
        this->total_cpu_time = ps.total_cpu_time;//total_cpu_time:copies the total time a process has been on a cpu
        this->total_time = start_time;//total_time: copies the total time for the process since it's creation
        this->ctxsw = ctxsw;

    };
    Process(int PID,int priority,int io_burst,double start_time,double cpu_time, double io_time,string state,string core)
    {
        this->PID = PID;//PID: stores a unique process id upon creation starting from 1024
        this->priority = priority; //priority: used to indicate priority for preemptive priority scheduler
        this->io_burst = io_burst;//io_burst: stores a random number of io bursts(2,10)
        this->cpu_burst = io_burst+1;//cpu_burst: calculates cpu_bursts based on a random io_bursts(2,10)+1
        this->start_time = start_time;//start_time: stores the time of the process creation
        this->cpu_time = cpu_time;//cpu_time: stores a random ms that a process will utilize cpu for(1000,6000)ms
        this->io_time = io_time;//io_time: stores a random ms that a process will be in a ready Queue for (1000,6000)ms
        this->cpu_burst = cpu_burst;//cput_burst: uses io_burst to determine the number of cpu_bursts
        this->state = state;//state: stores the state of the process (ready,running,i/o,or terminated)
        this->core = core;//core: stores the core on which a process is running
        this->remain_time = cpu_burst * cpu_time;//remain_time: calculates the remaining time for each process
        this->total_cpu_time = 0;//total_cpu_time:stores the total time a process has been on a cpu
        this->total_time = start_time;//total_time: stores the total time for the process since it's creation
        this->ctxsw = 0;
    };
    // returns process id
    int getPID()
    {
        return this->PID;
    }
    // returns process priority
    int getPriority()
    {
        return this->priority;
    }
    // returns number of io bursts
    int getIOBurst()
    {
        return this->io_burst;
    }
    // returns number of cpu burst
    int getCPUBurst()
    {
        return this->cpu_burst;
    }
    // calculates and return total time since the process  has been created
    double getStartTime()
    {
        //i.e. if the process is not working any more, stop calculating new a new time
        //and return the stored time instead
        if(this->state!="terminated")
        {
            gettimeofday(&tim, NULL);//calling tim to get current time
            double dTime = tim.tv_sec+(tim.tv_usec/1000000.0);//stores current time
            this->total_time = (dTime - this->start_time);//calculates returns current time - starting time
        }
        return total_time;
    }
    // returns cpu time for each cpu burst
    double getCPUTime()
    {
        return this->cpu_time;
    }
    // returns io time for each io burst
    double getIOTime()
    {
        return this->io_time;
    }
    // returns waiting time
    double getWaitTime()
    {
        return this->wait_time;
    }
    // returns remaining time
    double getRemainTime()
    {
        return this->remain_time;
    }
    // returns total cpu time
    double getTotalCPUTime()
    {
        return this->total_cpu_time;
    }
    // returns state
    string getState()
    {
        return this->state;
    }
    // returns core
    string getCore()
    {
        return this->core;
    }
};

/*::GLOBAL VARIABLES::*/
double fst_half_cpu_time=0;
double scnd_half_cpu_time=0;
int fst_half_ctxsw=0;
int scnd_half_ctxsw=0;
pthread_mutex_t lockit;//A mutex to prevent parallel threads from accessing the same resource
pthread_mutex_t lockit2;//A mutex to prevent parallel threads from accessing the same resource
int NUM_CORES;//stores the number of cores from the options arguments
int NUM_PROC;//stores the number of processes from the options arguments
int SCHED_ALG;//stores the scheduler algorithm value from the options arguments
int CTX_SW;//stores the context switching value from the options arguments
int T_SLC;//stores the time slices for round robin from the options arguments
int count_CTX_SW=0;//to track the number of context switching to calculate our statistics
vector<Process> ps;//A vector array to hold our process objects
queue<Process> rdyQ;//A queue that's used to order the processes executions
vector<Process> sortedPs;//For shortest first: A vector array to hold a sorted-by-cpu-time list of the same process

/*::FUNCTIONS DECLATIONS::*/
void *sort_ps(void*);//A function that sorts sortedPs, used when we want to push ps to the ready Q in shortest first alg.
int lenHelper(unsigned x);//A function that helps with sorting our numerical output that're used in printInfo
bool checkArgs(int argc, char* argv[]);//A function that checks the argument options that the user enters to run the sim.
void usage();//A function that prints the right way to use the simulator
void printInfo();//A function that prints the information of our processes
void printStats();//A function that prints the scheduler stats upon termination of all processes
void * createPs(void*);//A function that create the processes
void * ioWait(void* i);//A function that simulates a process waiting in i/o
bool checkAllTerminated();//A function that checks if all processes are terminated
void* schedAlgorithm(void*);//A function that checks which scheduler algorithm will be running and calls it
void * roundRobin(void*);//A function that simulates round robin algorithm
void * shortestFirst(void*);//A function that simulates shortest first algorithm
void * firstComeFirstServe(void* arg);//A function that simulates first come first serve algorithm
void * shortestFirstWait(void* i);
void * priority(void* arg);
void * sort_pr(void*);
/*Main function: calls checkArgs to checks for the arguments
If arguments are correct it will start the simulator,otherwise
it checkArgs will handle displaying the error
When the simulator starts it creates a thread that calls schedAlgorithim
to create our process and run the proper scheduler. While doing so main thread
will continue displaying the status of our process via printInfo and keep checking
if all processes are terminated then it will display that statistics.
*/
int main (int argc, char *argv[])
{
    //checking the user args
    if(checkArgs(argc,argv))
    {
        srand(time(NULL));   //seeding time for randomness
        pthread_t pid;//thread to call schedAlgorithm
        pthread_create(&pid,NULL,&schedAlgorithm,NULL);//creating the schedAlgorithim thread

        //keeps checking for the processes states and printing info
        while (!checkAllTerminated())
        {
            printInfo();

        }
        //print the info one more time
	//sleep(500);
        printInfo();
        //print the statistics of the scheduler
        printStats();
    }
    return 0;
}

/*schedAlgorithm function:
1-Creates the processes via createPs thread.
2-It calls the right algorithm to start its simulation
via another thread(s), based on the number of cores specified.
*/
void* schedAlgorithm(void*)
{
    pthread_t pid;
    //creating the createPs thread
    pthread_create(&pid,NULL,&createPs,NULL);
    //initializing our mutex
    pthread_mutex_init(&lockit, NULL);

    //If algorithm = Round Robin
    if(SCHED_ALG == 0)
    {
        //declaring the struct for our cores
        struct cores_struct args[NUM_CORES];
        //declaring the threads for our cores
        pthread_t cores[NUM_CORES];
        //for the number of cores
        for(int i=0; i<NUM_CORES; i++)
        {
            //assigning cores numbers
            args[i].core_num=i+1;
            //creating roundRobin cores
            pthread_create(&cores[i],NULL,roundRobin,(void*)&args[i]);
            //fix an issue where cores_num don't get initialized correctly by args[i]
            this_thread::sleep_for(std::chrono::milliseconds(10));
        }

    }
    //If algorithm = First Come First Serve
    if(SCHED_ALG == 1)
    {
        //declaring the struct for our cores
        struct cores_struct args[NUM_CORES];
        //declaring the threads for our cores
        pthread_t cores[NUM_CORES];
        //for the number of cores
        for(int i=0; i<NUM_CORES; i++)
        {
            //assigning cores numbers
            args[i].core_num=i+1;
            //creating FCFS cores
            pthread_create(&cores[i],NULL,firstComeFirstServe,(void*)&args[i]);
            //fix an issue where cores_num don't get initialized correctly by args[i]
            this_thread::sleep_for(std::chrono::milliseconds(10));
        }

    }
    //If algorithm = Shortest First
    if(SCHED_ALG == 2)
    {
        //declaring the struct for our cores
        struct cores_struct args[NUM_CORES];
        //declaring the threads for our cores
        pthread_t cores[NUM_CORES];
        //for the number of cores
        for(int i=0; i<NUM_CORES; i++)
        {
            //assigning cores numbers
            args[i].core_num=i+1;
            //creating Shortest First cores
            pthread_create(&cores[i],NULL,shortestFirst,(void*)&args[i]);
            //fix an issue where cores_num don't get initialized correctly by args[i]
            this_thread::sleep_for(std::chrono::milliseconds(1));
        }

    }
    //If algorithm = Preemptive Priority
    if(SCHED_ALG == 3)
    {
        //declaring the struct for our cores
        struct cores_struct args[NUM_CORES];
        //declaring the threads for our cores
        pthread_t cores[NUM_CORES];
        //for the number of cores
        for(int i=0; i<NUM_CORES; i++)
        {
            //assigning cores numbers
            args[i].core_num=i+1;
            //creating Preemptive Priority cores
            pthread_create(&cores[i],NULL,priority,(void*)&args[i]);
            //fix an issue where cores_num don't get initialized correctly by args[i]
            this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    return NULL;
}

/*
Round Robin: a function that we call for our threads that simulates round robin:
It takes the processes from the ready queue put them on a core, and core processes
for a the defined time slice, then it places the processes in i/o state, before it get
pushed back into the ready queue again.
It continues doing so until the ready queue is empty.
    
*/
void * roundRobin(void* arg)
{
    //Each thread will know what core it is by it's own arg
    struct cores_struct *input = (cores_struct*)arg;
    // gets the core number and store it as a string
    string thisCore = to_string(input->core_num);
    //loop that keeps running until all processes are terminated
    while(true)
    {   //boolean that indicates wither a core is free to work or not
        bool freetoWork = false;
        //locks each core so they will not be checking the queue front at the same time
        pthread_mutex_lock(&lockit);
        //A processes that get's the first process in the queue
        Process * temp;
        //checks if the ready queue is not empty, i.e. more work to do
        if(!rdyQ.empty())
        {
            //a core is free to work
            freetoWork = true;
            //we get the process in the front of the queue 
            temp = new Process(rdyQ.front());
            //we pop that process
            rdyQ.pop();
        }
        //unlock the core to do the work it has
        pthread_mutex_unlock(&lockit);
        
        //work indicator
        if(freetoWork)
        {
            //a loop that finds the front queue process in our processes array ps
            for(unsigned int i=0; i<ps.size(); i++)
            {   //found the process
                if(temp->getPID() == ps[i].getPID())
                {   /**Put the process on the core**/
                    //change state to running
                    ps[i].state = "running";
                    //assigns the core number
                    ps[i].core = thisCore;
                    //decrement the number of cpu bursts
                    ps[i].cpu_burst = ps[i].cpu_burst-1;
                    //simulate the process of a time slice
                    this_thread::sleep_for(std::chrono::milliseconds(((int)T_SLC)));
                    
                    //checks if remain time is <= 0: i.e. a process would be terminated
                    if((ps[i].remain_time-T_SLC)<= 0)
                    {   //update the state to terminated
                        ps[i].state = "terminated";
                        //it's not on a core
                        ps[i].core = "--";
                        //increment the total cpu time by the remaining times
                        ps[i].total_cpu_time += ps[i].remain_time;
                        //the remain time is 0 (so we don't get negative numbers as a remaining time)
                        ps[i].remain_time=0;
                    }
                    //otherwise, the thread will start simulating i/o
                    else
                    {   //decrement the total cpu time by the time slice
                        ps[i].total_cpu_time += T_SLC;
                        //decrement the remaining time by the time slice
                        ps[i].remain_time -= T_SLC;
                        //thread that will simulate i/o
                        pthread_t waitThread;
                        //create the thread running through iowait function
                        pthread_create(&waitThread,NULL,ioWait,(void*)&i);
                    }
                    break;//make sure the loop stops when the process is found
                }

            }
            //loop to find the current working process and increment it's context switching values
            for(unsigned int i=0;i<ps.size();i++){
                 if(temp->getPID() == ps[i].getPID())
                {
                    ps[i].ctxsw++;
                }
            }
            //increment total number of context switching
            count_CTX_SW++;
            //simulates context switching
            this_thread::sleep_for(std::chrono::milliseconds((int)CTX_SW));
            delete temp;//destroying the process that've been running on the core

        }
    }
    return NULL;
}


void * shortestFirst(void* arg)
{   pthread_mutex_init(&lockit2, NULL);
    struct cores_struct *input = (cores_struct*)arg;
    string thisCore = to_string(input->core_num);
    bool first_time = true;
    while(true)
    {
        bool freetoWork = false;
        pthread_mutex_lock(&lockit);
        Process * temp;
        if(!rdyQ.empty())
        {
            freetoWork = true;
            temp = new Process(rdyQ.front());
            rdyQ.pop();
        }else if (rdyQ.empty() && first_time == false){
	    void (*sort_ps)();
	    for(unsigned int i = 0; i < ps.size(); i++)
	    {
		if(ps[i].state=="ready")
		    {
			rdyQ.push(ps[i]);
		    }
	    }
	}
	first_time = false;
        pthread_mutex_unlock(&lockit);

        if(freetoWork)
        {
            for(unsigned int i=0; i<ps.size(); i++)
            {
                if(temp->getPID() == ps[i].getPID())
                {
                    ps[i].state = "running";
                    ps[i].core = thisCore;
                    ps[i].cpu_burst = ps[i].cpu_burst-1;
                    this_thread::sleep_for(std::chrono::milliseconds((int)ps[i].getCPUTime()));

                    if((ps[i].remain_time-ps[i].getCPUTime())<= 0)
                    {
                        ps[i].state = "terminated";
                        ps[i].core = "--";
                        ps[i].total_cpu_time += ps[i].remain_time;
                        ps[i].remain_time=0;
                    }
                    else
                    {
                        ps[i].total_cpu_time += ps[i].getCPUTime();
                        ps[i].remain_time -=ps[i].getCPUTime();
                        pthread_t waitThread;
                        pthread_create(&waitThread,NULL,shortestFirstWait,(void*)&i);
                    }
                    break;
                }

            }
            for(unsigned int i=0;i<ps.size();i++){
                 if(temp->getPID() == ps[i].getPID())
                {
                    ps[i].ctxsw++;

                }
            }
            count_CTX_SW++;
            this_thread::sleep_for(std::chrono::milliseconds((int)CTX_SW));
            delete temp;

        }
    }
    return NULL;
}
void * priority(void* arg)
{   pthread_mutex_init(&lockit2, NULL);
    struct cores_struct *input = (cores_struct*)arg;
    string thisCore = to_string(input->core_num);
    bool first_time = true;
    while(true)
    {
        bool freetoWork = false;
        pthread_mutex_lock(&lockit);
        Process * temp;
        if(!rdyQ.empty())
        {
            freetoWork = true;
            temp = new Process(rdyQ.front());
            rdyQ.pop();
        }else if (rdyQ.empty() && first_time == false){
	    void (*sort_pr)();
	    for(unsigned int i = 0; i < ps.size(); i++)
	    {
		if(ps[i].state=="ready")
		    {
			rdyQ.push(ps[i]);
		    }
	    }
	}
	first_time = false;
        pthread_mutex_unlock(&lockit);
        if(freetoWork)
        {
            for(unsigned int i=0; i<ps.size(); i++)
            {
                if(temp->getPID() == ps[i].getPID())
                {
                    ps[i].state = "running";
                    ps[i].core = thisCore;
                    ps[i].cpu_burst = ps[i].cpu_burst-1;
                    this_thread::sleep_for(std::chrono::milliseconds((int)ps[i].getCPUTime()));
                    if((ps[i].remain_time-ps[i].getCPUTime())<= 0)
                    {
                        ps[i].state = "terminated";
                        ps[i].core = "--";
                        ps[i].total_cpu_time += ps[i].remain_time;
                        ps[i].remain_time=0;
                    }
                    else
                    {
                        ps[i].total_cpu_time += ps[i].getCPUTime();
                        ps[i].remain_time -=ps[i].getCPUTime();
                        pthread_t waitThread;
                        pthread_create(&waitThread,NULL,shortestFirstWait,(void*)&i);
                    }
                    break;
                }
            }
            for(unsigned int i=0;i<ps.size();i++){
                 if(temp->getPID() == ps[i].getPID())
                {
                    ps[i].ctxsw++;
                }
            }
            count_CTX_SW++;
            this_thread::sleep_for(std::chrono::milliseconds((int)CTX_SW));
            delete temp;
        }
    }
    return NULL;
}
void * shortestFirstWait(void* i)
{
    int index = *(int*)i;
    ps[index].state="i/o";
    ps[index].core ="--";
    this_thread::sleep_for(std::chrono::milliseconds((int)ps[index].io_time));
    ps[index].io_burst-=1;
    ps[index].wait_time+=ps[index].io_time;
    ps[index].state="ready";
    ps[index].core="--";
    pthread_mutex_lock(&lockit2);
    pthread_t sortPid;
    pthread_create(&sortPid,NULL,sort_ps,(void*)NULL);
    pthread_mutex_lock(&lockit2);

    return NULL;
}
void * ioWait(void* i)
{
    int index = *(int*)i;
    ps[index].state="i/o";
    ps[index].core ="--";
    this_thread::sleep_for(std::chrono::milliseconds((int)ps[index].io_time));
    ps[index].io_burst-=1;
    ps[index].wait_time+=ps[index].io_time;
    ps[index].state="ready";
    ps[index].core="--";
    rdyQ.push(ps[index]);

    return NULL;
}

void * firstComeFirstServe(void* arg)
{

    struct cores_struct *input = (cores_struct*)arg;
    string thisCore = to_string(input->core_num);
    while(true)
    {
        bool freetoWork = false;
        pthread_mutex_lock(&lockit);
        Process * temp;

        if(!rdyQ.empty())
        {
            freetoWork = true;
            temp = new Process(rdyQ.front());
            rdyQ.pop();
        }
        pthread_mutex_unlock(&lockit);

        if(freetoWork)
        {

            for(unsigned int i=0; i<ps.size(); i++)
            {
                if(temp->getPID() == ps[i].getPID())
                {
                    ps[i].state = "running";
                    ps[i].core = thisCore;
                    ps[i].cpu_burst = ps[i].cpu_burst-1;
                    this_thread::sleep_for(std::chrono::milliseconds(((int)ps[i].cpu_time)));

                    if((ps[i].remain_time-ps[i].cpu_time)<= 0)
                    {
                        ps[i].state = "terminated";
                        ps[i].core = "--";
                        ps[i].total_cpu_time += ps[i].remain_time;
                        ps[i].remain_time=0;
                    }
                    else
                    {
                        ps[i].total_cpu_time += ps[i].cpu_time;
                        ps[i].remain_time -= ps[i].cpu_time;
                        pthread_t waitThread;
                        pthread_create(&waitThread,NULL,ioWait,(void*)&i);
                    }
                    break;
                }

            }
             for(unsigned int i=0;i<ps.size();i++){
                 if(temp->getPID() == ps[i].getPID())
                {
                    ps[i].ctxsw++;

                }
            }
            count_CTX_SW++;
            this_thread::sleep_for(std::chrono::milliseconds((int)CTX_SW));
            delete temp;

        }
    }
    return NULL;
}

void * createPs(void*)
{
    int pid = 1024;
    int third;
    int twoThird;
    if(NUM_PROC>=3)
    {
        third = round(NUM_PROC*.33);
        twoThird= NUM_PROC - third;
    }
    else
    {
        third=NUM_PROC;
    }
    for(int i=0; i<third; i++)
    {
        gettimeofday(&tim, NULL);
        double startTime = tim.tv_sec+(tim.tv_usec/1000000.0);
        //       #PID #*pr #*io_burst #startTime  #cpu time       #io_time         *state  *core
        Process p (pid,rand()%5,2+ rand()%9,startTime,1000+ rand()%5001, 1000+ rand()%5001,"ready","--");
        ps.push_back(p);

        if(SCHED_ALG==2)
        {
            sortedPs.push_back(p);
            void (*sort_ps)();

        }
        else
        {
            rdyQ.push(ps[i]);
        }
        pid++;
    }
    if(SCHED_ALG==2)
    {
        for(unsigned int i=0; i<sortedPs.size(); i++)
        {
            rdyQ.push(sortedPs[i]);

        }
    }

    int randWait = 500 + (rand()%7501);

    if(NUM_PROC >= 3)
    {
        this_thread::sleep_for(std::chrono::milliseconds(randWait));

        for(int i=0; i<twoThird; i++)
        {
            gettimeofday(&tim, NULL);
            double dTime = tim.tv_sec+(tim.tv_usec/1000000.0);
            Process p (pid,rand()%5,2+ rand()%9,dTime,1000+ rand()%5001, 1000+ rand()%5001,"ready","--");
            ps.push_back(p);
            if(SCHED_ALG==2)
            {
                sortedPs.push_back(p);
                void (*sortPr)();
            }
            else
            {
                rdyQ.push(ps[i+third]);
            }
            pid++;
        }
        if(SCHED_ALG==2)
        {
            for(unsigned int i=0; i<sortedPs.size(); i++)
            {
                rdyQ.push(sortedPs[i]);

            }
        }
    }
    return NULL;
}
void * sort_ps(void*)
{
    //Source: http://www.walletfox.com/course/sortvectorofcustomobjects.php
    sort(sortedPs.begin(), sortedPs.end(), [](const Process& lhs, const Process& rhs)
    {
        return lhs.cpu_time < rhs.cpu_time;
    });

    for(unsigned int i=0;i<sortedPs.size();i++){
        rdyQ.push(sortedPs[i]);
    }
    return NULL;
}

void * sort_pr(void*)
{
    //Source: http://www.walletfox.com/course/sortvectorofcustomobjects.php
    sort(sortedPs.begin(), sortedPs.end(), [](const Process& lhs, const Process& rhs)
    {
        return lhs.priority < rhs.priority;
    });

    for(unsigned int i=0;i<sortedPs.size();i++){
        rdyQ.push(sortedPs[i]);
    }
    return NULL;
}

bool checkArgs(int argc, char*argv[])
{
    bool cores, proc, sched, cxt_sw;

    if(argc<9 || argc > 11)
    {
        cout<<"-----------------------------------------------------------------"<<endl;
        cout<<"Error improper number of arguments"<<endl;
        usage();
        return false;
    }
    for(int i=1; i<argc; i++)
    {
        if(!argv[i+1] &&(i != argc-1))
        {
            cout<<"-----------------------------------------------------------------"<<endl;
            cout<<"Error: Missing option value for: "<<argv[i]<<endl;
            usage();
            return false;
        }
        if(i%2 ==1)
        {
            if(strcmp(argv[i],"-c") == 0)
            {
                cores=true;
                int num_cores = atoi(argv[i+1]);

                if(num_cores<1 || num_cores>4)
                {
                    cout<<"-----------------------------------------------------------------"<<endl;
                    cout<<"Error: Invalid number of cores: "<<num_cores<<endl;
                    usage();
                    return false;
                }
                else
                {
                    NUM_CORES = num_cores;
                }
            }
            else if(strcmp(argv[i],"-p") == 0)
            {
                proc=true;
                int num_proc = atoi(argv[i+1]);

                if(num_proc<1 || num_proc>24)
                {
                    cout<<"-----------------------------------------------------------------"<<endl;
                    cout<<"Error: Invalid number of processes: "<<num_proc<<endl;
                    usage();
                    return false;
                }
                else
                {
                    NUM_PROC= num_proc;
                }
            }
            else if(strcmp(argv[i],"-s") == 0)
            {
                sched=true;
                int sched_alg = atoi(argv[i+1]);

                if(sched_alg<0 || sched_alg>3)
                {
                    cout<<"-----------------------------------------------------------------"<<endl;
                    cout<<"Error: Invalid schedule algorithim argument: "<<sched_alg<<endl;
                    usage();
                    return false;
                }
                else
                {
                    SCHED_ALG = sched_alg;
                }
            }
            else if(strcmp(argv[i],"-o") == 0)
            {
                cxt_sw=true;
                int ctx_sw = atoi(argv[i+1]);

                if(ctx_sw<100 || ctx_sw>1000)
                {
                    cout<<"-----------------------------------------------------------------"<<endl;
                    cout<<"Error: Invalid context switching argument: "<<ctx_sw<<endl;
                    usage();
                    return false;
                }
                else
                {
                    CTX_SW = ctx_sw;
                }
            }
            else if(strcmp(argv[i],"-t") == 0)
            {

            }
            else
            {
                cout<<"Error: Invalid input: "<< argv[i]<<endl;
                usage();
                return false;
            }
        }

    }
    if(!cores || !proc ||  !sched || !cxt_sw)
    {
        cout<<"-----------------------------------------------------------------"<<endl;
        cout<<"Error: Missing options"<<endl;
        usage();
        return false;

    }
    bool checkTforRR = false;
    for(int i=1; i<argc; i+=2)
    {
        if(!argv[i+1] && (i != argc-1))
        {
            cout<<"-----------------------------------------------------------------"<<endl;
            cout<<"Error: Missing option value for: "<<argv[i]<<endl;
            usage();
            return false;
        }
        if(strcmp(argv[i],"-t" ) == 0)
        {
            checkTforRR = true;
            if(SCHED_ALG==0)
            {
                int t_slc = atoi(argv[i+1]);

                if(t_slc<200 || t_slc>2000)
                {
                    cout<<"-----------------------------------------------------------------"<<endl;
                    cout<<"Error: Invalid time slicing argument: "<<t_slc<<endl;
                    usage();
                    return false;
                }
                else
                {
                    T_SLC = t_slc;
                }
            }
            else
            {
                cout<<"-----------------------------------------------------------------"<<endl;
                cout<<"Invalid argument: time slicing is only used for round robin."<<endl;
                usage();
                return false;
            }

        }
    }
    if(!checkTforRR && SCHED_ALG==0)
    {
        cout<<"-----------------------------------------------------------------"<<endl;
        cout<<"Missing argument: need to specify -t for round robin."<<endl;
        usage();
        return false;
    }
    return true;

}
void usage()
{

    cout<<"-----------------------------------------------------------------"<<endl;
    cout<<"usage:"<<endl;
    cout<<"./scheduler -c Number of cores(1-4) | -p Number of processes(1-24) | -s Scheduling Alrgorithim(0-3) | -o context switching overhead(100-1000)ms | -t Time slice(200-2000)ms"<<endl;
    cout<<"Available Scheduling Algorithims:"<<endl;
    cout<<"0-Round Robin: need to specify (-t) for time slices"<<endl;
    cout<<"1-First Come First Serve"<<endl;
    cout<<"2-Shortest Job First"<<endl;
    cout<<"3-Preemptive Priority"<<endl;
    cout<<"-----------------------------------------------------------------"<<endl;

    return;

}

void printStats()
{

    double total_cpu_time_all = ps[0].getTotalCPUTime();
    double total_cpu_time_eachProcess =0;
    double total_wait_time=0;

    for(int i=0; i<NUM_PROC; i++)
    {
        if(total_cpu_time_all<ps[i].getTotalCPUTime())
        {
            total_cpu_time_all = ps[i].getTotalCPUTime();
        }
    }

    for(int i=0; i<NUM_PROC; i++)
    {
        total_wait_time += ps[i].wait_time;
    }
    for(int i=0; i<NUM_PROC; i++)
    {
        total_cpu_time_eachProcess += ps[i].total_cpu_time;
    }
if(SCHED_ALG==0)
    {
        cout<<"========================================ROUND ROBIN STATS======================================="<<endl;
    }
    if(SCHED_ALG==1)
    {
        cout<<"================================FIRST COME FIRST SERVE STATS===================================="<<endl;
    }
    if(SCHED_ALG==2)
    {
        cout<<"================================FASTEST FIRST STATS============================================="<<endl;
    }
    if(SCHED_ALG==3)
    {
        cout<<"================================PREEMPTIVE PRIORITY STATS======================================="<<endl;
    }

         cout<<"* CPU UTILIZATION: "<<(double)(((total_cpu_time_all)/(total_cpu_time_all+(CTX_SW*count_CTX_SW))))*100<<"%."<<endl;
        cout<<"* AVERAGE TURN AROUND TIME: "<<(double)(((total_cpu_time_eachProcess)/NUM_PROC))<<" MS."<<endl;
        cout<<"* AVERAGE WAITING TIME: "<<(double)(total_wait_time)/(double)(NUM_PROC)<<" MS."<<endl;
        //This is throughput for all processes, will work on first%50, last%50 later.
        cout<<"* THROUGHPUT: "<<(double)(NUM_PROC)/(double)(total_cpu_time_all/60000)<< "(processes/min)"<< endl;
        cout<<"* THROUGHPUT (First Half): "<<(double)(ceil(NUM_PROC/2))/(double)(fst_half_cpu_time/60000)<< "(processes/min)"<<endl;
        cout<<"* THROUGHPUT (Second Half): "<<(double)(floor(NUM_PROC/2))/(double)(abs(scnd_half_cpu_time/60000))<< "(processes/min)"<<endl;
//throughput calculates the time it took for the first and second 50% of the processes to terminate while ignorning context switching overhead
}
/**PrintInfo is a function that nicely display that state of our processes**/
void printInfo()
{
    cout<<"| PID | Priority |   State   | Core  | Turn Time |  Wait Time  |  CPU Time    |  Remain Time   |"<<endl;
    cout<<"+-----+----------+-----------+-------+-----------+-------------+--------------+----------------+"<<endl;
    for(unsigned int i=0; i<ps.size(); i++)
    {
        if(SCHED_ALG == 0 ||  SCHED_ALG == 2 || SCHED_ALG == 2)
	{
		cout<<"|"<<ps[i].getPID()<<" |     "<<"0"<<"    |";
	}
	else
	{
		cout<<"|"<<ps[i].getPID()<<" |     "<<ps[i].getPriority()<<"    |";
	}	

        if(ps[i].getState()=="terminated")//checks if state is terminated then do the proper indentation
        {
            cout<<""<<ps[i].getState()<<" ";
        }
        else if(ps[i].getState()=="ready")//checks if state is ready then do the proper indentation
        {
            cout<<"   "<<ps[i].getState()<<"   ";
        }
        else if(ps[i].getState()=="i/o")//checks if state is i/o then do the proper indentation
        {
            cout<<"    "<<ps[i].getState()<<"    ";
        }
        else if(ps[i].getState()=="running")//checks if state is running then do the proper indentation
        {
            cout<<"  "<<ps[i].getState()<<"  ";
        }
        cout<<"|   ";
        if(ps[i].getCore()=="--")//checks if the process is not on a core then do the proper indentation
        {
            cout<<ps[i].getCore()<<"  |   ";
        }
        else
        {
            cout<<ps[i].getCore()<<"   |   ";
        }
        cout<< fixed<<setprecision(2);
        switch(lenHelper(ps[i].getStartTime()))
        {
        case 1:
            cout<<ps[i].getStartTime() <<"    | ";
            break;
        case 2:
            cout<<ps[i].getStartTime() <<"   | ";
            break;
        case 3:
            cout<<ps[i].getStartTime() <<"  | ";
            break;
        case 4:
            cout<<ps[i].getStartTime() <<" | ";
            break;

        }
        switch(lenHelper(ps[i].getWaitTime()))
        {
        case 1:
            cout<< ps[i].getWaitTime() <<"        |  ";
            break;
        case 2:
            cout<< ps[i].getWaitTime() <<"       |  ";
            break;
        case 3:
            cout<< ps[i].getWaitTime() <<"      |  ";
            break;
        case 4:
            cout<< ps[i].getWaitTime() <<"     |  ";
            break;
        case 5:
            cout<< ps[i].getWaitTime() <<"    |  ";
            break;
        case 6:
            cout<< ps[i].getWaitTime() <<"   |  ";
            break;
        case 7:
            cout<< ps[i].getWaitTime() <<"  |  ";
            break;
        }
        switch(lenHelper(ps[i].getTotalCPUTime()))
        {
        case 1:
            cout<< ps[i].getTotalCPUTime() << "        |    ";
            break;
        case 2:
            cout<< ps[i].getTotalCPUTime() << "       |    ";
            break;
        case 3:
            cout<< ps[i].getTotalCPUTime() << "      |    ";
            break;
        case 4:
            cout<< ps[i].getTotalCPUTime() << "     |    ";
            break;
        case 5:
            cout<< ps[i].getTotalCPUTime() << "    |    ";
            break;
        case 6:
            cout<< ps[i].getTotalCPUTime() << "   |    ";
            break;
        case 7:
            cout<< ps[i].getTotalCPUTime() << "  |    ";
            break;
        }


        switch(lenHelper(ps[i].getRemainTime()))
        {
        case 1:
            cout<< ps[i].getRemainTime() << "        |"<<endl;
            break;
        case 2:
            cout<< ps[i].getRemainTime() << "       |"<<endl;
            break;
        case 3:
            cout<< ps[i].getRemainTime() << "      |"<<endl;
            break;
        case 4:
            cout<< ps[i].getRemainTime() << "     |"<<endl;
            break;
        case 5:
            cout<< ps[i].getRemainTime() << "    |"<<endl;
            break;
        default:
            cout<< ps[i].getRemainTime() << "        |"<<endl;

        }
    }
    sleep(1);
    if(!checkAllTerminated())
    {
        for (unsigned int i=0; i<ps.size()+2; i++)
        {
            fputs("\033[A\033[2K", stdout);
        }
        rewind(stdout);
    }
}

/**
A function that checks wither all processes are terminated or not
and return true/false based on the check
**/
bool checkAllTerminated()
{
    int psCount = 0;
    //A loop that count the number of processes that are terminated
    for(unsigned int i=0; i<ps.size(); i++)
    {
        if(ps[i].getState()=="terminated")
        {
            psCount++;
        }

        if(ps[i].getState()=="terminated" && psCount==(NUM_PROC/2))
    {
        fst_half_cpu_time +=ps[i].getTotalCPUTime();
        fst_half_ctxsw += ps[i].ctxsw;

        }

    }

    if(psCount ==NUM_PROC)//i.e. all processes are terminated
    {
        double total_all_cpu_time =0;

        for(unsigned int i=0; i<ps.size(); i++)
        {
            total_all_cpu_time+=ps[i].getTotalCPUTime();
        }
        scnd_half_cpu_time=total_all_cpu_time-fst_half_cpu_time;
        scnd_half_ctxsw=CTX_SW-fst_half_ctxsw;
        return true;
    }
    else
    {
        return false;
    }

}

/*
Source: https://stackoverflow.com/questions/3068397/finding-the-length-of-an-integer-in-c
Used mainly for printInfo(): aligns output of time, triple digits(123.000) VS double digits(12.000) VS. single digits(1.000)
*/
int lenHelper(unsigned x)
{
    //The following returns the number of digits for a number x
    if(x>=10000000) return 8;
    if(x>=1000000) return 7;
    if(x>=100000) return 6;
    if(x>=10000) return 5;
    if(x>=1000) return 4;
    if(x>=100) return 3;
    if(x>=10) return 2;
    return 1;
}
