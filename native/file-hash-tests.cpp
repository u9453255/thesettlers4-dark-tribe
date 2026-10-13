#include "file-hash.hpp"
#include <assert.h>
#include <stdio.h>
#include <vector>
static bool same(const BYTE*hash,const char*hex){for(int i=0;i<32;i++){unsigned byte=0;assert(sscanf(hex+2*i,"%2x",&byte)==1);if(hash[i]!=byte)return false;}return true;}
static void write(const wchar_t*path,const std::vector<BYTE>&data){HANDLE file=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);assert(file!=INVALID_HANDLE_VALUE);DWORD count=0;assert(WriteFile(file,data.data(),(DWORD)data.size(),&count,nullptr)&&count==data.size());assert(CloseHandle(file));}
int main(){
 // No game, save hooks or API modules are initialized in this original fixture.
 wchar_t directory[MAX_PATH],path[MAX_PATH];assert(GetTempPathW(MAX_PATH,directory));assert(GetTempFileNameW(directory,L"dth",0,path));BYTE hash[32]{};
 assert(dtio::hashFile(path,hash)&&same(hash,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
 write(path,{'a','b','c'});assert(dtio::hashFile(path,hash)&&same(hash,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
 write(path,{'a','b','d'});assert(dtio::hashFile(path,hash)&&!same(hash,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
 write(path,std::vector<BYTE>(65537,'a'));assert(dtio::hashFile(path,hash)&&same(hash,"008ffc88d3c96a9f307524eb361e47c5222a887fc45fa0c1fb8d429c5c23b430"));
 HANDLE locked=CreateFileW(path,GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);assert(locked!=INVALID_HANDLE_VALUE);assert(!dtio::hashFile(path,hash));assert(CloseHandle(locked));
 assert(DeleteFileW(path));assert(!dtio::hashFile(path,hash));
 puts("PASS: startup SHA256 before save-hook initialization; empty/changed/multi-block files, locked/missing refusal and handles closed; no game modules loaded");
}
