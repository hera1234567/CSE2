/*Task Administation*/
#include <stdio.h>
#include <stdlib.h>
#include "system_sam3x.h"
#include "at91sam3x8.h"
#include "kernel_functions.h"
#include "global_Variables.h"

struct l_obj * extractHead(struct _list **first);
int Ticks; /* global sysTick counter */
int KernelMode; /* can be equal to either INIT or RUNNING (constants defined in “kernel_functions.h”)*/
TCB *PreviousTask, *NextTask; /* Pointers to previous and next running tasks */
list *ReadyList, *WaitingList, *TimerList;


/*Task administrator*/

void idleTask()
{while(1);}

// Create a node from a task, 
struct l_obj *create(TCB *x)
{struct l_obj *newEl;
  newEl=(struct l_obj*)malloc(sizeof(struct l_obj));
  if (newEl==0)
  {return 0;}
  newEl->pTask=x; 
  newEl->pMessage=NULL;
  newEl->nTCnt=NULL;
  newEl->pNext=NULL;
  newEl->pPrevious=NULL;
  return newEl;
}

// Insert a node into readylist, the node will be placed in the correctly sorted place
exception insertReady(struct l_obj *el){
    struct l_obj *temp;
    struct l_obj *temp2;
    temp=ReadyList->pHead;
    
    if(el==NULL){return FAIL;}

    if(!ReadyList->pHead && !ReadyList->pTail)//Om listan är tom
    {//används bara till idleTask
    ReadyList->pHead=el;
    ReadyList->pTail=el;
    }
    else if(ReadyList->pHead == ReadyList->pTail) //Om det endast finns idleTask
    {
     ReadyList->pHead=el;
     ReadyList->pHead->pNext=ReadyList->pTail;
     ReadyList->pTail->pPrevious=el;
    } 
    else if (el->pTask->Deadline<=temp->pTask->Deadline)//Om el ska läggas in först
    {
      el->pNext=temp;
      temp->pPrevious=el;
      ReadyList->pHead=el;
    }
    else if(ReadyList->pHead->pNext==ReadyList->pTail&&ReadyList->pHead->pTask->PC!=idleTask)
    {
    if (ReadyList->pHead->pTask->Deadline<=el->pTask->Deadline)
    { 
      ReadyList->pHead->pNext=el;
      ReadyList->pTail->pPrevious=el;
      el->pNext=ReadyList->pTail;
      el->pPrevious=ReadyList->pHead; 
    } else { //om head är större än den nya
      el->pNext=temp;
      temp->pPrevious=el;
      ReadyList->pHead=el;    
    }
    }
    else //Om det finns objekt i listan redan
    {
        while(temp->pTask->PC!=idleTask)
        {
            if(temp->pTask->Deadline>=el->pTask->Deadline)
            break;

            else if(temp->pTask->Deadline<el->pTask->Deadline)
            temp=temp->pNext;
        }
        
        temp2=temp->pPrevious;
        el->pNext=temp;
        el->pPrevious=temp2;
        temp->pPrevious=el;
        temp2->pNext=el;
        
        if(temp->pTask->PC!=idleTask) {
          while(temp->pNext->pTask->PC!=idleTask){temp=temp->pNext;}
          temp->pNext=ReadyList->pTail;
          ReadyList->pTail->pPrevious=temp;
        }
    } 
}

// Insert a task into the back of a list
exception insertTask(struct _list *list, listobj *task) {
  if(task==NULL){return FAIL;} //check for nullpointer
  
  struct l_obj *elHead = create(task->pTask);
  struct l_obj *elTail = create(task->pTask);
  elHead->nTCnt=task->nTCnt;
  elHead->pMessage=task->pMessage;
  
  elTail->nTCnt=task->nTCnt;
  elTail->pMessage=task->pMessage;
  
  if(elHead==NULL||elTail==NULL){return FAIL;} //check if creat success
  
  if(list->pHead==NULL && list->pTail==NULL){
    list->pHead=elHead;
    list->pTail=elTail;
    list->pHead->pNext=list->pTail;
    list->pTail->pPrevious=list->pHead;
  } else if(list->pHead->pTask==list->pTail->pTask){
    free(elHead);
    list->pTail=elTail;
    list->pHead->pNext=list->pTail;
    list->pTail->pPrevious=list->pHead;
  } else {
    free(elHead);
    elTail->pPrevious=list->pTail;
    list->pTail->pNext=elTail;
    list->pTail=elTail;
  }
  list->pHead->pPrevious=NULL;
  list->pTail->pNext=NULL;
  return SUCCESS;
}


