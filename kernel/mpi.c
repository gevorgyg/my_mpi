#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/mm.h> /* dont show*/
#include <linux/spinlock.h> /* dont show*/
#include <asm/current.h>
#include <asm/uaccess.h>


#define REGISTERED 1

/* Helper: free a single message struct (data + container) */
 void mpi_free_message(struct mpi_message* msg) {
	if (!msg)
		return;
	if (msg->data)
		kfree(msg->data);
	kfree(msg);
}

/* Helper: free all pending messages for a given task */
 void mpi_clear_queue(struct task_struct* tsk) {
	struct list_head* pos, * n;
	struct mpi_message* msg;

	list_for_each_safe(pos, n, &tsk->mpi_queue) {
		msg = list_entry(pos, struct mpi_message, list);
		list_del(pos);
		mpi_free_message(msg);
	}
}

static inline int mpi_is_registered(struct task_struct* p)
{
	return p->mpi_registered && p->mpi_gid > 0;
}

/*
 * sys_mpi_register(int gid)
 *  - register current process to MPI system with group id 'gid'
 *  - initialize its message queue
 */

asmlinkage int sys_mpi_register(int mpi_gid) {

	if (mpi_is_registered(current)) //IF ALREDY REGISTER = DO NOTHING.
		return 0;

	current->mpi_gid = mpi_gid;
	current->mpi_registered = REGISTERED;
	INIT_LIST_HEAD(&current->mpi_queue);

	return 0;
}

/*
 * sys_mpi_unregister(void)
 *  - unregister current process
 *  - free all pending messages
 */


asmlinkage int sys_mpi_unregister(int mpi_gid) {

	if (current->mpi_registered != REGISTERED) // IF NOT REGISTERED
		return 0;
	/* If a specific gid is requested, only unregister if it matches */
	if (mpi_gid != -1 && mpi_gid != current->mpi_gid)
		return 0;

	mpi_clear_queue(current);
	INIT_LIST_HEAD(&current->mpi_queue);
	current->mpi_registered = 0;
	current->mpi_gid = 0;
	return 0;
}



asmlinkage int sys_mpi_send(pid_t pid, char* message, ssize_t message_size) {

	struct task_struct* receiver;
	struct mpi_message* msg;

	if (message == NULL || message_size < 1) {
		return -EINVAL;
	}
	receiver = find_task_by_pid(pid);
	if (!receiver) {
		return -ESRCH;
	}
	if (!current->mpi_registered || !receiver->mpi_registered || current->mpi_gid != receiver->mpi_gid) {
		return -EPERM;
	}

	msg = kmalloc(sizeof(struct mpi_message), GFP_KERNEL);
	if (!msg) {
		return -EFAULT;
	}
	msg->data = kmalloc(message_size, GFP_KERNEL);
	if (!msg->data) {
		kfree(msg);
		return -EFAULT;
	}
	if (copy_from_user(msg->data, message, message_size)) {
		kfree(msg->data);
		kfree(msg);
		return -EFAULT;
	}
	msg->sender = current->pid;
	msg->size = message_size;
	INIT_LIST_HEAD(&msg->list);
	list_add_tail(&msg->list, &receiver->mpi_queue);

	return 0;
}


asmlinkage int sys_mpi_receive(pid_t pid, char* message, ssize_t message_size)
{
	struct list_head* pos, * n;
	struct mpi_message* msg;
	ssize_t bytes_to_copy;

	/* Must be registered for MPI */
	if (!current->mpi_registered)
		return -EPERM;

	/* Invalid parameters */
	if (!message || message_size < 1)
		return -EINVAL;

	/* Search FIFO queue for a message from given pid */
	list_for_each_safe(pos, n, &current->mpi_queue) {

		msg = list_entry(pos, struct mpi_message, list);

		/* Only messages from the requested pid */
		if (msg->sender == pid) {

			/* Determine how many bytes to copy */
			bytes_to_copy = (msg->size < message_size) ?
				msg->size : message_size;

			/* Copy to user space */
			if (copy_to_user(message, msg->data, bytes_to_copy))
				return -EFAULT;

			/* Remove from queue */
			list_del(pos);
			kfree(msg->data);
			kfree(msg);

			/* Return number of bytes copied */
			return bytes_to_copy;
		}
	}

	/* No message from this pid */
	return -EAGAIN;
}
