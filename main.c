#include <stdio.h>
#include <stdlib.h>
#include "diag/trace.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"
#include <time.h>

#define CCM_RAM __attribute__((section(".ccmram")))

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"



#define MsgSize (sizeof(char) * 20)

SemaphoreHandle_t senderSemaphores[3];
SemaphoreHandle_t receiverSemaphore;
QueueHandle_t MessageQueue;
TimerHandle_t senderTimers[3];
TimerHandle_t receiverTimer;


int sentMessages[3] = {0};
int blockedMessages[3] = {0};
int totaltransmittedmessages = 0;
int totalBlockedMessages = 0;
int receivedMessages = 0;
int RandomPeriodSum[3] ={ 0,0,0};
int RandomPeriodCount[3] = {0,0,0};
int AvgOneTask[3];
int TotalAvg;

int StartPeriod[3];
int Min_Bounds[] = {50, 80, 110, 140, 170, 200};
int Max_Bounds[] = {150, 200, 250, 300, 350, 400};
int CurrentRangeIndex = -1;



void CreateQueue(void)
{

	   MessageQueue = xQueueCreate(3, MsgSize);
	    if (MessageQueue == NULL) {
	        printf("Queue isn't created \n");
	        return;
	    }
}



void CreateSemaphores(void)
{
    for (int i = 0; i < 3; i++) {
        senderSemaphores[i] = xSemaphoreCreateBinary();
        if (senderSemaphores[i] == NULL) {
            printf("Sender Semaphore isn't created %d\n", i);
            return;
        }
    }

    receiverSemaphore = xSemaphoreCreateBinary();
    if (receiverSemaphore == NULL) {
        printf("Receiver Semaphore isn't created\n");
        return;
    }
}




void SenderTask(void *Parameters)
{
    int senderID = (int) Parameters;
    char message[MsgSize];
    BaseType_t xStatus;
    RandomPeriodSum[senderID]=StartPeriod[senderID];
    RandomPeriodCount[senderID]++;

    while (1) {
        xSemaphoreTake(senderSemaphores[senderID], portMAX_DELAY);

        TickType_t CurrentTime = xTaskGetTickCount();
        snprintf(message, MsgSize, "Time is %lu", CurrentTime);
        xStatus=xQueueSend(MessageQueue, &message, 0);

        if (  xStatus == pdPASS) {
            sentMessages[senderID]++;
            totaltransmittedmessages++;
        } else {
            blockedMessages[senderID]++;
            totalBlockedMessages++;
        }

        int32_t NewPeriod = (rand() % (Max_Bounds[CurrentRangeIndex] - Min_Bounds[CurrentRangeIndex] + 1)) + Min_Bounds[CurrentRangeIndex];
        xTimerChangePeriod(senderTimers[senderID], pdMS_TO_TICKS(NewPeriod), 0);

        RandomPeriodSum[senderID] += NewPeriod;
        RandomPeriodCount[senderID]++;
    }
}

void receiverTask(void) {

    char receivedMessage[MsgSize];
    BaseType_t xStatus;
    while (1) {

        xSemaphoreTake(receiverSemaphore, portMAX_DELAY);
        xStatus = xQueueReceive(MessageQueue, &receivedMessage, 0);

        if ( xStatus == pdPASS) {
            printf("%s\n", receivedMessage);
            receivedMessages++;
        }

        if (receivedMessages >= 10) {
            reset();
        }

    }
}


// Timers Callback

void senderTimerCallback(TimerHandle_t xTimer)
{
    int senderID = (int) pvTimerGetTimerID(xTimer);
    xSemaphoreGive(senderSemaphores[senderID]);
}


void receiverTimerCallback(TimerHandle_t xTimer)
{
    xSemaphoreGive(receiverSemaphore);
}





