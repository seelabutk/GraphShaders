#include "vec.h"

int _vec_expand(char **data, unsigned long *length, unsigned long *capacity, unsigned long memsz){
	if(*length + 1 > *capacity){
		void *ptr;
		int n = (*capacity == 0) ? 1 : *capacity << 1;
		ptr = realloc(*data, n * memsz);
		if(ptr == NULL)	return -1;
		*data = ptr;
		*capacity = n;
	}
	return 0;
}
