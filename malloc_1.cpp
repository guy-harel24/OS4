//
// Created by student on 7/6/25.
//
#include <unistd.h>
#include <iostream>

using namespace  std;

void* smalloc(size_t size){
    //if size is 0
    if(size == 0 || size > 1e8){
        return nullptr;
    }
    void* ret = sbrk(size);
    if(ret == (void*)(-1)){
        cout << "return -1" << endl;
        return nullptr;
    }
    return ret;
}



//int main(){
//
//    cout << "p1: " << p1 << endl;
//    return 0;
//}