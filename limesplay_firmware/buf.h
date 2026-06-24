/**
 * @file    buf.h
 * @brief   MyBuffer - a fixed-capacity templated ring buffer (FIFO)
 *
 * A small, header-only ring buffer sized at compile time (no heap). Besides the
 * usual FIFO push/pop it offers non-destructive random access (`at`) and a way
 * to discard the newest elements (`dropBack`), which lets petcol inspect a
 * candidate frame at the tail of the stream and only consume it once its CRC
 * checks out - without copying bytes out and back.
 *
 * Overflow policy: pushing into a full buffer overwrites the oldest element.
 */

#ifndef MYBUFFER_H
#define MYBUFFER_H

#include <stdint.h>

template <typename T, uint16_t N>
class MyBuffer
{
    static_assert(N > 0, "MyBuffer capacity must be greater than zero");

private:
    T        _buf[N];
    uint16_t _tail = 0;    // index of the oldest element
    uint16_t _count = 0;   // number of elements currently held

    uint16_t headIndex() const { return (uint16_t)((_tail + _count) % N); }

public:
    /** Number of elements currently held. */
    uint16_t count() const { return _count; }

    /** Total capacity. */
    uint16_t capacity() const { return N; }

    /** True when no elements are held. */
    bool empty() const { return _count == 0; }

    /** Discard all elements. */
    void clear() { _tail = 0; _count = 0; }

    /** Append one element. Overwrites the oldest element when full. */
    void push(T data)
    {
        _buf[headIndex()] = data;
        if (_count == N)
        {
            _tail = (uint16_t)((_tail + 1) % N);   // full: drop the oldest
        }
        else
        {
            _count++;
        }
    }

    /** Remove and return the oldest element. Caller must ensure !empty(). */
    T pop()
    {
        T data = _buf[_tail];
        _tail = (uint16_t)((_tail + 1) % N);
        _count--;
        return data;
    }

    /** Non-destructive read of the k-th element from the oldest (0 = oldest). */
    T at(uint16_t k) const { return _buf[(uint16_t)((_tail + k) % N)]; }

    /** Discard the n newest elements. Caller must ensure n <= count(). */
    void dropBack(uint16_t n) { _count -= n; }
};

#endif
