/*
 * dactyl-watch — inotify-based file watcher for dactyl.clj hot reload
 *
 * Usage: dactyl-watch [path-to-watch] [command...]
 * Default: dactyl-watch src/dactyl_keyboard/dactyl.clj lein exec -p
 * src/dactyl_keyboard/dactyl.clj
 *
 * Build: cc -O2 -o dactyl-watch dactyl-watch.c
 */

#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define EVENT_BUF_LEN (sizeof(struct inotify_event) + NAME_MAX + 1)
#define DEBOUNCE_MS 300

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t child_pid = 0;

static void handle_signal(int sig) {
  (void)sig;
  running = 0;
  /* forward to child if running */
  if (child_pid > 0)
    kill(child_pid, SIGTERM);
}

static long elapsed_ms(struct timespec *start) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (now.tv_sec - start->tv_sec) * 1000 +
         (now.tv_nsec - start->tv_nsec) / 1000000;
}

static void run_command(char **argv) {
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return;
  }
  if (pid == 0) {
    execvp(argv[0], argv);
    perror("execvp");
    _exit(127);
  }

  child_pid = pid;
  int status;
  waitpid(pid, &status, 0);
  child_pid = 0;

  if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
    fprintf(stderr, "\033[31m✗ command exited with %d\033[0m\n",
            WEXITSTATUS(status));
  else if (WIFEXITED(status))
    fprintf(stderr, "\033[32m✓ done\033[0m\n");
  else if (WIFSIGNALED(status))
    fprintf(stderr, "\033[33m⚠ killed by signal %d\033[0m\n", WTERMSIG(status));
}

/* drain all pending inotify events without blocking */
static void drain_events(int fd) {
  char buf[EVENT_BUF_LEN * 8];
  for (;;) {
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n <= 0)
      break;
  }
}

int main(int argc, char **argv) {
  const char *watch_path = "src/dactyl_keyboard/dactyl.clj";
  char **cmd_argv;
  char *default_cmd[] = {"lein", "exec", "-p", "src/dactyl_keyboard/dactyl.clj",
                         NULL};

  if (argc >= 3) {
    watch_path = argv[1];
    cmd_argv = &argv[2];
  } else if (argc == 2) {
    watch_path = argv[1];
    cmd_argv = default_cmd;
  } else {
    cmd_argv = default_cmd;
  }

  /* verify target exists */
  if (access(watch_path, F_OK) != 0) {
    fprintf(stderr, "error: '%s' does not exist\n", watch_path);
    return 1;
  }

  struct sigaction sa = {.sa_handler = handle_signal};
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  int fd = inotify_init1(IN_NONBLOCK);
  if (fd < 0) {
    perror("inotify_init1");
    return 1;
  }

  /* watch for writes and moves (editors often write to tmp then rename) */
  int wd = inotify_add_watch(fd, watch_path,
                             IN_MODIFY | IN_CLOSE_WRITE | IN_MOVE_SELF);
  if (wd < 0) {
    perror("inotify_add_watch");
    close(fd);
    return 1;
  }

  fprintf(stderr, "\033[1m👁 watching: %s\033[0m\n", watch_path);
  fprintf(stderr, "\033[1m⚡ command:");
  for (char **p = cmd_argv; *p; p++)
    fprintf(stderr, " %s", *p);
  fprintf(stderr, "\033[0m\n");
  fprintf(stderr, "press Ctrl-C to stop\n\n");

  /* run once on startup */
  fprintf(stderr, "\033[36m→ initial build...\033[0m\n");
  run_command(cmd_argv);

  struct timespec last_trigger;
  clock_gettime(CLOCK_MONOTONIC, &last_trigger);

  while (running) {
    char buf[EVENT_BUF_LEN * 4];
    ssize_t len = read(fd, buf, sizeof(buf));

    if (len < 0) {
      if (errno == EAGAIN) {
        /* no events, sleep a bit */
        usleep(50000); /* 50ms poll interval */
        continue;
      }
      if (errno == EINTR)
        continue;
      perror("read");
      break;
    }

    /* debounce: skip if too recent */
    if (elapsed_ms(&last_trigger) < DEBOUNCE_MS) {
      drain_events(fd);
      continue;
    }

    clock_gettime(CLOCK_MONOTONIC, &last_trigger);
    drain_events(fd); /* clear any queued events */

    fprintf(stderr, "\033[36m→ file changed, rebuilding...\033[0m\n");
    run_command(cmd_argv);

    /* re-add watch in case of move/recreate (vim does this) */
    inotify_rm_watch(fd, wd);
    wd = inotify_add_watch(fd, watch_path,
                           IN_MODIFY | IN_CLOSE_WRITE | IN_MOVE_SELF);
    if (wd < 0 && running) {
      fprintf(stderr, "file disappeared, waiting for it to come back...\n");
      while (running) {
        usleep(500000);
        wd = inotify_add_watch(fd, watch_path,
                               IN_MODIFY | IN_CLOSE_WRITE | IN_MOVE_SELF);
        if (wd >= 0) {
          fprintf(stderr, "file is back, watching again\n");
          break;
        }
      }
    }
  }

  fprintf(stderr, "\n\033[1mstopped\033[0m\n");
  close(fd);
  return 0;
}
