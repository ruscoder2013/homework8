#include <cstring>

#ifndef LINKEDLIST_H
#define LINKEDLIST_H

namespace async {
    using handle_t = void*;
    handle_t connect(std::size_t bulk);
    void receive(handle_t handler, const char* data, std::size_t size);
    void disconnect(handle_t handler);
}

#endif