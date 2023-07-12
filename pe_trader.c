#include "pe_trader.h"

// Handle new messages
volatile sig_atomic_t new_message = 0;

void message_signal(int sig) {
	new_message++;
}

// Decode message
int decode_market_message(char* message, struct order_details* details) {
	char attribute[PRODUCT_BUFFER];
	char space[MESSAGE_BUFFER];
	
	// Check order type
	sscanf(message, "%s %s", attribute, space);
	
	// Create temporary variables
	char market[8];
	char type[TYPE_BUFFER];
	int id = -1;
	char product[PRODUCT_BUFFER];
	int quantity = -1;
	int price = -1;
	char excess[5];
	strcpy(excess, "");
	
	// Create struct based on order type
	if (strcmp(attribute, "MARKET") == 0) {
		sscanf(message, "%s %s %s %d %d %s", market, type, product, &quantity, &price, excess);
		strcpy(details->type, type);
		details->id = id;
		strcpy(details->product, product);
		details->quantity = quantity;
		details->price = price;
		
		if (strcmp(type, "SELL") != 0) {
			return 1;
		}	
	} else if (strcmp(attribute, "ACCEPTED") == 0) {
		sscanf(message, "%s %d %s", type, &id, excess);
		strcpy(details->type, type);
		details->id = id;
		strcpy(details->product, "\0");
		details->quantity = 1;
		details->price = 1;
	} else {		
		return 2;
	}
	
	if (strlen(excess) > 0) {
		return 3;
	}	
	
	return 0;
}

// Read message from pipe
void read_message(int* pe_exchange, char* message) {
	char c;
		
	int i = 0;
	while (read(*pe_exchange, &c, sizeof(char))) {
		if (c == ';') {
			message[i] = '\0';
			i = 0;
			break;
		} else {
			message[i] = c;
		}					
		i++;
	}	
}

int main(int argc, char ** argv) {
    if (argc < 2) {
        printf("Not enough arguments\n");
        return 1;
    }
	
    // Register signal handler
	struct sigaction sa = {0};
	sa.sa_handler = &message_signal;
	sa.sa_flags = 0;
	sigaction(SIGUSR1, &sa, NULL);
	
    // Connect to named pipes
	char trader_id = *argv[1];
	
	char exchange_filename[FILENAME_BUFFER];
	char trader_filename[FILENAME_BUFFER];
	
	sprintf(exchange_filename, "/tmp/pe_exchange_%c", trader_id);
	sprintf(trader_filename, "/tmp/pe_trader_%c", trader_id);
	
	int pe_exchange = open(exchange_filename, O_RDONLY);
	if (pe_exchange == -1) { return 1; }
	int pe_trader = open(trader_filename, O_WRONLY);
	if (pe_trader == -1) { return 2; }
	
	// Wait for market open
	char message[MESSAGE_BUFFER];
	pause();
	if (new_message > 0) {
		read_message(&pe_exchange, message);
		new_message = 0;
	}	
    
    // Event loop:
	int my_order_id = 0;	
	
	int unconfirmed_order = 0;

	while (1) {			
		// Wait for exchange update (MARKET message)
		if (new_message == 0) {
			pause();
		} else {
			new_message--;			
		}
		
		read_message(&pe_exchange, message);
		
		struct order_details details;
		int decode_id = decode_market_message(message, &details);
		if (decode_id == 0) { // Market message e.g. sell, accepted, invalid (exchange checks for invalid orders, so auto-trader won't receive it since it's always correct)
			// Previous order not confirmed by exchange
			if (unconfirmed_order > 0 && strcmp(details.type, "ACCEPTED") != 0 ) {
				kill(getppid(), SIGUSR1);
			}
			
			// Sell order listed
			if (strcmp(details.type, "SELL") == 0) {
				if (details.quantity >= 1000) {
					break;
				}
				
				// Create new order struct for the trader
				struct order_details new_order;
				strcpy(new_order.type, "BUY");
				new_order.id = my_order_id;
				strcpy(new_order.product, details.product);
				new_order.quantity = details.quantity;
				new_order.price = details.price;
				
				if (new_order.quantity > 0 && new_order.price > 0) {
					// Create and write response order message (trader -> exchange)
					char order_message[MESSAGE_BUFFER];
					sprintf(order_message, "%s %d %s %d %d;", new_order.type, new_order.id, new_order.product, new_order.quantity, new_order.price);
					
					// Send order
					if (write(pe_trader, order_message, strlen(order_message)) != -1) { kill(getppid(), SIGUSR1); }		
					unconfirmed_order++;
				}
			
			// Exchange accepted order
			} else if (strcmp(details.type, "ACCEPTED") == 0) {	
				my_order_id++;
				if (unconfirmed_order > 0) {
					unconfirmed_order--;
				}
			}
		}
	}
	
	// Close named pipes
	close(pe_trader);
	close(pe_exchange);
	
	return 0;
}