// (c) 2017-2019 minim.co
// unum helper json utils code

#include "unum.h"

// Max length of json value to print in logs
#define MAX_JSON_VAL_LOG 512


// Function for building libjansson JSON value from a template.
// The created value must be freed by json_decref() libjansson function.
// val - pointer to the value template
// key - name of the value key (array key for arrays)
// perr (out) - pointer to int where to store the error code
// Returs: ptr to json_t* value or NULL if nothing to add or
//         no memory to allocate the value
static json_t *util_tpl_to_json_val(JSON_VAL_TPL_t *val, char *key, int *perr)
{
    json_t *jval = NULL;
    char *s;
    int *pi;
    int ii;
    JSON_KEYVAL_TPL_t *o;

    *perr = 0;

    switch(val->type) {
        case JSON_VAL_STR:
            if(val->s && !(jval = json_string(val->s))) {
                log("%s: error adding '%s' value '%.*s%s'\n", __func__,
                    key, val->s, MAX_JSON_VAL_LOG,
                    (strlen(val->s) > MAX_JSON_VAL_LOG ? "..." : ""));
                *perr = -1;
            }
            break;
        case JSON_VAL_INT:
            if((jval = json_integer(val->i)) == NULL) {
                log("%s: error adding '%s' value '%d'\n",
                    __func__, key, val->i);
                *perr = -1;
            }
            break;
        case JSON_VAL_UL:
            // This should allow for > 0x7fffffff positive 32 bit integers
            if((jval = json_integer(val->ul)) == NULL) {
                log("%s: error adding '%s' value '%lu'\n",
                    __func__, key, val->ul);
                *perr = -1;
            }
            break;
        case JSON_VAL_PINT:
            if(val->pi && !(jval = json_integer(*(val->pi)))) {
                log("%s: error adding '%s' value '%d'\n",
                    __func__, key, *(val->pi));
                *perr = -1;
            }
            break;
        case JSON_VAL_PUL:
            // Unlike above this should preserve positive > 0x7fffffff
            if(val->pul && !(jval = json_integer(*(val->pul)))) {
                log("%s: error adding '%s' value '%lu'\n",
                    __func__, key, *(val->pul));
                *perr = -1;
            }
            break;
        case JSON_VAL_PUINT:
            if(val->pui && !(jval = json_integer(*(val->pui)))) {
                log("%s: error adding '%s' value '%u'\n",
                    __func__, key, *(val->pui));
                *perr = -1;
            }
            break;
        case JSON_VAL_PJINT:
            if(val->pji && !(jval = json_integer(*(val->pji)))) {
                log("%s: error adding '%s' value '%" JSON_INTEGER_FORMAT "'\n",
                    __func__, key, *(val->pji));
                *perr = -1;
            }
            break;
        case JSON_VAL_OBJ:
            if(val->o && !(jval = util_tpl_to_json_obj(val->o))) {
                log("%s: error adding '%s' object\n", __func__, key);
                *perr = -1;
            }
            break;
        case JSON_VAL_FSTR:
            if(val->fs != NULL) {
                break;
            }
            s = val->fs(key);
            if(s && !(jval = json_string(s))) {
                log("%s: error adding '%s' value '%.*s%s'\n", __func__,
                    key, s, MAX_JSON_VAL_LOG,
                    (strlen(s) > MAX_JSON_VAL_LOG ? "..." : ""));
                *perr = -1;
            }
            break;
        case JSON_VAL_FINT:
            if(val->fi == NULL) {
                break;
            }
            ii = val->fi(key);
            if((jval = json_integer(ii)) == NULL) {
                log("%s: error adding '%s' value '%d'\n",
                    __func__, key, ii);
                *perr = -1;
            }
            break;
        case JSON_VAL_PFINT:
            pi = val->fpi ? val->fpi(key) : NULL;
            if(pi && (jval = json_integer(*pi)) == NULL) {
                log("%s: error adding '%s' value '%d'\n",
                    __func__, key, *pi);
                *perr = -1;
            }
            break;
        case JSON_VAL_FOBJ:
            o = val->fo ? val->fo(key) : NULL;
            if(o && !(jval = util_tpl_to_json_obj(o))) {
                log("%s: error adding '%s' object\n", __func__, key);
                *perr = -1;
            }
            break;
        case JSON_VAL_ARRAY:
        case JSON_VAL_FARRAY:
            // Using the same code for static array spec and function...
            // since the pointer is a union val->a and val->fa are actually the
            // same.
            if(val->fa && val->a) {
                if((jval = json_array()) == NULL) {
                    log("%s: error allocating array object for '%s'\n",
                        __func__, key);
                    *perr = -1;
                    break;
                }
                for(ii = 0; ii < MAX_JSON_ARRAY_ELEMENTS && *perr == 0; ii++)
                {
                    JSON_VAL_TPL_t *v;
                    if(val->type == JSON_VAL_ARRAY) {
                        v = &(val->a[ii]);
                    } else {
                        v = val->fa(key, ii);
                    }
                    if(!v || v->type == JSON_VAL_END) {
                        break;
                    }
                    if(v->type == JSON_VAL_SKIP) {
                        continue;
                    }
                    json_t *javal = util_tpl_to_json_val(v, key, perr);
                    if(!javal) {
                        log("%s: error allocating item %d for array '%s'\n",
                            __func__, ii, key);
                        *perr = -1;
                        break;
                    }
                    if(json_array_append_new(jval, javal) < 0) {
                        log("%s: error adding item %d to array '%s'\n",
                            __func__, ii, key);
                        json_decref(javal);
                        *perr = -1;
                        break;
                    }
                }
                if(*perr != 0) {
                    // Cleanup if there was a failure
                    json_decref(jval);
                    jval = NULL;
                }
            }
            break;
        default:
            log("%s: value type %d is not supported\n",
                __func__, val->type);
            *perr = -1;
    }

    return jval;
}

