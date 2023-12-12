#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //PAGEBREAK: 13
  default:
//   in order to allow read/write access we should just treat page faults triggered by kernel same as by user mode
    // if(myproc() == 0 || (tf->cs&3) == 0){
    //   // In kernel, it must be our mistake.
    //   cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
    //           tf->trapno, cpuid(), tf->eip, rcr2());
    //   panic("trap");
    // }

    // currently when a page fault occurs we hit the default case in the trap handler
    // trapno is held in tf->trapno (the trap number 14 (T_PGFLT) represents a page fault)
    // rcr2() accesses a special register which contains the faulting address
    
    // In user space, assume process misbehaved.
    if(tf->trapno == T_PGFLT){
        // if it is a page fault see if the address is valid. (if so then allocate new page/memory for heap on demand) (else kill)
        void *address = (void *) rcr2();
        char *mem;
        
        // for each page fault we just want to allocate a singular page. we want to round all addresses down to ensure mappages will map to a singular page 
        address = (void*)PGROUNDDOWN((int)address);

        pte_t *pte = walkpgdir(myproc()->pgdir, (void *)address, 1);

        // when we walkpgdir and its a guard page we get (PTE_U & *pte) evaluates to 0
        // when we walkpgdir and its a an allocated page NOT GUARD we get (PTE_U & *pte) evaluates to 1
        // when we walkpgdir and not allocated page it also evaluates to 0
        // (PTE_U & *pte) - may be needed later checks if the page found with walkpgdir is user accesible (guard pages are not)

        if(((int)address < myproc()->sz) && (*pte == 0)){
            // set up memory/pages like growproc, allocuvm
            mem = kalloc();
            if(mem == 0){
                cprintf("allocuvm out of memory\n");
            }
            memset(mem, 0, PGSIZE);
            if(mappages(myproc()->pgdir, (char*)address, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
                cprintf("allocuvm out of memory (2)\n");
                kfree(mem);
            }
            // after changing any valid page table entry we will need to flush TLB (done originally in growproc)
            switchuvm(myproc());
        }
        else{
            // real page fault kill process and print error msg
            cprintf("pid %d %s: trap %d err %d on cpu %d "
                "eip 0x%x addr 0x%x--kill proc\n",
                myproc()->pid, myproc()->name, tf->trapno,
                tf->err, cpuid(), tf->eip, rcr2());
            myproc()->killed = 1;
        }
    }
    // if not a page fault kill it immediately
    else{
        cprintf("pid %d %s: trap %d err %d on cpu %d "
                "eip 0x%x addr 0x%x--kill proc\n",
                myproc()->pid, myproc()->name, tf->trapno,
                tf->err, cpuid(), tf->eip, rcr2());
        myproc()->killed = 1;
    }
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
