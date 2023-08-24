extern void idleTask();
extern listobj *deadlineReached(int whichList);
extern struct l_obj * extractHead(struct _list **first);
extern int Ticks; /* global sysTick counter */
extern int KernelMode; /* can be equal to either INIT or RUNNING (constants defined in “kernel_functions.h”)*/
extern TCB *PreviousTask, *NextTask; /* Pointers to previous and next running tasks */
extern list *ReadyList, *WaitingList, *TimerList;
