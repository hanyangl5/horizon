/*****************************************************************/ /**
 * \file   Memory.h
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#pragma once

namespace Horizon::Memory {

template <typename T, typename... Args> T *Alloc(Args &&...args) {
    void *memory = malloc(sizeof(T));
    return new (memory) T(std::forward<Args>(args)...);
}

template <typename T> void Free(T *ptr) {
    if (!ptr) {
        return;
    }
    ptr->~T();
    free(ptr);
}

} // namespace Horizon::Memory