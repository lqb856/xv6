
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int p[2];
  if (pipe(p) != 0) {
    printf("failed to create pipe!\n");
    exit(1);
  }

  // 一次调用，两次返回。父进程返回子进程的进程号，子进程返回 0
  int child = fork();
  if (child == -1) {
    printf("failed to fork!\n");
    exit(1);
  }

  char buf[10];
  if (child == 0) {
    read(p[0], buf, 5);
    close(p[0]);
    printf("%d: received ping\n", getpid());

    write(p[1], "pong\0", 5);
    close(p[1]);
  } else {
    write(p[1], "ping\0", 5);
    close(p[1]);

    read(p[0], buf, 5);
    close(p[0]);
    printf("%d: received pong\n", getpid());
  }
  exit(0);
}