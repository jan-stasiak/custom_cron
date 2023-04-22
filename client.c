#include "client.h"
#include "const.h"

#include "log.h"


int start_client(int argc, char **argv) {
    struct mq_attr attr = {
            .mq_flags = 0,
            .mq_maxmsg = 10,
            .mq_curmsgs = 0,
            .mq_msgsize = sizeof(struct message_t)
    };

    mqd_t queue = mq_open(DIRECTION_MAIN_QUEUE, O_CREAT | O_WRONLY, 0777, &attr);
    Message message = {
            .message = {'\0'}
    };

    if (argc == 1) {
        if (strcmp(argv[0], "-s") == 0) {
            sprintf(message.message, "%d_%d_%d_%d_%s_%s", getpid(), 0, 0, 0, "-s", "None");
            mq_send(queue, (char *) &message, sizeof(message), 1);
            mq_close(queue);
        } else if (strcmp(argv[0], "-t") == 0) {
            sprintf(message.message, "%d_%d_%d_%d_%s_%s", getpid(), 0, 0, 0, "-t", "None");
            char mem_name[20];
            char sem_name[20];
            sprintf(mem_name, "/mem_%d", getpid());
            sprintf(sem_name, "/sem_%d", getpid());
            sem_t *sem = sem_open(sem_name, O_CREAT | O_RDWR, 0777, 0);
            if (sem == SEM_FAILED) {
                logger_log(HIGH, "SEM_ERROR");
                return -2;
            }

            int fd = shm_open(mem_name, O_CREAT | O_RDWR, 0777);
            if (fd == -1) {
                sem_close(sem);
                sem_unlink(sem_name);
                logger_log(HIGH, "MEMORY_ERROR_1");
                return -2;
            }
            ftruncate(fd, sizeof(struct cron_table_info));
            struct cron_table_info *tasks = mmap(0, sizeof(struct cron_table_info), PROT_READ | PROT_WRITE, MAP_SHARED,
                                                 fd, 0);
            if (tasks == NULL) {
                close(fd);
                sem_close(sem);
                sem_unlink(sem_name);
                logger_log(HIGH, "SEM_ERROR_2");
                return -3;
            }

            sem_init(&tasks->mutex, 1, 1);
            tasks->is_next = false;
            mq_send(queue, (char *) &message, sizeof(message), 1);
            mq_close(queue);

            while (1) {
                sem_wait(sem);
                sem_wait(&tasks->mutex);
                if (tasks->is_next == false) {
                    break;
                }
                tasks->is_next = false;
                sem_post(&tasks->mutex);
            }

            munmap(tasks, sizeof(struct cron_table_info));
            close(fd);
            shm_unlink(mem_name);
            sem_close(sem);
            sem_unlink(sem_name);
        }
    } else if (argc == 2) {
        if (strcmp(argv[1], "-d") == 0) {
            sprintf(message.message, "%s_%d_%d_%d_%s_%s", argv[0], 0, 0, 0, "-d", "None");
            mq_send(queue, (char *) &message, sizeof(message), 1);
            mq_close(queue);
        }
    } else {
        sprintf(message.message, "%d_%s_%s_%s_%s_%s", getpid(), argv[0], argv[1], argv[2], argv[3], argv[4]);
        mq_send(queue, (char *) &message, sizeof(message), 1);
        mq_close(queue);
    }
    mq_close(queue);
    return 0;
}
