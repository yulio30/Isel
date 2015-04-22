#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <wiringPi.h>
#include "fsm.h"
#include <stdio.h>

#define GPIO_BUTTON	2
#define GPIO_LED	3
#define GPIO_CUP	4
#define GPIO_COFFEE	5
#define GPIO_MILK	6

#define GPIO_COIN 7
#define GPIO_C0   8
#define GPIO_C1   9
#define GPIO_C2   10

#define CUP_TIME	250
#define COFFEE_TIME	3000
#define MILK_TIME	3000
#define COFFEE_PRICE  50


enum cofm_state {
  COFM_WAITING,
  COFM_CUP,
  COFM_COFFEE,
  COFM_MILK,
};

enum monm_state {
  MONM_WAITING,
};

static int button,monedaintrod = 0;
static int flag_coin,final, comenzar = 0;


static int timer = 0;
static void timer_isr (union sigval arg) { timer = 1; }
static int quantity;

static void timer_start (int ms)
{
  timer_t timerid;
  struct itimerspec value;
  struct sigevent se;
  se.sigev_notify = SIGEV_THREAD;
  se.sigev_value.sival_ptr = &timerid;
  se.sigev_notify_function = timer_isr;
  se.sigev_notify_attributes = NULL;
  value.it_value.tv_sec = ms / 1000;
  value.it_value.tv_nsec = (ms % 1000) * 1000000;
  value.it_interval.tv_sec = 0;
  value.it_interval.tv_nsec = 0;
  timer_create (CLOCK_REALTIME, &se, &timerid);
  timer_settime (timerid, 0, &value, NULL);
}

static int coin_inserted (fsm_t* this)
{
if (quantity >= COFFEE_PRICE){
if (monedaintrod != 0) {printf("Ya has introducido dinero suficiente, devueltos tus %d céntimos \n",monedaintrod);}
flag_coin = 1;
}else { 
quantity += monedaintrod;
printf("Has introducido %d céntimos \n", quantity);
}

return flag_coin;
}

static int button_pressed (fsm_t* this)
{  
   if (quantity >= COFFEE_PRICE && button ==1){
   printf("Café elegido, espere %d \n", button);
   button = 0;
   comenzar = 1;
}
  return comenzar;
}

static int timer_finished (fsm_t* this)
{
  int ret = timer;
  timer = 0;
  return ret;
}

static void valid_coins (fsm_t* this)
{
int returncoins;
  if(flag_coin==1 && final==1) {
  returncoins = quantity - COFFEE_PRICE;
printf("A devolver: %d\n\n",returncoins);
  flag_coin=0;
  returncoins=0;
  quantity=0;
  comenzar = 0;
  final=0;
    }
}

static void cup (fsm_t* this)
{
  digitalWrite (GPIO_LED, LOW);
  digitalWrite (GPIO_CUP, HIGH);
  printf("Sirviendo vaso\n");
  timer_start (CUP_TIME);

}

static void coffee (fsm_t* this)
{
  digitalWrite (GPIO_CUP, LOW);
  digitalWrite (GPIO_COFFEE, HIGH);
  printf("Añadiendo cafe\n");
  timer_start (COFFEE_TIME);

}

static void milk (fsm_t* this)
{
  digitalWrite (GPIO_COFFEE, LOW);
  digitalWrite (GPIO_MILK, HIGH);
  printf("Añadiendo leche\n");
  timer_start (MILK_TIME);

}

static void finish (fsm_t* this)
{
  digitalWrite (GPIO_MILK, LOW);
  digitalWrite (GPIO_LED, HIGH);
  final=1;
  printf("Su bebida está lista. Disfrute\n");
}


// Explicit FSM description
static fsm_trans_t cofm[] = {
  { COFM_WAITING, button_pressed, COFM_CUP,     cup    },
  { COFM_CUP,     timer_finished, COFM_COFFEE,  coffee },
  { COFM_COFFEE,  timer_finished, COFM_MILK,    milk   },
  { COFM_MILK,    timer_finished, COFM_WAITING, finish },
  {-1, NULL, -1, NULL },
};

static fsm_trans_t monm[] = {
  { MONM_WAITING, coin_inserted, MONM_WAITING,valid_coins},
  {-1, NULL, -1, NULL },
};


// Utility functions, should be elsewhere

// res = a - b
void
timeval_sub (struct timeval *res, struct timeval *a, struct timeval *b)
{
  res->tv_sec = a->tv_sec - b->tv_sec;
  res->tv_usec = a->tv_usec - b->tv_usec;
  if (res->tv_usec < 0) {
    --res->tv_sec;
    res->tv_usec += 1000000;
  }
}

// res = a + b
void
timeval_add (struct timeval *res, struct timeval *a, struct timeval *b)
{
  res->tv_sec = a->tv_sec + b->tv_sec
    + a->tv_usec / 1000000 + b->tv_usec / 1000000; 
  res->tv_usec = a->tv_usec % 1000000 + b->tv_usec % 1000000;
}

// wait until next_activation (absolute time)
void delay_until (struct timeval* next_activation)
{
  struct timeval now, timeout;
  gettimeofday (&now, NULL);
  timeval_sub (&timeout, next_activation, &now);
  select (0, NULL, NULL, NULL, &timeout);
}


int main (int argc, char *argv[])
{

  struct timeval clk_period = { 0, 250 * 1000 };
  struct timeval next_activation;
  fsm_t* cofm_fsm = fsm_new (cofm);
  fsm_t* monm_fsm = fsm_new (monm);
  /*
  wiringPiSetup();
  pinMode (GPIO_BUTTON, INPUT);
  wiringPiISR (GPIO_BUTTON, INT_EDGE_FALLING, button_isr);
  pinMode (GPIO_CUP, OUTPUT);
  pinMode (GPIO_COFFEE, OUTPUT);
  pinMode (GPIO_MILK, OUTPUT);
  pinMode (GPIO_LED, OUTPUT);
  digitalWrite (GPIO_LED, HIGH);
  */
  gettimeofday (&next_activation, NULL);
  while (scanf("%d %d %d", &monedaintrod, &button, &timer)==3) {    
    fsm_fire (cofm_fsm);
    fsm_fire (monm_fsm);
    timeval_add (&next_activation, &next_activation, &clk_period);
    delay_until (&next_activation);
  } 
return 0;
}
