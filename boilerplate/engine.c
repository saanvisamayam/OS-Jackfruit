#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/ioctl.h>

// ================= IOCTL STRUCT =================
struct monitor_request {
    pid_t pid;
    unsigned long soft;
    unsigned long hard;
};

#define IOCTL_ADD _IOW('a', 'a', struct monitor_request *)

#define STACK_SIZE (1024 * 1024)

// ================= CONTAINER STRUCT =================
typedef struct {
    char id[32];
    pid_t pid;
    char status[32];
} container_t;

container_t containers[10];
int container_count = 0;

// ================= LOGGING =================
void *log_writer(void *arg) {
    int fd = *(int *)arg;
    char buffer[256];

    while (1) {
        int n = read(fd, buffer, sizeof(buffer));
        if (n <= 0) break;

        write(STDOUT_FILENO, buffer, n);
    }

    return NULL;
}

// ================= CHILD =================
int child_func(void *arg) {
    char **args = (char **)arg;

    chroot(args[0]);
    chdir("/");

execl(args[1], args[1], NULL);
    return 0;
}

// ================= START CONTAINER =================
void start_container(char *id, char *rootfs, char *cmd,
                     int soft_mib, int hard_mib)
{
    int pipefd[2];
    pipe(pipefd);

    char *stack = malloc(STACK_SIZE);

    char *args[] = {rootfs, cmd, NULL};

pid_t pid = clone(
    child_func,
    stack + STACK_SIZE,
    CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD,
    args
);
    if (pid < 0) {
        perror("clone failed");
        return;
    }

    // ================= REGISTER WITH KERNEL =================
    int fd = open("/dev/container_monitor", O_RDWR);

    if (fd < 0) {
        perror("open monitor");
    } else {
        struct monitor_request req;
        req.pid = pid;
        req.soft = soft_mib;
        req.hard = hard_mib;

        if (ioctl(fd, IOCTL_ADD, &req) < 0) {
            perror("ioctl failed");
        } else {
            printf("[engine] Registered PID %d with monitor\n", pid);
        }

        close(fd);
    }

    // ================= SAVE METADATA =================
    strcpy(containers[container_count].id, id);
    containers[container_count].pid = pid;
    strcpy(containers[container_count].status, "running");
    container_count++;

    printf("Started container %s (PID %d)\n", id, pid);
}

// ================= LIST =================
void list_containers() {
    for (int i = 0; i < container_count; i++) {
        printf("ID:%s PID:%d Status:%s\n",
               containers[i].id,
               containers[i].pid,
               containers[i].status);
    }
}

// ================= MAIN =================
// ================= MAIN =================
int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage:\n");
        printf("./engine supervisor <rootfs>\n");
        printf("./engine start <id> <rootfs> <cmd> [--soft-mib N] [--hard-mib N]\n");
        printf("./engine ps\n");
        return 0;
    }

    // ================= SUPERVISOR =================
    if (strcmp(argv[1], "supervisor") == 0) {
        printf("[engine] Supervisor running...\n");

        // keep running forever
        while (1) {
            sleep(5);
        }
    }

    // ================= START =================
    else if (strcmp(argv[1], "start") == 0) {
        char *id = argv[2];
        char *rootfs = argv[3];
        char *cmd = argv[4];

        int soft = 40;
        int hard = 64;

        for (int i = 5; i < argc; i++) {
            if (strcmp(argv[i], "--soft-mib") == 0)
                soft = atoi(argv[++i]);
            else if (strcmp(argv[i], "--hard-mib") == 0)
                hard = atoi(argv[++i]);
        }

        start_container(id, rootfs, cmd, soft, hard);

        // keep alive briefly so you can screenshot
        sleep(20);
    }

    // ================= PS =================
    else if (strcmp(argv[1], "ps") == 0) {
        list_containers();
    }

    return 0;
}
