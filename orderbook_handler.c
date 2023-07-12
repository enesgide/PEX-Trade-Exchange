#include "pe_exchange.h"

// Decode the message and store its data in the order_details struct
int decode_order_message(char* message, struct order_details* details, char** product_names, int num_products) {	
	char attribute[PRODUCT_BUFFER];
	char space[MESSAGE_BUFFER];
	
	// Check order type
	sscanf(message, "%s %s", attribute, space);
	
	// Create temporary variables
	char type[TYPE_BUFFER];
	int id = -1;
	char product[PRODUCT_BUFFER];
	int quantity = -1;
	int price = -1;
	char excess[5];
	strcpy(excess, "");
	
	// Create struct based on order type
	if (strcmp(attribute, "BUY") == 0 || strcmp(attribute, "SELL") == 0) {		
		sscanf(message, "%s %d %s %d %d %s", type, &id, product, &quantity, &price, excess);
		strcpy(details->product, product);
		details->quantity = quantity;
		details->price = price;
		
		if (quantity < 1 || quantity >  999999) { return 1; }
		if (price < 1 || price >  999999) { return 1; }
		
		int found_product_name = -1;
		for (int i = 0; i < num_products; i++) {
			char* p = product_names[i];
			if (strcmp(p, product) == 0) {
				found_product_name = 0;
				break;
			}
		}
		if (found_product_name == -1) {
			return 3;
		}
		
	} else if (strcmp(attribute, "AMEND") == 0) {
		sscanf(message, "%s %d %d %d %s", type, &id, &quantity, &price, excess);		
		strcpy(details->product, "\0");
		details->quantity = quantity;
		details->price = price;	
		
		if (quantity < 1 || quantity >  999999) { return 1; }
		if (price < 1 || price >  999999) { return 1; }
		
	} else if (strcmp(attribute, "CANCEL") == 0) {
		sscanf(message, "%s %d %s", type, &id, excess);
		strcpy(details->product, "\0");
		details->quantity = 1;
		details->price = 1;
		
	} else {		
		return 2;
	}
	
	// Common attributes
	strcpy(details->type, type);
	details->id = id;
	
	if (id < 0 || id >  999999) { return 1; }	
	
	if (strlen(excess) > 0) {
		//printf("Space: (%s) with len(%ld)\n", excess, strlen(excess));
		return 1;
	}	
	
	return 0;
}

// Qsort to sort orders in decreasing order (price: high to low)
int orderbook_qsort_decreasing(const void* a, const void* b) {
	struct level* level_a = (struct level*) a;
	struct level* level_b = (struct level*) b;
	
	if (level_a->price < level_b->price) {
		return 1;
	} else if (level_a->price > level_b->price) {
		return -1;
	} 
	
	return 0;
}

// Check the level of buy/sell orders
int get_level_from_orderbook(struct level* levels, struct order* order, int num_orders, char* product_name) {
	int num_levels = 0;
		
	while (order != NULL) {
		if (strcmp(order->product, product_name) == 0) {
			int found_index = -1;
			for (int i = 0; i < num_orders; i++) {
				struct level *check_level = &levels[i];
				if (check_level->price == order->price) {
					found_index = i;
				}
			}
			if (found_index != -1) {
				struct level *found_level = &levels[found_index];	
				found_level->quantity += order->quantity;				
				found_level->repeats++;	
			} else {
				struct level *new_level = &levels[num_levels];
				new_level->price = order->price;
				new_level->quantity = order->quantity;
				new_level->repeats = 1;	
				num_levels++;		
			}						
		}
		order = order->next;
	}	
	
	return num_levels;
}

// Print out the information for a product in the orderbook
void print_orderbook_product(struct orderbook* orderbook, char* product_name, int num_buy_orders, int num_sell_orders) {
	
	// Product levels
	struct level* buy_levels = (struct level*) malloc(num_buy_orders * sizeof(struct level));
	for (int i = 0; i < num_buy_orders; i++) {
		struct level temp_level = {-1, 0};
		buy_levels[i] = temp_level;
	}
	int num_buy_levels = get_level_from_orderbook(buy_levels, orderbook->buy_orders, num_buy_orders, product_name);	

	struct level* sell_levels = (struct level*) malloc(num_sell_orders * sizeof(struct level));
	for (int i = 0; i < num_sell_orders; i++) {
		struct level temp_level = {-1, 0};
		sell_levels[i] = temp_level;
	}
	int num_sell_levels = get_level_from_orderbook(sell_levels, orderbook->sell_orders, num_sell_orders, product_name);	
	
	printf("[PEX]\tProduct: %s; Buy levels: %d; Sell levels: %d\n", product_name, num_buy_levels, num_sell_levels);
	
	// Sell orders
	qsort(sell_levels, num_sell_levels, sizeof(struct level), orderbook_qsort_decreasing);
	
	for (int i = 0; i < num_sell_levels; i++) {
		struct level* level_info = &sell_levels[i];
		if (level_info->repeats > 1) {
			printf("[PEX]\t\tSELL %d @ $%d (%d orders)\n", level_info->quantity, level_info->price, level_info->repeats);
		} else {
			printf("[PEX]\t\tSELL %d @ $%d (%d order)\n", level_info->quantity, level_info->price, level_info->repeats);
		}	
	}
	
	// Buy orders	
	qsort(buy_levels, num_buy_levels, sizeof(struct level), orderbook_qsort_decreasing);
	
	for (int i = 0; i < num_buy_levels; i++) {
		struct level* level_info = &buy_levels[i];				
		if (level_info->repeats > 1) {
			printf("[PEX]\t\tBUY %d @ $%d (%d orders)\n", level_info->quantity, level_info->price, level_info->repeats);
		} else {
			printf("[PEX]\t\tBUY %d @ $%d (%d order)\n", level_info->quantity, level_info->price, level_info->repeats);
		}		
	}
	
	// Free dynamic allocations
	free(buy_levels);
	free(sell_levels);
}

