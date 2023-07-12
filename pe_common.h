#ifndef PE_COMMON_H
#define PE_COMMON_H

#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <errno.h>
#include <math.h>
#include <ctype.h>

#define FIFO_EXCHANGE "/tmp/pe_exchange_%d"
#define FIFO_TRADER "/tmp/pe_trader_%d"
#define FEE_PERCENTAGE 1

#define FILENAME_BUFFER 30
#define MESSAGE_BUFFER 50
#define TYPE_BUFFER 10
#define PRODUCT_BUFFER 17

struct order_details {
	char type[TYPE_BUFFER];
	int id;
	char product[PRODUCT_BUFFER];
	int quantity;
	int price;
};

#define DEBUG 1

#ifdef DEBUG
	#define PRINT(x) printf(x)
#else
	#define PRINT(x)
#endif

#endif
