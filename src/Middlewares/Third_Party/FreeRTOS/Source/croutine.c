/*
 * FreeRTOS Kernel V10.0.1
 * Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

#include "FreeRTOS.h"
#include "task.h"
#include "croutine.h"

/* Remove the whole file is co-routines are not being used. */
#if( configUSE_CO_ROUTINES != 0 )

/*
 * Some kernel aware debuggers require data to be viewed to be global, rather
 * than file scope.
 */
#ifdef portREMOVE_STATIC_QUALIFIER
	#define static
#endif


/* Lists for ready and blocked co-routines. --------------------*/
/* 下面这些链表的设计跟Task的实现看上去是差不多的 */
static List_t pxReadyCoRoutineLists[ configMAX_CO_ROUTINE_PRIORITIES ];	/*< Prioritised ready co-routines. */
static List_t xDelayedCoRoutineList1;									/*< Delayed co-routines. */
static List_t xDelayedCoRoutineList2;									/*< Delayed co-routines (two lists are used - one for delays that have overflowed the current tick count. */
static List_t * pxDelayedCoRoutineList;									/*< Points to the delayed co-routine list currently being used. */
static List_t * pxOverflowDelayedCoRoutineList;							/*< Points to the delayed co-routine list currently being used to hold co-routines that have overflowed the current tick count. */
static List_t xPendingReadyCoRoutineList;								/*< Holds co-routines that have been readied by an external event.  They cannot be added directly to the ready lists as the ready lists cannot be accessed by interrupts. */

/* Other file private variables. --------------------------------*/
CRCB_t * pxCurrentCoRoutine = NULL;
static UBaseType_t uxTopCoRoutineReadyPriority = 0;
static TickType_t xCoRoutineTickCount = 0, xLastTickCount = 0, xPassedTicks = 0;

/* The initial state of the co-routine when it is created. */
/* 任务创建的初始状态其实是ready，这个难道不可以直接ready？ */
#define corINITIAL_STATE	( 0 )

/*
 * Place the co-routine represented by pxCRCB into the appropriate ready queue
 * for the priority.  It is inserted at the end of the list.
 *
 * This macro accesses the co-routine ready lists and therefore must not be
 * used from within an ISR.
 */
/* 往就绪任务链表中增加，肯定是加到最后。如果优先级高于当前，那么需要标注最高优先级。 */
/* 结合之前的文档，这里并不会涉及到任务调度，即使是ready的 CoRoutine 优先级更高 */
#define prvAddCoRoutineToReadyQueue( pxCRCB )																		\
{																													\
	if( pxCRCB->uxPriority > uxTopCoRoutineReadyPriority )															\
	{																												\
		uxTopCoRoutineReadyPriority = pxCRCB->uxPriority;															\
	}																												\
	vListInsertEnd( ( List_t * ) &( pxReadyCoRoutineLists[ pxCRCB->uxPriority ] ), &( pxCRCB->xGenericListItem ) );	\
}

/*
 * Utility to ready all the lists used by the scheduler.  This is called
 * automatically upon the creation of the first co-routine.
 */
static void prvInitialiseCoRoutineLists( void );

/*
 * Co-routines that are readied by an interrupt cannot be placed directly into
 * the ready lists (there is no mutual exclusion).  Instead they are placed in
 * in the pending ready list in order that they can later be moved to the ready
 * list by the co-routine scheduler.
 */
static void prvCheckPendingReadyList( void );

/*
 * Macro that looks at the list of co-routines that are currently delayed to
 * see if any require waking.
 *
 * Co-routines are stored in the queue in the order of their wake time -
 * meaning once one co-routine has been found whose timer has not expired
 * we need not look any further down the list.
 */
static void prvCheckDelayedList( void );

/*-----------------------------------------------------------*/

