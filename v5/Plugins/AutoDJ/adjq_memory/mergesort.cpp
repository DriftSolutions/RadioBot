#include "Queue_Memory.h"

MergeListType *MergeSortLinkedList(MergeListType * list,int (*compare)(MergeListType *one,MergeListType *two)) {
    int listSize=1,numMerges,leftSize,rightSize;
    MergeListType *tail,*left,*right,*next;
    if (!list || !list->Next) return list;  // Trivial case

    do { // For each power of two<=list length
        numMerges=0,left=list;tail=list=0; // Start at the start

        while (left) { // Do this list_len/listSize times:
            numMerges++,right=left,leftSize=0,rightSize=listSize;
            // Cut list into two halves (but don't overrun)
            while (right && leftSize<listSize) leftSize++,right=right->Next;
            // Run through the lists appending onto what we have so far.
            while (leftSize>0 || (rightSize>0 && right)) {
                // Left empty, take right OR Right empty, take left, OR compare.
                if (!leftSize)                  {next=right;right=right->Next;rightSize--;}
                else if (!rightSize || !right)  {next=left;left=left->Next;leftSize--;}
                else if (compare(left,right)<0) {next=left;left=left->Next;leftSize--;}
                else                            {next=right;right=right->Next;rightSize--;}
                // Update pointers to keep track of where we are:
                if (tail) tail->Next=next;  else list=next;
                // Sort prev pointer
                next->Prev=tail; // Optional.
                tail=next;          
            }
            // Right is now AFTER the list we just sorted, so start the next sort there.
            left=right;
        }
        // Terminate the list, double the list-sort size.
        tail->Next=0,listSize<<=1;
    } while (numMerges>1); // If we only did one merge, then we just sorted the whole list.
    return list;
}
