/**
 * comp2017 - assignment 3
 * Muhammed Enes Gide
 * mgid9133
 */

#include "pe_exchange.h"
#include "products_reader.c"
#include "orderbook_handler.c"

// Handle SIGUSR1 signals (new messages)
volatile sig_atomic_t new_message = 0;
volatile sig_atomic_t new_message_pid = 0;

void message_signal(int sig, siginfo_t *info, void *context) {	
	new_message++;
	new_message_pid = info->si_pid;
}

// Handle SIGCHLD signals (disconnected traders)
volatile sig_atomic_t running_traders = 0;
volatile sig_atomic_t trader_disconnected = 0;

void disconnect_signal(int sig, siginfo_t *info, void *context) {	
	running_traders--;
	trader_disconnected = 1;
}

// Start up a new trader program
void execute_trader(int trader_id, char* trader_binary) {
	char trader_id_str[12];
	sprintf(trader_id_str, "%d", trader_id);
	char* exec_args[] = {trader_binary, trader_id_str, NULL};
	printf("[PEX] Starting trader %d (%s)\n", trader_id, trader_binary);
	int succ = execvp(trader_binary, exec_args);
	if (succ == -1) {
		exit(1);
	}
}

// Disconnect trader from market
void disconnect_traders(pid_t* trader_pids, int num_traders) {
	for (int trader_id = 0; trader_id < num_traders; trader_id++) {
		int trader_pid = trader_pids[trader_id];
		if (trader_pid != -1) {			
			int dc_pid = waitpid(trader_pid, NULL, WNOHANG); 
			if (dc_pid != 0) {
				trader_pids[trader_id] = -1;
				printf("[PEX] Trader %d disconnected\n", trader_id);					
			}						
		}
	}
}

// Free all dynamically allocated memory

void free_products(char** product_names, int num_products) {
	// Products
	if (product_names != NULL) {
		for (int i = 0; i < num_products; i++) {
			if (product_names[i] != NULL) {
				free(product_names[i]);
				product_names[i] = NULL;
			}
		}
		free(product_names);
		product_names = NULL;
	}
}

void free_traders(pid_t* trader_pids, struct product** trader_products, int num_traders, char** product_names, int num_products) {	
	// Trader products	
	if (trader_products != NULL) {
		for (int i = 0; i < num_traders; i++) {
			if (trader_products[i] != NULL) {
				free(trader_products[i]);
				trader_products[i] = NULL;
			}
		}
		free(trader_products);
		trader_products = NULL;
	}	
	
	// Trader PIDs
	if (trader_pids != NULL) {
		/*for (int i = 0; i < num_traders; i++) {
			if (trader_pids > 0) {
				kill(trader_pids[i], SIGTERM);
				trader_pids[i] = -1;
			}
		}*/
		free(trader_pids);
		trader_pids = NULL;
	}
}

void free_fifos(int* exchange_fifos, int* trader_fifos, char** exchange_fifo_names, char** trader_fifo_names, int num_traders) {
	// Clean up FIFOs
	for (int i = 0; i < num_traders; i++) {		
		close(exchange_fifos[i]);
		close(trader_fifos[i]);
		unlink(exchange_fifo_names[i]);
		unlink(trader_fifo_names[i]);
	}
	
	// FIFO names
	for (int i = 0; i < num_traders; i++) {
		if (exchange_fifo_names[i] != NULL) {
			free(exchange_fifo_names[i]);
			exchange_fifo_names[i] = NULL;
		}
	}
	if (exchange_fifo_names != NULL) {
		free(exchange_fifo_names);
		exchange_fifo_names = NULL;
	}
	
	for (int i = 0; i < num_traders; i++) {
		if (trader_fifo_names[i] != NULL) {
			free(trader_fifo_names[i]);
			trader_fifo_names[i] = NULL;
		}
	}
	if (trader_fifo_names != NULL) {
		free(trader_fifo_names);
		trader_fifo_names = NULL;
	}	
	
	// FIFOs
	if (exchange_fifos != NULL) {
		free(exchange_fifos);
		exchange_fifos = NULL;
	}
	if (trader_fifos != NULL) {
		free(trader_fifos);
		trader_fifos = NULL;
	}
}

