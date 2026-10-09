/* Creates N threads, each of which sleeps a different, fixed
   duration, M times.  Records the wake-up order and verifies
   that it is valid. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"

static void test_sleep (int thread_cnt, int iterations);

void
test_alarm_single (void) 
{
  test_sleep (5, 1);
}

void
test_alarm_multiple (void) 
{
  test_sleep (5, 7);
}

/* Information about the test. */
struct sleep_test 
  {
    int64_t start;              /* Current time at start of test. */
    int iterations;             /* Number of iterations per thread. */

    /* Output. */
    struct lock output_lock;    /* Lock protecting output buffer. */
    int *output_pos;            /* Current position in output buffer. */
  };

/* Information about an individual thread in the test. */
struct sleep_thread 
  {
    struct sleep_test *test;     /* Info shared between all threads. */
    int id;                     /* Sleeper ID. */
    int duration;               /* Number of ticks to sleep. */
    int iterations;             /* Iterations counted so far. */
  };

static void sleeper (void *);

/* Runs THREAD_CNT threads thread sleep ITERATIONS times each. */
static void test_sleep (int thread_cnt, int iterations) 
{
  struct sleep_test test;
  struct sleep_thread *threads;
  int *output, *op;
  int product;
  int i;

  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);

  msg ("Creating %d threads to sleep %d times each.", thread_cnt, iterations); // 설명 
  msg ("Thread 0 sleeps 10 ticks each time,");
  msg ("thread 1 sleeps 20 ticks each time, and so on.");
  msg ("If successful, product of iteration count and");
  msg ("sleep duration will appear in nondescending order.");

  /* Allocate memory. */
  threads = malloc (sizeof *threads * thread_cnt); // 스레드 할당
  output = malloc (sizeof *output * iterations * thread_cnt * 2); // 깨어난 걸 순서대로 적음 
  if (threads == NULL || output == NULL)
    PANIC ("couldn't allocate memory for test"); // 할당 실패하면 커널 멈춤 

  /* Initialize test. */
  test.start = timer_ticks () + 100; // tick 출발선
  test.iterations = iterations; // iteratrions 복사 
  lock_init (&test.output_lock); // lock
  test.output_pos = output; // 첫칸 부터 씀 

  /* Start threads. */
  ASSERT (output != NULL);
  for (i = 0; i < thread_cnt; i++)
    {
      struct sleep_thread *t = threads + i; // 스레드 선택
      char name[16];
      
      t->test = &test;
      t->id = i;
      t->duration = (i + 1) * 10;
      t->iterations = 0;

      snprintf (name, sizeof name, "thread %d", i); // 스레드 초기화 
      thread_create (name, PRI_DEFAULT, sleeper, t); // default priority 31로 sleepter 함 수 cereate 
      // 여기까지는 ready 큐에 ㄱwnfask todna 
    }
  
  /* Wait long enough for all the threads to finish. */
  timer_sleep (100 + thread_cnt * iterations * 10 + 100); // 다 실행되게 250 tick 기다림 -> 실행은 바로 137번줄에 있는 코드만 따로 스레드로 실행 됨 

  /* Acquire the output lock in case some rogue thread is still
     running. */
  lock_acquire (&test.output_lock); // lock 얻어서 들어감 

  /* Print completion order. */
  product = 0;
  for (op = output; op < test.output_pos; op++) 
    {
      struct sleep_thread *t;
      int new_prod;

      ASSERT (*op >= 0 && *op < thread_cnt); // theread 개수보다 작아야함 
      t = threads + *op;

      new_prod = ++t->iterations * t->duration; // t->iterations: 0 → 1 (thread 0이 1번 깼다고 셈), (몇 번 깸 x 간격 )
        
      msg ("thread %d: duration=%d, iteration=%d, product=%d",
           t->id, t->duration, t->iterations, new_prod);
      
      if (new_prod >= product)
        product = new_prod; //최대값 갱신 
      else
        fail ("thread %d woke up out of order (%d > %d)!", // 기록장에는 깨어난 시간 대로 써져잇으니까 더 전에 깨어난 애가 오면 fail
              t->id, product, new_prod);
    }

  /* Verify that we had the proper number of wakeups. */
  for (i = 0; i < thread_cnt; i++)
    if (threads[i].iterations != iterations)
      fail ("thread %d woke up %d times instead of %d",
            i, threads[i].iterations, iterations);
  
  lock_release (&test.output_lock); //자물쇠 풀기
  free (output);
  free (threads);
}

/* Sleeper thread. */
static void sleeper (void *t_) 
{
  struct sleep_thread *t = t_; // 원래 타입으로 되돌림 
  struct sleep_test *test = t->test;
  int i;

  for (i = 1; i <= test->iterations; i++) 
    {
      int64_t sleep_until = test->start + i * t->duration; // 깨어날 시간 계산 
      timer_sleep (sleep_until - timer_ticks ()); // 남은 시간 만큼 잠 
      lock_acquire (&test->output_lock);// sleep_test 구조체에서 lock 선언 
      *test->output_pos++ = t->id; // 현재 칸 을 씀 
      lock_release (&test->output_lock);
    }
}
