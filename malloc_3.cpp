//
// Created by student on 7/7/25.
//
#include <unistd.h>
#include <iostream>
#include <cstring>

#include <sys/mman.h>
#include <vector>
#include <complex>

using namespace std;
#define MMAP_TREHSHOLD 128*1024


struct MallocMetadata {
    size_t size;
    bool is_free;
    bool is_mmap = false; // for the free part
    MallocMetadata* next = nullptr;
    MallocMetadata* prev = nullptr;
    MallocMetadata* buddy = nullptr;
};

const size_t BYTE_SIZE = sizeof(MallocMetadata);
const int MAX_ORDER = 10;



size_t _size_meta_data(){
    return sizeof(MallocMetadata);
}

//TODO: add Orders array

//TODO: understand how to split blocks and how to keep track of allocated blocks

//TODO: understand how to make two blocks buddy blocks


struct BuddyAllocator{
    MallocMetadata* array[MAX_ORDER + 1];
    int orders[MAX_ORDER + 1];
    MallocMetadata *mmapHead = nullptr;
    MallocMetadata *mmapTail = nullptr;
    size_t num_free_blocks = 0;
    size_t num_free_bytes = 0;
    size_t num_used_blocks = 0;
    size_t num_used_bytes = 0;
    size_t num_meta_data_bytes = 0; //TODO: if matters


    void init(){
        num_free_blocks = 32;
        num_free_bytes = 32 * (MMAP_TREHSHOLD - BYTE_SIZE);
        num_used_blocks = 0;
        num_used_bytes = 0;
        num_meta_data_bytes = 32 * BYTE_SIZE; //TODO: if matters

        int num = 128;
        for(int i = 0; i <= MAX_ORDER; i++) {
            orders[i] = num;
            num *= 2;
            array[i] = nullptr;
        }
        MallocMetadata* prev = nullptr;
        for(int i = 0; i < 32; i++){
            MallocMetadata* meta_ptr = (MallocMetadata*)sbrk(1024 * 128);
            meta_ptr->size = 1024 * 128 - BYTE_SIZE;
            meta_ptr->is_free = true;
            meta_ptr->buddy = nullptr;
            meta_ptr->is_mmap = false;
            meta_ptr->prev = prev;
            if (prev){
                prev->next = meta_ptr;
            }
            if (i == 0){
                array[MAX_ORDER] = meta_ptr;
            }
            if (i == 31){
                meta_ptr->next = nullptr;
            }
            prev = meta_ptr;
        }

    }

    int getOrder(size_t size){
        int tmp = size + BYTE_SIZE;
        if ((size_t)tmp < BYTE_SIZE || (size_t)tmp > 128 * 1024) {

            return -1;
        }
        for (int i = 0; i <= MAX_ORDER; i++) {

            if (orders[i] >= tmp) { return i; }
        }
        return -1;
    }

    void insert(MallocMetadata* p, int order){
        if (order > MAX_ORDER || order < 0) {
            return;
        }
        MallocMetadata* head = array[order];
        MallocMetadata* current = head;
        p->size = orders[order] - BYTE_SIZE;
        p->is_free = true;
        if(!current) {
            array[order] = p;
            p->prev = nullptr;
            p->next = nullptr;
        }

        while (current){
            if(current == p){
                return;
            }
            if (p < current){
                if (current->prev){
                    current->prev->next = p;
                }
                if (current == head) {
                    array[order] = p;
                }
                p->prev = current->prev;
                current->prev = p;
                p->next = current;
                break;
            }
            if (current->next == nullptr){
                current->next = p;
                p->prev = current;
                p->next = nullptr;
                break;
            }
            current = current->next;
        }

        num_free_bytes += p->size;
        num_free_blocks++;
//        num_used_bytes -= p->size;

//        num_used_blocks--;
//        num_meta_data_bytes-=BYTE_SIZE;
    }
    void remove(MallocMetadata* p, int order){
        if (array[order] == p){
            array[order] = p->next;
        }
        if (p->prev){
            p->prev->next = p->next;
        }
        if (p->next){
            p->next->prev = p->prev;
        }
        p->next = nullptr;
        p->prev = nullptr;
        p->is_free = false;

        num_free_bytes -= p->size;
        num_free_blocks--;
//        num_used_bytes += p->size;

    }

