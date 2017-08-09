#include "stdafx.h"
#include "asyn_rtpproxy_client.h"

// int main(int argc, char* argv[])
// {
//     Log::open(false, "-dGMfyds", false);
//     AsynStorageAgent::Obj()->init(2);
// 
//     StorageStrategy ss(false, true, ET_CREATE_TIME, 100);
//     ISACreatFileCB* create_file_cb = new ISACreatFileCB;
//     ISAOpenFileCB* open_file_cb = new ISAOpenFileCB;
//     ISACloseFileCB* close_file_cb = new ISACloseFileCB;
//     ISADelFileCB* del_file_cb = new ISADelFileCB;
// 
//     int i = 3;
//     while (i--) {
//         AsynStorageAgent::Obj()->asyn_create_file("E:\\yw\\readme.txt", 100, ss, create_file_cb, 10);
//         AsynStorageAgent::Obj()->asyn_open_file("E:\\yw\\readme.txt", open_file_cb);
//         AsynStorageAgent::Obj()->asyn_close_file(10, close_file_cb, 0, 5000, &GMAddrEx("10.0.2.15", 27381));
//         AsynStorageAgent::Obj()->asyn_del_file("E:\\yw\\readme.txt", true, del_file_cb, 0);
//     }
//     std::cout << "keystroke to exit\n";
//     getchar();
//     delete create_file_cb;
//     delete open_file_cb;
//     delete close_file_cb;
//     delete del_file_cb;
//     Log::close();
// }
