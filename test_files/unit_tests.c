#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include "cmocka.h"

#include "../pe_exchange.h"

// Test if count_products reads the correct number of products
static void count_products_test(void **state) {	
	char* filepath = "test_file.txt";
	FILE *file = fopen(filepath, "w");
    fprintf(file, "5\nBread\nBanana\nFish\nCheese\nApple\n");
    fclose(file);	
    assert_int_equal(count_products(filepath), 5);
    remove(filepath);
}

// Test if read_products correctly reads and stores products from the file into an array
static void read_products_test(void **state) {
	char* filepath = "test_file.txt";
	FILE *file = fopen("test_file.txt", "w");
    fprintf(file, "3\nBread\nBanana\nFish\n");
    fclose(file);	
	
	int num_products = count_products(filepath);
	assert_int_equal(num_products, 3);
	
	char** product_names = test_malloc(num_products * sizeof(char*));	
	read_products(filepath, product_names);
	remove(filepath);
	
	assert_true(strcmp(product_names[0], "Bread") == 0);
	assert_true(strcmp(product_names[1], "Banana") == 0);
	assert_true(strcmp(product_names[2], "Fish") == 0);
    
	for (int i = 0; i < num_products; i++) {
		free(product_names[i]);
		product_names[i] = NULL;
	}
    test_free(product_names);
}

// Test if trader messages are correctly decoded and stored in a struct
static void decode_test(void **state) {	
	int num_products = 2;
	char* product_names_arr[] = {"Apple", "Banana"};
	char** product_names = product_names_arr;
	
	// Fail case (invalid product)
	struct order_details fail_details;
	int fail_result = decode_order_message("BUY 0 Cheese 3 450", &fail_details, product_names, num_products);
	assert_true(fail_result != 0);
	
	// Success case
	struct order_details succ_details;
	int succ_result = decode_order_message("BUY 0 Apple 3 450", &succ_details, product_names, num_products);
	assert_true(succ_result == 0);
	assert_true(strcmp(succ_details.type, "BUY") == 0);
	assert_true(strcmp(succ_details.product, "Apple") == 0);
	assert_true(succ_details.quantity == 3);
	assert_true(succ_details.price == 450);
}

static void amend_test(void **state) {
	struct orderbook orderbook;
	orderbook.buy_orders = NULL;
	orderbook.sell_orders = NULL;
	
	int unique_id = 0;
	
	// Create test order
	struct order* new_order = (struct order*) test_malloc(sizeof(struct order));						
	new_order->unique_id = unique_id;
	new_order->trader_id = 0;
	strcpy(new_order->type, "BUY");
	new_order->order_id = 0;
	strcpy(new_order->product, "Apple");
	new_order->quantity = 5;
	new_order->price = 100;
	new_order->prev = NULL;
	new_order->next = NULL;
	orderbook.buy_orders = new_order;
	
	// Check correct initialisation
	assert_non_null(orderbook.buy_orders);
	assert_null(orderbook.sell_orders);
	assert_int_equal(new_order->unique_id, 0);
	assert_int_equal(new_order->quantity, 5);
	assert_int_equal(new_order->price, 100);
	
	unique_id++;
	
	// Success case
	int succ_result = amend_order(&orderbook, new_order->trader_id, unique_id, new_order->order_id, 20, 150);	
	assert_int_equal(succ_result, 0);
	assert_int_equal(new_order->unique_id, 1);
	assert_int_equal(new_order->quantity, 20);
	assert_int_equal(new_order->price, 150);
	
	test_free(new_order);
}

static void cancel_test(void **state) {
	struct orderbook orderbook;
	orderbook.buy_orders = NULL;
	orderbook.sell_orders = NULL;
	
	int unique_id = 0;
	
	// Create test order
	struct order* new_order = (struct order*) malloc(sizeof(struct order));						
	new_order->unique_id = unique_id;
	new_order->trader_id = 0;
	strcpy(new_order->type, "BUY");
	new_order->order_id = 0;
	strcpy(new_order->product, "Apple");
	new_order->quantity = 5;
	new_order->price = 100;
	new_order->prev = NULL;
	new_order->next = NULL;
	orderbook.buy_orders = new_order;
	
	// Check correct initialisation
	assert_non_null(new_order);
	assert_non_null(orderbook.buy_orders);
	assert_null(orderbook.sell_orders);	
	
	unique_id++;
	
	// Success case
	int succ_result = cancel_order(&orderbook, new_order->trader_id, new_order->order_id); // order freed here		
	new_order = NULL;
	assert_int_equal(succ_result, 0);
	assert_null(new_order);
	assert_null(orderbook.buy_orders);
	assert_null(orderbook.sell_orders);
}

int main(void) {
    const struct CMUnitTest tests[] = {
		cmocka_unit_test(count_products_test),
		cmocka_unit_test(read_products_test),
		cmocka_unit_test(decode_test),
		cmocka_unit_test(amend_test),
		cmocka_unit_test(cancel_test),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}