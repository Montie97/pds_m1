//
// Created by ale on 07/07/20.
//

#ifndef BLOCKINGQUEUE_BLOCKINGQUEUE_H
#define BLOCKINGQUEUE_BLOCKINGQUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
using namespace std;

template<class T>
class BlockingQueue
{
private:
    int capacity;
    queue<T> q;
    mutex m;
    condition_variable cv_put;
    condition_variable cv_take;

public:
    BlockingQueue(int cap)
    {
        capacity = cap;
    }

    void put(T t)
    {
        unique_lock<mutex> ul(m);

        while(q.size() == capacity)
            cv_put.wait(ul);

        q.push(t);
        cv_take.notify_one();
    }

    T take()
    {
        unique_lock<mutex> ul(m);

        while(q.size() == 0)
            cv_take.wait(ul);

        T t = q.front();
        q.pop();
        cv_put.notify_one();

        return t;
    }

    int size()
    {
        lock_guard<mutex> l(m);
        return q.size();
    }
};


#endif //BLOCKINGQUEUE_BLOCKINGQUEUE_H
