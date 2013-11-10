/*
  Blink
  Turns on an LED on for one second, then off for one second, repeatedly.
 
  This example code is in the public domain.
 */
// библиотека используется для получения температуры с датчиков DHT-11
#include <dht.h>
 
// количество обогревателей
#define HEATERS 3
#define MINUTE 1
// время переключения обогревателей
int workCounter;
int switchInterval = 6;
// выходы конроллера, управляющие обогревателями
int heaters[HEATERS] = {8,9,10};
// статус обогревателей включены или нет
int heater_status[HEATERS] = {0,0,0};
// потребляемый обогревателями ток
int heater_current[HEATERS] = {500,500,500};
// время работы до отключения
int heater_timeout[HEATERS] = {0,0,0};
// максимальный ток
int max_current = 4000;
int redline_current = 3000;
// ток используемый прочими потребителями
int other_current = 2100;

int current;
// очередь на использование тока обогревателями
int queue[HEATERS] = {0,0,0};
// индекс следующего в очереди на добавление
int queue_next = 0;
// приоритеты нагревателей
int priority[HEATERS] = {1,0,0};

// текущие температуры комнат где расположены обогреватели
int temperature[HEATERS] =  {15,15,15};
// необходимая температура
int select_temperature[HEATERS] =  {30,29,29};
DHT dhts[HEATERS];
int dhtpins[HEATERS] = {A0,A1,A2};

void setup() {                
  // initialize the digital pin as an output.
  for(int i = 0; i < HEATERS; i++) {
      pinMode(heaters[i], OUTPUT);
      dhts[i] = DHT();
      dhts[i].attach(dhtpins[i]);
  }
  // ожидание готовности сенсоров температуры
  delay(1000);
  current = 0;
  Serial.begin(9600);
  workCounter = 0;
}

int get_heater_current() {
  int current = 0;
  for(int i = 0; i < HEATERS; i++) {
    current += heater_status[i]?heater_current[i]:0;
  }
  return current;
}

int get_free_current() {
  return redline_current - other_current - get_heater_current();
}

void refresh_temperature() {
  for(int i = 0; i < 1; i++ ) { //HEATERS; i++ ) {
    dhts[i].update();
    if( dhts[i].getLastError() == DHT_ERROR_OK ) {
      temperature[i] = dhts[i].getTemperatureInt();
    }
  }
}

void print_info() {
  Serial.print("Temp:");
  Serial.println(temperature[0]);
  Serial.println(temperature[1]);
  Serial.println(temperature[2]);
  Serial.print("Queue:");
  Serial.println(queue[0]);
  Serial.println(queue[1]);
  Serial.println(queue[2]);
  Serial.println(queue_next);
  Serial.println("============");
}

boolean compareTimeout(int x,int y) {
  int xheater_timeout = heater_timeout[x];
  int yheater_timeout = heater_timeout[y];
  if( xheater_timeout && !yheater_timeout ||
      xheater_timeout && yheater_timeout && xheater_timeout < yheater_timeout ) {
        return true;
  }
  return false;
}

// поместить обогреватель в очередь
void add_to_queue(int i_heater) {
  Serial.print("Add to queue:");
  Serial.println(i_heater);
  for(int i = 0; i < queue_next; i++) {
    if(queue[i] == i_heater)
      return;
    if(priority[i_heater] > priority[queue[i]] || 
       priority[i_heater] == priority[queue[i]] && 
       compareTimeout(i_heater,i)){
      int j = i;
      for(; j < queue_next && queue[i] != i_heater;j++);
      if(j==queue_next)
        queue_next++;
      for(;j > i;j--) {
        queue[j] = queue[j-1];
      }
      queue[i]=i_heater;
      return;
    }
  }
  queue[queue_next++] = i_heater;
}

// получить из очереди обогреватель
int pop_queue() {
  if(queue_next) {
    int ret = queue[0];
    queue_next--;
    for(int i = 0; i < queue_next; i++)
      queue[i] = queue[i+1];
    return ret;
  }
  return -1;
}

// включить обогреватель
void turn_on_heater(int i_heater) {
  Serial.print("Turn on ");
  Serial.println(i_heater);
  if(heater_timeout[i_heater] == 0) {
    heater_timeout[i_heater] = switchInterval * MINUTE;
  }
  heater_status[i_heater] = 1;
  digitalWrite(heaters[i_heater],HIGH);
}

// выключить обогреватель
void turn_off_heater(int i_heater) {
  // обнулить таймер
  Serial.print("Turn off ");
  Serial.println(i_heater);
  heater_timeout[i_heater] = 0;
  // выключить обогреватель
  digitalWrite(heaters[i_heater],LOW);
  heater_status[i_heater] = 0;
}

// приостановить обогреватель
void pause_heater(int i_heater) {
  Serial.print("Pause ");
  Serial.println(i_heater);
  heater_status[i_heater] = 0;
  digitalWrite(heaters[i_heater],LOW);
}

// обогреватель отработал интервал
void process_heater_timeout(int i_heater) {
  // если очередь не пуста, но текущий обогреватель приоритетный
  Serial.print("Process heater timeout ");
  Serial.println(i_heater);
  if((queue_next == 0 || queue_next > 0 && priority[queue[0]] < priority[i_heater] )
     && temperature[i_heater] < select_temperature[i_heater]
  ) {
    heater_timeout[i_heater] = switchInterval * MINUTE;
  }
  // выключить обогреватель и поместить его в очередь
  else {
    turn_off_heater(i_heater);
    if(temperature[i_heater] < select_temperature[i_heater] - 2)
      add_to_queue(i_heater);
  }
}

// обработать очередь
boolean process_queue() {
  if(queue_next) {
    // есть свободная мощность
    int i_heater = queue[0];
    if(heater_current[i_heater] <= get_free_current()) {
      pop_queue();
      turn_on_heater(i_heater);
      return true;
    }
    // свободной мощности нет, но возможно есть включенные менее приоритетные обогреватели
    int free_current = 0;
    int need_current = heater_current[i_heater] - get_free_current();
    for(int i = 0; i < HEATERS; i++) {
      if(i != i_heater && heater_status[i] && priority[i] < priority[i_heater]) {
        free_current += heater_current[i];
      }
    }
    // если высвобождаемой мощности хватает
    if(free_current >= need_current) {
      pop_queue();
      free_current = 0;
      for(int i=0; i < HEATERS; i++ ) {
        if(free_current >= need_current) {
          break;
        }
        else {
          if(i != i_heater && heater_status[i] && priority[i] < priority[i_heater]) {
              pause_heater(i);
              free_current += heater_current[i];
          }
        }
      }
      turn_on_heater(i_heater);
    }
  }
  return false;
}

// проверить температуру
void check_temperature() {
  for(int i = 0; i < HEATERS; i++ ) {
    if( temperature[i] < select_temperature[i] - 2 && !heater_status[i] ) {
      add_to_queue(i);
    }
    if( temperature[i] > select_temperature[i] + 2 ) {
      turn_off_heater(i);
    }
  }
}

void check_timeout() {
  for(int i = 0; i < HEATERS; i++ ) {
    if(heater_timeout[i]) {
      if(!(--heater_timeout[i])) {
        process_heater_timeout(i);
      }
    }
  }  
}

// the loop routine runs over and over again forever:
void loop() {
  // обновляем данные о температуре раз в две секунды
  if(workCounter%2) {
    refresh_temperature();
  }
  print_info();
  check_temperature();
  process_queue();
  check_timeout();
  
  delay(1000);
  ++workCounter;
}
