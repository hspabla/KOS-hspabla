/******************************************************************************
    Copyright © 2012-2015 Martin Karsten

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/
#include "syscalls.h"
#include "pthread.h"

#include <cstdio>
#include <cstring>

static pthread_mutex_t iolock;

static void* task(void* x) {
  void* p = malloc(1023*1023);
  pthread_mutex_lock(&iolock);
  printf("%p\n",p);
  pthread_mutex_unlock(&iolock);
  memset(p, 0, 1023*1023);
}

int main() {
  pthread_mutex_init( &iolock, nullptr );
  pthread_t t;
  pthread_create(&t, nullptr, task, nullptr);
  while (1) {
    void* p = malloc(1023*1023);
    pthread_mutex_lock(&iolock);
    printf("%p\n",p);
    pthread_mutex_unlock(&iolock);
    memset(p, 0, 1023*1023);
  }
  return 0;
}
