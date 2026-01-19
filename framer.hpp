#pragma once
#include <stddef.h>
#include <stdint.h>

namespace umsg
{

    struct bufferSpan
    {
        uint8_t *data;
        size_t length;
    };

    template <size_t MaxPacketSize>
    class Framer
    {
    public:
        Framer();

        bool createPacket(bufferSpan frame, bufferSpan &packet);
        bool processByte(uint8_t byte);

        // Register C++ member-function callback using type erasure and trampolines
        template <class T>
        bool registerOnPacketCallback(T *obj, void (T::*method)(uint8_t *frame, size_t length))
        {
            cbObj_ = static_cast<void *>(obj);
            cbThunk_ = &memberThunk<T, void (T::*)(uint8_t *, size_t)>;
            cbMethod_ = reinterpret_cast<void *>(method);
            return true;
        }

    private:
        // internal buffer for incoming packets
        uint8_t rxBuffer_[MaxPacketSize]; 
        size_t rxIndex_ = 0;


        // ---- callback internals ----
        typedef void (*Thunk)(void *obj, void *method, uint8_t *frame, size_t length);

        void *cbObj_ = 0;    // object for member function callback
        void *cbMethod_ = 0; // member function pointer (type-erased)
        Thunk cbThunk_ = 0;  // thunk to call member function

        template <class T, class M>
        static void memberThunk(void *obj, void *method, uint8_t *frame, size_t length)
        {
            M m = reinterpret_cast<M>(method);
            (static_cast<T *>(obj)->*m)(frame, length);
        }

        // Call this internally when a packet is complete
        void emitPacket(uint8_t *frame, size_t length)
        {
            if (cbThunk_)
            {
                cbThunk_(cbObj_, cbMethod_, frame, length);
            }
        }
    };
} // namespace umsg