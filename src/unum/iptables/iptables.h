// (c) 2021 minim.co
// unum router iptables include file

#ifndef __IPTABLES_H
#define __IPTABLES_H

// Note: this has to go to platform specific includes and
//       has to use full path to the iptables command.
// iptables command to use for getting filter table rules
// If not already defined by the platform
#ifndef IPTABLES_FILTER_RULES
#define IPTABLES_FILTER_RULES "iptables --list-rules"
#endif // IPTABLES_FILTER_RULES
// iptables command to use for getting NAT table rules
#ifndef IPTABLES_NAT_RULES
#define IPTABLES_NAT_RULES "iptables -t nat --list-rules"
#endif // IPTABLES_NAT_RULES

// Additional space to allocate when need more space for rules
#define IPTABLES_REALLOC_SIZE 4096

// Max length of a rule in the report
#define MAX_IPT_ENTRY_LENGTH 256

// Load rules reported by iptables command into a memory buffer.
// Rotate the buffers (to keep previous and the new rules.
void ipt_collect_data(void);

// Subsystem init function
int iptables_init(int level);

// Get pointer iptables rules (nat or filter)
// key can be either filter or nat
char* ipt_get_rules(const char *key);

#endif // __IPTABLES_H
