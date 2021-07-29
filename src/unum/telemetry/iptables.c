// (c) 2018 minim.co
// Router iptables and iptables diffs
// Note: there's more to do here, calling ipt_diffs_prep_to_send() and
//       re-running the diff to store data in ipt_diff[][] can be avoided.
//       ipt_diffs_array_f() can do the diff itself tracking the offsets
//       of the diff procedure in the compared buffers. This will allow it
//       to continue rather than re-run diff from start for each request.

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// Buffer to hold the output of the iptables commands
static char *iptables_buf = NULL;
// Same as iptables_buf but from previous iteration
static char *iptables_buf_prev = NULL;

// Size of the memory allocated for iptables_buf
static int iptables_buf_len = 0;
// Size of the memory allocated for iptables_buf_prev
static int iptables_buf_prev_len = 0;
// Length of the iptables data in the buffer
static int iptables_str_len = 0;
// Same as iptables_buf but from previous iteration
static int iptables_str_len_prev = 0;

// Flag indicating an error while collecting the data
static int data_error = FALSE;

// Differences between iptables rules between this iteration and 
// previous iteration
static char ipt_diff[MAX_IPT_DIFF_ENTRIES][MAX_IPT_ENTRY_LENGTH];


// Read the output of the command
// Here the command can be "iptables --list-rules" or "
// "iptables -t nat --list-rules"
// cmd: the iptables command
// cmd_prefix: Single character string prefix to identify
//             the table like "f" or "n"
// buf: the bufer to hold output of the iptables command
// offset: offset from the start of the buffer to fill the command output
// buf_len_ptr: Pointer to maximum  length of the buffer. 
//              Can be increased if needed after allocating more memory to buf
//              it is an IN/OUT parameter.
// str_len_ptr: Pointer to actual length of the buffer (which contains the conent)
//              it is an IN/OUT parameter.
// Returns the buffer with the output of the cmd
static char* read_iptables_rules_cmd(char *cmd, char *cmd_prefix,
                                     char *buf, int offset,
                                     int *buf_len_ptr, int *str_len_ptr)
{
    FILE *fp;
    int i = offset; // Start filling the buffer from offset
    int cmd_len = strlen(cmd_prefix);
    char rule[MAX_IPT_ENTRY_LENGTH];

    fp = popen(cmd, "r");
    if(fp == NULL) {
        log("%s: not able to get the output of the the command: %s\n",
            __func__, cmd);
        data_error = TRUE;
        return buf;
    }

    // Keep track of the rules ordering within chains (using separate storage
    // for last_rule since we might realloc and lose the pointer)
    char last_rule[MAX_IPT_ENTRY_LENGTH] = "";
    int last_rule_clen = 0;
    int rule_num = 0;

    // Read characters line-by-lines (one line per rule)
    // And store in the buffer
    while(fgets(rule, sizeof(rule), fp) != NULL) 
    {
        int rule_len = strlen(rule);

        // The rule lines should have at least 4 caracters followed by \n
        if(rule_len < 5) {
            log_dbg("%s: skipping invalid rule line: %s\n",
                    __func__, rule);
            continue;
        }
                
        // Make sure we have \n at the end (diff won't work without it)
        if(rule[rule_len - 1] != '\n') {
            log_dbg("%s: rule doesn't end with CR: %s\n",
                    __func__, rule);
            rule[rule_len - 1] = '\n';
        }

        // Realloc buffer if the size is not sufficient
        // It accounts for prefix, rule position number in the chain,
        // rule length and the terminating 0 at the end of the buffer.
        if((i + cmd_len + 10 + rule_len + 1) > *buf_len_ptr)
        {
            char *ptr = UTIL_REALLOC(buf, (*buf_len_ptr+IPTABLES_REALLOC_SIZE));
            if(ptr == NULL)
            {
                log("%s: Error while reallocating memory for the buffer: %d\n",
                    __func__, i);
                // Read the remaining data to clear the buffers
                while(fgets(rule, sizeof(rule), fp) != NULL);
                pclose(fp);
                fp = NULL;
                data_error = TRUE;
                *buf = 0;
                i = 0;
                break;
            }
            buf = ptr;
            *buf_len_ptr += IPTABLES_REALLOC_SIZE;
        }

        // Find out rule position in the chain
        char *ptr = strchr(rule, ' ');
        ptr = ptr ? strchr(ptr + 1, ' ') : NULL;
        if(last_rule_clen > 0 && ptr && (ptr - rule) == last_rule_clen &&
           memcmp(last_rule, rule, last_rule_clen) == 0)
        {
           ++rule_num;
        }
        else
        {
            rule_num = 1;
            last_rule_clen = ptr ? (ptr - rule) : 0;
            last_rule_clen = last_rule_clen <= sizeof(last_rule) ?
                             last_rule_clen : sizeof(last_rule);
            memcpy(last_rule, rule, last_rule_clen);
        }

        // Store the rule
        memcpy(&buf[i], cmd_prefix, cmd_len);
        i += cmd_len;
        i += sprintf(&buf[i], "%d ", rule_num);
        memcpy(&buf[i], rule, rule_len);
        i += rule_len;
    }

    if(fp) {
        int status = pclose(fp);
        if(status < 0 || !(WIFEXITED(status) && WEXITSTATUS(status) ==0)) {
            log("%s: error collecting iptable rules, status: 0x%x\n",
                    __func__, status);
            // Setting this flag will prevent us from using the bad data
            data_error = TRUE;
        }
    }

    // Update the length of the string
    *str_len_ptr = i;
    // We need 0 at the end of the buffer for strchr() in diff routine
    // looking for \n to stop when we reach the end of it.
    buf[i] = 0;
    return buf;
}

