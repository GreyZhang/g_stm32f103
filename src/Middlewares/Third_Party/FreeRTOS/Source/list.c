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


#include <stdlib.h>
#include "FreeRTOS.h"
#include "list.h"

/*-----------------------------------------------------------
 * PUBLIC LIST API documented in list.h
 *----------------------------------------------------------*/

/* 链表的初始化 */
void vListInitialise( List_t * const pxList )
{
    /* The list structure contains a list item which is used to mark the
       end of the list.  To initialise the list the list end is inserted
       as the only list entry. */
    /* 链表中包含一个链表元素用来指示链表的结束，初始化的时候就是插入这个元素 */
    /* 初始化的链表只包含一个表征链表结束的元素 */
    /* 创建一个链表对象之后，这个信息其实在内存中已经分配了。 */
    /*lint !e826 !e740 The mini list structure is used as the list end to save RAM.  This is checked and valid. */
    pxList->pxIndex = ( ListItem_t * ) &( pxList->xListEnd );

    /* The list end value is the highest possible value in the list to
       ensure it remains at the end of the list. */
    /* 这里填充的就是一个u32的最大值，看起来这个链表的设计对于元素数目的限制其实是没什么限制的。
     * 而这个数值的定义其实是用了tick的基础类型，感觉上可能并不是一个元素个数的概念，而是用来
     * 表征时间信息的一个概念。因此，从其他地方也看到了排序的说法。 */
    pxList->xListEnd.xItemValue = portMAX_DELAY;

    /* The list end next and previous pointers point to itself so we know
       when the list is empty. */
    /* 链表为空的时候，链表结束节点的前驱后继只能够指向本身 */
    /*lint !e826 !e740 The mini list structure is used as the list end to save RAM.  This is checked and valid. */
    pxList->xListEnd.pxNext = ( ListItem_t * ) &( pxList->xListEnd );
    /*lint !e826 !e740 The mini list structure is used as the list end to save RAM.  This is checked and valid. */
    pxList->xListEnd.pxPrevious = ( ListItem_t * ) &( pxList->xListEnd );

    /* 链表的元素个数标记为0 */
    pxList->uxNumberOfItems = ( UBaseType_t ) 0U;

    /* Write known values into the list if
       configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES is set to 1. */
    /* 如果设计了检查，那么下面两部分执行检查。看了一下定义信息其实也是5a5a这样的数据的检查 */
    listSET_LIST_INTEGRITY_CHECK_1_VALUE( pxList );
    listSET_LIST_INTEGRITY_CHECK_2_VALUE( pxList );
}
/*-----------------------------------------------------------*/

/* 初始化一个链表元素：只需要把链表元素所属的链表指向NULL，用以指示这个元素还没有被任何链表应用 */
void vListInitialiseItem( ListItem_t * const pxItem )
{
    /* Make sure the list item is not recorded as being on a list. */
    pxItem->pvContainer = NULL;

    /* Write known values into the list item if
       configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES is set to 1. */
    listSET_FIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem );
    listSET_SECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem );
}
/*-----------------------------------------------------------*/

