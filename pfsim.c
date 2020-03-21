#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define NUM_PROCESSES 4
#define NUM_PTES 128
#define NUM_PHYSICAL_PAGES 32

typedef enum {READ, WRITE} AccessType;

typedef struct{
	uint8_t pid;
	uint16_t virtualPageNum;
	AccessType accessType;
} Operation;

/* Page table entry */
typedef struct {
    bool        valid;
    uint16_t    pfn;
} pte;

/* Reverse mapping entry */
typedef struct{
    bool        avail;
    bool dirty;
    bool referenced;
    uint8_t     proc;
    uint16_t    vpn;
} rme ;

int numMemoryAccesses = 0;
int numPageFaults = 0;
int numDiskAccesses = 0;


//2D array representing the page table for each process. Row index represents process ID, column index represents pte entry
pte pageTables[NUM_PROCESSES][NUM_PTES];

//Array of reverse-mapping entries, indexed by physical page number
rme physicalMemory[NUM_PHYSICAL_PAGES];




uint16_t evictPage(){
	//Look for an unreferenced page frame where the dirty bit is off, and replace it.
	for(uint16_t i = 0; i<NUM_PHYSICAL_PAGES; i++){
		if(!physicalMemory[i].referenced && !physicalMemory[i].dirty){
			//Set the mapped page table entry to invalid.
			pageTables[physicalMemory[i].proc][physicalMemory[i].vpn].valid = false;
			//Set the page to available
			physicalMemory[i].avail = true;
			//Return available index
			return i;
		}
	}
	
	//If that's not found, look for one where dirty bit is on, replace it.
	for(uint16_t i = 0; i<NUM_PHYSICAL_PAGES; i++){
		if(!physicalMemory[i].referenced && physicalMemory[i].dirty){
			//Write page back to disk, because it was dirty.
			numDiskAccesses++;
			//Set the mapped page table entry to invalid.
			pageTables[physicalMemory[i].proc][physicalMemory[i].vpn].valid = false;
			//Set the page to available
			physicalMemory[i].avail = true;
			//Return available index
			return i;
		}
	}

	//If that's not found, look for a referenced page with dirty bit on.
	for(uint16_t i = 0; i<NUM_PHYSICAL_PAGES; i++){
		if(physicalMemory[i].referenced && physicalMemory[i].dirty){
			//Write page back to disk, because it was dirty.
			numDiskAccesses++;
			//Set the mapped page table entry to invalid.
			pageTables[physicalMemory[i].proc][physicalMemory[i].vpn].valid = false;
			//Set the page to available
			physicalMemory[i].avail = true;
			//Return available index
			return i;
		}
	}

	return -1;
}


void handlePageFault(Operation op){
	//Try searching physical memory for an available page frame. If none are found, evict a page.
	int availablePageIndex = -1;
	for(uint16_t i = 0; i<NUM_PHYSICAL_PAGES; i++){
		if(physicalMemory[i].avail){
			availablePageIndex = i;
			break;
		}
	}

	if(availablePageIndex == -1){
		availablePageIndex = evictPage();
	}


	//Allocate the page frame.  
	physicalMemory[availablePageIndex].avail = false;
	physicalMemory[availablePageIndex].proc = op.pid;
	physicalMemory[availablePageIndex].vpn = op.virtualPageNum;

	//Copy the page into physical memory from disk.
	numDiskAccesses++;
	physicalMemory[availablePageIndex].dirty = false;

	//Set up translation from virtual page to physical page
	pageTables[op.pid][op.virtualPageNum].pfn = availablePageIndex;
	pageTables[op.pid][op.virtualPageNum].valid = true;

}

void performOp(Operation op){
	//Check the translation. If the translation is not valid, handle a page fault. Then, perform the op.
	if(!pageTables[op.pid][op.virtualPageNum].valid){
		numPageFaults++;
		//handle page fault
		handlePageFault(op);
	}

	//Actually perform the operation

	if(op.accessType == WRITE){
		physicalMemory[pageTables[op.pid][op.virtualPageNum].pfn].dirty = true;
	}

	//Since the page has been accessed, set the reference to true
	physicalMemory[pageTables[op.pid][op.virtualPageNum].pfn].referenced = true;

	//Regardless of operation type, a page access has occurred
	numMemoryAccesses++;
}



int parseFile(FILE* file){
	// Read each line into the buffer
	size_t bufferSize = 12;
	char buffer[bufferSize];

    while( fgets(buffer, bufferSize, file) != NULL ){
        uint8_t pid = 0;
        uint16_t address = 0;
        char a = 0;
        sscanf(buffer, "%hhu %hx %c", &pid, &address, &a);
        Operation op;
        op.pid = pid;
        //Extract the virtual page number from the address
        op.virtualPageNum = address >> 9;
        if(a == 'R'){
        	op.accessType = READ;
        }
        else if(a == 'W'){
        	op.accessType = WRITE;
        }

        //Operate on the command
        performOp(op);
        //Every 200 memory accesses, reset all the referenced fields
    	if(numMemoryAccesses%200 == 0){
    		for(int i = 0; i<NUM_PHYSICAL_PAGES; i++){
    			physicalMemory[i].referenced = false;
    		}
    	}

    }

    if( ferror(file) ){
        perror( "The following error occurred" );
        return -1;
    }

    return 0;
}

void initDataStructs(){
	for(int i = 0; i<NUM_PROCESSES; i++){
		for(int j = 0; j<NUM_PTES; j++){
			pageTables[i][j].valid = false;
			pageTables[i][j].pfn = 33;
		}
	}
	for(int i = 0; i< NUM_PHYSICAL_PAGES; i++){
		physicalMemory[i].avail = true;
		physicalMemory[i].dirty = false;
		physicalMemory[i].referenced = false;
		physicalMemory[i].proc = -1;
		physicalMemory[i].vpn = 129;
	}
}


int main(int argc, char **argv){
	if(argc < 0){
		printf("Please input a file\n");
		return -1;
	}
	char* filename = argv[1];
	FILE* file = fopen(filename, "r");
	if(file){
		initDataStructs();
		//Parse input file. for every line, execute the command
		if(parseFile(file) == 0){
			fclose( file );

			printf("Page accesses: %d\n", numMemoryAccesses);
			printf("Page faults: %d\n", numPageFaults);
			printf("Disk accesses: %d\n", numDiskAccesses);

			return 0;
		}
		else{
			printf("Error while parsing file\n");
			fclose(file);
			return -1;
		}
	}
	else{
		printf("Invalid file\n");
		return -1;
	}
}