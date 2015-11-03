// Problem description : https://www.hackerrank.com/companies/vmware/challenges/hierarchical-page-table-translation

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define LEAF_BIT 4
#define PL1_INDEX_START 12
#define PL2_INDEX_START 22
#define NUM_PAGE_INDEX_BITS 12
#define NUM_PL1_INDEX_BITS 10
#define NUM_PL2_INDEX_BITS 10

/*
 * readPhys --
 *   Read an aligned 32bit value from physical memory.
 */
static uint32_t readPhys(uint32_t physAddr);

enum access {
   ACCESS_READ = 1,
   ACCESS_WRITE = 2,
   ACCESS_EXECUTE = 3,
};

enum fault {
   FAULT_NONE = 0,
   FAULT_NOT_PRESENT = 1,
   FAULT_MALFORMED = 2,
   FAULT_VIOLATION = 3,
};

/* walkOnelevel()
 *  Walk one level of the page-table, checking for Not-present and Malformed faults in the walk.
 *  If no such faults are present, one level of page-table walk is performed.
 *  Input:
 *      entry - source page-table entry
 *      index - index into the next level page-table
 *      level - level of page-table walk being performed(starting at root : 3)
 *      access - the type of access being performed(ACCESS_READ, ACCESS_WRITE, or ACCESS_EXECUTE)
 *   Return:
 *       FAULT_NONE on success.
 *       Otherwise FAULT_NOT_PRESENT or FAULT_MALFORMED.
 *   Output:
 *       On success, populate *nextEntry to the next-level page-table entry. flag is populated to indicate 4GB
 *       and 4MB pages, so that linearToPhys() can compute physical addresses accordingly.
 */
static enum fault
walkOneLevel(uint32_t entry, uint32_t *nextEntry, uint32_t index, uint32_t level, enum access access, int *flag) {
   if((entry & 1) == 0)
       return FAULT_NOT_PRESENT;
   
    if((entry >> LEAF_BIT) & 1) { //Leaf entry
       if(level == 3) {
           if((entry >> 12) != 0)
               return FAULT_MALFORMED;
           *flag = 1;
       }
       else if(level == 2) {
           if(((entry << 10) >> 22) != 0)
               return FAULT_MALFORMED;
           *flag = 1;
       }
       else if(level == 1) {
           uint32_t ppn = entry >> 12;
           uint32_t base_addr = ppn << 12;
           *nextEntry = base_addr + index;
       }
   }
   else { //Non-leaf entry
       uint32_t ppn = entry >> 12;
       uint32_t base_addr = ppn << 12;
       uint32_t offset = index * 4;
       *nextEntry = readPhys(base_addr + offset);
   }
   return FAULT_NONE;
}

/*
 * linearToPhys()
 *   Traverse the page tables rooted at pl3e for the linearAddr and access type.
 *   Input:
 *       pl3e - The contents of the PL3e register.
 *       linearAddr - The linear address being accessed.
 *       access - ACCESS_READ, ACCESS_WRITE, or ACCESS_EXECUTE.
 *   Return:
 *       FAULT_NONE on success.
 *       Otherwise FAULT_NOT_PRESENT, FAULT_MALFORMED, or FAULT_VIOLATION.
 *   Output:
 *       On success, populate *physAddr.
 */
static enum fault
linearToPhys(uint32_t pl3e, uint32_t linearAddr, enum access access,
             uint32_t *physAddr /* OUT */) {
   /* !!! PUT YOUR CODE HERE !!! */
   uint32_t page_index = ((1 << NUM_PAGE_INDEX_BITS ) - 1) & linearAddr;
   uint32_t pl1_index = ((1 << NUM_PL1_INDEX_BITS ) - 1) & (linearAddr >> PL1_INDEX_START);
   uint32_t pl2_index = ((1 << NUM_PL2_INDEX_BITS ) - 1) & (linearAddr >> PL2_INDEX_START);
   
   uint32_t entry, nextEntry, index, level;
   enum fault fault;
   int flag;
   uint32_t pl2e, pl1e;
    
   entry = pl3e; 
   nextEntry = 0;
   index = pl2_index;
   level = 3;
   flag = 0;
   fault = walkOneLevel(entry, &nextEntry, index, level, access, &flag);
   if(fault != FAULT_NONE)
       return fault;
   if(flag == 1) {
       *physAddr = linearAddr;
       return FAULT_NONE;
   }
   
   pl2e = nextEntry;
   entry = nextEntry;
   nextEntry = 0;
   index = pl1_index;
   level = 2;
   flag = 0;
   fault = walkOneLevel(entry, &nextEntry, index, level, access, &flag);
   if(fault != FAULT_NONE)
       return fault;
   if(flag == 1) {
       *physAddr = ((entry >> PL2_INDEX_START) << PL2_INDEX_START) | ((linearAddr << 10) >> 10);
       return FAULT_NONE;
   }
   
   pl1e = nextEntry;
   entry = nextEntry;
   nextEntry = 0;
   index = page_index;
   level = 1;
   flag = 0;
   fault = walkOneLevel(entry, &nextEntry, index, level, access, &flag);
   if(fault != FAULT_NONE)
       return fault;
   
   //Check for Permission violation faults(R, W, X)
   if((((pl3e >> access) & 1) == 0 || ((pl2e >> access) & 1) == 0 || ((pl1e >> access) & 1) == 0))
       return FAULT_VIOLATION; 
    
   *physAddr = nextEntry;
   return FAULT_NONE;
}