// Print out the general orderbook format
void print_orderbook(struct orderbook* orderbook, char** product_names, int num_products, int num_buy_orders, int num_sell_orders) {	
	printf("[PEX]\t--ORDERBOOK--\n");

	for (int i = 0; i < num_products; i++) {
		print_orderbook_product(orderbook, product_names[i], num_buy_orders, num_sell_orders);
	}
}

// Print out the trader positions (owned products, quantity, value)
void print_trader_positions(struct product** trader_products, int num_traders, int num_products) {
	printf("[PEX]\t--POSITIONS--\n");
	for (int trader_id = 0; trader_id < num_traders; trader_id++) {
		printf("[PEX]\tTrader %d: ", trader_id);
		struct product* products = trader_products[trader_id];
		for (int i = 0; i < num_products; i++) {
			struct product p = products[i];
			if (i == num_products - 1) {
				printf("%s %d ($%ld)\n", p.name, p.quantity, p.profit);
			} else {
				printf("%s %d ($%ld), ", p.name, p.quantity, p.profit);
			}			
		}
	}
}

// Try matching the given buy order to a sell order
void match_to_sell_orders(struct order** matching_order_ptr, struct order* buy_order, struct order* sell_order) {	
	// No complementary orders
	if (sell_order == NULL) { 
		return;
	}
	
	// Find matching order
	while (sell_order != NULL) {
		if (strcmp(sell_order->product, buy_order->product) == 0) {
			if (buy_order->price >= sell_order->price) {
				if (*matching_order_ptr == NULL) {
					*matching_order_ptr = sell_order;
				} else {
					// Match to best price, and if equal price then oldest order
					// Consider that amended orders will appear first, but may be 'newer' since the time prioritiy resets
					if ((sell_order->price < (*matching_order_ptr)->price) || ((sell_order->price == (*matching_order_ptr)->price) && (sell_order->unique_id < (*matching_order_ptr)->unique_id))) {						
						*matching_order_ptr = sell_order;
					}
				}
			}
		}
		sell_order = sell_order->next;
	}
}

// Try matching the given sell order to a buy order
void match_to_buy_orders(struct order** matching_order_ptr, struct order* sell_order, struct order* buy_order) {
	// No complementary orders
	if (buy_order == NULL) { 
		return;
	}
	
	// Find matching order
	while (buy_order != NULL) {
		if (strcmp(sell_order->product, buy_order->product) == 0) {
			if (buy_order->price >= sell_order->price) {
				if (*matching_order_ptr == NULL) {
					*matching_order_ptr = buy_order;
				} else { 
					if ((buy_order->price > (*matching_order_ptr)->price) || ((buy_order->price == (*matching_order_ptr)->price) && (buy_order->unique_id < (*matching_order_ptr)->unique_id))) {							
						*matching_order_ptr = buy_order;
					} // May need else if == statement, and compare unique ids to get older/newer order
				}
			}
		}
		buy_order = buy_order->next;
	}
}

// Remove the order from the list and free as necessary
void remove_order(struct orderbook* orderbook, struct order* order) {
	if (orderbook->buy_orders == order) { // First order
		orderbook->buy_orders = order->next;
		if (orderbook->buy_orders != NULL) {
			(orderbook->buy_orders)->prev = NULL;
		}			
	} else if (orderbook->sell_orders == order) { // First order
		orderbook->sell_orders = order->next;
		if (orderbook->sell_orders != NULL) {
			(orderbook->buy_orders)->prev = NULL;
		}	
	} else {
		struct order* next_order = order->next;
		if (next_order == NULL) { // Last order in the list
			(order->prev)->next = NULL;
		} else { // Middle order
			(order->prev)->next = order->next;
			(order->next)->prev = order->prev;
		}
	}		
	free(order); 
}

