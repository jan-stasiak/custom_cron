#ifndef SCR_LAB2_CONST_H
#define SCR_LAB2_CONST_H

#include <stdio.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <mqueue.h>
#include <errno.h>
#include <spawn.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <time.h>
#include <semaphore.h>



#define DIRECTION_MAIN_QUEUE  "/server_message_queue"

typedef struct message_t{
    char message[120];
}Message;

struct cron_table_info {
    sem_t mutex;
    char tasks[120];
    bool is_next;
};

#endif //SCR_LAB2_CONST_H