// Read all iptables rules from iptables commands
// For now we read the output of the commands
// "iptables --list-rules" and "iptables -t nat --list-rules"
static char *read_iptables_rules(char *buf, int *buf_len_ptr, int *str_len_ptr)
{
    // start assuming no error
    data_error = FALSE;

    // Read iptables --list-rules
    buf = read_iptables_rules_cmd(IPTABLES_RULES, IPTABLES_FLT_CMD, 
                                  buf, 0, buf_len_ptr, str_len_ptr);

    // No point to run the second command if having an error already.
    if(data_error) {
        return buf;
    }

    // Read and append iptables -t nat --list-rules
    buf = read_iptables_rules_cmd(IPTABLES_NAT_RULES, IPTABLES_NAT_CMD,
                                  buf, *str_len_ptr, 
                                  buf_len_ptr, str_len_ptr);
    return buf;
}

// Compute the diffs if any between two sets of iptables rules
// ipt: iptables list
// ipt_prev: iptables second list
// full_data: Where to store the diffs
// max_diff_entries: Maximum number of entries to report
// max_diff_length: Maximum length of each entry
// start_offset: where to start filling in full_data
// skip_offset: Skip these many entries from ipt (Already sent)
// Return value: Number of entries that ipt differs (+ / -) from ipt_prev
static int compute_ipt_diffs(char *ipt, char *ipt_prev,
                             char full_data[MAX_IPT_DIFF_ENTRIES]
                                           [MAX_IPT_ENTRY_LENGTH],
                             int max_diff_entries,
                             int max_diff_length,
                             int start_offset,
                             int skip_offset,
                             char *plus)
{
    char *rule_start;
    char *rule_end;
    char *rule_start_prev;
    char *rule_end_prev;
    int  found = 0;
    int  ipt_diff_num = start_offset;
    int  skip_until = 0;

    rule_start = ipt;
    // Get each line from ipt
    // And check if it exists in the other list
    while((rule_end = strchr(rule_start, '\n')) != NULL)
    {
        rule_start_prev = ipt_prev;
        found = 0;

        // Iterate through each entry from the second list
        while((rule_end_prev = strchr(rule_start_prev, '\n')) != NULL)
        {
            // length comparison eliminates lot of expensive operations
            if((rule_end - rule_start) != (rule_end_prev - rule_start_prev))
            {
                rule_start_prev = rule_end_prev + 1;//+1 is to skip \n character
                continue;
            }
            // Are the entries the same?
            if(strncmp(rule_start, rule_start_prev,
                       rule_end - rule_start) == 0)
            {
                // Entry exists in both lists, nothing to do.
                found = 1;
                break;
            }
            rule_start_prev = rule_end_prev + 1; // +1 is to skip \n character
        }

        if(!found)
        {
            if(ipt_diff_num < max_diff_entries)
            {
                if(skip_until < skip_offset) {
                    // Go for next line
                    skip_until++;
                } else {
                    // Copy table type, then +/-, then the rule itself
                    full_data[ipt_diff_num][0] = *rule_start;
                    full_data[ipt_diff_num][1] = *plus;
                    memcpy(full_data[ipt_diff_num] + 2, rule_start + 1,
                           UTIL_MIN((rule_end - (rule_start + 1)),
                                    MAX_IPT_ENTRY_LENGTH - 3));
                    // Ending 0 is not needed.
                    // We init full_data to 0's before coming here
                    ipt_diff_num++;
                }
            }
        }

        rule_start = rule_end + 1; // +1 is to skip \n character
    }

    return ipt_diff_num;
}

