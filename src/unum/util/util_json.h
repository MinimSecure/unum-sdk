// (c) 2017-2019 minim.co
// unum helper json utils include file

#ifndef _UTIL_JSON_H
#define _UTIL_JSON_H

// Max array elementes allowed during during JSON
// serialization through templates
#define MAX_JSON_ARRAY_ELEMENTS 8192

// Forward declaration of the template structures
struct _JSON_KEYVAL_TPL;
struct _JSON_VAL_TPL;

// Function type returning string for JSON_VAL_TPL_t, passed in the key name
typedef char *(*JSON_VAL_FSTR_t)(char *);
// Function type returning int for JSON_VAL_TPL_t, passed in the key name
typedef int (*JSON_VAL_FINT_t)(char *);
// Function type returning ptr to int for JSON_VAL_TPL_t, passed in the key name
typedef int *(*JSON_VAL_PFINT_t)(char *);
// Function type returning object template ptr for JSON_VAL_TPL_t,
// passed in the key name
typedef struct _JSON_KEYVAL_TPL *(*JSON_VAL_FOBJ_t)(char *);
// Function type returning JSON_VAL_TPL_t* for array elements,
// passed in the key name and called sequentially 0...N
// till reaches N that returns value template with JSON_VAL_END or NULL
typedef struct _JSON_VAL_TPL *(*JSON_VAL_FARRAY_t)(char *, int);

// Template struct for JSON val in JSON_KEYVAL_TPL_t
typedef struct _JSON_VAL_TPL {
    int type;
    union {
        char *s; // string (included only if not NULL)
        int   i; // integer (always included)
        unsigned long ul; // unsigned long (always included)
        int *pi; // ptr to integer (int included if not NULL)
        unsigned long *pul;      // ptr unsigned long (included if not NULL)
        unsigned int *pui;       // ptr unsigned int (included if not NULL)
        json_int_t *pji;         // ptr json_int_t (included if not NULL)
        struct _JSON_KEYVAL_TPL *o; // object (only if not NULL)
        struct _JSON_VAL_TPL *a; // ptr to array of values (only if not NULL)
        JSON_VAL_FSTR_t fs;      // string func (included only if not NULL)
        JSON_VAL_FINT_t fi;      // integer func (always included)
        JSON_VAL_PFINT_t fpi;    // ptr to int func (int included if not NULL)
        JSON_VAL_FOBJ_t fo;      // object func (only if not NULL)
        JSON_VAL_FARRAY_t fa;    // array of values func (only if not NULL)
    };
} JSON_VAL_TPL_t;

// Defines for the value type filed of _JSON_VAL_TPL
#define JSON_VAL_STR    0  // pointer to a string
#define JSON_VAL_INT    1  // integer
#define JSON_VAL_UL     2  // unsigned long
#define JSON_VAL_OBJ    3  // pointer to a teplate keyval array
#define JSON_VAL_ARRAY  4  // array of JSON_VAL_TPL_t, end w/ JSON_VAL_END
#define JSON_VAL_PINT   5  // integer from pointer
#define JSON_VAL_PJINT  6  // pointer to json_int_t (it is 64b if supported)
#define JSON_VAL_PUL    7  // pointer to unsigned long
#define JSON_VAL_PUINT  8  // unsigned integer from pointer
#define JSON_VAL_FSTR   10 // function returning string poiter
#define JSON_VAL_FINT   11 // function returning int
#define JSON_VAL_FOBJ   12 // function returning ptr to teplate keyval array
#define JSON_VAL_FARRAY 13 // function returning JSON_VAL_TPL_t* for array items
#define JSON_VAL_PFINT  14 // function returning pointer to int
#define JSON_VAL_SKIP   15 // allows to skip the value of the array
#define JSON_VAL_END    -1 // marks the end of the array

// Template structure for building JSON keyval pairs
typedef struct _JSON_KEYVAL_TPL {
    char *key;          // key
    JSON_VAL_TPL_t val; // value
} JSON_KEYVAL_TPL_t;

// Template structure for building JSON object.
// It is an array pointer to keyvalue pairs of the object.
// The last keyval pair must hould key == NULL
typedef JSON_KEYVAL_TPL_t JSON_OBJ_TPL_t[];

// The functioon sets value pointer in the JSON object template
// for the specified template entry. It suppors setting only the
// pointers for values that are not functions.
// Returns: 0 - success, negative - key not found or unsupported type
int util_json_obj_tpl_set_val_ptr(JSON_OBJ_TPL_t tpl, char *key, void *ptr);
// Function for building libjansson JSON object from a template
// The created object must be freed by json_decref() libjansson function.
json_t *util_tpl_to_json_obj(JSON_OBJ_TPL_t tpl);
// Function for building JSON str from JSON object.
// The string must be freed by util_free_json_str().
char *util_json_obj_to_str(json_t *obj);
// Function for building JSON string from a template.
// The string must be freed by util_free_json_str().
char *util_tpl_to_json_str(JSON_OBJ_TPL_t tpl);
// Function for freeing JSON string built by util_tpl_to_json_str()
// or util_json_obj_to_str()
void util_free_json_str(char *jstr);

// Converts JSON port list w/ ranges to port range map (PORT_RANGE_MAP_t)
// array - array of the port list strings [ "80","443","500-600",....]
// Retunrs: newly allocated part range map (allocated w/
//          UTIL_PORT_RANGE_ALLOC(), must be freed w/ UTIL_PORT_RANGE_FREE())
//          or NULL if fails
PORT_RANGE_MAP_t *util_json_to_port_range(json_t *ports_obj);

// Converts port range map (PORT_RANGE_MAP_t) to JSON port list w/ ranges
// pr - port range to conver
// Returns: newly allocated JSON array like [ "80","443","500-600", ... ]
//          (must be freed w/ json_decref()) or NULL if fails
json_t *util_port_range_to_json(PORT_RANGE_MAP_t *pr);


// Note: for parsing and retrieving JSON data use libjansson APIs directly.

#endif // _UTIL_JSON_H
