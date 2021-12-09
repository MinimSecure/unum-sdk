// (c) 2018 minim.co
// Router iptables

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// Buffer to hold the output of the iptables commands
static char *iptables_buf = NULL;
// Size of the memory allocated for iptables_buf
static int iptables_buf_len = 0;
// Offset of nat entries
static int iptables_nat_offset = 0;


// Flag indicating an error while collecting the data
static int data_error = FALSE;

// Read the output of the command
// Here the command can be "iptables --list-rules" or "
// "iptables -t nat --list-rules"
// cmd: the iptables command
// buf: the bufer to hold output of the iptables command
// offset: offset from the start of the buffer to fill the command output
// buf_len_ptr: Pointer to maximum  length of the buffer. 
//              Can be increased if needed after allocating more memory to buf
//              it is an IN/OUT parameter.
// str_len_ptr: Pointer to actual length of the buffer (which contains the conent)
//              it is an IN/OUT parameter.
// Returns the buffer with the output of the cmd
static char* read_iptables_rules_cmd(char *cmd,
                                     char *buf, int offset,
                                     int *buf_len_ptr, int *str_len_ptr)
{
    FILE *fp;
    int i = offset; // Start filling the buffer from offset
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

    // Read characters line-by-lines (one line per rule)
    // And store in the buffer
    while(fgets(rule, sizeof(rule), fp) != NULL) {
        int rule_len = strlen(rule);

        // The rule lines should have at least 4 caracters followed by \n
        if(rule_len < 5) {
            log_dbg("%s: skipping invalid rule line: %s\n",
                    __func__, rule);
            continue;
        }
                
        // Make sure we have \n at the end
        if(rule[rule_len - 1] != '\n') {
            log_dbg("%s: rule doesn't end with CR: %s\n",
                    __func__, rule);
            rule[rule_len - 1] = '\n';
        }

        // Realloc buffer if the size is not sufficient
        // It accounts for rule position number in the chain,
        // rule length and the terminating 0 at the end of the buffer.
        if((i + 10 + rule_len + 1) > *buf_len_ptr) {
            char *ptr = UTIL_REALLOC(buf, (*buf_len_ptr+IPTABLES_REALLOC_SIZE));
            if(ptr == NULL) {
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
           memcmp(last_rule, rule, last_rule_clen) == 0) {
            // Do nothing for now
        } else {
            last_rule_clen = ptr ? (ptr - rule) : 0;
            last_rule_clen = last_rule_clen <= sizeof(last_rule) ?
                             last_rule_clen : sizeof(last_rule);
            memcpy(last_rule, rule, last_rule_clen);
        }

        // Store the rule
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
    buf[i] = 0;
    return buf;
}

// Read all iptables rules from iptables commands
// For now we read the output of the commands
// "iptables --list-rules" and "iptables -t nat --list-rules"
static char *read_iptables_rules(char *buf, int *buf_len_ptr)
{
    int str_len_ptr = 0;

    // start assuming no error
    data_error = FALSE;

    // Read iptables --list-rules
    buf = read_iptables_rules_cmd(IPTABLES_FILTER_RULES, 
                                  buf, 0, buf_len_ptr, &str_len_ptr);

    // No point to run the second command if having an error already.
    if(data_error) {
        return buf;
    }

    // Offset in buf where the nat table rules start
    iptables_nat_offset = str_len_ptr;
    // Read and append iptables -t nat --list-rules
    buf = read_iptables_rules_cmd(IPTABLES_NAT_RULES,
                                  buf, str_len_ptr, 
                                  buf_len_ptr, &str_len_ptr);
    return buf;
}

// Load rules reported by iptables command into a memory buffer.
// Rotate the buffers (to keep previous and the new rules)
void ipt_collect_data(void)
{
    if(iptables_buf == NULL) {
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

    // Try to read into iptables_buff,
    // if fails we will try again next time
    iptables_buf = read_iptables_rules(iptables_buf, &iptables_buf_len);
    if(data_error) {
        return;
    }

    return;
}

// Get pointer iptables rules (nat or filter)
// key can be either filter or nat
char* ipt_get_rules(const char *key)
{
    if (data_error) {
        return NULL;
    }

    if(strcmp(key, "filter") == 0) {
        return iptables_buf;
    } else if(strcmp(key, "nat") == 0) {
        return &iptables_buf[iptables_nat_offset];
    } else {
        return NULL;
    }
}
