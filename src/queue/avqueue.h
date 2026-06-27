#ifndef AVQUEUE_H
#define AVQUEUE_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <QDebug>

template<typename T>
class AVQueue{
public:
    AVQueue(){}
    ~AVQueue(){}

    int Push(T val){
        std::lock_guard<std::mutex> lock(mtx_);
        if(1 == abort_){
            return -1;
        }
        queue_.push(val);
        cond_.notify_one();
        return 0;
    }


    int Pop(T& val,const int timeout = 0){
        std::unique_lock<std::mutex> lock(mtx_);
        if(queue_.empty()){
            cond_.wait_for(lock,std::chrono::milliseconds(timeout),[this]{
                return !queue_.empty() || abort_;
            });
        }

        if(1 == abort_){
            return -1;
        }
        if(queue_.empty()){
            return -2;
        }

        val = queue_.front();
        queue_.pop();
        return 0;
    }

    int Front(T& val){
        std::lock_guard<std::mutex> lock(mtx_);
        if(1 == abort_) {
            return -1;
        }
        if(queue_.empty()) {
            return -2;
        }
        val = queue_.front();
        return 0;
    }

    void Abort() {
        abort_ = 1;
        cond_.notify_all();
    }

    int Size(){
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.size();
    }

    bool isEmpty() const {
        return queue_.empty();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mtx_);
        std::queue<T>().swap(queue_);
    }

    int dropFront() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (abort_ || queue_.empty()) {
            return -1;
        }
        queue_.pop();
        return 0;
    }

private:
    std::queue<T> queue_;
    std::atomic<int> abort_{0};
    std::mutex mtx_;
    std::condition_variable cond_;
};

#endif
