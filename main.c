#include "client.h"
#include "server.h"
#include "const.h"

#include "log.h"

static struct mq_attr attr = {
        .mq_flags = 0,
        .mq_maxmsg = 10,
        .mq_curmsgs = 0,
        .mq_msgsize = sizeof(struct message_t)
};

int main(int argc, char **argv) {
    argv++;
    argc--;

    logger_init(NULL, "/home/jan/Downloads/SCR_Lab2/log", "/home/jan/Downloads/SCR_Lab2/dump", SIGUSR1, SIGUSR2, MAX);
    mqd_t test = mq_open(DIRECTION_MAIN_QUEUE, O_CREAT | O_EXCL | O_RDONLY, 0777, &attr);
    if (test == -1) {
        mq_close(test);
        start_client(argc, argv);

    } else {
        start_server(argc, argv);
    }
    logger_stop();

    return 0;
}
