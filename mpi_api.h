#ifndef MPI_API_H
#define MPI_API_H
#include <linux/stddef.h>
#include <errno.h>
#include <sys/types.h>

int mpi_register(int mpi_gid)
{
    int res;
    __asm__(
        "pushl %%eax;"
        "pushl %%ebx;"
        "movl $243, %%eax;"
        "movl %1, %%ebx;"
        "int $0x80;"
        "movl %%eax,%0;"
        "popl %%ebx;"
        "popl %%eax;"
        : "=m"(res)
        : "m"(mpi_gid));
    if (res < 0)
    {
        errno = -res;
        res = -1;
    }
    return res;
}

int mpi_send(pid_t pid, char *message, ssize_t message_size)
{
    int res;
    __asm__(
        "pushl %%eax;"
        "pushl %%ebx;"
        "pushl %%ecx;"
        "pushl %%edx;"
        "movl $244, %%eax;"
        "movl %1, %%ebx;"
        "movl %2, %%ecx;"
        "movl %3, %%edx;"
        "int $0x80;"
        "movl %%eax,%0;"
        "popl %%edx;"
        "popl %%ecx;"
        "popl %%ebx;"
        "popl %%eax;"
        : "=m"(res)
        : "m"(pid), "m"(message), "m"(message_size));
    if (res < 0)
    {
        errno = -res;
        res = -1;
    }
    return res;
}

int mpi_receive(pid_t pid, char *message, ssize_t message_size)
{
    int res;
    __asm__(
        "pushl %%eax;"
        "pushl %%ebx;"
        "pushl %%ecx;"
        "pushl %%edx;"
        "movl $245, %%eax;"
        "movl %1, %%ebx;"
        "movl %2, %%ecx;"
        "movl %3, %%edx;"
        "int $0x80;"
        "movl %%eax,%0;"
        "popl %%edx;"
        "popl %%ecx;"
        "popl %%ebx;"
        "popl %%eax;"
        : "=m"(res)
        : "m"(pid), "m"(message), "m"(message_size));
    if (res < 0)
    {
        errno = -res;
        res = -1;
    }
    return res;
}

int mpi_unregister(int mpi_gid)
{
    int res;
    __asm__(
        "pushl %%eax;"
        "pushl %%ebx;"
        "movl $246, %%eax;"
        "movl %1, %%ebx;"
        "int $0x80;"
        "movl %%eax, %0;"
        "popl %%ebx;"
        "popl %%eax;"
        : "=m"(res)
        : "m"(mpi_gid));
    if (res < 0)
    {
        errno = -res;
        res = -1;
    }
    return res;
}
#endif //MPI_API_H
