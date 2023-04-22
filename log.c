#include "log.h"

#define LOG_DIRECTION_SIZE (strlen(log_location) + 36)
#define DUMP_DIRECTION_SIZE (strlen(dump_location) + 37)
#define DIRECTORY_LOG logger.log_dir
#define DIRECTORY_DUMP logger.dump_dir
#define SEMAPHORE_DUMP logger.sem_dump
#define SEMAPHORE_FILE logger.sem_file
#define ALL_PERMISSIONS 0777


struct logger_t {
    bool is_init;
    sem_t sem_file;
    sem_t sem_dump;
    struct sigaction sa_log;
    struct sigaction sa_dump;
    int signal_1;
    int signal_2;
    pthread_t pthread_dump;
    volatile sig_atomic_t logger_detail;
    char *log_dir;
    char *dump_dir;
    size_t log_dir_size;
    size_t dump_dir_size;
};



static struct logger_t logger;

void signal_handler_log(int signo, siginfo_t *info, void *other) {
    atomic_store(&logger.logger_detail, info->si_value.sival_int);
}

void signal_handler_dump(int signo, siginfo_t *info, void *other) {
    sem_post(&SEMAPHORE_DUMP);
}

void *dump_function(void *args) {
    while (1) {
        sem_wait(&SEMAPHORE_DUMP);
        time_t current_time;
        struct tm *timeinfo;
        time(&current_time);
        timeinfo = localtime(&current_time);

        char *temp_dir = malloc((logger.dump_dir_size + 37) * sizeof(char ));
        if (temp_dir != NULL) {
            sprintf(temp_dir, "%s/Dump_file__%04d-%02d-%02d__%02d-%02d-%02d.bin", logger.dump_dir, timeinfo->tm_year + 1900,
                    timeinfo->tm_mon + 1, timeinfo->tm_mday,
                    timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

            FILE *fp = fopen(temp_dir, "wb+");
            if (fp != NULL) {
                fwrite(&logger.log_dir_size, sizeof(size_t), 1, fp);
                fwrite(&logger.dump_dir_size, sizeof(size_t), 1, fp);
                fwrite(logger.log_dir, sizeof(char), logger.log_dir_size, fp);
                fwrite(logger.dump_dir, sizeof(char), logger.log_dir_size, fp);
                fwrite(&logger.signal_1, sizeof(int), 1, fp);
                fwrite(&logger.signal_2, sizeof(int), 1, fp);
                fclose(fp);
            }
        }
        free(temp_dir);
    }
}

enum status_t
logger_init(char *load_dump, char *log_location, char *dump_location, int signal_no_log, int signal_no_dump,
            enum specificity_t) {
    if (logger.is_init == true) {
        return ALREADY_INIT;
    }

    logger.log_dir = (char *) malloc(LOG_DIRECTION_SIZE * sizeof(char));
    if (logger.log_dir == NULL) {
        return MEMORY_ERROR;
    }

    logger.dump_dir = (char *) malloc(DUMP_DIRECTION_SIZE * sizeof(char));
    if (logger.dump_dir == NULL) {
        free(logger.log_dir);
        return MEMORY_ERROR;
    }

    if (load_dump == NULL) {
        time_t current_time;
        struct tm *timeinfo;
        time(&current_time);
        timeinfo = localtime(&current_time);


        sprintf(DIRECTORY_LOG, "%s/Log_file__%04d-%02d-%02d__%02d-%02d-%02d.log", log_location,
                timeinfo->tm_year + 1900,
                timeinfo->tm_mon + 1, timeinfo->tm_mday,
                timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        sprintf(DIRECTORY_DUMP, "%s", dump_location);

        logger.dump_dir_size = DUMP_DIRECTION_SIZE;
        logger.log_dir_size = LOG_DIRECTION_SIZE;
    } else {

        FILE *fp = fopen(load_dump, "rb");
        if (fp == NULL) {
            logger_log(HIGH, "There is a problem with the dump file!");
            printf("There is a problem with the dump file!\n");
            return FILE_ERROR;
        }
        fread(&logger.log_dir_size, sizeof(size_t), 1, fp);
        fread(&logger.dump_dir_size, sizeof(size_t), 1, fp);
        fread(logger.log_dir, sizeof(char), logger.log_dir_size, fp);
        fread(logger.dump_dir, sizeof(char), logger.log_dir_size, fp);
        fread(&logger.signal_1, sizeof(int), 1, fp);
        fread(&logger.signal_2, sizeof(int), 1, fp);
        fclose(fp);
    }

    sigset_t set_log;
    sigfillset(&set_log);
    logger.sa_log.sa_sigaction = signal_handler_log;
    logger.sa_log.sa_mask = set_log;
    logger.sa_log.sa_flags = SA_SIGINFO;
    sigaction(signal_no_log, &logger.sa_log, NULL);
    logger.signal_1 = signal_no_log;

    sigset_t set_dump;
    sigfillset(&set_dump);
    logger.sa_dump.sa_sigaction = signal_handler_dump;
    logger.sa_dump.sa_mask = set_dump;
    logger.sa_dump.sa_flags = SA_SIGINFO;
    sigaction(signal_no_dump, &logger.sa_dump, NULL);
    logger.signal_2 = signal_no_dump;


    struct stat st = {0};
    if (stat(log_location, &st) == -1) {
        mkdir(log_location, ALL_PERMISSIONS);
    }


    if (stat(dump_location, &st) == -1) {
        mkdir(dump_location, ALL_PERMISSIONS);
    }

    sem_init(&SEMAPHORE_DUMP, 0, 0);
    sem_init(&SEMAPHORE_FILE, 0, 1);

    pthread_create(&logger.pthread_dump, NULL, dump_function, NULL);

    logger.is_init = true;
    printf("Successfully created and launched the logger program.\n");
    logger_log(HIGH, "Successfully created and launched the logger program.");

    return OK;
}

enum status_t logger_log(enum importance_t importance, char *format, ...) {
    if (logger.is_init == false) {
        printf("The logging library was not started using the init function!\n");
        return NOT_INIT;
    }
    if (importance < (enum importance_t) atomic_load(&logger.logger_detail)) {
        printf("The information will not be saved because of its low importance.\n");
        return TOO_LOW_IMPORTANCE;
    }

    sem_wait(&SEMAPHORE_FILE);
    time_t current_time;
    struct tm *timeinfo;
    time(&current_time);
    timeinfo = localtime(&current_time);

    FILE *fp = fopen(DIRECTORY_LOG, "a+");
    if (fp == NULL) {
        sem_post(&SEMAPHORE_FILE);
        return FILE_ERROR;
    }

    fprintf(fp, "[  %04d-%02d-%02d__%02d-%02d-%02d  ]      ", timeinfo->tm_year + 1900,
            timeinfo->tm_mon + 1, timeinfo->tm_mday,
            timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    va_list args;
    va_start (args, format);
    vfprintf(fp, format, args);
    va_end (args);
    fprintf(fp, "\n");
    fclose(fp);
    sem_post(&SEMAPHORE_FILE);

    return OK;
}

enum status_t logger_stop() {
    if (logger.is_init == false) {
        printf("Logger is already disabled!");
        return ALREADY_DISABLED;
    }

    logger.is_init = false;
    pthread_cancel(logger.pthread_dump);

    sem_destroy(&SEMAPHORE_DUMP);
    sem_destroy(&SEMAPHORE_FILE);

    free(DIRECTORY_LOG);
    free(DIRECTORY_DUMP);
    return OK;
}