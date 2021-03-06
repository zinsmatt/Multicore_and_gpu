/*
 * stack.c
 *
 *  Created on: 18 Oct 2011
 *  Copyright 2011 Nicolas Melot
 *
 * This file is part of TDDD56.
 *
 *     TDDD56 is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 *
 *     TDDD56 is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with TDDD56. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef DEBUG
#define NDEBUG
#endif

#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "stack.h"
#include "non_blocking.h"

#if NON_BLOCKING == 0
#warning Stacks are synchronized through locks
#else
#if NON_BLOCKING == 1
#warning Stacks are synchronized through hardware CAS
#else
#warning Stacks are synchronized through lock-based CAS
#endif
#endif



void
stack_check(stack_t *stack)
{
// Do not perform any sanity check if performance is bein measured
#if MEASURE == 0
	// Use assert() to check if your stack is in a state that makes sens
	// This test should always pass
	assert(1 == 1);

	// This test fails if the task is not allocated or if the allocation failed
	assert(stack != NULL);
#endif
}

int /* Return the type you prefer */
stack_push(stack_t  *stack, int value, int pool_num)
{
  // item_t *nouv = (item_t*) malloc(sizeof(item_t)); REPLACED BY from pool()

  item_t *nouv = (item_t*) from_pool(pool_num);
  nouv->value = value;

#if NON_BLOCKING == 0
  // Implement a lock_based stack
  if(stack)
  {
    pthread_mutex_lock(&stack->lock);
    nouv->next = stack->head;
    stack->head = nouv;
    pthread_mutex_unlock(&stack->lock);
  }

#elif NON_BLOCKING == 1
  // Implement a harware CAS-based stack
  if(stack)
  {
    item_t *old;
    do
    {
      old = stack->head;
      nouv->next = old;
    }while(cas((size_t*)&(stack->head),(size_t)old,(size_t)nouv) != (size_t)old);

  }

#else
  /*** Optional ***/
  // Implement a software CAS-based stack
	if(stack)
  {
    item_t *old;
    do
    {
      old = stack->head;
      nouv->next = old;
    }while(software_cas((size_t*)&(stack->head),(size_t)old,(size_t)nouv, &stack->lock) != (size_t)old);

  }
#endif

  // Debug practice: you can check if this operation results in a stack in a consistent check
  // It doesn't harm performance as sanity check are disabled at measurement time
  // This is to be updated as your implementation progresses
  stack_check((stack_t*)1);

  return 0;
}

int /* Return the type you prefer */
stack_pop(stack_t *stack, int pool_num)
{
  int old_value = 0;
  item_t *old = 0;

#if NON_BLOCKING == 0
  // Implement a lock_based stack
  if(stack->head)
  {
    pthread_mutex_lock(&stack->lock);
    old = stack->head;
    stack->head = old->next;
    pthread_mutex_unlock(&stack->lock);

    old_value = old->value;
  }


#elif NON_BLOCKING == 1
  // Implement a harware CAS-based stack
  if(stack && stack->head)
  {
    do
    {
      old = stack->head;
    }while(cas((size_t*)&(stack->head),(size_t) old,(size_t) old->next) != (size_t) old);

    old_value = old->value;
  }

#else
  /*** Optional ***/
  // Implement a software CAS-based stack

	if(stack && stack->head)
  {
    do
    {
      old = stack->head;
    }while(software_cas((size_t*)&(stack->head),(size_t) old,(size_t) old->next, &stack->lock) != (size_t) old);

    old_value = old->value;
  }
#endif

  // add_pool INSTEAD OF free()

  add_pool(pool_num, old);

  return old_value;
}



void stack_init(stack_t *stack)
{
#if NON_BLOCKING == 0
 if (pthread_mutex_init(&stack->lock, NULL) != 0)
  {
      printf("\n Error : stack lock init failed\n");
  }
#endif
  stack->head = NULL;
}

void stack_destroy(stack_t *stack)
{
#if NON_BLOCKING == 0
  pthread_mutex_destroy(&stack->lock);
#endif
  free(stack);
}

int stack_size(stack_t* stack)
{
  int i=0;

  if(!stack) return -1;

  item_t *pt = stack->head;
  while(pt)
  {
    i++;
    pt = pt->next;
  }
  return i;
}


void add_pool(int pool_num, item_t* item)
{
  if(!pools[pool_num])
  {
    pools[pool_num] = item;
  }else
  {
    pools_end[pool_num]->next = item;
    //pools_end[pool_num] = item;
    //item_t *pt = pools[pool_num];
    //while(pt->next)
    //  pt = pt->next;
    //pt->next = item;
  }
  item->next = NULL;
  pools_end[pool_num] = item;
}

item_t* from_pool(int pool_num)
{
  item_t *pt;

  if(pools[pool_num])
  {
    pt = pools[pool_num];
    pools[pool_num] = pt->next;
    if(pt->next == NULL)
      pools_end[pool_num] = NULL;
  }else
  {     // the pool is empty
    pt = (item_t*) malloc(sizeof(item_t));
  }
  return pt;
}

void free_pools()
{

  int i;
  for(i=0;i<NB_THREADS;++i)
  {
    if(pools[i])
    {
      item_t *pool = pools[i];
      item_t *prev = NULL;
      while(pool)
      {
        prev = pool;
        pool = (pool)->next;
        free(prev);
      }
      pools[i] = NULL;
    }
  }

}