/* 看完了整个函数，其实这里有一个思维概念需要转换。之前我看过的链表，都有一个链表头来存储一些链表 */
/* 的基本信息。但是在FreeRTOS提供的这套链表信息中，整个叫做链表尾。不过双向链表是一个头尾相接的环， */
/* 因此，头或者尾没有什么影响。而且，不管是头或者尾，其实一个链表中只有一种存在 */
void vListInsertEnd( List_t * const pxList, ListItem_t * const pxNewListItem )
{
    /* 获取链表最后一个元素点的指针 */
    ListItem_t * const pxIndex = pxList->pxIndex;

    /* Only effective when configASSERT() is also defined, these tests may catch
       the list data structures being overwritten in memory.  They will not catch
       data errors caused by incorrect configuration or use of FreeRTOS. */
    /* 检查链表的数据完整性 */
    listTEST_LIST_INTEGRITY( pxList );
    listTEST_LIST_ITEM_INTEGRITY( pxNewListItem );

    /* Insert a new list item into pxList, but rather than sort the list,
       makes the new list item the last item to be removed by a call to
       listGET_OWNER_OF_NEXT_ENTRY(). */
    /* 设置新的链表元素的后继以及前驱，其中后继是链表的尾结点，因此它现在是链表中的最后一个元素。
     * 前驱设置为原来的链表最后一个元素的前驱。这样，后面还得处理链表的尾结点，这个在后面的代码中。 */
    pxNewListItem->pxNext = pxIndex;
    pxNewListItem->pxPrevious = pxIndex->pxPrevious;

    /* Only used during decision coverage testing. */
    /* 从名称看，这个是用于覆盖度测试的，目前的代码中没有使能 */
    mtCOVERAGE_TEST_DELAY();

    /* 前面分析了还有一个尾结点的处理，这里就是尾结点的处理。主要的处理点在于处理最后元素点的前驱，
     * 把它的前驱设置为最后一个真实的链表最后一个有效元素 */
    pxIndex->pxPrevious->pxNext = pxNewListItem;

    /* 由于现在使用的是双向链表，因此每一次插入处理修改其实是涉及到三个节点。其中，新元素的前驱和后继已经处理。*/
    /* 第二个是之前尾结点的前驱，尾结点的前驱的前驱不需要处理，后继应该是新元素，这个前面已经处理 */
    /* 第三个处理的节点是插入点原来元素的后继，这个其实就是尾节点。尾结点需要处理的是前驱，前驱应该是新元素。 */
    pxIndex->pxPrevious = pxNewListItem;

    /* Remember which list the item is in. */
    /* 插入的工作最后，标记现在的新元素已经有了归属链表 */
    pxNewListItem->pvContainer = ( void * ) pxList;

    /* 插入完成的时候，链表的元素增加一个。 */
    ( pxList->uxNumberOfItems )++;
}
/*-----------------------------------------------------------*/

void vListInsert( List_t * const pxList, ListItem_t * const pxNewListItem )
{
    /* 链表元素类型指针，用作迭代器 */
    ListItem_t *pxIterator;
    /* 元素的插入位置来自于元素本身，这个元素的设计前面看过，感觉后面可能会是跟系统tick相关的信息来填充 */
    const TickType_t xValueOfInsertion = pxNewListItem->xItemValue;

    /* Only effective when configASSERT() is also defined, these tests may catch
       the list data structures being overwritten in memory.  They will not catch
       data errors caused by incorrect configuration or use of FreeRTOS. */
    /* 检查链表的数据完整性 */
    listTEST_LIST_INTEGRITY( pxList );
    /* 检查信插入的元素的数据完整性 */
    listTEST_LIST_ITEM_INTEGRITY( pxNewListItem );

    /* Insert the new list item into the list, sorted in xItemValue order.

       If the list already contains a list item with the same item value then the
       new list item should be placed after it.  This ensures that TCB's which are
       stored in ready lists (all of which have the same xItemValue value) get a
       share of the CPU.  However, if the xItemValue is the same as the back marker
       the iteration loop below will not end.  Therefore the value is checked
       first, and the algorithm slightly modified if necessary. */
    /* 如果插入的元素指定的位置是一个极大值，那么迭代链表指针取值指向链表的最后一个元素，也就是尾结点前面的元素 */
    /* 结合下面的代码，这个迭代器其实是为了找到链表的插入点。尾结点的数据结构跟一般元素不同，只是一个标记信息。 */
    if( xValueOfInsertion == portMAX_DELAY )
    {
        pxIterator = pxList->xListEnd.pxPrevious;
    }
    else
    {
        /* *** NOTE ***********************************************************
           If you find your application is crashing here then likely causes are
           listed below.  In addition see http://www.freertos.org/FAQHelp.html for
           more tips, and ensure configASSERT() is defined!
           http://www.freertos.org/a00110.html#configASSERT

           1) Stack overflow -
           see http://www.freertos.org/Stacks-and-stack-overflow-checking.html
           2) Incorrect interrupt priority assignment, especially on Cortex-M
           parts where numerically high priority values denote low actual
           interrupt priorities, which can seem counter intuitive.  See
           http://www.freertos.org/RTOS-Cortex-M3-M4.html and the definition
           of configMAX_SYSCALL_INTERRUPT_PRIORITY on
           http://www.freertos.org/a00110.html
           3) Calling an API function from within a critical section or when
           the scheduler is suspended, or calling an API function that does
           not end in "FromISR" from an interrupt.
           4) Using a queue or semaphore before it has been initialised or
           before the scheduler has been started (are interrupts firing
           before vTaskStartScheduler() has been called?).
        **********************************************************************/

        /*lint !e826 !e740 The mini list structure is used as the list end to save RAM.  This is checked and valid. */
        /* 如果新插入的元素所指定的位置不是极大值，那么从链表的最后一个元素开始找，如果最后一个元素的数值依然不大于这个数值，那么按照链表指向继续往后找 */
        /* 通过这个动作来看，其实这个链表是一个双向循环链表。下面的这个循环，会从链表的（初始值为xListEnd的目的就是下一个指向第一项）首元素开始逐个对比。 */
        /* 这样的链表如果一直按照这种方式排序，那么其实是一个按照 xItemValue 排序的一个结果 */
        for( pxIterator = ( ListItem_t * ) &( pxList->xListEnd ); pxIterator->pxNext->xItemValue <= xValueOfInsertion; pxIterator = pxIterator->pxNext )
        {
            /* There is nothing to do here, just iterating to the wanted
               insertion position. */
        }
    }

    /* 新的元素与找到的节点位置的下一个节点相连 */
    pxNewListItem->pxNext = pxIterator->pxNext;
    pxNewListItem->pxNext->pxPrevious = pxNewListItem;
    /* 新插入的元素与找到的插入点相连作为其后继 */
    pxNewListItem->pxPrevious = pxIterator;
    pxIterator->pxNext = pxNewListItem;

    /* Remember which list the item is in.  This allows fast removal of the
       item later. */
    /* 指定链表元素的归属链表 */
    pxNewListItem->pvContainer = ( void * ) pxList;

    /* 链表的元素数目增加一个数 */
    ( pxList->uxNumberOfItems )++;
}