    MallocMetadata* divideBlock(MallocMetadata* metaPtr, size_t size,int order, int index){
//        int diff = index - order;
        remove(metaPtr,index);
        MallocMetadata* left = metaPtr;
        for (int i = index - 1; i >= order; i--) {
            // left = splitBlock(left,order,i);
            int dist = orders[i];
            char* new_addr = (char*)left + dist;
            MallocMetadata* right = (MallocMetadata*) new_addr;

            right->is_free = true;
            right->buddy = left;
            right->is_mmap = false;
            if(i == 9){
                left->buddy = right;
            }
//            right->size = orders[i]/2 - BYTE_SIZE;
            insert(right,i); //Next, Prev, and Size are set here
        }
        left->size = orders[order] - BYTE_SIZE;
        left->next = nullptr;
        left->prev = nullptr;
        return left;
    }

    void* searchBlock(size_t size){
        int order = getOrder(size);
        if(order == -1){
            return nullptr;
        }
        for(int i = order; i <= MAX_ORDER; i++){
            if(array[i]){
                MallocMetadata* ret = divideBlock(array[i],size,order, i);
                return (char*)ret + BYTE_SIZE;
            }
        }
        return nullptr;
    }

    void* mmapBlock(size_t size){
        size_t total_size = size + BYTE_SIZE;

        MallocMetadata* p = (MallocMetadata*)mmap(nullptr, total_size,
                                                  PROT_READ | PROT_WRITE,
                                                  MAP_PRIVATE | MAP_ANONYMOUS,
                                                  -1, 0);
        if(!p){
            return nullptr;
        }
        p->is_mmap = true;
        p->is_free = false;
        if(mmapTail) {
            mmapTail->next = p;
        }
        if(!mmapHead){
            mmapHead = p;
        }
        p->prev = mmapTail;
        mmapTail = p;
        p->next = nullptr;
        p->size = size;

//            num_used_blocks++;
//            num_used_bytes+=p->size;
//            num_meta_data_bytes+=BYTE_SIZE;

        return ((char*)p+BYTE_SIZE);
    }

    void ummapBlock(MallocMetadata* p){
        if (p->prev){
            p->prev->next = p->next;
        }
        if (p->next){
            p->next->prev = p->prev;
        }
        if(p == mmapHead){
            mmapHead = p->next;
        }
        if(p == mmapTail){
            mmapTail = p->prev;
        }
        p->next = nullptr;
        p->prev = nullptr;

//        num_used_blocks--;
//        num_used_bytes-=p->size;
//        num_meta_data_bytes-=BYTE_SIZE;
        munmap(p,p->size+BYTE_SIZE);
    }

    MallocMetadata* uniteBlocks(MallocMetadata* metaPtr){
        int order = getOrder(metaPtr->size);
        insert(metaPtr,order);

        if(order == MAX_ORDER){
            return nullptr;
        }
//        if(metaPtr->next && (metaPtr->buddy == metaPtr->next || metaPtr->next->buddy == metaPtr)){
        if(metaPtr->next && metaPtr->next->buddy &&
           (metaPtr->buddy == metaPtr->next || metaPtr->next->buddy == metaPtr)){
            remove(metaPtr->next,order);
            remove(metaPtr,order);
            insert(metaPtr, order + 1);
            return metaPtr;
        }
        if(metaPtr->prev && (metaPtr->buddy == metaPtr->prev)){
            metaPtr = metaPtr->prev; //TODO: be carefull from bugs here
            remove(metaPtr->next,order);
            remove(metaPtr,order);
            insert(metaPtr, order + 1);
            return metaPtr;
        }

        return nullptr;
    }

};


