//
// Created by student on 7/6/25.
//
#include <unistd.h>
#include <iostream>
#include <cstring>
using namespace std;


struct MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};
const size_t BYTE_SIZE = sizeof(MallocMetadata);

size_t _size_meta_data(){
    return sizeof(MallocMetadata);
}

struct HeapMetaData{
    size_t num_free_blocks = 0;
    size_t num_free_bytes = 0;
    size_t num_allocated_blocks = 0;
    size_t num_allocated_bytes = 0;
    size_t num_meta_data_bytes = 0;
    MallocMetadata* head = nullptr;
    MallocMetadata* tail = nullptr;
    void* searchAndInsert(size_t size){
        MallocMetadata* current = head;
        while(current){
            if(current->is_free && current->size >= size){
                //TODO: insert the block update the data
//                return current->block_start;
                current->is_free = false;
                num_free_blocks--;
                num_free_bytes -= current->size;
//                num_allocated_blocks++;
//                num_allocated_bytes += current->size;
                return (char*)current + BYTE_SIZE;
            }
            current = current->next;
        }
        return nullptr;
    }

    void* insertNewBlock(size_t size){
        //TODO: use sbrk(), update HMD,
        void* ret = sbrk(size + BYTE_SIZE);
        if(ret == (void*)(-1)){
            cout << "return -1" << endl;
            return nullptr;
        }
        MallocMetadata* meta_ptr = (MallocMetadata*)(ret);
        meta_ptr->size = size;
        meta_ptr->next = nullptr;
        meta_ptr->prev = nullptr;
        if(!head){
            head = meta_ptr;
        }
        if(tail){
            meta_ptr->prev = tail;
            tail->next = meta_ptr;
        }
        tail = meta_ptr;
        num_allocated_blocks++;
        num_allocated_bytes += size;
        num_meta_data_bytes += BYTE_SIZE;
        meta_ptr->is_free = false;
        return (void *) ((char *) meta_ptr + BYTE_SIZE);
    }
};


HeapMetaData hmd;

size_t _num_free_blocks(){
    return hmd.num_free_blocks;
}

size_t _num_free_bytes(){
    int x = 0;
    return hmd.num_free_bytes;
}

size_t _num_allocated_blocks(){
    return hmd.num_allocated_blocks;
}

size_t _num_allocated_bytes(){
    return hmd.num_allocated_bytes;
}

size_t _num_meta_data_bytes(){
    return hmd.num_meta_data_bytes;
}


void* smalloc(size_t size){
    if(size == 0 || size > 1e8){
        return nullptr;
    }

    //search for a free block - use heapMetaData
    void* result = hmd.searchAndInsert(size);

    //if not found use sbrk
    if(!result){
        result = hmd.insertNewBlock(size);
    }

    return result;
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
void sfree(void* p){
    if(!p){
        return;
    }

    char *current = (char *) p - BYTE_SIZE;
    MallocMetadata *tmp = (MallocMetadata *) current;
    if(!tmp->is_free) {
//        hmd.num_allocated_bytes -= tmp->size;
//        hmd.num_meta_data_bytes -= BYTE_SIZE;
        hmd.num_free_blocks++;
        hmd.num_free_bytes += tmp->size;
//        hmd.num_allocated_blocks--;
        tmp->is_free = true;
    }
}

void* srealloc(void* oldp, size_t size){

     if(!oldp){
         return smalloc(size);
     }
     MallocMetadata* tmp = (MallocMetadata*)((char*)oldp - BYTE_SIZE);
     if(tmp->size >= size && size > 0){
         tmp->is_free = false;
//         tmp->size = size; //TODO: check if we need to change it
         return oldp;
     }
     void* ret = smalloc(size);
     if(!ret){
         return nullptr;
     }
    memmove(ret, oldp, tmp->size);
    sfree(oldp);
    return ret;
}




//int main(){
////    int* intptr = (int*) smalloc(4);
////    *intptr = 8;
////    cout << "address: " << intptr << " value: " << *intptr << endl;
////    srealloc(intptr, 8);
////    int* intptr2 = (int*) scalloc(1,4);
////    cout << "address: " << intptr2 << " value: " << *intptr2 << endl;
////    *intptr = 10;
////    cout << "address: " << intptr2 << " value: " << *intptr2 << endl;
////
//////    MallocMetadata* m_ptr = (MallocMetadata*)((char*)intptr2-32);
//////    cout << m_ptr->size << endl;
//////    cout << m_ptr->is_free << endl;
//////    cout << m_ptr->prev << endl;
//////    cout << m_ptr->next << endl;
//////    cout << *intptr << endl;
//
//    char *a = (char *)smalloc(10);
//    cout << "address: " << _num_allocated_blocks() <<  endl;
//    sfree(a);
//    cout << "address: " << _num_allocated_blocks() <<  endl;
//
//    return 0;
//}