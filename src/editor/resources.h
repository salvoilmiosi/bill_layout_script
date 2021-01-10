#ifndef __RESOURCES_H__
#define __RESOURCES_H__

#define RESOURCE_DATA(name) __resource__##name
#define RESOURCE_LENGTH(name) __resource__##name##_length

#define DECLARE_RESOURCE(name) extern const char RESOURCE_DATA(name)[]; extern const int RESOURCE_LENGTH(name);

struct binary_resource {
    const char *data;
    const int len;
};

#define GET_RESOURCE(name) binary_resource{RESOURCE_DATA(name), RESOURCE_LENGTH(name)}

#endif