BuddyAllocator ba;
bool smalloc_called = false;

size_t _num_free_blocks(){

    return ba.num_free_blocks;
}

size_t _num_free_bytes(){
    return ba.num_free_bytes;
}

size_t _num_allocated_blocks(){

    return ba.num_used_blocks + ba.num_free_blocks;
}

size_t _num_allocated_bytes(){
    return ba.num_used_bytes + ba.num_free_bytes;
}

size_t _num_meta_data_bytes(){
//    return (ba.num_used_blocks + ba.num_free_blocks) * _size_meta_data();
    return _num_allocated_blocks() * _size_meta_data();
}

void* smalloc(size_t size){
    if(!smalloc_called){
        ba.init();
    }
    smalloc_called = true;
    if(size == 0 || size > 1e8){
        return nullptr;
    }
    void* ret;
    if(size + BYTE_SIZE <= MMAP_TREHSHOLD){
        ret = ba.searchBlock(size);
    } else{
        ret = ba.mmapBlock(size);
    }
    if (ret) {
        MallocMetadata* metaPtr = (MallocMetadata*)((char*)ret-BYTE_SIZE);
        ba.num_used_blocks++;
        ba.num_used_bytes += metaPtr->size;
//        if(!metaPtr->is_mmap){ PROBABLY NOT NEEDED BECAUSE IT HAPPENS IN REMOVE
//            ba.num_free_bytes -= metaPtr->size;
//        }
    }
    return ret;
}

void sfree(void* p){
    if(!p){
        return;
    }
    MallocMetadata* metaPtr = (MallocMetadata*)((char*)p-BYTE_SIZE);
    if(metaPtr->is_free){
        return;
    }
    size_t original_size = metaPtr->size;
    //TODO: add checks if free fails
    ba.num_used_bytes -= original_size; // a block of original size is definitely not used anymore
    ba.num_used_blocks--;
    if(metaPtr->is_mmap){
        ba.ummapBlock(metaPtr);
        return;
    }
    // ba.num_free_bytes += original_size; //TODO: number of free bytes should change according to successful unions
    while (metaPtr){
        metaPtr = ba.uniteBlocks(metaPtr);
    };
}

void* scalloc(size_t num, size_t size){
    void* ret = smalloc(num*size);
    if(!ret){
        return nullptr;
    }
//    char* tmp = (char*) ret;
//    for(size_t i = 0; i < num * size;i++){
//        tmp[i] = 0;
//    }
    memset(ret, 0, num * size);
    return ret;
}


void* srealloc(void* oldp, size_t size){

    if(!oldp){
        return smalloc(size);
    }

    if(size == 0){ // TODO: check
        sfree(oldp);
        return nullptr;
    }

    MallocMetadata* tmp = (MallocMetadata*)((char*)oldp - BYTE_SIZE);
//    ba.array;
    if(tmp->size >= size && size > 0){
        tmp->is_free = false;
//         tmp->size = size; //TODO: check if we need to change it
        return oldp;
    }
    size_t old_size = tmp->size;

    while (tmp){
        if(tmp->size>= size){
            break;
        }
        tmp = ba.uniteBlocks(tmp);
    };

    void* ret;
    if(tmp){
        int order = ba.getOrder(tmp->size);
        ba.remove(tmp, order);
        // ba.num_used_blocks++;
        ba.num_used_bytes -= old_size;
        ba.num_used_bytes += tmp->size;
        ret = (char*)tmp + BYTE_SIZE;
        memmove(ret, oldp, old_size);
        return ret;

    }

    ret = smalloc(size);
    if(!ret){
        return nullptr;
    }
    memmove(ret, oldp, old_size);
    sfree(oldp);
    return ret;
}




//int main(){
//
//
//    void* ptr1 = smalloc(40);
//    void* ptr2 = srealloc(ptr1, 400);
//    ba.array;
//    return 0;
//
//}