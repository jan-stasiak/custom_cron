#include "server.h"
#include "const.h"
#include <signal.h>
#include "log.h"

struct user_message_t {
    char pid[10];
    char flag[3];
    char sec[10];
    char min[10];
    char hour[10];
    char command[50];
};

struct cron_task_t {
    bool is_run;
    timer_t timer;
    struct itimerspec timer_spec;
    struct user_message_t message;
    struct cron_task_t *next_task;
};

struct cron_tab_t {
    struct cron_task_t *handle;
};

static struct mq_attr attr = {
        .mq_flags = 0,
        .mq_maxmsg = 10,
        .mq_curmsgs = 0,
        .mq_msgsize = sizeof(struct message_t)
};

void *run_task(void *args) {
    struct cron_task_t *task = (struct cron_task_t *) args;
    task->is_run = false;
    char *child_argv[] = {"sh", "-c", task->message.command, NULL};
    int pid = getpid();
    posix_spawn(&pid, "/bin/sh", NULL, NULL, child_argv, NULL);
    return NULL;
}

void delete_memory(struct cron_tab_t *cron_tab) {
    if (cron_tab != NULL) {
        if (cron_tab->handle != NULL) {
            struct cron_task_t *curr_task = cron_tab->handle;
            struct cron_task_t *next_task = NULL;
            while (1) {
                if (curr_task->next_task != NULL) {
                    next_task = curr_task->next_task;
                } else {
                    next_task = NULL;
                }
                free(curr_task);
                if (next_task == NULL) {
                    return;
                }
                curr_task = next_task;
            }
            free(cron_tab->handle);
        }
        free(cron_tab);
    }

}


