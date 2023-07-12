#ifndef PE_EXCHANGE_H
#define PE_EXCHANGE_H

#include "pe_common.h"

#define LOG_PREFIX "[PEX]"

struct order {
	int unique_id;
	int trader_id;
	char type[TYPE_BUFFER];
	int order_id;
	char product[PRODUCT_BUFFER];
	int quantity;
	int price;
	struct order* prev;
	struct order* next;
};

struct orderbook {
	struct order* buy_orders;
	struct order* sell_orders;
};

struct level {
	int price;
	int quantity;
	int repeats;
};

struct product {
	char name[PRODUCT_BUFFER];
	int quantity;
	long profit;
};

#endif

// products_reader.c
int count_products(char* file_path);
void read_products(char* file_path, char** product_names);

// orderbook_handler.c
int decode_order_message(char* message, struct order_details* details, char** product_names, int num_products);
int orderbook_qsort_decreasing(const void* a, const void* b);
int get_level_from_orderbook(struct level* levels, struct order* order, int num_orders, char* product_name);
void print_orderbook_product(struct orderbook* orderbook, char* product_name, int num_buy_orders, int num_sell_orders);
void print_orderbook(struct orderbook* orderbook, char** product_names, int num_products, int num_buy_orders, int num_sell_orders);
void print_trader_positions(struct product** trader_products, int num_traders, int num_products);
void match_to_sell_orders(struct order** matching_order_ptr, struct order* buy_order, struct order* sell_order);
void match_to_buy_orders(struct order** matching_order_ptr, struct order* sell_order, struct order* buy_order);
void remove_order(struct orderbook* orderbook, struct order* order);
int match_order(struct orderbook* orderbook, struct order* order, pid_t* trader_pids, int* exchange_fifos, long* total_fees, struct product** trader_products, int num_products);
struct order* find_order(struct orderbook* orderbook, int trader_id, int order_id);
int cancel_order(struct orderbook* orderbook, int trader_id, int order_id);
void update_order(struct order* order, int unique_id, int quantity, int price);
int amend_order(struct orderbook* orderbook, int trader_id, int unique_id, int order_id, int quantity, int price);