#include "FU.h"

extern config *CPU_cfg;
extern clk_tick sys_clk;
extern FU_CDB fCDB;
extern vector<int*> clk_wait_list;
extern ROB *CPU_ROB;


FU_CDB::FU_CDB()
{
    next_vdd = 1;
    front = rear = 0;
    bus.source = -1;
    Q_lock = PTHREAD_MUTEX_INITIALIZER;
}

bool FU_CDB::enQ(valType t, void *v, int s)
{
    pthread_mutex_lock(&Q_lock);
    if ((rear+1)%Q_LEN == front)
    {
        err_log("The reserv_CDB queue is full");
        pthread_mutex_unlock(&Q_lock);
        return false;
    }
    int index = rear;
    rear = (++rear)%Q_LEN;
    queue[index].type = t;
    if (t == INTGR)
        queue[index].value.i = *(int*)v;
    else
        queue[index].value.f = *(float*)v;
    queue[index].source = s;
    pthread_mutex_unlock(&Q_lock);
    return true;
}

int FU_CDB::get_source()
{
    return bus.source;
}

bool FU_CDB::get_val(void *v)
{
    if (bus.type == INTGR)
        *(int*)v = bus.value.i;
    else
        *(float*)v = bus.value.f;
    return true;
}

void FU_CDB::CDB_automat()
{
    mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    while(true)
    {
        at_rising_edge(&lock, next_vdd);
        at_falling_edge(&lock, next_vdd);
        if (front != rear)
        {
            bus.source = queue[front].source;
            bus.type = queue[front].type;
            bus.value = queue[front].value;
            front = (++front)%Q_LEN;
        }
    }
}

void init_FU_CDB()
{
    fCDB.next_vdd = 1;
    clk_wait_list.push_back(&fCDB.next_vdd);
    pthread_create(&fCDB.handle, NULL, [](void *arg)->void*{fCDB.CDB_automat(); return NULL;}, NULL);
}

FU_Q::FU_Q()
{
    front = rear = 0;
    Q_lock = PTHREAD_MUTEX_INITIALIZER;
}

const FU_QEntry* FU_Q::enQ(int d, void *r, void *op1, void *op2)
{
    pthread_mutex_lock(&Q_lock);
    if ((rear+1)%Q_LEN == front)
    {
        pthread_mutex_unlock(&Q_lock);
        throw -1;
    }
    int index = rear;
    rear = (++rear)%Q_LEN;
    queue[index].dest = d;
    queue[index].res = r;
    queue[index].oprnd1 = op1;
    queue[index].oprnd2 = op2;
    queue[index].done = false;
    pthread_mutex_unlock(&Q_lock);
    return &queue[index];
}

FU_QEntry *FU_Q::deQ()
{
    if (front == rear)
        return nullptr;
    FU_QEntry* ret = &queue[front];
    front = (++front)%Q_LEN;
    return ret;
}

functionUnit::functionUnit()
{
    next_vdd = 1;
    front = rear = 0;
    task.done = true;
}

void intAdder::FU_automate()
{
    mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    while (true)
    {
        at_rising_edge(&lock, next_vdd);
        if (task.done)
        {
            auto newTask = queue.deQ();
            if (newTask != nullptr)
            {
                task.dest = newTask->dest;
                task.oprnd1 = newTask->oprnd1;
                task.oprnd2 = newTask->oprnd2;
                task.res = newTask->res;
                task.done = false;
            }
        }
        if (!task.done)
        {
            ROBEntry *R = CPU_ROB->get_entry(task.dest);
            R->output.exe = sys_clk.get_prog_cyc();
            for (int i = 0; ;i++)
            {
                msg_log("Calculating int "+to_string(*(int*)task.oprnd1)+" + "+to_string(*(int*)task.oprnd2), 3);
                if (i == CPU_cfg->int_add->exe_time - 1)
                {
                    *(int*)task.res = *(int*)task.oprnd1 + *(int*)task.oprnd2;
                    fCDB.enQ(INTGR, task.res, task.dest);
                    task.done = true;
                    break;
                }
                at_falling_edge(&lock, next_vdd);
                at_rising_edge(&lock, next_vdd);
            }
        }
        at_falling_edge(&lock, next_vdd);
    }
}

void flpAdder::FU_automate()
{
    mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    while (true)
    {
        at_rising_edge(&lock, next_vdd);
        if (task.done)
        {
            auto newTask = queue.deQ();
            if (newTask != nullptr)
            {
                task.dest = newTask->dest;
                task.oprnd1 = newTask->oprnd1;
                task.oprnd2 = newTask->oprnd2;
                task.res = newTask->res;
                task.done = false;
            }
        }
        if (!task.done)
        {
            ROBEntry *R = CPU_ROB->get_entry(task.dest);
            R->output.exe = sys_clk.get_prog_cyc();
            for (int i = 0; ;i++)
            {
                msg_log("Calculating flp "+to_string(*(float*)task.oprnd1)+" + "+to_string(*(float*)task.oprnd2), 3);
                if (i == CPU_cfg->fp_add->exe_time - 1)
                {
                    *(float*)task.res = *(float*)task.oprnd1 + *(float*)task.oprnd2;
                    fCDB.enQ(FLTP, task.res, task.dest);
                    task.done = true;
                    break;
                }
                at_falling_edge(&lock, next_vdd);
                at_rising_edge(&lock, next_vdd);
            }
        }
        at_falling_edge(&lock, next_vdd);
    }
}

void flpMtplr::FU_automate()
{
    mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    while (true)
    {
        at_rising_edge(&lock, next_vdd);
        if (task.done)
        {
            auto newTask = queue.deQ();
            if (newTask != nullptr)
            {
                task.dest = newTask->dest;
                task.oprnd1 = newTask->oprnd1;
                task.oprnd2 = newTask->oprnd2;
                task.res = newTask->res;
                task.done = false;
            }
        }
        if (!task.done)
        {
            ROBEntry *R = CPU_ROB->get_entry(task.dest);
            R->output.exe = sys_clk.get_prog_cyc();
            for (int i = 0; ;i++)
            {
                msg_log("Calculating flp"+to_string(*(float*)task.oprnd1)+" X "+to_string(*(float*)task.oprnd2), 3);
                if (i == CPU_cfg->fp_mul->exe_time - 1)
                {
                    *(float*)task.res = *(float*)task.oprnd1 * *(float*)task.oprnd2;
                    fCDB.enQ(FLTP, task.res, task.dest);
                    task.done = true;
                    break;
                }
                at_falling_edge(&lock, next_vdd);
                at_rising_edge(&lock, next_vdd);
            }
        }
        at_falling_edge(&lock, next_vdd);
    }
}