// Load rules reported by iptables command into a memory buffer.
// Rotate the buffers (to keep previous and the new rules)
void ipt_diffs_colect_data(void)
{
    // Initialize buffers if not yet allocated
    if(iptables_buf_prev == NULL)
    {
        iptables_buf_prev = UTIL_MALLOC(IPTABLES_REALLOC_SIZE);
        if(!iptables_buf_prev) {
            log("%s: failed to allocate %d bytes\n",
                __func__, IPTABLES_REALLOC_SIZE);
            data_error = TRUE;
            return;
        }
        iptables_buf_prev_len = IPTABLES_REALLOC_SIZE;
        *iptables_buf_prev = 0;
    }
    if(iptables_buf == NULL)
    {
        iptables_buf = UTIL_MALLOC(IPTABLES_REALLOC_SIZE);
        if(!iptables_buf) {
            log("%s: failed to allocate %d bytes\n",
                __func__, IPTABLES_REALLOC_SIZE);
            data_error = TRUE;
            return;
        }
        iptables_buf_len = IPTABLES_REALLOC_SIZE;
        *iptables_buf = 0;
    }

    // Store prev buffer state
    char *tmp_buf = iptables_buf_prev;
    int tmp_buf_len = iptables_buf_prev_len;
    int tmp_str_len = iptables_str_len_prev;

    // Try to read into prev buf, if success then swap it with current,
    // if fails we will try again next time
    tmp_buf = read_iptables_rules(tmp_buf, &tmp_buf_len, &tmp_str_len);
    if(data_error) {
        return;
    }

    // New data is now in prev buffer, swap the buffers
    iptables_buf_prev = iptables_buf;
    iptables_buf_prev_len = iptables_buf_len;
    iptables_str_len_prev = iptables_str_len;

    iptables_buf = tmp_buf;
    iptables_buf_len = tmp_buf_len;
    iptables_str_len = tmp_str_len;

    return;
}