exception init_kernel(void)
{
  int ret=OK;
    Ticks=0;

    ReadyList =(list*)malloc(sizeof(list));
    if(ReadyList==0)
    {ret=FAIL;
    isr_off();
    printf("Failed to allocate memory for ReadyList");
    isr_on();}
    ReadyList->pHead=ReadyList->pTail=NULL;
    
    WaitingList =(list*)malloc(sizeof(list));
      if(WaitingList==0)
    {ret=FAIL;
    isr_off();
    printf("Failed to allocate memory for WaitingList");
    isr_on();}
    WaitingList->pHead=WaitingList->pTail=NULL;

    TimerList =(list*)malloc(sizeof(list));
      if(TimerList==0)
    {ret=FAIL;
    isr_off();
    printf("Failed to allocate memory for TimerList");
    isr_on();}
    TimerList->pHead=TimerList->pTail=NULL;
    
    create_task(&idleTask, UINT_MAX);

    KernelMode=INIT;

    return ret;
 }

// Takes in a void task and deadline and creates a TCB task
exception create_task(void(*taskBody)(), uint deadline)
{
  int ret=OK;
/*Create a task and insert it in the ready list*/

 TCB *new_tcb;
 new_tcb = (TCB *) calloc (1, sizeof(TCB));
 /* you must check if calloc was successful or not! */
 if (new_tcb==0)
 {ret=FAIL;
 isr_off();
 printf("Failed to allocate memory for new task");
 isr_on();
 }

 new_tcb->PC = taskBody;
 new_tcb->SPSR = 0x21000000;
 new_tcb->Deadline = deadline;

 new_tcb->StackSeg [STACK_SIZE - 2] = 0x21000000;
 new_tcb->StackSeg [STACK_SIZE - 3] = (unsigned int) taskBody;
 new_tcb->SP = &(new_tcb->StackSeg [STACK_SIZE - 9]);

if (KernelMode==INIT)
{
   //Lägga till objektet i ReadeListan 
   insertReady(create(new_tcb));

   //Returnera status
   return ret;
}

else
{
    isr_off();

    //Update previous task
    PreviousTask=NextTask;

    //Lägg till ny task i listan
    insertReady(create(new_tcb));

    //Uppdatera NextTask
    NextTask=ReadyList->pHead->pTask;

    //Switch
    SwitchContext();   
}

//Returnera status
return ret;
}

// Takes in a list and returns and removes the head of the list
struct l_obj * extractHead(struct _list **first){
   struct _list *temp;
   temp=*first;
   struct l_obj *temp2;
   
   if(*first!=ReadyList && *first!=WaitingList && *first!=TimerList){return NULL;}

   if(temp->pHead != temp->pTail) { 
     temp2 = temp->pHead; //ta ut huvudet 
     temp->pHead = temp->pHead->pNext; //sätt att huvudet är huvud->next
     temp->pHead->pPrevious = NULL; //sött att huvud->prev är null
   } 
   
  return temp2;
}

void terminate(void) {
 isr_off();
 struct l_obj *leavingObj;
 leavingObj = extractHead(&ReadyList);
 NextTask = ReadyList->pHead->pTask;
 switch_to_stack_of_next_task();


 free(leavingObj->pTask);
 free(leavingObj);
 LoadContext_In_Terminate();
}

void run (void)
{
 Ticks=0;
 KernelMode=RUNNING;
 
 NextTask = ReadyList->pHead->pTask;

 LoadContext_In_Run();
}

/*Inter-process communication*/

//insert message into mailbox
exception insertMsg(mailbox *mBox, msg *message){
  if(message==NULL){return FAIL;}
  if(mBox->nMaxMessages==mBox->nMessages){return FAIL;}
  
  if(no_messages(mBox)){
    mBox->pHead=mBox->pTail=message;
  } else {
    mBox->pTail->pNext=message;
    message->pPrevious=mBox->pTail;
    mBox->pTail=message;
  }
  mBox->pHead->pPrevious=NULL;
  mBox->pTail->pNext=NULL;
  mBox->nMessages++;
  return SUCCESS;
}

