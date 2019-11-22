# LockFreeQueue
A lightweight lock free queue implemented in c++11 based on hazard pointer.\
c++11实现的基于风险指针的轻量级无锁队列。
## Feature
  * C++11 implementation.
  * Thread-safe and Lock-free. 
  * Hazard pointer.
  * Inter thread helping.
  * Support Multi-producer & Multi-consumer
  * You can customize the maximum number of threads you want.
  * Dynamically allocate nodes(performance bottlneck). Or you can simply implement a thread-safe memory pool, e.g. by thread_local   storage identifier.
## Benchmark

  Magnitude     | Enqueue     | Dequeue     | Enqueue & Dequeue|
  :-----------  | :-----------| :-----------| :-----------------
  10K           | 1.8ms       | 1.4ms       | 2.6ms
  100K          | 33.6ms      | 32.3ms      | 38.6ms
  1000K         | 209.2ms     | 185.1ms     | 299.3ms
  
The above data was tested on my i5-7500 cpu with gcc -O3.

The data of first and scecond column was obtained by starting 4 threads to enqueue concurrently and dequeue concurrently, the data of third column was obtained by starting 2 threads to enqueue and 2 threads to dequeue concurrently, each looped 10 times to calculate the average time consumption.
See also [test](test.cc).
## Build
```
make && ./test
```
## API
```
void Enqueue(const T& data);
void Enqueue(T&& data);
bool Dequeue(T& data);
size_t size() const;
```