// The functioon sets value pointer in the JSON object template
// for the specified template entry. It suppors setting only the
// pointers for values that are not functions.
// Returns: 0 - success, negative - key not found or unsupported type
int util_json_obj_tpl_set_val_ptr(JSON_OBJ_TPL_t tpl, char *key, void *ptr)
{
    JSON_KEYVAL_TPL_t *kv;
    int err = 0;

    // Loop through all keyvalue pairs of the template and find what we
    // are asked to change.
    int found = FALSE;
    for(kv = tpl; kv && kv->key; kv++)
    {
        if(strcmp(kv->key, key) != 0) {
            continue;
        }
        found = TRUE;
        JSON_VAL_TPL_t *val = &kv->val;
        switch(val->type) {
            case JSON_VAL_STR:
                val->s = (char *)ptr;
                break;
            case JSON_VAL_PINT:
                val->pi = (int *)ptr;
                break;
            case JSON_VAL_PUL:
                val->pul = (unsigned long *)ptr;
                break;
            case JSON_VAL_PUINT:
                val->pui = (unsigned int *)ptr;
                break;
            case JSON_VAL_PJINT:
                val->pji = (json_int_t *)ptr;
                break;
            case JSON_VAL_OBJ:
                val->o = (struct _JSON_KEYVAL_TPL *)ptr;
                break;
            default:
                log("%s: key %s value type %d is not supported\n",
                    __func__, key, val->type);
                err = -2;
                break;
        }
    }
    if(!found) {
        log("%s: key %s is not found\n", __func__, key);
        err = -1;
    }

    return err;
}

// Function for building libjansson JSON object from a template.
// The created object must be freed by json_decref() libjansson function.
json_t *util_tpl_to_json_obj(JSON_OBJ_TPL_t tpl)
{
    json_t *obj;
    JSON_KEYVAL_TPL_t *kv;
    int err = 0;

    obj = json_object();
    if(!obj) {
        return NULL;
    }

    // Loop through all keyvalue pairs of the object
    for(kv = tpl; kv && kv->key; kv++)
    {
        int err = 0;
        char *key = kv->key;
        JSON_VAL_TPL_t *val = &kv->val;

        json_t *jval = util_tpl_to_json_val(val, key, &err);

        // If there was an error we are done here
        if(err) {
            break;
        }
        // If no value to add skip the key
        if(!jval) {
            continue;
        }
        // Add the new keypair to the object
        if(json_object_set_new_nocheck(obj, key, jval) != 0) {
            log("%s: error adding key '%s'\n", __func__, key);
            json_decref(jval);
            jval = NULL;
            err = -1;
            break;
        }
    }

    if(err && obj) {
        json_decref(obj);
        obj = NULL;
    }

    return obj;
}

// Function for building JSON str from JSON object.
// The string must be freed by util_free_json_str().
char *util_json_obj_to_str(json_t *obj)
{
    char *jstr;

#ifdef DEBUG
    jstr = json_dumps(obj, JSON_INDENT(2));
#else  // DEBUG
    jstr = json_dumps(obj, JSON_COMPACT);
#endif // DEBUG

    return jstr;
}

// Function for building JSON string from a template.
// The string must be freed by util_free_json_str().
char *util_tpl_to_json_str(JSON_OBJ_TPL_t tpl)
{
    json_t *obj = util_tpl_to_json_obj(tpl);

    if(!obj) {
        return NULL;
    }
    char *jstr = util_json_obj_to_str(obj);

    json_decref(obj);
    return jstr;
}

// Function for freeing JSON string built by util_tpl_to_json_str().
void util_free_json_str(char *jstr)
{
    free(jstr);
}