//remove mailbox head
msg *removeMsg(mailbox *mBox, msg *m){
  msg *message=mBox->pHead;
  msg *mes=message;
  if(no_messages(mBox)) {return NULL;} 
  if(mBox==NULL) {return NULL;} 
  
  if(m==NULL) { //if there is no message, extract the head
    if(mBox->pHead==mBox->pTail){
      mBox->pHead=NULL;
      mBox->pTail=NULL;
      mBox->pTail->pNext=NULL;
      free(mes->pData);
      free(mes);
      mBox->nMessages--;
      return message;
    } else {
      mBox->pHead=mBox->pHead->pNext;
      mBox->pHead->pPrevious=NULL;
      free(mes->pData);
      free(mes);
      mBox->nMessages--;
      return message; 
    }
  } else { //if there is a message, remove that
    if(mBox->pHead==m) { //if the message is the head
      removeMsg(mBox, NULL);  
    } else {
      while(message->pNext!=m && message->pNext!=NULL){message=message->pNext;}
      
      if(message->pNext!=m){ //if the mBox doesn't contain the message
        isr_off();
        printf("Failed to remove message in mailbox");
        isr_on();
        return NULL; }
      
      mes=message->pNext->pNext;
      message->pNext=message->pNext->pNext;
      mes->pPrevious=message;
      free(m->pData);
      free(m);
      mBox->nMessages--;
      return NULL; //returns nothing because it is irrelevant 
    }
    
  }
  
}

//listobj found in list
exception contains(struct _list *first, TCB *task){
  struct l_obj *temp;
  temp=first->pHead; 
  
  if(task==NULL || first==NULL){return FAIL;}
  
  if((first->pHead==NULL&&first->pTail==NULL)||(first->pHead->pTask->PC==idleTask&&first->pTail->pTask->PC==idleTask))
  {return FAIL;}
  
  while(temp->pNext!=NULL){
    if(temp->pTask==task) {
      return SUCCESS;
    }
    temp=temp->pNext;
  }
  if(temp->pTask==task) {
      return SUCCESS;}
  return FAIL;
}

//remove listobj from list
exception removeEl(struct _list *first, struct l_obj *el){
  struct l_obj *temp;
  struct l_obj *temp2;
  temp=first->pHead; 
  
  if(el==NULL || first==NULL){return FAIL;} //fail if nullpointers
  if(contains(first,el->pTask)==0){return FAIL;} //fail if it isn't an element
  if((first->pHead==NULL&&first->pTail==NULL)||(first->pHead->pTask->PC==idleTask&&first->pTail->pTask->PC==idleTask))
  {return FAIL;}
  
  if(first->pHead->pTask==first->pTail->pTask){
    first->pHead=first->pTail=NULL;
  } else {
    if(first->pHead->pTask==el->pTask) {temp=extractHead(&first);}
    
    if(first->pTail->pTask==el->pTask) {
      first->pTail=first->pTail->pPrevious;
      first->pTail->pNext=NULL;
    } else if(contains(first,el->pTask)==1){
          //find the el in the list
      while(temp->pNext!=NULL ||temp->pNext->pTask->PC!=idleTask){
        if(temp->pNext->pTask==el->pTask) {
          break;
        }
        temp=temp->pNext;
      } 
      if(temp->pTask!=el->pTask){return FAIL;} //fail if temp is not the el
      temp2=temp->pNext->pNext;
      temp->pNext=temp->pNext->pNext;
      temp2->pPrevious=temp;
    }
  }
  return SUCCESS;
}

//move listobj from one list to another
exception moveListobj(struct _list *from, struct _list *to, struct l_obj *el) {
  if(el==NULL || from==NULL || to==NULL){return FAIL;} //Fail if nullpointer
  if(contains(from,el->pTask)==0){return FAIL;} //fail both lists do not contain el
  
  removeEl(from,el); //remove el from waitinglist
  if (to==ReadyList){
  insertReady(el); //insert el into readylist
  }
  else {
  insertTask(to, el);
  }
  return SUCCESS;
}