/*-----------------------------------------------------------*/

UBaseType_t uxListRemove( ListItem_t * const pxItemToRemove )
{
    /* The list item knows which list it is in.  Obtain the list from the list
    item. */
    /* 定义链表元素的时候，链表元素有一个Container的成员信息，其实就是存储了元素的链表归属 */
    /* 这里通过这个信息，获取了链表的信息 */
    List_t * const pxList = ( List_t * ) pxItemToRemove->pvContainer;

    /* 被移除的元素的处理，后继的前驱为前驱 */
    pxItemToRemove->pxNext->pxPrevious = pxItemToRemove->pxPrevious;
    /* 前驱的后继为后继 */
    pxItemToRemove->pxPrevious->pxNext = pxItemToRemove->pxNext;

    /* Only used during decision coverage testing. */
    /* 没有应用，暂时没有弄清楚具体的作用 */
    mtCOVERAGE_TEST_DELAY();

    /* Make sure the index is left pointing to a valid item. */
    /* 如果移除的元素是链表的最后一个元素 */
    if( pxList->pxIndex == pxItemToRemove )
    {
        /* 链表的最后一个元素的指针需要修改，改成被移除对象的前驱 */
        pxList->pxIndex = pxItemToRemove->pxPrevious;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }

    /* 到此，链表的处理基本上结束。可以看得出来，其实这样的修改修改的只是链表中的元素的链接关系。 */
    /* 那么，被移除的元素本身的信息其实没有什么处理变化。那么，如何让这个元素本身的处理彻底呢？那就是 */
    /* 下面的这一步操作：抹除元素跟链表之间的关系 */
    pxItemToRemove->pvContainer = NULL;
    /* 一切处理结束后，链表的元素数目减一 */
    ( pxList->uxNumberOfItems )--;

    /* 这个接口，返回的是链表的元素个数。 */
    return pxList->uxNumberOfItems;
}
/*-----------------------------------------------------------*/