// Prepare to report diffs for the specified offsets.
// The diff entries are stored in the arrays and reported
// when the router telemetry request JSON payload is generated.
// Parameters:
// plus_offset - reported so far offset for "add" entries
// minus_offset - reported so far offset for "remove" entries
// next_plus_offset_p - ptr to write to plus offset for 
//                      querying the next chunk of diffs
// next_minus_offset_p - ptr to write to minus offset for 
//                       querying the next chunk of diffs
// Returns: TRUE if there are diffs to report
int ipt_diffs_prep_to_send(int plus_offset, int minus_offset,
                           int *next_plus_offset_p, int *next_minus_offset_p)
{
    int ipt_diff_num = 0;

    // If there was a data error do not report anything
    if(data_error || iptables_buf_prev == NULL || iptables_buf == NULL) {
        return FALSE;
    }

    // Check if there are any diffs
    if(iptables_str_len == iptables_str_len_prev &&
       memcmp(iptables_buf_prev, iptables_buf, iptables_str_len) == 0)
    {
        return FALSE;
    }

    // Reset diffs array
    memset(ipt_diff, 0, sizeof(ipt_diff));
    ipt_diff_num = 0;

    // Look for added rules unless deleted rules from previous iteration(s)
    // lookup is still in progress.
    if(minus_offset == 0)
    {
        // Check for added rules
        ipt_diff_num = compute_ipt_diffs(iptables_buf, 
                                         iptables_buf_prev,
                                         ipt_diff,
                                         MAX_IPT_DIFF_ENTRIES, 
                                         MAX_IPT_ENTRY_LENGTH,
                                         0,
                                         plus_offset,
                                         "+");
    }
    if(ipt_diff_num >= MAX_IPT_DIFF_ENTRIES)
    {
        *next_plus_offset_p = plus_offset + MAX_IPT_DIFF_ENTRIES;
        // No need to check the deleted rules
    }
    else
    {
        // All plus entries are sent
        *next_plus_offset_p = plus_offset + ipt_diff_num;
        int start_offset = ipt_diff_num;
        // Check for deleted rules
        ipt_diff_num = compute_ipt_diffs(iptables_buf_prev,
                                         iptables_buf,
                                         ipt_diff,
                                         MAX_IPT_DIFF_ENTRIES, 
                                         MAX_IPT_ENTRY_LENGTH,
                                         start_offset,
                                         minus_offset,
                                         "-");
        if(ipt_diff_num >= MAX_IPT_DIFF_ENTRIES) {
            *next_minus_offset_p = minus_offset + MAX_IPT_DIFF_ENTRIES;
        } else {
            *next_minus_offset_p = minus_offset + ipt_diff_num;
        }
    }

    return (ipt_diff_num > 0);
}

// JSON teplate builder callback for NAT and forwarding table diffs.
// The callback is called for all the entries of one key then the other.
// It never mixes the calls up together. That allows to use single entry
// counter variable for both JSON arrrays the function is used for.
JSON_VAL_TPL_t *ipt_diffs_array_f(char *key, int idx)
{
    static int last_idx = 0;
    static JSON_VAL_TPL_t ipt_diff_val = {
        .type = JSON_VAL_STR
    };

    char *table_type;
    if(strcmp(key, "ipt_filter_diff") == 0) {
        table_type = IPTABLES_FLT_CMD;
    } else if(strcmp(key, "ipt_nat_diff") == 0) {
        table_type = IPTABLES_NAT_CMD;
    } else {
        return NULL;
    }

    // Reset last index if starting a new report or new array
    // Increment if fetching the next entry
    if(idx == 0) {
        last_idx = 0;
    } else {
        ++last_idx;
    }

    // Get next available filter table entry
    int ii;
    char *val = NULL;
    for(ii = last_idx;
        ii < MAX_IPT_DIFF_ENTRIES && *ipt_diff[ii] != 0;
        ++ii)
    {
        // Skip if no match, the string starts with +/- (add/remove),
        // then the next symbol is the table_type (they are 1 char long)
        if(*ipt_diff[ii] != *table_type) {
            continue;
        }
        // The CMD types are 1 character long, skip them
        val = ipt_diff[ii] + 1;
        break;
    }
    last_idx = ii;
    if(!val) {
        return NULL;
    }

    ipt_diff_val.s = val;
    return &ipt_diff_val;
}
