#ifndef DEFERRED_RWLOCK_HPP
#define DEFERRED_RWLOCK_HPP
#include <graphlab/parallel/pthread_tools.hpp>
#include <graphlab/logger/assertions.hpp>
namespace graphlab {
class deferred_rw_lock{
 public:

  struct request{
    void* id;
    request* next;
    char lockclass;
  };
 private:
  mutex lock;
  request* head;
  request* tail;
  uint16_t reader_count;
  bool writer;
 public:

  deferred_rw_lock(): head(NULL),
                      tail(NULL), reader_count(0),writer(false) { }

  // debugging purposes only
  size_t get_reader_count() {
    __sync_synchronize();
    return reader_count;
  }

  // debugging purposes only
  bool has_waiters() {
    return head != NULL || tail != NULL;
  }

  void insert_queue(request *I) {
    if (head == NULL) {
      head = I;
      tail = I;
    }
    else {
      tail->next = I;
      tail = I;
    }
  }
  inline bool writelock(request *I) {
    I->next = NULL;
    I->lockclass = QUEUED_RW_LOCK_REQUEST_WRITE;
    lock.lock();
    if (reader_count == 0 && writer == false) {
      // fastpath
      writer = true;
      lock.unlock();
      return true;
    }
    else {
      insert_queue(I);
      lock.unlock();
      return false;
    }
  }

  // completes the write lock on the head. lock must be acquired
  // head must be a write lock
  inline void complete_wrlock() {
  //  ASSERT_EQ(reader_count.value, 0);
    head = head->next;
    if (head == NULL) tail = NULL;
    writer = true;
  }

  // completes the read lock on the head. lock must be acquired
  // head must be a read lock
  inline size_t complete_rdlock(request* &released) {
    released = head;
    size_t numcompleted = 1;
    head = head->next;
    while (head != NULL && head->lockclass == QUEUED_RW_LOCK_REQUEST_READ) {
      head = head->next;
      numcompleted++;
    }
    reader_count += numcompleted;
    if (head == NULL) tail = NULL;
    return numcompleted;
  }
  
  inline size_t wrunlock(request *I, request* &released) {
    released = NULL;
    lock.lock();
    writer = false;
    size_t ret = 0;
    if (head != NULL) {
      if (head->lockclass == QUEUED_RW_LOCK_REQUEST_READ) {
        ret = complete_rdlock(released);
      }
      else {
        writer = true;
        released = head;
        complete_wrlock();
        ret = 1;
      }
    }
    lock.unlock();
    return ret;
  }

  inline size_t readlock(request *I, request* &released)  {
    released = NULL;
    size_t ret = 0;
    I->next = NULL;
    I->lockclass = QUEUED_RW_LOCK_REQUEST_READ;
    lock.lock();
    // there are readers and no one is writing
    if (head == NULL && writer == false) {
      // fast path
      ++reader_count;
      lock.unlock();
      released = I;
      return 1;
    }
    else {
      // slow path. Insert into queue
      insert_queue(I);
      if (head->lockclass == QUEUED_RW_LOCK_REQUEST_READ && writer == false) {
        ret = complete_rdlock(released);
      }
      lock.unlock();
      return ret;
    }
  }

  inline size_t rdunlock(request *I, request* &released)  {
    released = NULL;
    lock.lock();
    --reader_count;
    if (reader_count == 0) {
      size_t ret = 0;
      if (head != NULL) {
        if (head->lockclass == QUEUED_RW_LOCK_REQUEST_READ) {
          ret = complete_rdlock(released);
        }
        else {
          writer = true;
          released = head;
          complete_wrlock();
          ret = 1;
        }
      }
      lock.unlock();
      return ret;
    }
    else {
      lock.unlock();
      return 0;
    }
  }
};

}
#endif