# MPI Kernel Extension for Linux 2.4

A lightweight message-passing interface (MPI) implemented as custom system calls in the Linux 2.4 kernel (i386). Processes that share a group ID can send and receive arbitrary messages through per-process in-kernel FIFO queues.

## System Calls

Four new system calls are added (numbers 243–246):

| Syscall | Number | Signature | Description |
|---------|--------|-----------|-------------|
| `mpi_register` | 243 | `int mpi_register(int mpi_gid)` | Register the calling process for MPI with the given group ID. Initializes its message queue. No-op if already registered. |
| `mpi_send` | 244 | `int mpi_send(pid_t pid, char *message, ssize_t message_size)` | Send a message to the process identified by `pid`. Both sender and receiver must be registered and share the same `mpi_gid`. |
| `mpi_receive` | 245 | `int mpi_receive(pid_t pid, char *message, ssize_t message_size)` | Receive the first queued message from `pid`. Copies up to `message_size` bytes into `message` and returns the number of bytes copied. Returns `-EAGAIN` if no message is available. |
| `mpi_unregister` | 246 | `int mpi_unregister(int mpi_gid)` | Unregister the calling process, freeing all pending messages. Pass `-1` to unregister regardless of group; otherwise only unregisters if `mpi_gid` matches. |

## Kernel Data Structures

Each `task_struct` gains three new fields (in `include/linux/sched.h`):

- `int mpi_registered` — whether the process is registered for MPI
- `int mpi_gid` — the MPI group ID
- `struct list_head mpi_queue` — per-process FIFO queue of incoming messages

Messages are represented by `struct mpi_message`:

```c
struct mpi_message {
    pid_t sender;
    ssize_t size;
    char *data;
    struct list_head list;
};
```


`mpi_api.h` provides thin inline wrappers that invoke the system calls via `int $0x80` inline assembly. 
