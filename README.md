# MPI Kernel Extension for Linux 7.0

Based on lab coursework (upgraded from Linux 2.4 to 7.0 and modified to work on armm64).

A message-passing interface (MPI) implemented as custom system calls in the Linux 7.0 (arm64). Processes that share a group ID can send and receive text messages through per-process in kernel queues.

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

