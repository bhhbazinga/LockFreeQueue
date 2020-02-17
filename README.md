# LockFreeQueue
Lock Free Queue Based On Hazard Pointer.
## Feature
  * C++20 implementation.
  * Thread-safe and Lock-free. 
  * Hazard pointer.
  * ABA safe.
  * Inter thread helping.
  * Support Multi-producer & Multi-consumer.
  * No limitation of number of threads.
  * Dynamically allocate nodes(performance bottlneck). Or you can simply implement a thread-safe memory pool, e.g. by thread_local   storage identifier.
## Benchmark

  Magnitude     | Enqueue     | Dequeue     | Enqueue & Dequeue|
  :-----------  | :-----------| :-----------| :-----------------
  10K           | 1.8ms       | 1.4ms       | 2.6ms
  100K          | 33.6ms      | 32.3ms      | 38.6ms
  1000K         | 209.2ms     | 185.1ms     | 299.3ms
  
The above data was tested on my i5-7500 cpu with gcc -O3.\
You can also compare the tested data with [BlockingQueue](https://github.com/bhhbazinga/BlockingQueue) which is implemented by mutex.

The data of first and second column was obtained by starting 4 threads to enqueue concurrently and dequeue concurrently, the data of third column was obtained by starting 2 threads to enqueue and 2 threads to dequeue concurrently, each looped 10 times to calculate the average time consumption.
See also [test](test.cc).
## Build
```
make && ./test
```
## API
```C++
void Enqueue(const T& data);
void Enqueue(T&& data);
bool Dequeue(T& data);
size_t size() const;
```
## Reference
[Lock-Free Data Structures with Hazard Pointers](https://www.drdobbs.com/lock-free-data-structures-with-hazard-po/184401890)\
[C++ Concurrency in Action, Second Edition](https://chenxiaowei.gitbook.io/c-concurrency-in-action-second-edition-2019/)
