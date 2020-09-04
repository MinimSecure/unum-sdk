// (c) 2020 minim.co
// unum dns utils include file

#ifndef _UTIL_DNS_H
#define _UTIL_DNS_H

// Minim hostname domains allowed in DNS TXT record agent uses
// to generates URLs for reaching the cloud service
#ifndef API_SERVER_WHITELIST
#define API_SERVER_WHITELIST {\
        "minim.co",\
        "minim.xyz",\
        NULL\
};
#endif

// Default DNS query timeout for queries built and sent by the agent
// (i.e. those that are sent through util_dns.c APIs)
#define DNS_TIMEOUT 10

// Array with fallback DNS entries for http subsystem. It has to be populated
// during startup before http subsystem could be used by any other thread
// besides the one populating it (there's no concurrent access protection)
// TBD: hide it in dns_util.c and use APIs to access
#define DNS_ENTRIES_MAX 16
#define DNS_ENTRY_MAX_LENGTH 64
extern char dns_entries[DNS_ENTRIES_MAX + 1][DNS_ENTRY_MAX_LENGTH];

// Get Server List from DNS and Populate DNS and Server Mapping
// use_defaults - if TRUE fills the list with static defaults
// Returns: 0 - success, negative value - network error,
//          positive value - internal or data processing error
int util_get_servers(int use_defaults);

// Resolve DNS name to IPv4 address when normal DNS APIs don't work
// This function queries the same public DNS servers we use to get server list.
// name - (in) name to resolve
// addr_str - (out) a buffer for IPv4 string (at least INET_ADDRSTRLEN bytes)
//            it can be NULL to just check if a name resolves
// Returns: 0 - success, negative value - error
int util_nodns_get_ipv4(char *name, char *addr_str);

// Get number of public servers in the DNS server list for out-of-band queries.
// This allows to gauge the timeout for calls to util_nodns_get_ipv4() and
// util_get_servers()
// Returns: number of the servers 
int util_num_oob_dns(void);

// Add entry to the list of DNS etries we use when name resolution
// doesn't work.
// See util_dns.c for the format (i.e. "minim.co:80:34.207.26.129")
// If entry already exists its number is returned and no duplicate
// is added.
// entry - string for adding to dns_entries[]
// Returns: entry number or negative if error
int util_add_nodns_entry(char *entry);

#endif // _UTIL_DNS_H