// Converts JSON port list w/ ranges to port range map (PORT_RANGE_MAP_t)
// array - array of the port list strings [ "80","443","500-600", ... ]
// Returns: newly allocated part range map (allocated w/
//          UTIL_PORT_RANGE_ALLOC(), must be freed w/ UTIL_PORT_RANGE_FREE())
//          or NULL if fails
PORT_RANGE_MAP_t *util_json_to_port_range(json_t *ports_obj)
{
    PORT_RANGE_MAP_t *pr = NULL;
    unsigned int ports_len = 0;

    if(!ports_obj || (ports_len = json_array_size(ports_obj)) <= 0) {
        return pr;
    }

    // The first pass is to find out the min and max ports and verify
    // the JSON
    int i;
    int min_port = 0x10000;
    int max_port = 0;
    for(i = 0; i < ports_len; i++)
    {
        json_t *port_or_range_obj = json_array_get(ports_obj, i);
        if(!port_or_range_obj) {
            log("%s: error corrupt JSON array at position: %d\n",
                __func__, i);
            return pr;
        }
        const char *port_or_range = json_string_value(port_or_range_obj);
        if(!port_or_range) {
            log("%s: error getting port_or_range\n", __func__);
            return pr;
        }
        int start, end, scan;
        scan = sscanf(port_or_range, "%d-%d", &start, &end);
        if(scan < 2) {
            end = start;
        }
        if(scan <= 0 ||
           start <= 0 || start >= 0x10000 || end <= 0 || end >= 0x10000)
        {
            log("%s: bad port_or_range <%s>\n", __func__, port_or_range);
            return pr;
        }
        min_port = (min_port > start) ? start : min_port;
        max_port = (max_port < end) ? end : max_port;
    }
    // Allocate the port range ans set the bits
    pr = UTIL_PORT_RANGE_ALLOC(min_port, max_port - min_port + 1);
    if(!pr) {
        log("%s: no memory for range %d-%d\n", __func__, min_port, max_port);
        return pr;
    }
    UTIL_PORT_RANGE_RESET(pr);
    int failed = FALSE;
    for(i = 0; i < ports_len; i++)
    {
        json_t *port_or_range_obj = json_array_get(ports_obj, i);
        if(!port_or_range_obj) {
            log("%s: error corrupt JSON array at position: %d\n",
                __func__, i);
            failed = TRUE;
            break;
        }
        const char *port_or_range = json_string_value(port_or_range_obj);
        if(!port_or_range) {
            log("%s: error getting port_or_range\n", __func__);
            failed = TRUE;
            break;
        }
        int start, end, scan;
        scan = sscanf(port_or_range, "%d-%d", &start, &end);
        if(scan < 2) {
            end = start;
        }
        if(scan <= 0)
        {
            log("%s: bad port_or_range <%s>\n", __func__, port_or_range);
            failed = TRUE;
            break;
        }
        int jj;
        for(jj = start; jj <= end; ++jj) {
            UTIL_PORT_RANGE_SET(pr, jj);
        }
    }
    if(failed) {
        UTIL_PORT_RANGE_FREE(pr);
        pr = NULL;
    }

    return pr;
}

// Converts port range map (PORT_RANGE_MAP_t) to JSON port list w/ ranges
// pr - port range to conver
// Returns: newly allocated JSON array like [ "80","443","500-600", ... ]
//          (must be freed w/ json_decref()) or NULL if fails
json_t *util_port_range_to_json(PORT_RANGE_MAP_t *pr)
{
    unsigned short start, end;
    int ii, error = FALSE;
    json_t *port_array = NULL;

    if(pr == NULL) {
        return NULL;
    }

    port_array= json_array();
    if(!port_array) {
        log("%s: cannot create JSON array\n", __func__);
        return NULL;
    }

    start = end = 0;
    for(ii = 0; ii < pr->len; ++ii)
    {
        json_t *jstr;
        char buf[16]; // has to fit at least "xxxxx-xxxxx"
        int port = pr->start + ii;

        if(UTIL_PORT_RANGE_GET(pr, port)) { // Port is set
            if(start == 0) {
                start = port;
            } else {
                end = port;
            }
            // if loop is not done, just continue to check the next
            if(ii < pr->len) {
                continue;
            }
        }
        // Port is not set or it is the last run of the loop
        if(end > 0) {
            sprintf(buf, "%u-%u", start, end);
        } else if(start > 0) {
            sprintf(buf, "%u", start);
        } else {
            continue;
        }
        start = end = 0;
        if((jstr = json_string_nocheck(buf)) == NULL ||
           json_array_append_new(port_array, jstr) != 0)
        {
            log("%s: failed to add <%s> to port list\n", __func__, buf);
            if(jstr) {
                json_decref(jstr);
                jstr = NULL;
            }
            error = TRUE;
            break;
        }
        jstr = NULL;
    }

    if(error && port_array != NULL) {
        json_decref(port_array);
        port_array = NULL;
    }

    return port_array;
}
