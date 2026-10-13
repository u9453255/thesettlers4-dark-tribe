#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <string>
namespace dtio {
// This module's Windows imports are independent of the game's save-hook IAT.
// Startup checks must work before those hook originals have been initialized.
inline bool hashFile(const std::wstring&path,BYTE out[32]){
 HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(file==INVALID_HANDLE_VALUE)return false;
 BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;bool ok=false;BYTE buffer[65536];DWORD count=0;
 if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0&&BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0){
  ok=true;
  for(;;){if(!ReadFile(file,buffer,sizeof(buffer),&count,nullptr)){ok=false;break;}if(!count)break;if(BCryptHashData(hash,buffer,count,0)<0){ok=false;break;}}
  if(ok)ok=BCryptFinishHash(hash,out,32,0)>=0;
 }
 if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);CloseHandle(file);return ok;
}
}
