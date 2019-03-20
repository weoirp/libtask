#include "taskimpl.h"

/*
 * locking
 */
static int
_qlock(QLock *l, int block)
{
	// 没有被占有的情况下，获得锁
	if(l->owner == nil){
		l->owner = taskrunning;
		return 1;
	}
	if(!block)
		return 0;
	// 阻塞情况下，加入等待队列
	addtask(&l->waiting, taskrunning);
	taskstate("qlock");
	taskswitch();
	if(l->owner != taskrunning){
		fprint(2, "qlock: owner=%p self=%p oops\n", l->owner, taskrunning);
		abort();
	}
	return 1;
}

void
qlock(QLock *l)
{
	_qlock(l, 1);
}

int
canqlock(QLock *l)
{
	return _qlock(l, 0);
}

void
qunlock(QLock *l)
{
	Task *ready;
	
	if(l->owner == 0){
		fprint(2, "qunlock: owner=0\n");
		abort();
	}
	// 如果等待队列不为空，等待队列第一个task获得锁，并唤醒
	if((l->owner = ready = l->waiting.head) != nil){
		deltask(&l->waiting, ready);
		taskready(ready);
	}
}

static int
_rlock(RWLock *l, int block)
{
	// 没有写者 且 写等待队列为空
	if(l->writer == nil && l->wwaiting.head == nil){
		l->readers++;	// 读者加1
		return 1;
	}
	if(!block)
		return 0;
	// 阻塞情况下，加入读等待队列
	addtask(&l->rwaiting, taskrunning);
	taskstate("rlock");
	taskswitch();
	return 1;
}

void
rlock(RWLock *l)
{
	_rlock(l, 1);
}

int
canrlock(RWLock *l)
{
	return _rlock(l, 0);
}

static int
_wlock(RWLock *l, int block)
{
	// 没有写者 且 没有读者
	if(l->writer == nil && l->readers == 0){
		l->writer = taskrunning;	//获得锁
		return 1;
	}
	if(!block)
		return 0;
	// 阻塞情况下，加入写等待队列
	addtask(&l->wwaiting, taskrunning);
	taskstate("wlock");
	taskswitch();
	return 1;
}

void
wlock(RWLock *l)
{
	_wlock(l, 1);
}

int
canwlock(RWLock *l)
{
	return _wlock(l, 0);
}

void
runlock(RWLock *l)
{
	Task *t;

	// 没有读者 但 有等待的写者
	if(--l->readers == 0 && (t = l->wwaiting.head) != nil){
		deltask(&l->wwaiting, t); 
		l->writer = t;	// 第一个等待的写者获得锁
		taskready(t);	// 唤醒该写者
	}
}

void
wunlock(RWLock *l)
{
	Task *t;
	
	if(l->writer == nil){
		fprint(2, "wunlock: not locked\n");
		abort();
	}
	l->writer = nil;
	if(l->readers != 0){
		fprint(2, "wunlock: readers\n");
		abort();
	}
	// 唤醒所有等待读者
	while((t = l->rwaiting.head) != nil){
		deltask(&l->rwaiting, t);
		l->readers++;
		taskready(t);
	}
	// 如果没有等待的读者 但 有等待的写者，唤醒第一个等待的写者
	if(l->readers == 0 && (t = l->wwaiting.head) != nil){
		deltask(&l->wwaiting, t);
		l->writer = t;
		taskready(t);
	}
}
