/*
 * tests/test_mpi.c
 *
 * Userspace tests for the MPI kernel syscalls (243-246).
 * Must be compiled and run on the patched Linux 2.4 i386 kernel.
 *
 * Compile: gcc -o test_mpi test_mpi.c -Wall
 * Run:     ./test_mpi
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include "../mpi_api.h"

#define MPI_GID 42
#define BUF_SIZE 256

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { printf("  TEST: %-50s ", name); tests_run++; } while(0)

#define PASS() \
    do { printf("[PASS]\n"); tests_passed++; } while(0)

#define FAIL(msg) \
    do { printf("[FAIL] %s\n", msg); } while(0)

/* ------------------------------------------------------------------ */
/* Test 1: register / unregister returns 0                            */
/* ------------------------------------------------------------------ */
static void test_register_unregister(void)
{
    int ret;

    TEST("mpi_register returns 0");
    ret = mpi_register(MPI_GID);
    if (ret == 0)
        PASS();
    else
        FAIL("mpi_register failed");

    TEST("mpi_register idempotent (second call returns 0)");
    ret = mpi_register(MPI_GID);
    if (ret == 0)
        PASS();
    else
        FAIL("second mpi_register failed");

    TEST("mpi_unregister returns 0");
    ret = mpi_unregister(MPI_GID);
    if (ret == 0)
        PASS();
    else
        FAIL("mpi_unregister failed");

    TEST("mpi_unregister when not registered returns 0");
    ret = mpi_unregister(MPI_GID);
    if (ret == 0)
        PASS();
    else
        FAIL("unregister on unregistered process failed");
}

/* ------------------------------------------------------------------ */
/* Test 2: send/receive requires registration                         */
/* ------------------------------------------------------------------ */
static void test_send_receive_unregistered(void)
{
    char buf[BUF_SIZE];
    int ret;

    TEST("mpi_send fails when sender not registered");
    ret = mpi_send(getpid(), "hello", 5);
    if (ret == -1 && (errno == EPERM || errno == ESRCH))
        PASS();
    else
        FAIL("expected EPERM or ESRCH");

    TEST("mpi_receive fails when not registered");
    ret = mpi_receive(getpid(), buf, BUF_SIZE);
    if (ret == -1 && errno == EPERM)
        PASS();
    else
        FAIL("expected EPERM");
}

/* ------------------------------------------------------------------ */
/* Test 3: send with invalid args                                     */
/* ------------------------------------------------------------------ */
static void test_send_invalid_args(void)
{
    int ret;

    mpi_register(MPI_GID);

    TEST("mpi_send with NULL message returns EINVAL");
    ret = mpi_send(getpid(), NULL, 10);
    if (ret == -1 && errno == EINVAL)
        PASS();
    else
        FAIL("expected EINVAL");

    TEST("mpi_send with size 0 returns EINVAL");
    ret = mpi_send(getpid(), "hello", 0);
    if (ret == -1 && errno == EINVAL)
        PASS();
    else
        FAIL("expected EINVAL");

    TEST("mpi_send with negative size returns EINVAL");
    ret = mpi_send(getpid(), "hello", -1);
    if (ret == -1 && errno == EINVAL)
        PASS();
    else
        FAIL("expected EINVAL");

    TEST("mpi_send to nonexistent PID returns ESRCH");
    ret = mpi_send(99999, "hello", 5);
    if (ret == -1 && errno == ESRCH)
        PASS();
    else
        FAIL("expected ESRCH");

    mpi_unregister(MPI_GID);
}

/* ------------------------------------------------------------------ */
/* Test 4: receive with no pending message returns EAGAIN              */
/* ------------------------------------------------------------------ */
static void test_receive_empty_queue(void)
{
    char buf[BUF_SIZE];
    int ret;

    mpi_register(MPI_GID);

    TEST("mpi_receive with no messages returns EAGAIN");
    ret = mpi_receive(getpid(), buf, BUF_SIZE);
    if (ret == -1 && errno == EAGAIN)
        PASS();
    else
        FAIL("expected EAGAIN");

    mpi_unregister(MPI_GID);
}

/* ------------------------------------------------------------------ */
/* Test 5: receive with invalid args                                  */
/* ------------------------------------------------------------------ */
static void test_receive_invalid_args(void)
{
    int ret;

    mpi_register(MPI_GID);

    TEST("mpi_receive with NULL buffer returns EINVAL");
    ret = mpi_receive(getpid(), NULL, BUF_SIZE);
    if (ret == -1 && errno == EINVAL)
        PASS();
    else
        FAIL("expected EINVAL");

    TEST("mpi_receive with size 0 returns EINVAL");
    ret = mpi_receive(getpid(), (char *)1, 0);
    if (ret == -1 && errno == EINVAL)
        PASS();
    else
        FAIL("expected EINVAL");

    mpi_unregister(MPI_GID);
}

