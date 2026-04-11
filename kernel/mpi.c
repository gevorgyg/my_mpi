// SPDX-License-Identifier: GPL-2.0
/*
 * kernel/mpi.c - MPI (Message Passing Interface) kernel extension
 *
 * Provides per-process FIFO message queues with group-based isolation.
 * Processes in the same MPI group can exchange arbitrary messages.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/rcupdate.h>
#include <linux/pid.h>
#include <linux/spinlock.h>

struct mpi_message {
	pid_t		sender;
	ssize_t		size;
	char		*data;
	struct list_head list;
};

static void mpi_free_message(struct mpi_message *msg)
{
	if (!msg)
		return;
	kfree(msg->data);
	kfree(msg);
}

void mpi_clear_queue(struct task_struct *tsk)
{
	struct mpi_message *msg, *tmp;

	spin_lock(&tsk->mpi_lock);
	list_for_each_entry_safe(msg, tmp, &tsk->mpi_queue, list) {
		list_del(&msg->list);
		mpi_free_message(msg);
	}
	spin_unlock(&tsk->mpi_lock);
}

void mpi_init_task(struct task_struct *tsk)
{
	tsk->mpi_registered = 0;
	tsk->mpi_gid = 0;
	INIT_LIST_HEAD(&tsk->mpi_queue);
	spin_lock_init(&tsk->mpi_lock);
}

void mpi_fork_task(struct task_struct *child, struct task_struct *parent)
{
	INIT_LIST_HEAD(&child->mpi_queue);
	spin_lock_init(&child->mpi_lock);
	if (parent->mpi_registered) {
		child->mpi_registered = 1;
		child->mpi_gid = parent->mpi_gid;
	} else {
		child->mpi_registered = 0;
		child->mpi_gid = 0;
	}
}

SYSCALL_DEFINE1(mpi_register, int, mpi_gid)
{
	if (current->mpi_registered)
		return 0;

	current->mpi_gid = mpi_gid;
	current->mpi_registered = 1;
	INIT_LIST_HEAD(&current->mpi_queue);

	return 0;
}

SYSCALL_DEFINE1(mpi_unregister, int, mpi_gid)
{
	if (!current->mpi_registered)
		return 0;

	if (mpi_gid != -1 && mpi_gid != current->mpi_gid)
		return 0;

	mpi_clear_queue(current);
	current->mpi_registered = 0;
	current->mpi_gid = 0;

	return 0;
}

SYSCALL_DEFINE3(mpi_send, pid_t, pid, char __user *, message, ssize_t, message_size)
{
	struct task_struct *receiver;
	struct mpi_message *msg;

	if (!message || message_size < 1)
		return -EINVAL;

	if (!current->mpi_registered)
		return -EPERM;

	rcu_read_lock();
	receiver = find_task_by_vpid(pid);
	if (receiver)
		get_task_struct(receiver);
	rcu_read_unlock();

	if (!receiver)
		return -ESRCH;

	if (!receiver->mpi_registered || current->mpi_gid != receiver->mpi_gid) {
		put_task_struct(receiver);
		return -EPERM;
	}

	msg = kmalloc(sizeof(*msg), GFP_KERNEL);
	if (!msg) {
		put_task_struct(receiver);
		return -ENOMEM;
	}

	msg->data = kmalloc(message_size, GFP_KERNEL);
	if (!msg->data) {
		kfree(msg);
		put_task_struct(receiver);
		return -ENOMEM;
	}

	if (copy_from_user(msg->data, message, message_size)) {
		kfree(msg->data);
		kfree(msg);
		put_task_struct(receiver);
		return -EFAULT;
	}

	msg->sender = current->pid;
	msg->size = message_size;
	INIT_LIST_HEAD(&msg->list);

	spin_lock(&receiver->mpi_lock);
	list_add_tail(&msg->list, &receiver->mpi_queue);
	spin_unlock(&receiver->mpi_lock);

	put_task_struct(receiver);
	return 0;
}

SYSCALL_DEFINE3(mpi_receive, pid_t, pid, char __user *, message, ssize_t, message_size)
{
	struct mpi_message *msg, *tmp;
	ssize_t bytes_to_copy;

	if (!current->mpi_registered)
		return -EPERM;

	if (!message || message_size < 1)
		return -EINVAL;

	spin_lock(&current->mpi_lock);
	list_for_each_entry_safe(msg, tmp, &current->mpi_queue, list) {
		if (msg->sender == pid) {
			bytes_to_copy = min(msg->size, message_size);
			list_del(&msg->list);
			spin_unlock(&current->mpi_lock);

			if (copy_to_user(message, msg->data, bytes_to_copy)) {
				mpi_free_message(msg);
				return -EFAULT;
			}

			mpi_free_message(msg);
			return bytes_to_copy;
		}
	}
	spin_unlock(&current->mpi_lock);

	return -EAGAIN;
}