BaseType_t xCoRoutineCreate( crCOROUTINE_CODE pxCoRoutineCode, UBaseType_t uxPriority, UBaseType_t uxIndex )
{
    BaseType_t xReturn;
    CRCB_t *pxCoRoutine;

    /* Allocate the memory that will store the co-routine control block. */
    pxCoRoutine = ( CRCB_t * ) pvPortMalloc( sizeof( CRCB_t ) );
    /* 从这里看，这个分配机制也是动态分配，如果分配成功 */
    if( pxCoRoutine )
    {
        /* If pxCurrentCoRoutine is NULL then this is the first co-routine to
           be created and the co-routine data structures need initialising. */
        /* 当前还没有 Routine 存在或运行 */
        if( pxCurrentCoRoutine == NULL )
        {
            /* 处理当前 Routine 指针，之后初始化 Routine链表 */
            pxCurrentCoRoutine = pxCoRoutine;
            prvInitialiseCoRoutineLists();
        }

        /* Check the priority is within limits. */
        /* 优先级不能够超限 */
        if( uxPriority >= configMAX_CO_ROUTINE_PRIORITIES )
        {
            uxPriority = configMAX_CO_ROUTINE_PRIORITIES - 1;
        }

        /* Fill out the co-routine control block from the function parameters. */
        /* 创建后有一个默认状态 */
        pxCoRoutine->uxState = corINITIAL_STATE;
        /* 优先级、index以及绑定的函数执行体注册 */
        pxCoRoutine->uxPriority = uxPriority;
        pxCoRoutine->uxIndex = uxIndex;
        pxCoRoutine->pxCoRoutineFunction = pxCoRoutineCode;

        /* Initialise all the other co-routine control block parameters. */
        /* 调度以及事件相关链表初始化 */
        vListInitialiseItem( &( pxCoRoutine->xGenericListItem ) );
        vListInitialiseItem( &( pxCoRoutine->xEventListItem ) );

        /* Set the co-routine control block as a link back from the ListItem_t.
           This is so we can get back to the containing CRCB from a generic item
           in a list. */
        /* 链表Owner都设置为控制块 */
        listSET_LIST_ITEM_OWNER( &( pxCoRoutine->xGenericListItem ), pxCoRoutine );
        listSET_LIST_ITEM_OWNER( &( pxCoRoutine->xEventListItem ), pxCoRoutine );

        /* Event lists are always in priority order. */
        /* 事件链表元素赋值 */
        listSET_LIST_ITEM_VALUE( &( pxCoRoutine->xEventListItem ), ( ( TickType_t ) configMAX_CO_ROUTINE_PRIORITIES - ( TickType_t ) uxPriority ) );

        /* Now the co-routine has been initialised it can be added to the ready
           list at the correct priority. */
        /* 最后也是加入到就绪链表，跟任务的处理其实是相似的 */
        prvAddCoRoutineToReadyQueue( pxCoRoutine );

        xReturn = pdPASS;
    }
    else
    {
        /* 如果存储分配失败，那么通过返回值标注失败 */
        xReturn = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

void vCoRoutineAddToDelayedList( TickType_t xTicksToDelay, List_t *pxEventList )
{
    TickType_t xTimeToWake;

    /* Calculate the time to wake - this may overflow but this is
       not a problem. */
    /* 设置一个唤醒的定时时刻，传入的时间是一个阻塞时间 */
    xTimeToWake = xCoRoutineTickCount + xTicksToDelay;

    /* We must remove ourselves from the ready list before adding
       ourselves to the blocked list as the same list item is used for
       both lists. */
    /* 从就绪链表中移除相应的任务管理 */
    ( void ) uxListRemove( ( ListItem_t * ) &( pxCurrentCoRoutine->xGenericListItem ) );

    /* The list item will be inserted in wake time order. */
    /* 设置唤醒时间用以提供后续的唤醒顺序的排序 */
    listSET_LIST_ITEM_VALUE( &( pxCurrentCoRoutine->xGenericListItem ), xTimeToWake );

    /* 如果出现了溢出 */
    if( xTimeToWake < xCoRoutineTickCount )
    {
        /* Wake time has overflowed.  Place this item in the
           overflow list. */
        /* 加入到溢出delayed CoRoutine链表 */
        vListInsert( ( List_t * ) pxOverflowDelayedCoRoutineList, ( ListItem_t * ) &( pxCurrentCoRoutine->xGenericListItem ) );
    }
    else
    {
        /* The wake time has not overflowed, so we can use the
           current block list. */
        /* 加入到正常的 delayed CoRoutine 链表 */
        vListInsert( ( List_t * ) pxDelayedCoRoutineList, ( ListItem_t * ) &( pxCurrentCoRoutine->xGenericListItem ) );
    }

    /* 如果等待事件 */
    if( pxEventList )
    {
        /* Also add the co-routine to an event list.  If this is done then the
           function must be called with interrupts disabled. */
        vListInsert( pxEventList, &( pxCurrentCoRoutine->xEventListItem ) );
    }
}
/*-----------------------------------------------------------*/

static void prvCheckPendingReadyList( void )
{
    /* Are there any co-routines waiting to get moved to the ready list?  These
       are co-routines that have been readied by an ISR.  The ISR cannot access
       the	ready lists itself. */
    /* 如果 pending ready list 不是空的 */
    while( listLIST_IS_EMPTY( &xPendingReadyCoRoutineList ) == pdFALSE )
    {
        CRCB_t *pxUnblockedCRCB;

        /* The pending ready list can be accessed by an ISR. */
        /* 进入中断保护,后面应该是从pending ready往 ready里面做搬运 */
        portDISABLE_INTERRUPTS();
        {
            /* 获取 pending ready list的首元素 */
            pxUnblockedCRCB = ( CRCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( (&xPendingReadyCoRoutineList) );
            /* 事件链表元素从 pending ready list中将其移除 */
            ( void ) uxListRemove( &( pxUnblockedCRCB->xEventListItem ) );
        }
        /* 退出中断保护 */
        portENABLE_INTERRUPTS();

        /* 通用链表元素解除与之前的链表绑定，之后加入到就绪队列中 */
        ( void ) uxListRemove( &( pxUnblockedCRCB->xGenericListItem ) );
        prvAddCoRoutineToReadyQueue( pxUnblockedCRCB );
    }
}
/*-----------------------------------------------------------*/

static void prvCheckDelayedList( void )
{
    CRCB_t *pxCRCB;

    /* xLastTickCount是上次执行的时候的tick数值，会在这个接口最后面进行更新 */
    /* 这样，下面的代码实现了这一次执行距离上一次执行的时间差的获取 */
    xPassedTicks = xTaskGetTickCount() - xLastTickCount;
    /* 下面的代码根据经过的tick数值来做了一个循环 */
    while( xPassedTicks )
    {
        /* 下面这个计数器实现了一个可能不均等间隔的系统tick数目的记录模拟 */
        /* 感觉上，直接放到循环外面做一个整体数值的增加似乎也是可以的 */
        xCoRoutineTickCount++;
        xPassedTicks--;

        /* If the tick count has overflowed we need to swap the ready lists. */
        /* 出现溢出，2个delayed list进行交换 */
        if( xCoRoutineTickCount == 0 )
        {
            List_t * pxTemp;

            /* Tick count has overflowed so we need to swap the delay lists.  If there are
               any items in pxDelayedCoRoutineList here then there is an error! */
            pxTemp = pxDelayedCoRoutineList;
            pxDelayedCoRoutineList = pxOverflowDelayedCoRoutineList;
            pxOverflowDelayedCoRoutineList = pxTemp;
        }

        /* See if this tick has made a timeout expire. */
        /* pxDelayedCoRoutineList有元素的时候循环处理 */
        while( listLIST_IS_EMPTY( pxDelayedCoRoutineList ) == pdFALSE )
        {
            /* 找到首元素 */
            pxCRCB = ( CRCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxDelayedCoRoutineList );

            /* 看当前的tick数值是否到了定时点，还没到剩下的直接不处理了，直接退出 */
            if( xCoRoutineTickCount < listGET_LIST_ITEM_VALUE( &( pxCRCB->xGenericListItem ) ) )
            {
                /* Timeout not yet expired. */
                break;
            }

            /* 此时至少首元素的定时点到了，关中断 */
            portDISABLE_INTERRUPTS();
            {
                /* The event could have occurred just before this critical
                   section.  If this is the case then the generic list item will
                   have been moved to the pending ready list and the following
                   line is still valid.  Also the pvContainer parameter will have
                   been set to NULL so the following lines are also valid. */
                /* 从delayed链表中移除进入到就绪队列 */
                ( void ) uxListRemove( &( pxCRCB->xGenericListItem ) );

                /* Is the co-routine waiting on an event also? */
                if( pxCRCB->xEventListItem.pvContainer )
                {
                    /* 如果等待事件，也从事件等待中移除 */
                    ( void ) uxListRemove( &( pxCRCB->xEventListItem ) );
                }
            }
            /* 开中断 */
            portENABLE_INTERRUPTS();

            /* 加入到就绪队列 */
            prvAddCoRoutineToReadyQueue( pxCRCB );
        }
    }

    xLastTickCount = xCoRoutineTickCount;
}
/*-----------------------------------------------------------*/

void vCoRoutineSchedule( void )
{
    /* See if any co-routines readied by events need moving to the ready lists. */
    prvCheckPendingReadyList();

    /* See if any delayed co-routines have timed out. */
    prvCheckDelayedList();

    /* Find the highest priority queue that contains ready co-routines. */
    /* 如果没有就绪的 CoRoutine 那么直接退出 */
    while( listLIST_IS_EMPTY( &( pxReadyCoRoutineLists[ uxTopCoRoutineReadyPriority ] ) ) )
    {
        if( uxTopCoRoutineReadyPriority == 0 )
        {
            /* No more co-routines to check. */
            return;
        }
        --uxTopCoRoutineReadyPriority;
    }

    /* listGET_OWNER_OF_NEXT_ENTRY walks through the list, so the co-routines
       of the	same priority get an equal share of the processor time. */
    /* 获取就绪的最高优先级就绪任务链表中的首元素执行其CODE */
    listGET_OWNER_OF_NEXT_ENTRY( pxCurrentCoRoutine, &( pxReadyCoRoutineLists[ uxTopCoRoutineReadyPriority ] ) );

    /* Call the co-routine. */
    ( pxCurrentCoRoutine->pxCoRoutineFunction )( pxCurrentCoRoutine, pxCurrentCoRoutine->uxIndex );

    return;
}
/*-----------------------------------------------------------*/

static void prvInitialiseCoRoutineLists( void )
{
    UBaseType_t uxPriority;

    /* 给每一个优先级创建一个就绪的 Routine 链表 */
    for( uxPriority = 0; uxPriority < configMAX_CO_ROUTINE_PRIORITIES; uxPriority++ )
    {
        vListInitialise( ( List_t * ) &( pxReadyCoRoutineLists[ uxPriority ] ) );
    }

    /* 初始化阻塞调度相关的三个链表 */
    vListInitialise( ( List_t * ) &xDelayedCoRoutineList1 );
    vListInitialise( ( List_t * ) &xDelayedCoRoutineList2 );
    vListInitialise( ( List_t * ) &xPendingReadyCoRoutineList );

    /* Start with pxDelayedCoRoutineList using list1 and the
       pxOverflowDelayedCoRoutineList using list2. */
    /* 初始化 delayed 事件相关的链表指针 */
    pxDelayedCoRoutineList = &xDelayedCoRoutineList1;
    pxOverflowDelayedCoRoutineList = &xDelayedCoRoutineList2;
}
/*-----------------------------------------------------------*/

BaseType_t xCoRoutineRemoveFromEventList( const List_t *pxEventList )
{
    CRCB_t *pxUnblockedCRCB;
    BaseType_t xReturn;

    /* This function is called from within an interrupt.  It can only access
       event lists and the pending ready list.  This function assumes that a
       check has already been made to ensure pxEventList is not empty. */
    /* 获取事件链表中的首元素所对应的 CoRoutine */
    pxUnblockedCRCB = ( CRCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxEventList );
    ( void ) uxListRemove( &( pxUnblockedCRCB->xEventListItem ) );
    /* 从事件链表转移到就绪 CoRoutine 链表中 */
    vListInsertEnd( ( List_t * ) &( xPendingReadyCoRoutineList ), &( pxUnblockedCRCB->xEventListItem ) );

    /* 下面主要是用来判断是否有调度请求的需要 */
    if( pxUnblockedCRCB->uxPriority >= pxCurrentCoRoutine->uxPriority )
    {
        xReturn = pdTRUE;
    }
    else
    {
        xReturn = pdFALSE;
    }

    return xReturn;
}

#endif /* configUSE_CO_ROUTINES == 0 */

