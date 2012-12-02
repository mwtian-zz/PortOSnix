#include <stdio.h>
#include <stdlib.h>

#include "miniroute_cache.h"
#include "miniroute.h"
#include "network.h"
#include "miniheader.h"

int main() {
	struct routing_header header;
	network_address_t addr;
	miniroute_path_t item;
	miniroute_cache_t cache;
	char hosts[10][20] = {"www.google.com", "www.cnn.com", "localhost", "www.baidu.com", "www.cornell.edu", "www.cs.cornell.edu", "www.yahoo.com", "www.msn.com", "www.apple.com", "www.ibm.com"};
	int i;

	cache = miniroute_cache_new(20, 1000, 10000);
	miniroute_cache_set_max_num(cache, 5);

	for (i = 0; i < 10; i++) {
		network_translate_hostname(hosts[i], addr);
		pack_address(header.destination, addr);
		pack_unsigned_int(header.path_len, 4);
		item = miniroute_path_from_hdr(&header);
		miniroute_cache_put_item(cache, item);
		miniroute_cache_print_path(cache);
		printf("\n\n");
	}

	for (i = 0; i < 10; i++) {
		network_translate_hostname(hosts[i], addr);
		if (miniroute_cache_get_by_addr(cache, addr, (void**)&item) == 0) {
			printf("Found host %s\n", hosts[i]);
			miniroute_cache_delete_item(cache, item);
			miniroute_cache_print_path(cache);
		}
	}

	return 0;
}