//takes in mailbox, message, task and status and create message and give datapointer
msg * setData(msg *m, mailbox *mBox, void *pData, int status){
  if (mBox==NULL) {return NULL;}
  
  if(status==SENDER){ //if from sender
    m=(msg*)malloc(sizeof(msg)+mBox->nDataSize);
    if(m==NULL){
      isr_off();
      printf("Failed to allocate memory for message");
      isr_on();
      return NULL;
    }
    m->pData=(void *)(m+sizeof(msg));//create empty void of size 
    memcpy(m->pData, pData, mBox->nDataSize);//copy pData to pData
  } else { //if from receiver
    m=(msg*)malloc(sizeof(msg));
    if(m==NULL){
      isr_off();
      printf("Failed to allocate memory for message");
      isr_on();
      return NULL;
    }
    m->pData=pData; //ge msg det givna meddelandet
  }
  m->Status=status;//ge meddelandet statusen receiver/sender
  
  return m;
}

mailbox* create_mailbox(uint nMessages, uint nDataSize ){

  mailbox *mail;
  mail =(mailbox*)malloc(sizeof(mailbox));
  if (mail == 0) {
    isr_off();
    printf("Can't allocate memory for mailbox");
    isr_on();
    return NULL;
  }

  mail->pHead=mail->pTail=NULL;
  mail->nBlockedMsg=mail->nMessages=0;
  mail->nDataSize=nDataSize;
  mail->nMaxMessages=nMessages;
  
  return mail;
}

exception remove_mailbox (mailbox* mBox) {
  if(no_messages(mBox)==TRUE) {
    free(mBox);
    return OK;
  } else {
    return NOT_EMPTY;
  } 
}

int no_messages(mailbox* mBox){ //is empty function
  int ret = 0;
  if(mBox->nMessages == 0) {
    ret = 1; 
  }
  return ret;
}

exception send_wait( mailbox* mBox, void* pData ) {
  isr_off(); 
  if(mBox->pHead->Status==RECEIVER) {
    memcpy(mBox->pHead->pData,pData,mBox->nDataSize);
    msg *message=removeMsg(mBox, NULL);
    PreviousTask = ReadyList->pHead->pTask;
    moveListobj(WaitingList, ReadyList, mBox->pHead->pBlock);
    NextTask = ReadyList->pHead->pTask;
  } else {
    msg *message = NULL;
    message=setData(message,mBox,pData,SENDER); //allocate memory, gives message pData
    message->pBlock=ReadyList->pHead;
    insertMsg(mBox, message);
    PreviousTask = ReadyList->pHead->pTask;
    moveListobj(ReadyList,WaitingList, ReadyList->pHead);
    NextTask = ReadyList->pHead->pTask;
  }
  SwitchContext();

  if(deadline()<=Ticks){
    isr_off();
    removeMsg(mBox,ReadyList->pHead->pMessage); 
    isr_on();
    return DEADLINE_REACHED;
  } else {
    isr_on();
    return OK;
  
  }
}

exception receive_wait( mailbox* mBox, void* pData ){
  isr_off();

  if(mBox->pHead->Status==SENDER) {
     memcpy(pData, mBox->pHead->pData,mBox->nDataSize);
     msg *message=removeMsg(mBox,NULL);
    if(message->pBlock!=NULL) {
      PreviousTask = ReadyList->pHead->pTask;
      moveListobj(WaitingList,ReadyList, message->pBlock);
      NextTask = ReadyList->pHead->pTask;
      SwitchContext();
    } else {
      free(message->pData);
      free(message);
    }
  } else {
    msg *message=NULL; 
    message=setData(message,mBox,pData,RECEIVER); //allocate memory, gives message pData
    message->pBlock=ReadyList->pHead;
    insertMsg(mBox, message);
    PreviousTask = ReadyList->pHead->pTask;
    moveListobj(ReadyList,WaitingList,ReadyList->pHead);
    NextTask = ReadyList->pHead->pTask;
    SwitchContext();
  }

  if(deadline()<=Ticks) {
    isr_off();
    removeMsg(mBox, ReadyList->pHead->pMessage); 
    isr_on();
    return DEADLINE_REACHED;
  } else {
    isr_on();
    return OK;
  }
}

exception send_no_wait( mailbox* mBox, void* pData ){
  isr_off();

  if(mBox->pHead->Status==RECEIVER) {
    memcpy(mBox->pHead->pData, pData, mBox->nDataSize);
    msg* message = removeMsg(mBox, NULL);
    PreviousTask = ReadyList->pHead->pTask;
    moveListobj(WaitingList, ReadyList, message->pBlock);
    NextTask = ReadyList->pHead->pTask;
    SwitchContext();
  } else { 
    msg *message=NULL;
    message=setData(message,mBox,pData,SENDER); //allocate memory, gives message pData
    message->pBlock=0;//no blocked messages
    if(mBox->nMaxMessages <= mBox->nMessages) {
      removeMsg(mBox, NULL);
    }
    insertMsg(mBox, message);
  }
  isr_on();
  return OK;


}

