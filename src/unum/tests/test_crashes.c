// (c) 2018 minim.co
// unum system tests common code

#include "unum.h"

// Compile only in debug version
#ifdef DEBUG

static int main_tread_id;
static int monitor_pid;
static int control_in_thread;

static volatile int tmp;
static void (*runme)(void) = 0;

static void invlaid_func(void) {
    asm(".long 0x0");
    printf("%s, asm(\".long 0x0\") executed!\n", __func__);
    return;
}

static void align_func(void) {
    printf("%s, called at %p\n", __func__, align_func);
    return;
}

static void sleeper_thread(int how_long)
{
    int tid = syscall(SYS_gettid);
    printf("Thread %d will sleep %d sec, then terminate\n",
           tid, how_long);
    sleep(how_long);
    printf("Thread %d done sleeping for %d sec, exiting\n",
           tid, how_long);
}

static void sleeper_thread_10(THRD_PARAM_t *p)
{
    sleeper_thread(10);
}

static void sleeper_thread_300(THRD_PARAM_t *p)
{
    sleeper_thread(300);
}

static void ctrl_thread(THRD_PARAM_t *p)
{
    int i, type = 0;
    char str[16];
    int tid = syscall(SYS_gettid);

repeat:

    for(;;)
    {
        printf("Cur thread:%d, parent:%d, main:%d, monitor:%d, test types:\n",
               tid, getppid(), main_tread_id, monitor_pid);
        printf("0 - call abort()\n");
        printf("1 - start 10 sec sleeper thread\n");
        printf("2 - start 300 sec sleeper thread\n");
        printf("3 - leave the thread sleeping and continue in a new one\n");
        printf("4 - return control back to main thread\n");
        printf("5 - read from address 0\n");
        printf("6 - write to address 0\n");
        printf("7 - execute instruction at address 0\n");
        printf("8 - divide by 0\n");
        printf("9 - execute invalid instruction\n");
        printf("10- misaligned instruction access\n");
        printf("11- misaligned data access\n");
        printf("12- kill -9 the main thread\n");
        printf("13- kill -9 the monitor process\n");
        printf("14- test util_system() call handling\n");
        printf("15- call util_restart() (restart agent)\n");
        printf("16- call util_shutdown(0) (terminate agent)\n");
        printf("17- call util_reboot() (reboot device)\n");

        printf("Enter the test #:\n");
        scanf("%02s", str);
        str[2] = 0;
        type = 0;
        sscanf(str, "%d", &type);

        if(type >= 0 && type <= 17) {
            break;
        }
        printf("\nUrecognized test type %d\n", type);
    }

    switch(type) {
        case 0:
            printf("Calling abort()...\n");
            abort();
            printf("abort() call has returned\n");
            break;
        case 1:
            printf("Calling util_start_thrd(sleeper_10)\n");
            i = util_start_thrd("sleeper_10", sleeper_thread_10, NULL, NULL);
            printf("util_start_thrd(sleeper_10) returned %d\n", i);
            break;
        case 2:
            printf("Calling util_start_thrd(sleeper_300)\n");
            i = util_start_thrd("sleeper_300", sleeper_thread_300, NULL, NULL);
            printf("util_start_thrd(sleeper_300) returned %d\n", i);
            break;
        case 3:
            if(control_in_thread) {
                printf("Control is not in main thread already\n");
                break;
            }
            printf("Moving control to new thread\n");
            i = util_start_thrd("control", ctrl_thread, NULL, NULL);
            printf("util_start_thrd(control) returned %d\n", i);
            if(i == 0) {
                control_in_thread = TRUE;
            }
            while(control_in_thread) {
                sleep(1);
            }
            break;
        case 4:
            if(!control_in_thread) {
                printf("Control is in main thread already\n");
                break;
            }
            control_in_thread = -1;
            break;
        case 5:
            printf("Reading from address 0\n");
            tmp = 0;
            i = *((int *)tmp);
            printf("No exception reading from address 0, got 0x%x\n", i);
            break;
        case 6:
            printf("Writing to address 0\n");
            tmp = 0;
            *((int *)0x00000000) = 0;
            printf("No exception writing to address 0\n");
            break;
        case 7:
            runme = NULL;
            printf("Execution at address %p\n", runme);
            runme();
            printf("Execution at address %p, no exception\n", runme);
            break;
        case 8:
            printf("Division by 0\n");
              tmp = 0;
            i = 100 / tmp;
            printf("Division result: 100/0 == %d, no exception\n", i);
            break;
        case 9:
            printf("Execute instruction 0x00000000\n");
              invlaid_func();
            printf("Execute instruction 0x00000000, no exception\n");
            break;
        case 10:
            printf("Misaligned instruction access\n");
            runme = (void *)(((char *)align_func) + 1);
            printf("Calling functon at %p\n", runme);
            runme();
            printf("Call at %p, no exception\n", runme);
            break;
        case 11:
            printf("Misaligned data access\n");
            tmp = 1;
            i = *((int *)(((char *)&tmp) + 1));
            printf("int access at %p, val %d, no exception\n",
                   ((char *)&tmp) + 1, i);
            break;
        case 12:
            printf("Killing the main thread\n");
            i = kill(main_tread_id, SIGKILL);
            printf("kill() returned %d\n", i);
        case 13:
            printf("Killing the monitor process\n");
            i = kill(monitor_pid, SIGKILL);
            printf("kill() returned %d\n", i);
            break;
        case 14:
            printf("Testing util_system(\"sleep 10\", 20)\n");
            i = util_system("sleep 10", 20, NULL);
            printf("util_system(\"sleep 10\", 20) returned %d\n", i);
            printf("Testing util_system(\"sleep 10\", 5)\n");
            i = util_system("sleep 10", 5, NULL);
            printf("util_system(\"sleep 10\", 5) returned %d\n", i);
            break;
        case 15:
            if(control_in_thread) {
                printf("The test mode does not fully support this test\n");
                printf("The test has to be run from the main thread\n");
                break;
            }
            printf("Calling util_restart()\n");
            util_restart(UNUM_START_REASON_TEST_RESTART);
            printf("util_restart() returned, no restart\n");
            break;
        case 16:
            if(control_in_thread) {
                printf("The test mode does not fully support this test\n");
                printf("The test has to be run from the main thread\n");
                break;
            }
            printf("Calling util_shutdown(0)\n");
            util_shutdown(0);
            printf("util_shutdown() returned, not terminated\n");
            break;
        case 17:
            printf("Calling util_reboot()\n");
            util_reboot();
            printf("util_reboot() returned, no reboot\n");
            break;
    }

    if(!control_in_thread) {
        printf("Main thread %d continuing after sleep(10)...\n", tid);
        sleep(10);
        goto repeat;
    } else if(control_in_thread < 0) {
        printf("Thread %d returning cotrol to main in 10 sec...\n", tid);
        sleep(10);
        control_in_thread = FALSE;
    } else {
        printf("Control in thread %d continuing after sleep(10)...\n", tid);
        sleep(10);
        goto repeat;
    }
}

// Test how the agent monitor process reacts to agent process
// exceptions and/or termination w/ restart/reboot/unknown requests.
// This is the test entry point.
void test_crash_handling(void)
{
    monitor_pid = getppid();
    main_tread_id = syscall(SYS_gettid);

    // Redirect stdin, stdout and stderr
    int fd;
    char *con = "/dev/console";
    fd = open(con, O_WRONLY);
    if(fd < 0) {
        log("%s: open(%s) for write has failed: %s\n",
            __func__, con, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if(dup2(fd, 2) < 0) {
        log("%s: dup2() for stderr failed: %s\n", __func__, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if(dup2(fd, 1) < 0) {
        log("%s: dup2() for stdout failed: %s\n", __func__, strerror(errno));
        exit(EXIT_FAILURE);
    }
    close(fd);
    fd = open(con, O_RDONLY);
    if(fd < 0) {
        log("%s: open(%s) for read has failed: %s\n",
            __func__, con, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if(dup2(fd, 0) < 0) {
        log("%s: dup2() for stdin failed: %s\n", __func__, strerror(errno));
        exit(EXIT_FAILURE);
    }
    close(fd);

    ctrl_thread(NULL);
}

#endif // DEBUG
