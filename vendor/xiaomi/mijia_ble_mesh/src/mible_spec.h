#ifndef MIBLE_SPEC_H_
#define MIBLE_SPEC_H_

#include <zephyr/types.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum _mible_spec_property_operation_type {
    MIBLE_SPEC_PROPERTY_OPERATION_GET = 0,
    MIBLE_SPEC_PROPERTY_OPERATION_SET = 1,
    MIBLE_SPEC_PROPERTY_OPERATION_GET_INTERNAL = 2,
    MIBLE_SPEC_PROPERTY_OPERATION_SET_INTERNAL = 3,
} mible_spec_property_operation_type;

// The following code defines an enumeration named 'mible_spec_operation_code_t'.
typedef enum _operation_code {
    MIBLE_SPEC_OPERATION_OK = 0,             // Represents successful operation.
    MIBLE_SPEC_OPERATION_OK_ON = 1,             // Represents operation that is turned on.
    MIBLE_SPEC_OPERATION_ERROR_CANNOT_READ = -4001,     // Represents error in reading operation.
    MIBLE_SPEC_OPERATION_ERROR_CANNOT_WRITE = -4002, // Represents error in writing operation.
    MIBLE_SPEC_OPERATION_INVALID = -4003,         // Represents invalid operation.
    MIBLE_SPEC_OPERATION_ERROR_INNER = -4004,     // Represents inner error.
    MIBLE_SPEC_OPERATION_ERROR_VALUE = -4005,     // Represents error in value.
    MIBLE_SPEC_OPERATION_ERROR_METHOD = -4006,     // Represents error in method.
    MIBLE_SPEC_OPERATION_ERROR_DID = -4007,         // Represents error in Device ID.
} mible_spec_operation_code_t;

// The following code defines an enumeration type called 'mible_spec_property_format_t'.
typedef enum {
    MIBLE_SPEC_PROPERTY_FORMAT_NUMBER = 0, // Represents a numeric format property.
    MIBLE_SPEC_PROPERTY_FORMAT_BOOL,       // Represents a boolean format property.
    MIBLE_SPEC_PROPERTY_FORMAT_FLOAT,      // Represents a floating point format property.
    MIBLE_SPEC_PROPERTY_FORMAT_STRING,     // Represents a string format property.
    MIBLE_SPEC_PROPERTY_FORMAT_UCHAR = 0x80,                  // Represents an unsigned character format property.
    MIBLE_SPEC_PROPERTY_FORMAT_CHAR,      // Represents a character format property.
    MIBLE_SPEC_PROPERTY_FORMAT_USHORT,    // Represents an unsigned short format property.
    MIBLE_SPEC_PROPERTY_FORMAT_SHORT,     // Represents a short format property.
    MIBLE_SPEC_PROPERTY_FORMAT_ULONG,     // Represents an unsigned long format property.
    MIBLE_SPEC_PROPERTY_FORMAT_LONG,      // Represents a long format property.
    MIBLE_SPEC_PROPERTY_FORMAT_ULONGLONG, // Represents an unsigned long long format property.
    MIBLE_SPEC_PROPERTY_FORMAT_LONGLONG,  // Represents a long long format property.
    MIBLE_SPEC_PROPERTY_FORMAT_ERRORCODE = 0xFE, // Represents an error code.
    MIBLE_SPEC_PROPERTY_FORMAT_UNDEFINED = 0xFF, // Represents an undefined format property.
} mible_spec_property_format_t;

enum {
    MIBLE_GATT_SPEC_TYPE_BOOL = 0,
    MIBLE_GATT_SPEC_TYPE_UINT8,
    MIBLE_GATT_SPEC_TYPE_INT8,
    MIBLE_GATT_SPEC_TYPE_UINT16,
    MIBLE_GATT_SPEC_TYPE_INT16,
    MIBLE_GATT_SPEC_TYPE_UINT32,
    MIBLE_GATT_SPEC_TYPE_INT32,
    MIBLE_GATT_SPEC_TYPE_UINT64,
    MIBLE_GATT_SPEC_TYPE_INT64,
    MIBLE_GATT_SPEC_TYPE_FLOAT,
    MIBLE_GATT_SPEC_TYPE_STRING,
    MIBLE_GATT_SPEC_TYPE_UNKNOW = 0x0F,
};

enum {
    SPEC_TX,
    SPEC_RSP,
};

// The following code defines a union named 'mible_spec_property_data_t'.
typedef union {
    bool boolean;         // Represents a boolean value.
    float floatValue;     // Represents a floating point value.
    int32_t integerValue;     // Represents a 32-bit integer value.
    int8_t charValue;     // Represents a character value.
    uint8_t ucharValue;     // Represents an unsigned character value.
    int16_t shortValue;     // Represents a short integer value.
    uint16_t ushortValue;     // Represents an unsigned short integer value.
    int32_t longValue;     // Represents a long integer value.
    uint32_t ulongValue;     // Represents an unsigned long integer value.
    int64_t longlongValue;     // Represents a long long integer value.
    uint64_t ulonglongValue; // Represents an unsigned long long integer value.

    struct {            // A nested structure defining a string data type.
        const uint8_t *str; // Pointer to the string.
        uint32_t length;    // Length of the string.
    } string;
} mible_spec_property_data_t;