int receive_no_wait(mailbox* mBox, void* pData){
  isr_off();
  if(mBox->pHead->Status==SENDER) {
     memcpy(pData, mBox->pHead->pData, mBox->nDataSize);
     msg* message=removeMsg(mBox,NULL);
    if(message->pBlock!=NULL) {
      PreviousTask = ReadyList->pHead->pTask;
      moveListobj(WaitingList, ReadyList, message->pBlock);
      NextTask = ReadyList->pHead->pTask;
      SwitchContext();
    } else {
      free(message->pData);
      free(message);
    }
  } else  {
    isr_on();
    return FAIL;
  }
  isr_on();
  return OK;

}
 

//Timing

//check reached deadlines for a specific list
listobj *deadlineReached(int whichList){
  if(whichList!=0&&whichList!=1){return NULL;}
  
  if (whichList==0){ //for waitinglist
    listobj *node = WaitingList->pHead;
    //if deadline is reached
    while(node!=NULL){
      if(node->pTask->Deadline<=Ticks){
        return node;
      }
      node=node->pNext;
    }
  } else { //for timerlist
    listobj *node = TimerList->pHead;
    
    while(node!=NULL){
      //if deadline is reached or sleeptime is over
      if(node->pTask->Deadline<=Ticks || node->nTCnt<=Ticks){
        return node;
      }
      node=node->pNext;
    }
    
  }
  
  return NULL;
  
}

exception wait(uint nTicks){
  uint status = 0;
  
  //Disable interrupt
  isr_off();
  
  //Update PreviousTask
  PreviousTask=ReadyList->pHead->pTask;
  
  //Place running task in the TimerList
  ReadyList->pHead->nTCnt=nTicks + Ticks; //gives the task its sleeptime
  moveListobj(ReadyList,TimerList,ReadyList->pHead);
  
  //Update NextTask
  NextTask=ReadyList->pHead->pTask;
  
  //Switch context
  SwitchContext();
  
  //IF deadline is reached THEN
  if(deadline()<=Ticks){
  //Status is DEADLINE_REACHED
    status=DEADLINE_REACHED;
  } else {
  //Status is OK
    status=OK;
  }
  isr_on();
  //Return status
  return status;
}


void set_ticks(uint nTicks){
  //Set the tick counter.
  Ticks=nTicks;

}

uint ticks( void ){
  //Return the tick counter
  return Ticks;

}

uint deadline( void ){
  //Return the deadline of the current task
  return NextTask->Deadline;
}

void set_deadline( uint deadline ){
  //Disable interrupt
  isr_off();
  
  //Set the deadline field in the calling TCB.
  ReadyList->pHead->pTask->Deadline=deadline;
  
  //Update PreviousTask
  PreviousTask=ReadyList->pHead->pTask;
  
  //Reschedule ReadyList
  extractHead(&ReadyList);//takes the nexttask out
  insertReady(create(NextTask));//inserts it again but where the new deadline is correctly scheduled
  
  //Update NextTask
  NextTask=ReadyList->pHead->pTask;
  
  //Switch context
  SwitchContext();
}


void TimerInt(void){
  listobj *node;
  //Increment tick counter
  Ticks++;
  
  /* Check the TimerList for tasks that are ready for
  execution (a task that was sleeping is ready for
  execution if either its sleep period is over, OR
  if its deadline has expired), move these to
  ReadyList. */
  while(1){
    node=deadlineReached(1);//checking for deadlines reached
    if(node==NULL){
      break;
    } else {
      PreviousTask=ReadyList->pHead->pTask;
      moveListobj(TimerList,ReadyList,node);
      NextTask=ReadyList->pHead->pTask;
    }
  }
  
  /*Check the WaitingList for tasks that have
  expired deadlines, move these to ReadyList
  and clean up their Mailbox entry.*/
  while(1){
    node=deadlineReached(0);
    if(node==NULL){
      break;
    } else {
      PreviousTask=ReadyList->pHead->pTask;
      moveListobj(WaitingList,ReadyList,node); 
      NextTask=ReadyList->pHead->pTask;
    }
  } 

}
