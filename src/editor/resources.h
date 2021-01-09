#ifndef __RESOURCES_H__
#define __RESOURCES_H__

#define BINARY_START(name) _binary_##name##_start
#define BINARY_END(name) _binary_##name##_end
#define BINARY_SIZE(name) (BINARY_END(name) - BINARY_START(name))

#define DECLARE_RESOURCE(name) extern char BINARY_START(name)[]; extern char BINARY_END(name)[];

struct binary_resource {
    const void *data;
    const size_t len;

    binary_resource(const void *data, size_t len) : data(data), len(len) {}
};

#define GET_RESOURCE(name) binary_resource(BINARY_START(name), BINARY_SIZE(name))

#endif