/* ------------------------------------------------------------------ */
/* Test 6: unregister with wrong gid is a no-op                       */
/* ------------------------------------------------------------------ */
static void test_unregister_wrong_gid(void)
{
    char buf[BUF_SIZE];
    int ret;

    mpi_register(MPI_GID);

    TEST("mpi_unregister with wrong gid keeps registration");
    ret = mpi_unregister(MPI_GID + 999);
    if (ret == 0) {
        /* Still registered — receive should give EAGAIN, not EPERM */
        ret = mpi_receive(getpid(), buf, BUF_SIZE);
        if (ret == -1 && errno == EAGAIN)
            PASS();
        else
            FAIL("process was unexpectedly unregistered");
    } else {
        FAIL("mpi_unregister returned error");
    }

    TEST("mpi_unregister with -1 always unregisters");
    ret = mpi_unregister(-1);
    if (ret == 0) {
        ret = mpi_receive(getpid(), buf, BUF_SIZE);
        if (ret == -1 && errno == EPERM)
            PASS();
        else
            FAIL("process should be unregistered");
    } else {
        FAIL("mpi_unregister(-1) returned error");
    }
}

/* ------------------------------------------------------------------ */
/* Test 7: parent-child send/receive (fork-based)                     */
/* ------------------------------------------------------------------ */
static void test_parent_child_messaging(void)
{
    pid_t child, parent;
    int status;
    int pipe_fd[2];

    parent = getpid();

    /* Use a pipe to synchronize parent and child */
    if (pipe(pipe_fd) < 0) {
        perror("pipe");
        return;
    }

    mpi_register(MPI_GID);

    child = fork();
    if (child < 0) {
        perror("fork");
        return;
    }

    if (child == 0) {
        /* --- Child process --- */
        char buf[BUF_SIZE];
        char ready = 'R';
        int ret;

        close(pipe_fd[0]); /* close read end */

        mpi_register(MPI_GID);

        /* Tell parent we are registered */
        write(pipe_fd[1], &ready, 1);
        close(pipe_fd[1]);

        /* Give parent time to send */
        usleep(100000);

        ret = mpi_receive(parent, buf, BUF_SIZE);
        if (ret > 0 && strncmp(buf, "hello from parent", ret) == 0)
            _exit(0); /* success */
        else
            _exit(1); /* failure */
    }

    /* --- Parent process --- */
    {
        char ready;
        close(pipe_fd[1]); /* close write end */

        /* Wait for child to register */
        read(pipe_fd[0], &ready, 1);
        close(pipe_fd[0]);

        TEST("mpi_send from parent to child succeeds");
        if (mpi_send(child, "hello from parent", 17) == 0)
            PASS();
        else
            FAIL("mpi_send failed");

        waitpid(child, &status, 0);

        TEST("child receives correct message from parent");
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            PASS();
        else
            FAIL("child did not receive expected message");
    }

    mpi_unregister(MPI_GID);
}

/* ------------------------------------------------------------------ */
/* Test 8: cross-group send should fail                               */
/* ------------------------------------------------------------------ */
static void test_cross_group_send_fails(void)
{
    pid_t child;
    int status;
    int pipe_fd[2];

    if (pipe(pipe_fd) < 0) {
        perror("pipe");
        return;
    }

    mpi_register(MPI_GID);

    child = fork();
    if (child < 0) {
        perror("fork");
        return;
    }

    if (child == 0) {
        char ready = 'R';
        close(pipe_fd[0]);

        /* Register with a DIFFERENT group */
        mpi_register(MPI_GID + 1);

        write(pipe_fd[1], &ready, 1);
        close(pipe_fd[1]);

        /* Keep alive for parent to try sending */
        usleep(200000);
        mpi_unregister(MPI_GID + 1);
        _exit(0);
    }

    {
        char ready;
        int ret;
        close(pipe_fd[1]);

        read(pipe_fd[0], &ready, 1);
        close(pipe_fd[0]);

        TEST("mpi_send to different group returns EPERM");
        ret = mpi_send(child, "cross group", 11);
        if (ret == -1 && errno == EPERM)
            PASS();
        else
            FAIL("expected EPERM for cross-group send");

        waitpid(child, &status, 0);
    }

    mpi_unregister(MPI_GID);
}

