#pragma once
#include <cstdint>
#include <limits>
#include <memory>
#include <cstdio>

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

template<typename T>
class ring {
public:
    ring();
    ring(uint32_t);

    static const uint32_t DEFAULT_SIZE = 4096;
    static const uint32_t SIZE_MASK = std::numeric_limits<uint32_t>::max();
    typedef struct {
        uint32_t head;
        uint32_t tail;
    } pointer_pair_t;
    typedef enum {
        FIXED,
        VARIABLE
    } behavior_t;
    
    uint32_t enqueue(T& obj);
    uint32_t enqueue_bulk(T const *obj_table, uint32_t n);
    uint32_t enqueue_burst(T const *obj_table, uint32_t n);
    T dequeue();
    uint32_t dequeue_bulk(T *obj_table, uint32_t n);
    uint32_t dequeue_burst(T *obj_table, uint32_t n);
    T head();
    T tail();
    void pop();
    // TODO tail ele to erase
private:
    uint32_t m_size;
    uint32_t m_mask;
    std::unique_ptr<T> m_buf;
    pointer_pair_t m_prod;
    pointer_pair_t m_cons;
    uint32_t do_enqueue(T const *obj_table, uint32_t n, behavior_t behavior);
    uint32_t move_prod_head(uint32_t n, uint32_t &prod_head, uint32_t &prod_next, behavior_t behavior);
    inline void parse_proper_size(uint32_t size)
    {
        m_size = size;
        m_mask = m_size - 1;
    }
    inline void enqueue_objs(uint32_t prod_head, T const * obj_table, uint32_t n)
    {
        uint32_t i = 0;
        uint32_t idx = prod_head & m_mask;
        if (likely(idx + n < m_size))
        {
            // Four elements a group to copy.
            for (i = 0; i < (n & ((~(unsigned)0x3))); i += 4, idx += 4)
            {
                // TODO use memcpy to accelerate?
                m_buf[idx] = obj_table[i];
                m_buf[idx+1] = obj_table[i+1];
                m_buf[idx+2] = obj_table[i+2];
                m_buf[idx+3] = obj_table[i+3];
            }
            switch (n & 0x3) {
                case 3:
                    m_buf[idx++] = obj_table[i++];
                case 2:
                    m_buf[idx++] = obj_table[i++];
                case 1:
                    m_buf[idx++] = obj_table[i++];
            }
        }
        else
        {
            for (i = 0; idx < m_size; ++i, ++idx)
                m_buf[idx] = obj_table[i];
            for (idx = 0; i < n; ++i, ++idx)
                m_buf[idx] = obj_table[i];
        }
    }
    inline int atomic32_cmpset(volatile uint32_t *dst, uint32_t exp, uint32_t src)
    {
    	uint8_t res;

    	asm volatile(
    			"lock ;"
    			"cmpxchgl %[src], %[dst];"
    			"sete %[res];"
    			: [res] "=a" (res),     /* output */
    			  [dst] "=m" (*dst)
    			: [src] "r" (src),      /* input */
    			  "a" (exp),
    			  "m" (*dst)
    			: "memory");            /* no-clobber list */
    	return res;
    }
    inline void pause() {
        asm volatile ("yield" ::: "memory");
    }
    inline void update_tail(pointer_pair_t &pair, uint32_t old_val, uint32_t new_val)
    {
        while (pair.tail != old_val)
            pause();
        pair.tail = new_val;
    }

};

template<typename T>
ring<T>::ring(uint32_t ring_size) :
    m_prod(0, 0),
    m_cons(0, 0)
{
    // TODO init m_size and m_mask
    parse_proper_size(ring_size);
    m_buf = new T[m_size];
}

template <typename T>
ring<T>::ring() : ring(DEFAULT_SIZE) { }

template <typename T>
uint32_t ring<T>::enqueue_bulk(T const *obj_table, uint32_t n)
{
    return do_enqueue(obj_table, n, FIXED);
}

template <typename T>
uint32_t ring<T>::enqueue_burst(T const *obj_table, uint32_t n)
{
    return do_enqueue(obj_table, n, VARIABLE);
}

template <typename T>
uint32_t ring<T>::enqueue(T &obj)
{
    return enqueue_bulk(&obj, 1);
}


template<typename T>
uint32_t ring<T>::do_enqueue(T const * obj_table, uint32_t n, behavior_t behavior)
{
    uint32_t prod_head, prod_next;
    n = move_prod_head(n, prod_head, prod_next, behavior);
    if (n == 0)
        return 0;
    enqueue_objs(prod_head, obj_table, n);
    update_tail(m_prod, prod_head, prod_next);
    return n;
}

template <typename T>
uint32_t ring<T>::move_prod_head(uint32_t n, uint32_t &prod_head, uint32_t &prod_next, behavior_t behavior)
{
    uint32_t free_space;
    int success;
    do
    {
        prod_head = m_prod.head;
        free_space = (m_size + m_cons.tail - prod_head) & m_mask;
        if (unlikely(free_space < n))
            n = (behavior == FIXED) ? 0 : free_space;
        if (n == 0)
            break;
        prod_next = prod_head + n;
        success = atomic32_cmpset(m_prod.head, prod_head, prod_next);
    } while (unlikely(success == 0));
    return n;
}
