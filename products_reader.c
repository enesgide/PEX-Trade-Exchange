#include "pe_exchange.h"

// Count the number of products
int count_products(char* file_path) {
	FILE *file = fopen(file_path, "r");	
	
	// Read number
	char num_products_str[7];
	fgets(num_products_str, sizeof(num_products_str), file);	
	int num_products = atoi(num_products_str);	
	
	fclose(file);
	
	return num_products;
}	

// Read and store the product names
void read_products(char* file_path, char** product_names) {
	FILE *file = fopen(file_path, "r");	
	fseek(file, 0, SEEK_SET);
	
	printf("[PEX] Trading ");
	
	// Read number
	char num_products_str[7];
	fgets(num_products_str, sizeof(num_products_str), file);	
	int num_products = atoi(num_products_str);	
	
	printf("%d", num_products);	
	printf(" products: ");
	
	// Read products
	char product[PRODUCT_BUFFER];	
	for (int i = 0; i < num_products; i++) {		
		int eof_found = -1;
		while (1) {
			if (fgets(product, sizeof(product), file) == NULL) {
				eof_found = 0;
				break;
			}
			product[strcspn(product, "\n")] = '\0';
			if (strlen(product) > 0) {
				break;
			}
		}
		if (eof_found == 0) { break; }			
		
		char *product_copy = malloc(strlen(product) + 1);  // allocate memory for the string
		strcpy(product_copy, product);
		product_names[i] = product_copy;
		if (i == num_products - 1) {
			printf("%s\n", product);
		} else {
			printf("%s ", product);
		}		
	}
	
	fclose(file);
}