// Find a matching order
int match_order(struct orderbook* orderbook, struct order* order, pid_t* trader_pids, int* exchange_fifos, long* total_fees, struct product** trader_products, int num_products) {
	// Initialise matching_order to the complementary order type list
	struct order* matching_order = NULL;
	
	if (strcmp(order->type, "BUY") == 0) {
		match_to_sell_orders(&matching_order, order, orderbook->sell_orders);
	} else if (strcmp(order->type, "SELL") == 0) {
		match_to_buy_orders(&matching_order, order, orderbook->buy_orders);
	} else {
		return 1;
	}

	if (matching_order == NULL) { return 1; }
	
	// Order age
	struct order* older_order;
	if (order->unique_id < matching_order->unique_id) {
		older_order = order;
	} else {
		older_order = matching_order;
	}
	
	// Trader / order ids
	struct order* buyer_order;
	struct order* seller_order;
	
	int buyer_id;
	int seller_id;
	
	if (strcmp(order->type, "BUY") == 0) {
		buyer_order = order;
		buyer_id = order->trader_id;
		seller_order = matching_order;
		seller_id = matching_order->trader_id;
	} else {
		buyer_order = matching_order;
		buyer_id = matching_order->trader_id;
		seller_order = order;
		seller_id = order->trader_id;
	}
	
	// Order price 
	int price = older_order->price; // Matching price is the price of the older order
	
	// Order quanty
	int quantity = (order->quantity <= matching_order->quantity) ? order->quantity : matching_order->quantity;
	order->quantity -= quantity;
	matching_order->quantity -= quantity;
	
	// Print match
	long value = (long) price * quantity;	
	long fee = round(value/100.0);
	*total_fees += fee;
	
	printf("[PEX] Match: Order %d [T%d], New Order %d [T%d], value: $%ld, fee: $%ld.\n", matching_order->order_id, matching_order->trader_id, order->order_id, order->trader_id, value, fee);
	
	// Message buyer
	char message[MESSAGE_BUFFER];
	sprintf(message, "FILL %d %d;", buyer_order->order_id, quantity);
	if (trader_pids[buyer_id] != -1 && write(exchange_fifos[buyer_id], message, strlen(message)) != -1) { kill(trader_pids[buyer_id], SIGUSR1); }
		
	// Message seller
	sprintf(message, "FILL %d %d;", seller_order->order_id, quantity);
	if (trader_pids[seller_id] != -1 && write(exchange_fifos[seller_id], message, strlen(message)) != -1) { kill(trader_pids[seller_id], SIGUSR1); }
	
	// Buyer products
	char* product_name = order->product;
	struct product* buyer_products = trader_products[buyer_id];
	for (int i = 0; i < num_products; i++) {
		struct product* p = &(buyer_products[i]);
		if (strcmp(p->name, product_name) == 0) {
			p->quantity += quantity;
			if (buyer_order->unique_id > seller_order->unique_id) { // If buyer is newer, pay transaction fee
				p->profit -= (value + fee);
			} else {
				p->profit -= value;
			}			
		}			
	}
	
	// Seller products
	struct product* seller_products = trader_products[seller_id];
	for (int i = 0; i < num_products; i++) {
		struct product* p = &(seller_products[i]);
		if (strcmp(p->name, product_name) == 0) {
			p->quantity -= quantity;
			if (seller_order->unique_id > buyer_order->unique_id) { // If seller is newer, pay transaction fee
				p->profit += (value - fee);
			} else {
				p->profit += value;
			}
		}			
	}
		
	// Remove completed orders
	if (matching_order->quantity == 0) {
		remove_order(orderbook, matching_order);
	}
	
	if (order->quantity == 0) {
		remove_order(orderbook, order);
		return 1; // Return 1 to break the loop (so that it doesn't try to reuse order as parameter in the next iteration)
	}
	
	return 0;
}

// Find the order pointer using the trader_id and order_id
struct order* find_order(struct orderbook* orderbook, int trader_id, int order_id) {
	struct order* order = orderbook->buy_orders;	
	while (order != NULL) {
		if (order->order_id == order_id && order->trader_id == trader_id) {
			return order;
		}
		order = order->next;
	}
	
	order = orderbook->sell_orders;	
	while (order != NULL) {
		if (order->order_id == order_id && order->trader_id == trader_id) {
			return order;
		}
		order = order->next;
	}
	
	return NULL;
}

// Cancel the order
int cancel_order(struct orderbook* orderbook, int trader_id, int order_id) {
	struct order* order = find_order(orderbook, trader_id, order_id);
	if (order != NULL) { 
		remove_order(orderbook, order); 
		return 0;
	}
	return 1;
}

// Update the order in accordance with the AMEND command
void update_order(struct order* order, int unique_id, int quantity, int price) {
	order->unique_id = unique_id;
	order->quantity = quantity;
	order->price = price;
}

// Amend the order
int amend_order(struct orderbook* orderbook, int trader_id, int unique_id, int order_id, int quantity, int price) {	
	struct order* order = find_order(orderbook, trader_id, order_id);
	if (order != NULL) { 
		update_order(order, unique_id, quantity, price); 
		return 0;
	}
	return 1;
}