/* ------------------------------------------------------------------ */
/* Test 9: multiple messages, FIFO ordering                           */
/* ------------------------------------------------------------------ */
static void test_fifo_ordering(void)
{
    pid_t child, parent;
    int status;
    int pipe_fd[2];

    parent = getpid();

    if (pipe(pipe_fd) < 0) {
        perror("pipe");
        return;
    }

    mpi_register(MPI_GID);

    child = fork();
    if (child < 0) {
        perror("fork");
        return;
    }

    if (child == 0) {
        char buf[BUF_SIZE];
        char ready = 'R';
        int ok = 1;

        close(pipe_fd[0]);
        mpi_register(MPI_GID);
        write(pipe_fd[1], &ready, 1);
        close(pipe_fd[1]);

        usleep(200000);

        /* Should receive "msg1" first, then "msg2" */
        if (mpi_receive(parent, buf, BUF_SIZE) != 4 ||
            strncmp(buf, "msg1", 4) != 0)
            ok = 0;
        if (mpi_receive(parent, buf, BUF_SIZE) != 4 ||
            strncmp(buf, "msg2", 4) != 0)
            ok = 0;

        mpi_unregister(MPI_GID);
        _exit(ok ? 0 : 1);
    }

    {
        char ready;
        close(pipe_fd[1]);
        read(pipe_fd[0], &ready, 1);
        close(pipe_fd[0]);

        mpi_send(child, "msg1", 4);
        mpi_send(child, "msg2", 4);

        waitpid(child, &status, 0);

        TEST("messages arrive in FIFO order");
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            PASS();
        else
            FAIL("FIFO order not preserved");
    }

    mpi_unregister(MPI_GID);
}

/* ------------------------------------------------------------------ */
/* Test 10: receive truncates to buffer size                          */
/* ------------------------------------------------------------------ */
static void test_receive_truncation(void)
{
    pid_t child, parent;
    int status;
    int pipe_fd[2];

    parent = getpid();

    if (pipe(pipe_fd) < 0) {
        perror("pipe");
        return;
    }

    mpi_register(MPI_GID);

    child = fork();
    if (child < 0) {
        perror("fork");
        return;
    }

    if (child == 0) {
        char buf[4]; /* smaller than the message */
        char ready = 'R';
        int ret;

        close(pipe_fd[0]);
        mpi_register(MPI_GID);
        write(pipe_fd[1], &ready, 1);
        close(pipe_fd[1]);

        usleep(100000);

        ret = mpi_receive(parent, buf, 4);
        /* Should get only 4 bytes of "hello world" */
        if (ret == 4 && strncmp(buf, "hell", 4) == 0)
            _exit(0);
        else
            _exit(1);
    }

    {
        char ready;
        close(pipe_fd[1]);
        read(pipe_fd[0], &ready, 1);
        close(pipe_fd[0]);

        mpi_send(child, "hello world", 11);

        waitpid(child, &status, 0);

        TEST("receive truncates to buffer size");
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            PASS();
        else
            FAIL("truncation not working correctly");
    }

    mpi_unregister(MPI_GID);
}

/* ------------------------------------------------------------------ */
/* Test 11: receive filters by sender PID                             */
/* ------------------------------------------------------------------ */
static void test_receive_filters_by_pid(void)
{
    pid_t child1, child2, parent;
    int status1, status2;
    int pipe1[2], pipe2[2];

    parent = getpid();

    if (pipe(pipe1) < 0 || pipe(pipe2) < 0)
        return;

    mpi_register(MPI_GID);

    child1 = fork();
    if (child1 == 0) {
        char ready = 'R';
        close(pipe1[0]);
        mpi_register(MPI_GID);
        mpi_send(parent, "from_c1", 7);
        write(pipe1[1], &ready, 1);
        close(pipe1[1]);
        usleep(200000);
        mpi_unregister(MPI_GID);
        _exit(0);
    }

    child2 = fork();
    if (child2 == 0) {
        char ready = 'R';
        close(pipe2[0]);
        mpi_register(MPI_GID);
        mpi_send(parent, "from_c2", 7);
        write(pipe2[1], &ready, 1);
        close(pipe2[1]);
        usleep(200000);
        mpi_unregister(MPI_GID);
        _exit(0);
    }

    {
        char ready, buf[BUF_SIZE];
        int ret;

        close(pipe1[1]); close(pipe2[1]);
        read(pipe1[0], &ready, 1);
        read(pipe2[0], &ready, 1);
        close(pipe1[0]); close(pipe2[0]);

        TEST("receive filters messages by sender PID");
        ret = mpi_receive(child2, buf, BUF_SIZE);
        if (ret == 7 && strncmp(buf, "from_c2", 7) == 0)
            PASS();
        else
            FAIL("did not get message from child2");

        TEST("message from child1 still in queue");
        ret = mpi_receive(child1, buf, BUF_SIZE);
        if (ret == 7 && strncmp(buf, "from_c1", 7) == 0)
            PASS();
        else
            FAIL("did not get message from child1");

        waitpid(child1, &status1, 0);
        waitpid(child2, &status2, 0);
    }

    mpi_unregister(MPI_GID);
}

/* ================================================================== */
int main(void)
{
    printf("=== MPI Kernel Syscall Tests ===\n\n");

    printf("[Single-process tests]\n");
    test_register_unregister();
    test_send_receive_unregistered();
    test_send_invalid_args();
    test_receive_empty_queue();
    test_receive_invalid_args();
    test_unregister_wrong_gid();

    printf("\n[Multi-process tests]\n");
    test_parent_child_messaging();
    test_cross_group_send_fails();
    test_fifo_ordering();
    test_receive_truncation();
    test_receive_filters_by_pid();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