typedef struct {
    mible_spec_property_data_t data;
    mible_spec_property_format_t format;
} mible_spec_property_value_t;

typedef struct {
    uint8_t siid;
    uint16_t piid;
    mible_spec_property_value_t value;
} mible_spec_property_t;

typedef struct {
    uint8_t siid;
    uint16_t piid;
    int16_t code;
    mible_spec_property_value_t value;
} mible_spec_property_operation_t;

typedef struct {
    uint16_t piid;
    mible_spec_property_value_t value;
} mible_spec_argument_t;

typedef struct {
    uint16_t size;
    mible_spec_argument_t *arguments;
} mible_spec_arguments_t;

typedef struct {
    uint16_t siid;
    uint16_t aiid;
    int16_t code;
    mible_spec_arguments_t vals;
} mible_spec_action_operation_t;

typedef enum {
    MIBLE_SPEC_BEARER_GATT,
    MIBLE_SPEC_BEARER_MESH,
} mible_spec_bearer_t;

typedef void (*mible_property_operation_cb_t)(mible_spec_bearer_t bearer,
                        mible_spec_property_operation_t *o);
typedef void (*mible_action_operation_cb_t)(mible_spec_bearer_t bearer,
                        mible_spec_action_operation_t *o);

struct mible_spec_piid_or_aiid_list {
    uint16_t piid_or_aiid;
    mible_property_operation_cb_t get;
    mible_property_operation_cb_t set;
    mible_action_operation_cb_t invoke;
};

#define MIBLE_SPEC_DEFINE(_name)                                                            \
    static const struct {                                                                   \
        uint16_t siid;                                                                      \
        const struct mible_spec_piid_or_aiid_list *const lists;                             \
    } _name[] = {

#define MIBLE_SPEC_SIID_START(siid)                                                         \
    {                                                                                       \
        (siid), (const struct mible_spec_piid_or_aiid_list[])                               \
        {

#define MIBLE_SPEC_HANDLER(piid_or_aiid, get, set, invoke)                                  \
    {                                                                                       \
        (piid_or_aiid), (get), (set), (invoke)                                              \
    }

#define MIBLE_SPEC_SIID_END                                                                 \
    MIBLE_SPEC_HANDLER(0, NULL, NULL, NULL)                                                 \
    }                                                                                       \
    }                                                                                       \
    ,
#define MIBLE_SPEC_END                                                                      \
    }                                                                                       \
    ;

struct mible_gatt_spec_tx {
    void *node;
    uint8_t *val;
    uint16_t len;
    void *user_data;
    atomic_t flags;
};

typedef int (*gatt_spec_init_t)(void);
typedef void (*gatt_spec_deinit_t)(void);


/* The function `mible_spec_property_new_value` takes in three parameters:
 * - a pointer to a struct of type `mible_spec_property_value_t`, which will store the new value of
 * the property
 * - a parameter of type `mible_spec_property_format_t`, which specifies the format of the new value
 * (e.g. whether it's an integer, a float, or a string)
 * - a pointer to a constant void object, which represents the new value that needs to be stored
*/
void mible_spec_property_new_value(mible_spec_property_value_t *value,
                        mible_spec_property_format_t format, const void *new_value);

void mible_spec_property_value_new_boolean(mible_spec_property_value_t *value, bool new_value);
void mible_spec_property_value_new_integer(mible_spec_property_value_t *value, int32_t new_value);
void mible_spec_property_value_new_float(mible_spec_property_value_t *value, float new_value);
void mible_spec_property_value_new_string(mible_spec_property_value_t *value,
                        const uint8_t *new_value);
void mible_spec_property_value_new_nstring(mible_spec_property_value_t *value,
                        const uint8_t *new_value, uint32_t length);

void mible_spec_property_value_new_char(mible_spec_property_value_t *value, int8_t new_value);
void mible_spec_property_value_new_uchar(mible_spec_property_value_t *value, uint8_t new_value);
void mible_spec_property_value_new_short(mible_spec_property_value_t *value, int16_t new_value);
void mible_spec_property_value_new_ushort(mible_spec_property_value_t *value, uint16_t new_value);
void mible_spec_property_value_new_long(mible_spec_property_value_t *value, int32_t new_value);
void mible_spec_property_value_new_ulong(mible_spec_property_value_t *value, uint32_t new_value);
void mible_spec_property_value_new_longlong(mible_spec_property_value_t *value, int64_t new_value);
void mible_spec_property_value_new_ulonglong(mible_spec_property_value_t *value, uint64_t new_value);

uint8_t gatt_format_to_mesh(uint8_t gatt_type);
uint8_t spec_format_to_type(mible_spec_property_format_t format);
int spec_format_to_size(mible_spec_property_format_t format);

void do_spec_property_set(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o);
void do_spec_property_get(mible_spec_bearer_t bearer, mible_spec_property_operation_t *o);
void do_spec_action_invoke(mible_spec_bearer_t bearer, mible_spec_action_operation_t *o);

int mible_gatt_spec_enable(void);
int mible_gatt_spec_send_properties_changed(uint8_t nums, mible_spec_property_t *newProps);
int mible_gatt_spec_send_event_occurred(uint8_t siid, uint16_t eiid,
                        mible_spec_arguments_t *newArgs);



/****************lyt*************************/
uint16_t get_property_len(mible_spec_property_value_t *prop);

#endif // MIBLE_SPEC_H_
