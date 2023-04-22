#ifndef SCR_LAB2_LOG_H
#define SCR_LAB2_LOG_H


#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#define LOG_DIRECTION_SIZE (strlen(log_location) + 36)
#define DUMP_DIRECTION_SIZE (strlen(dump_location) + 37)
#define LOG_DIR logger.log_dir
#define DUMP_DIR logger.dump_dir
#define SEM_DUMP logger.sem_dump
#define SEM_FILE logger.sem_file
#define SEM_LOG logger.sem_log

enum importance_t {
    LOW = 0,
    AVERAGE = 1,
    HIGH = 2
};

enum specificity_t {
    MIN = 2,
    STANDART = 1,
    MAX = 0
};

enum status_t {
    OK = 0,
    MEMORY_ERROR,
    ALREADY_INIT,
    ALREADY_DISABLED,
    NOT_INIT,
    FILE_ERROR,
    TOO_LOW_IMPORTANCE
};


enum status_t
logger_init(char *load_dump, char *log_location, char *dump_location, int signal_no_log, int signal_no_dump,
            enum specificity_t);

enum status_t logger_log(enum importance_t importance, char *format, ...);

enum status_t logger_stop();


#endif //SCR_LAB2_LOG_H
