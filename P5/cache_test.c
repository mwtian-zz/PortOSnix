#include <stdio.h>
#include <stdlib.h>

#include "cache.h"
#include "miniroute.h"
#include "network.h"
#include "miniheader.h"

int main() {
	struct routing_header header;
	network_address_t addr;
	cache_item_t item;
	cache_t cache;
	char hosts[10][20] = {"www.google.com", "www.cnn.com", "localhost", "www.baidu.com", "www.cornell.edu", "www.cs.cornell.edu", "www.yahoo.com", "www.msn.com", "www.apple.com", "www.ibm.com"};
	int i;
	
	cache = cache_new(20);
	cache_set_max_num(cache, 5);
	
	for (i = 0; i < 10; i++) {
		network_translate_hostname(hosts[i], addr);
		pack_address(header.destination, addr);
		pack_unsigned_int(header.path_len, 4);
		item = item_new(header);
		cache_put_item(cache, item);
		cache_print(cache);
		printf("\n\n");
	}
	
	for (i = 0; i < 10; i++) {
		network_translate_hostname(hosts[i], addr);
		if (cache_get_by_addr(cache, addr, &item) == 0) {
			printf("Found host %s\n", hosts[i]);
			cache_delete_item(cache, item);
			cache_print(cache);
		}
	}
	
	return 0;
}