void reset(void)
{

	for (int i = 0; i < 3; i++) {
	    AvgOneTask[i] = RandomPeriodSum[i] / RandomPeriodCount[i];}

    printf("Total sent messages are  %d\n", totaltransmittedmessages);
    printf("Total blocked messages are %d\n", totalBlockedMessages);

    for (int i = 0; i < 3; i++) {
    	int x = i+1;
         printf("Average time of Task %d is %d ms\n",x, AvgOneTask[i]);}

    TotalAvg=(AvgOneTask[0]+AvgOneTask[1]+AvgOneTask[2])/3;

    printf("Average time of three tasks is %d ms\n",TotalAvg);

    for (int i = 0; i < 3; i++)
    {
    	int x = i+1;
        printf("for task %d The transmitted messages: %d, Blocked ones: %d\n", x, sentMessages[i], blockedMessages[i]);
        sentMessages[i] = 0;
        blockedMessages[i] = 0;
    }

    totaltransmittedmessages = 0;
    totalBlockedMessages = 0;
    receivedMessages = 0;

    for (int i = 0; i < 3; i++) {
    	RandomPeriodSum[i] =0;
    	RandomPeriodCount[i]=0;}

    xQueueReset(MessageQueue);
    CurrentRangeIndex++;

    if ( CurrentRangeIndex>= 6) {
        for (int i = 0; i < 3; i++) {
            xTimerDelete(senderTimers[i], 0);
        }
        xTimerDelete(receiverTimer, 0);
        printf("Game Over\n");
        exit(0);
        vTaskEndScheduler();
    }
}



int main(void)
{

    srand(time(NULL));
    CreateSemaphores();
    CreateQueue ();

    for (int i = 0; i < 3; i++) {
    	int taskPriority;

    	if( i == 0 )
    	{
    		 taskPriority = 2;
    	}
    	else
    	{
             taskPriority = 1;
    	}
        xTaskCreate(SenderTask, "SenderTask", 1000, (void*) i, taskPriority, NULL);
    }
    xTaskCreate(receiverTask, "ReceiverTask", 1000, NULL,3, NULL);

    reset();


    for (int i = 0; i < 3; i++)
    {
    	StartPeriod[i] = (rand() % (Max_Bounds[CurrentRangeIndex] - Min_Bounds[CurrentRangeIndex] + 1)) + Min_Bounds[CurrentRangeIndex];
        senderTimers[i] = xTimerCreate("SenderTimer", pdMS_TO_TICKS(StartPeriod[i]), pdFALSE, (void*) i, senderTimerCallback);
        xTimerStart(senderTimers[i], 0);
    }
    receiverTimer = xTimerCreate("ReceiverTimer", pdMS_TO_TICKS(100), pdTRUE, NULL, receiverTimerCallback);
    xTimerStart(receiverTimer, 0);



    vTaskStartScheduler();

    return 0;
}

#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------

void vApplicationMallocFailedHook( void )
{
	/* Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configconfigCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
volatile size_t xFreeStackSpace;

	/* This function is called on each cycle of the idle task.  In this case it
	does nothing useful, other than report the amout of FreeRTOS heap that
	remains unallocated. */
	xFreeStackSpace = xPortGetFreeHeapSize();

	if( xFreeStackSpace > 1000 )
	{
		/* By now, the kernel has allocated everything it is going to, so
		if there is a lot of heap remaining unallocated then
		the value of configTOTAL_HEAP_SIZE in FreeRTOSConfig.h can be
		reduced accordingly. */
	}
}

void vApplicationTickHook(void) {
}

StaticTask_t xIdleTaskTCB CCM_RAM;
StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE] CCM_RAM;

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize) {
  /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
  state will be stored. */
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

  /* Pass out the array that will be used as the Idle task's stack. */
  *ppxIdleTaskStackBuffer = uxIdleTaskStack;

  /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
  Note that, as the array is necessarily of type StackType_t,
  configMINIMAL_STACK_SIZE is specified in words, not bytes. */
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

static StaticTask_t xTimerTaskTCB CCM_RAM;
static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH] CCM_RAM;

/* configUSE_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize) {
  *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
  *ppxTimerTaskStackBuffer = uxTimerTaskStack;
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

