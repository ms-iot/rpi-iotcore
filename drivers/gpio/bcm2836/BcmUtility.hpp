#ifndef _BCMUTILITY_HPP_
#define _BCMUTILITY_HPP_ 1
//
// Copyright (C) Microsoft.  All rights reserved.
//
//
// Module Name:
//
//  BcmUtility.hpp
//
// Abstract:
//
//    This file contains helpers used by BcmGpio.
//
// Environment:
//
//    Kernel mode only.
//

//
// Macros to be used for proper PAGED/NON-PAGED code placement
//

#define BCM_NONPAGED_SEGMENT_BEGIN \
    __pragma(code_seg(push)) \
    //__pragma(code_seg(.text))

#define BCM_NONPAGED_SEGMENT_END \
    __pragma(code_seg(pop))

#define BCM_PAGED_SEGMENT_BEGIN \
    __pragma(code_seg(push)) \
    __pragma(code_seg("PAGE"))

#define BCM_PAGED_SEGMENT_END \
    __pragma(code_seg(pop))

#define BCM_INIT_SEGMENT_BEGIN \
    __pragma(code_seg(push)) \
    __pragma(code_seg("INIT"))

#define BCM_INIT_SEGMENT_END \
    __pragma(code_seg(pop))

//
// We have some non-paged functions that supposed to be called on low IRQL.
// The following macro defines unified assert to be put at the beginning of
// such functions.
//
// NOTE: We can't use standard PAGED_CODE macro as it requires function to be
// placed in paged segment during compilation.
//
#define BCM_ASSERT_MAX_IRQL(Irql) NT_ASSERT(KeGetCurrentIrql() <= (Irql))
#define BCM_ASSERT_LOW_IRQL() BCM_ASSERT_MAX_IRQL(APC_LEVEL)

//
// class BITFIELD_ARRAY<...>
//
// Container for storing dense bitfields in ULONG-backed storage array.
// Useful for shadowing device registers or efficiently storing an array
// of small values.
//
template <
    unsigned int T_ELEM_COUNT,
    unsigned int T_BITS_PER_ELEMENT,
    typename T_STORAGE_TYPE=ULONG
    >
class BITFIELD_ARRAY {
    static_assert(T_BITS_PER_ELEMENT != 0, "T_BITS_PER_ELEMENT cannot be 0");
    static_assert(T_ELEM_COUNT != 0, "T_ELEM_COUNT cannot be 0");

    enum {
        _BITS_PER_STORAGE_ELEM = sizeof(T_STORAGE_TYPE) * 8,
        _ELEMS_PER_STORAGE = _BITS_PER_STORAGE_ELEM / T_BITS_PER_ELEMENT,
        _STORAGE_ELEM_COUNT = (T_ELEM_COUNT + _ELEMS_PER_STORAGE - 1) /
            _ELEMS_PER_STORAGE,
        _VALUE_MASK = (1 << T_BITS_PER_ELEMENT) - 1
    };

public:

    template <unsigned int T_BITS_PER_ELEMENT, unsigned int T_ELEMS_PER_STORAGE>
    struct _ELEM_INDEX {
        explicit _ELEM_INDEX (unsigned int Index) :
            StorageIndex(Index / T_ELEMS_PER_STORAGE),
            BitPosition((Index % T_ELEMS_PER_STORAGE) * T_BITS_PER_ELEMENT)
        { }

        unsigned int StorageIndex;
        unsigned int BitPosition;
    };

    typedef _ELEM_INDEX<T_BITS_PER_ELEMENT, _ELEMS_PER_STORAGE> INDEX_TYPE;

    BITFIELD_ARRAY ()
    {
        RtlZeroMemory(this->storage, sizeof(this->storage));
    }

    T_STORAGE_TYPE Get (unsigned int Index) const
    {
        INDEX_TYPE i(Index);
        return (this->storage[i.StorageIndex] >> i.BitPosition) & _VALUE_MASK;
    }

    void Set (unsigned int Index, T_STORAGE_TYPE Value)
    {
        NT_ASSERT((Value & _VALUE_MASK) == Value);
        INDEX_TYPE i(Index);
        T_STORAGE_TYPE value = this->storage[i.StorageIndex];
        value &= ~(_VALUE_MASK << i.BitPosition);
        value |= (Value & _VALUE_MASK) << i.BitPosition;
        this->storage[i.StorageIndex] = value;
    }

    T_STORAGE_TYPE& operator[] (unsigned int Index)
    {
        return this->storage[Index];
    }

    static INDEX_TYPE MakeIndex (ULONG Index) { return INDEX_TYPE(Index); }

private:
    T_STORAGE_TYPE storage[_STORAGE_ELEM_COUNT];
};

#endif // _BCMUTILITY_HPP_