void free_orderbook(struct orderbook* orderbook, int* expected_order_ids) {
	// Orderbook
	struct order* free_order = orderbook->buy_orders;
	struct order* next_order = NULL;
	while (free_order != NULL) {
		next_order = free_order->next;
		free(free_order);
		free_order = next_order;
	}
	
	free_order = orderbook->sell_orders;
	next_order = NULL;
	while (free_order != NULL) {
		next_order = free_order->next;
		free(free_order);
		free_order = next_order;
	}
	
	// Order ids
	if (expected_order_ids != NULL) {
		free(expected_order_ids);
		expected_order_ids = NULL;
	}
}

// Main function
int main(int argc, char** argv) {
	if (argc < 3) {
        printf("Not enough arguments\n");
        return 1;
    }
	
	printf("[PEX] Starting\n");
	
	// Read products
	char* products_path = argv[1];
	int num_products = count_products(products_path);	
	char** product_names = malloc(num_products * sizeof(char*));
	read_products(products_path, product_names);
	
	// Prepare exchange/trader variables
	long fees_collected = 0;
	int unique_order_id = 0;
	
	int num_traders = argc - 2;
	running_traders = num_traders;
	pid_t pid;		
	pid_t* trader_pids = (pid_t*) malloc(num_traders * sizeof(pid_t));	
	
	// Prepare orderbook
	struct orderbook orderbook;
	orderbook.buy_orders = NULL;
	orderbook.sell_orders = NULL;
	
	int num_buy_orders = 0;
	int num_sell_orders = 0;
	
	int* expected_order_ids = (int*) calloc(num_traders, sizeof(int));
	
	// Prepare trader positions
	struct product** trader_products = (struct product**) malloc(num_traders * sizeof(struct product*));
	for (int i = 0; i < num_traders; i++) {
		trader_products[i] = (struct product*) malloc(num_products * sizeof(struct product));
	}
	
	// Register message signal handler
	struct sigaction sa_usr1 = {0};
	sa_usr1.sa_sigaction = &message_signal;
	sa_usr1.sa_flags = SA_SIGINFO | SA_RESTART;
	sigaction(SIGUSR1, &sa_usr1, NULL);
	
	// Register trader disconnect signal handler
	struct sigaction sa_chld = {0};
	sa_chld.sa_sigaction = &disconnect_signal;
	sigemptyset(&sa_chld.sa_mask);
	sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
	sigaction(SIGCHLD, &sa_chld, NULL);
	
	// Create FIFOs			
	int* exchange_fifos = (int*) malloc(num_traders * sizeof(int));
	int* trader_fifos = (int*) malloc(num_traders * sizeof(int));
	char** exchange_fifo_names = malloc(num_traders * sizeof(char*));
	char** trader_fifo_names = malloc(num_traders * sizeof(char*));
		
	char exchange_fifo[FILENAME_BUFFER];
	char trader_fifo[FILENAME_BUFFER];	
	
	// Create and open FIFOs + child (trader) processes
	for (int trader_id = 0; trader_id < num_traders; trader_id++) {	
		// Set up FIFO for trader
		sprintf(exchange_fifo, "/tmp/pe_exchange_%d", trader_id);
		sprintf(trader_fifo, "/tmp/pe_trader_%d", trader_id);
		
		// Exchange -> trader FIFO
		if (mkfifo(exchange_fifo, 0777) == -1) {
			if (errno != EEXIST) {
				printf("exchange_fifo err\n");
			}
		}
		exchange_fifo_names[trader_id] = malloc(strlen(exchange_fifo) + 1);
		strcpy(exchange_fifo_names[trader_id], exchange_fifo);
		printf("[PEX] Created FIFO %s\n", exchange_fifo);
		
		// Trader -> exchange FIFO
		if (mkfifo(trader_fifo, 0777) == -1) {
			if (errno != EEXIST) {
				printf("trader_fifo err\n");
			}
		}
		trader_fifo_names[trader_id] = malloc(strlen(trader_fifo) + 1);
		strcpy(trader_fifo_names[trader_id], trader_fifo);
		printf("[PEX] Created FIFO %s\n", trader_fifo);	
		
		// Create child processes and execute traders
		pid = fork();		
		if (pid == -1) { break; }		
		if (pid == 0) {
			execute_trader(trader_id, argv[trader_id + 2]);
			printf("Exec failed");
			pid = -1;
			break;
		}
		trader_pids[trader_id] = pid;
		
		// Initialise trader products
		for (int product_i = 0; product_i < num_products; product_i++) {
			struct product new_product;
			strcpy(new_product.name, product_names[product_i]);
			new_product.quantity = 0;
			new_product.profit = 0;
			trader_products[trader_id][product_i] = new_product;
		}
		
		// Open and connect FIFOs
		char* exchange_fifo = exchange_fifo_names[trader_id];	
		int pe_exchange = open(exchange_fifo, O_WRONLY);
		if (pe_exchange != -1) {			
			exchange_fifos[trader_id] = pe_exchange;
			printf("[PEX] Connected to %s\n", exchange_fifo);
		}
		
		char* trader_fifo = trader_fifo_names[trader_id];
		int pe_trader = open(trader_fifo, O_RDONLY);
		if (pe_trader != -1) {				
			trader_fifos[trader_id] = pe_trader;		
			printf("[PEX] Connected to %s\n", trader_fifo);
		}
	}	
		
	// Error occurred, exit
	if (pid == -1 || running_traders < num_traders) {
		free_fifos(exchange_fifos, trader_fifos, exchange_fifo_names, trader_fifo_names, num_traders);	
		free_traders(trader_pids, trader_products, num_traders, product_names, num_products);
		free_products(product_names, num_products);	
		free_orderbook(&orderbook, expected_order_ids);
		exit(1);
	}
	
	// Return if trader / child process
	if (pid == 0) { return 0; }
	
	// Exchange / Parent process

	// Write to traders about market open
	for (int trader_id = 0; trader_id < num_traders; trader_id++) {
		char* message = "MARKET OPEN;";
		int message_bytes = strlen(message) * sizeof(char);
		if (write(exchange_fifos[trader_id], message, message_bytes) == -1) { // Signal is sent in the for loop below	
			printf("Error writing to FIFO\n");
		}	
	}
	
	// Signal all traders
	for (int trader_id = 0; trader_id < num_traders; trader_id++) {
		kill(trader_pids[trader_id], SIGUSR1);
	}

	// Event loop
		
	while(running_traders > 0 || new_message > 0) {
		// Pause until unread message is found
		if (new_message == 0) {
			pause();
		}
		
		// Find disconnected traders
		if (trader_disconnected == 1) {
			trader_disconnected = 0;
			disconnect_traders(trader_pids, num_traders);
		}
		
		// Check for new messages
		if (new_message > 0) {
			new_message--; 
			int message_pid = new_message_pid;
			
			int i = 0;
			int trader_id;
			for (trader_id = 0; trader_id < num_traders; trader_id++) {
				if (trader_pids[trader_id] == message_pid) { break; }
			}
			
			// Read message
			char message[MESSAGE_BUFFER];
			char c;
			
			while (read(trader_fifos[trader_id], &c, sizeof(char))) {
				if (c == ';') {
					message[i] = '\0';
					i = 0;
					break;
				} else {
					message[i] = c;
				}					
				i++;
			}	
			
			// Decode message from trader
			printf("[PEX] [T%d] Parsing command: <%s>\n", trader_id, message);
			
			int success = -1;
			
			struct order_details details;
			char response_message[MESSAGE_BUFFER];
			
			// Decode the message and store its info in details
			if (decode_order_message(message, &details, product_names, num_products) == 0) {
				// Cancel order
				if (strcmp(details.type, "CANCEL") == 0) {
					struct order* cancelled_order = find_order(&orderbook, trader_id, details.id);	
					if (cancelled_order != NULL) {
						strcpy(details.type, cancelled_order->type);
						strcpy(details.product, cancelled_order->product);
					}					
					int result = cancel_order(&orderbook, trader_id, details.id);	
					if (result == 0) {
						// Message trader
						sprintf(response_message, "CANCELLED %d;", details.id);	
						if (write(exchange_fifos[trader_id], response_message, strlen(response_message)) != -1) { kill(message_pid, SIGUSR1); }	
						success = 0;	
						
						// Message other traders
						sprintf(response_message, "MARKET %s %s 0 0;", details.type, details.product);
						for (int other_trader_id = 0; other_trader_id < num_traders; other_trader_id++) {
							if (other_trader_id != trader_id && trader_pids[other_trader_id] != -1) {
								if (write(exchange_fifos[other_trader_id], response_message, strlen(response_message)) != -1) { kill(trader_pids[other_trader_id], SIGUSR1); }					
							}
						}
					}				
				
				// Amend order
				} else if (strcmp(details.type, "AMEND") == 0) {
					int result = amend_order(&orderbook, trader_id, unique_order_id, details.id, details.quantity, details.price);	
					if (result == 0) {
						unique_order_id++;	
						struct order* amended_order = find_order(&orderbook, trader_id, details.id);	
						if (amended_order != NULL) {
							// Message trader
							sprintf(response_message, "AMENDED %d;", details.id);	
							if (write(exchange_fifos[trader_id], response_message, strlen(response_message)) != -1) { kill(message_pid, SIGUSR1); }	
							success = 0;
							
							// Message other traders
							sprintf(response_message, "MARKET %s %s %d %d;", amended_order->type, amended_order->product, amended_order->quantity, amended_order->price);
							for (int other_trader_id = 0; other_trader_id < num_traders; other_trader_id++) {
								if (other_trader_id != trader_id && trader_pids[other_trader_id] != -1) {
									if (write(exchange_fifos[other_trader_id], response_message, strlen(response_message)) != -1) { kill(trader_pids[other_trader_id], SIGUSR1); }					
								}
							}
							
							// Find matching orders						
							if (amended_order != NULL) {
								while (match_order(&orderbook, amended_order, trader_pids, exchange_fifos, &fees_collected, trader_products, num_products) == 0) { // Breaks when no matches are found (multiple matches possible)
									;
								}	
							}	
						}
					}													
				
				// Buy or sell order
				} else {	
					if (details.id == expected_order_ids[trader_id]) {
						int expected_order_id = expected_order_ids[trader_id];
						expected_order_ids[trader_id] = expected_order_id + 1;
						
						// Create new order
						struct order* new_order = (struct order*) malloc(sizeof(struct order));						
						new_order->unique_id = unique_order_id;
						new_order->trader_id = trader_id;
						strcpy(new_order->type, details.type);
						new_order->order_id = details.id;
						strcpy(new_order->product, details.product);
						new_order->quantity = details.quantity;
						new_order->price = details.price;
						new_order->prev = NULL;
						new_order->next = NULL;
							
						// Add new order to the orderbook lists
						struct order* check_order = NULL;	
						if (strcmp(details.type, "BUY") == 0) {
							check_order = orderbook.buy_orders;
							if (check_order == NULL) {
								orderbook.buy_orders = new_order;
							}
							num_buy_orders++;
						} else if (strcmp(details.type, "SELL") == 0) {
							check_order = orderbook.sell_orders;
							if (check_order == NULL) {
								orderbook.sell_orders = new_order;
							}
							num_sell_orders++;
						}		

						if (check_order != NULL) {					
							while (check_order->next != NULL) { // Breaks when last order is found
								check_order = check_order->next;
							}
							check_order->next = new_order;			
							new_order->prev = check_order;
						}
						
						// Update unique order id and respond
						unique_order_id++;
						sprintf(response_message, "ACCEPTED %d;", details.id);	
						
						// Message trader
						if (write(exchange_fifos[trader_id], response_message, strlen(response_message)) != -1) { kill(message_pid, SIGUSR1); }					
						
						// Message other traders
						sprintf(response_message, "MARKET %s %s %d %d;", details.type, details.product, details.quantity, details.price);
						for (int other_trader_id = 0; other_trader_id < num_traders; other_trader_id++) {
							if (other_trader_id != trader_id && trader_pids[other_trader_id] != -1) {
								if (write(exchange_fifos[other_trader_id], response_message, strlen(response_message)) != -1) { kill(trader_pids[other_trader_id], SIGUSR1); }					
							}
						}
						
						success = 0;

						// Find matching orders
						while (match_order(&orderbook, new_order, trader_pids, exchange_fifos, &fees_collected, trader_products, num_products) == 0) { // Breaks when no matches are found (multiple matches possible)
							;
						}
					}
				}
			} else {
				strcpy(response_message, "INVALID;");				
			}
			
			if (success == -1) {
				// Message trader
				if (write(exchange_fifos[trader_id], response_message, strlen(response_message)) != -1) { kill(message_pid, SIGUSR1); }		
			} else {
				// Print out orderbook
				print_orderbook(&orderbook, product_names, num_products, num_buy_orders, num_sell_orders);
				
				// Print out positions
				print_trader_positions(trader_products, num_traders, num_products);
			}
		}
	}		
	
	disconnect_traders(trader_pids, num_traders); // Disconnect any leftover traders
	
	printf("[PEX] Trading completed\n");
	printf("[PEX] Exchange fees collected: $%ld\n", fees_collected);
	
	// Free memory	
	free_fifos(exchange_fifos, trader_fifos, exchange_fifo_names, trader_fifo_names, num_traders);	
	free_traders(trader_pids, trader_products, num_traders, product_names, num_products);
	free_products(product_names, num_products);	
	free_orderbook(&orderbook, expected_order_ids);
	
	return 0;
}