int start_server(int argc, char **argv) {

    mqd_t queue = mq_open(DIRECTION_MAIN_QUEUE,
                          O_CREAT | O_RDONLY,
                          0777, &attr);

    Message message;
    struct cron_tab_t *cron_tab = (struct cron_tab_t *) malloc(1 * sizeof(struct cron_tab_t));
    if (cron_tab == NULL) {
        delete_memory(cron_tab);
        logger_log(HIGH, "MEMORY_ERROR");
        return -1;
    }
    cron_tab->handle = NULL;

    while (1) {
        mq_receive(queue, (char *) &message, sizeof(message), NULL);
        char *token;
        token = strtok(message.message, "_");

        struct user_message_t u_m;
        int counter = 0;
        while (token != NULL) {
            switch (counter) {
                case 0:
                    strcpy(u_m.pid, token);
                    break;
                case 1:
                    strcpy(u_m.hour, token);
                    break;
                case 2:
                    strcpy(u_m.min, token);
                    break;
                case 3:
                    strcpy(u_m.sec, token);
                    break;
                case 4:
                    strcpy(u_m.flag, token);
                    break;
                case 5:
                    strcpy(u_m.command, token);
            }
            token = strtok(NULL, "_");
            counter++;
        }
        if (strcmp(u_m.flag, "-s") == 0) {
            mq_close(queue);
            mq_unlink("/test_q4");
            delete_memory(cron_tab);
            return 0;
        } else if (strcmp(u_m.flag, "-d") == 0) {
            struct cron_task_t *curr_task;
            curr_task = cron_tab->handle;
            while (1) {
                if (strcmp(curr_task->message.pid, u_m.pid) == 0) {
                    timer_delete(curr_task->timer);
                    curr_task->is_run = false;
                }
                if (curr_task->next_task == NULL) {
                    break;
                }
                curr_task = curr_task->next_task;
            }

        } else if (strcmp(u_m.flag, "-t") == 0) {
            char mem_name[20];
            char sem_name[20];
            sprintf(mem_name, "/mem_%s", u_m.pid);
            sprintf(sem_name, "/sem_%s", u_m.pid);

            sem_t *sem = sem_open(sem_name, 0);
            if (sem == SEM_FAILED) {
                logger_log(HIGH, "SEM_ERROR");
                delete_memory(cron_tab);
                return -1;
            }

            int fd = shm_open(mem_name, O_RDWR, 0777);
            if (fd == -1) {
                logger_log(HIGH, "SHM_MEMORY_ERROR_1");
                sem_close(sem);
                delete_memory(cron_tab);
                return -2;
            }

            struct cron_table_info *tasks = (struct cron_table_info *) mmap(NULL, sizeof(struct cron_table_info),
                                                                            PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
            if (tasks == NULL) {
                logger_log(HIGH, "SHM_MEMORY_ERROR_2");
                sem_close(sem);
                close(fd);
                delete_memory(cron_tab);
                return -2;
            }

            struct cron_task_t *task = cron_tab->handle;
            if (task == NULL) {
                sem_wait(&tasks->mutex);
                char one_task[120];
                sprintf(one_task, "%s", "There are no tasks\n");
                strcpy(tasks->tasks, one_task);
                tasks->is_next = false;
                sem_post(&tasks->mutex);
                sem_post(sem);
                usleep(100);
            } else {
                int i = 1;
                while (task != NULL) {
                    sem_wait(&tasks->mutex);
                    while (task->is_run == false && task->next_task != NULL) task = task->next_task;
                    if (task->is_run == true) {
                        char one_task[120];
                        sprintf(one_task, "%d. %s %s %s %s %s\n", i++, task->message.pid, task->message.hour,
                                task->message.min, task->message.sec, task->message.command);
                        strcpy(tasks->tasks, one_task);
                    }
                    task = task->next_task;
                    if (task != NULL) {
                        tasks->is_next = true;
                    }
                    sem_post(&tasks->mutex);
                    sem_post(sem);
                    usleep(100);
                }
            }

            sem_post(&tasks->mutex);
            munmap(tasks, sizeof(struct cron_table_info));
            close(fd);
            sem_close(sem);
        } else {
            if (cron_tab->handle == NULL) {
                struct cron_task_t *new_task = malloc(1 * sizeof(struct cron_task_t));
                if (new_task == NULL) {
                    logger_log(HIGH, "MEMORY_ERROR");
                    delete_memory(cron_tab);
                    return -3;
                }
                strcpy(new_task->message.pid, u_m.pid);
                strcpy(new_task->message.command, u_m.command);
                strcpy(new_task->message.flag, u_m.flag);
                strcpy(new_task->message.sec, u_m.sec);
                strcpy(new_task->message.min, u_m.min);
                strcpy(new_task->message.hour, u_m.hour);
                new_task->is_run = true;
                new_task->next_task = NULL;
                struct sigevent sigevent;
                sigevent.sigev_notify = SIGEV_THREAD;
                sigevent.sigev_notify_function = run_task;
                sigevent.sigev_value.sival_ptr = (void *) new_task;
                sigevent.sigev_notify_attributes = NULL;

                long sec = strtol(new_task->message.sec, NULL, 10);
                long min = strtol(new_task->message.min, NULL, 10);
                long hour = strtol(new_task->message.hour, NULL, 10);

                if (strcmp(new_task->message.flag, "-r") == 0) {
                    timer_create(CLOCK_REALTIME, &sigevent, &new_task->timer);
                    new_task->timer_spec.it_value.tv_sec = sec + (min * 60) + (hour * 60 * 60);
                    new_task->timer_spec.it_value.tv_nsec = 0;
                    new_task->timer_spec.it_interval.tv_sec = 0;
                    new_task->timer_spec.it_interval.tv_nsec = 0;

                    timer_settime(new_task->timer, 0, &new_task->timer_spec, NULL);
                } else if (strcmp(new_task->message.flag, "-a") == 0) {
                    struct timespec current_time;
                    clock_gettime(CLOCK_REALTIME, &current_time);
                    timer_create(CLOCK_REALTIME, &sigevent, &new_task->timer);
                    new_task->timer_spec.it_value.tv_sec = current_time.tv_sec + sec + (min * 60) + (hour * 60 * 60);
                    new_task->timer_spec.it_value.tv_nsec = current_time.tv_nsec;
                    new_task->timer_spec.it_interval.tv_sec = 0;
                    new_task->timer_spec.it_interval.tv_nsec = 0;

                    timer_settime(new_task->timer, TIMER_ABSTIME, &new_task->timer_spec, NULL);
                } else if (strcmp(new_task->message.flag, "-i") == 0) {
                    timer_create(CLOCK_REALTIME, &sigevent, &new_task->timer);
                    new_task->timer_spec.it_value.tv_sec = sec + (min * 60) + (hour * 60 * 60);
                    new_task->timer_spec.it_value.tv_nsec = 0;
                    new_task->timer_spec.it_interval.tv_sec = sec + (min * 60) + (hour * 60 * 60);
                    new_task->timer_spec.it_interval.tv_nsec = 0;

                    timer_settime(new_task->timer, 0, &new_task->timer_spec, NULL);
                }
                cron_tab->handle = new_task;
            } else {
                struct cron_task_t *curr_task;
                curr_task = cron_tab->handle;
                while (curr_task->next_task != NULL) {
                    curr_task = curr_task->next_task;
                }
                struct cron_task_t *new_task = malloc(1 * sizeof(struct cron_task_t));
                if (new_task == NULL) {
                    delete_memory(cron_tab);
                    logger_log(HIGH, "MEMORY_ERROR");
                    return -3;
                }
                strcpy(new_task->message.pid, u_m.pid);
                strcpy(new_task->message.command, u_m.command);
                strcpy(new_task->message.flag, u_m.flag);
                strcpy(new_task->message.sec, u_m.sec);
                strcpy(new_task->message.min, u_m.min);
                strcpy(new_task->message.hour, u_m.hour);
                new_task->is_run = true;
                new_task->next_task = NULL;
                curr_task->next_task = new_task;
                struct sigevent sigevent;
                sigevent.sigev_notify = SIGEV_THREAD;
                sigevent.sigev_notify_function = run_task;
                sigevent.sigev_value.sival_ptr = (void *) new_task;
                sigevent.sigev_notify_attributes = NULL;

                long sec = strtol(new_task->message.sec, NULL, 10);
                long min = strtol(new_task->message.min, NULL, 10);
                long hour = strtol(new_task->message.hour, NULL, 10);
                if (strcmp(new_task->message.flag, "-r") == 0) {
                    timer_create(CLOCK_REALTIME, &sigevent, &new_task->timer);
                    new_task->timer_spec.it_value.tv_sec = sec + (min * 60) + (hour * 60 * 60);
                    new_task->timer_spec.it_value.tv_nsec = 0;
                    new_task->timer_spec.it_interval.tv_sec = 0;
                    new_task->timer_spec.it_interval.tv_nsec = 0;

                    timer_settime(new_task->timer, 0, &new_task->timer_spec, NULL);
                } else if (strcmp(new_task->message.flag, "-a") == 0) {
                    struct timespec current_time;
                    clock_gettime(CLOCK_REALTIME, &current_time);
                    timer_create(CLOCK_REALTIME, &sigevent, &new_task->timer);
                    new_task->timer_spec.it_value.tv_sec = current_time.tv_sec + sec + (min * 60) + (hour * 60 * 60);
                    new_task->timer_spec.it_value.tv_nsec = current_time.tv_nsec;
                    new_task->timer_spec.it_interval.tv_sec = 0;
                    new_task->timer_spec.it_interval.tv_nsec = 0;
                    timer_settime(new_task->timer, TIMER_ABSTIME, &new_task->timer_spec, NULL);
                } else if (strcmp(new_task->message.flag, "-i") == 0) {
                    timer_create(CLOCK_REALTIME, &sigevent, &new_task->timer);
                    new_task->timer_spec.it_value.tv_sec = sec + (min * 60) + (hour * 60 * 60);
                    new_task->timer_spec.it_value.tv_nsec = 0;
                    new_task->timer_spec.it_interval.tv_sec = sec + (min * 60) + (hour * 60 * 60);
                    new_task->timer_spec.it_interval.tv_nsec = 0;
                    timer_settime(new_task->timer, 0, &new_task->timer_spec, NULL);
                }
            }
        }
    }

    mq_close(queue);
    mq_unlink(DIRECTION_MAIN_QUEUE);
    delete_memory(cron_tab);
    return 0;
}