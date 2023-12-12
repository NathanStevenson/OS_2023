#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

// sbrk should still call growproc inside proc.c but growproc should only modify process size and not create new page table entries
int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_yield(void)
{
  yield();
  return 0;
}

int sys_shutdown(void)
{
  shutdown();
  return 0;
}

int sys_getpagetableentry(void){
    int pid, address;
    // get the pid and address if either of them fail them return -1
    if(argint(0, &pid) < 0)
        return -1;
    if(argint(1, &address) < 0)
        return -1;
    
    uint addr = address;

    // get the process from the pid, then call walkpgdir with p->pgdir and virtual address
    return getpagetableentry(pid, addr);
    // getpagetableentry located inside proc.c (similar control flow to kill syscall)
}

int sys_isphysicalpagefree(void){
    // 1. We get the physical frame number (top 20 bits of physical byte address) (shift these left by 12 to get physical byte address w/ offset 0) 
    // 2. Use P2V() to convert physical byte address to virtual byte address in kernel space
    // 3. Now that we know the kernel virtual address iterate thru linked list with header kmem.freelist
    // 4. If we find the virtual address in LL return true else return false
    int ppn;
    if(argint(0, &ppn) < 0)
        return -1;

    // physical page number is the top 20 bits of the physical byte address therefore shift it left by 12 for byte address
    uint phys_addr = ppn << 12;
    void* virtual_addr = P2V(phys_addr);
    
    // want to have some sort of function wrapper like we did for lab #4 where dummy header in shared .h and below logic is inside kalloc.c
    return in_free_table(virtual_addr);
}

int sys_dumppagetable(void) {
    int pid;
    if (argint(0, &pid) < 0)
        return -1;
    return dumppagetable(pid);
}
