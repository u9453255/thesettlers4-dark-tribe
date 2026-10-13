#include <windows.h>
#include <stdio.h>
int wmain(int argc,wchar_t **argv){
 if(argc!=3){fprintf(stderr,"usage: inject.exe PID ABSOLUTE_DLL_PATH\n");return 2;} DWORD pid=wcstoul(argv[1],0,10);
 HANDLE p=OpenProcess(PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ,FALSE,pid);
 if(!p){printf("open-error %lu\n",GetLastError());return 3;}
 SIZE_T n=(wcslen(argv[2])+1)*sizeof(wchar_t);void *arg=VirtualAllocEx(p,0,n,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 if(!arg){printf("allocation-error %lu\n",GetLastError());CloseHandle(p);return 4;}
 SIZE_T wrote=0;
 if(!WriteProcessMemory(p,arg,argv[2],n,&wrote)||wrote!=n){printf("write-error %lu bytes=%lu/%lu\n",GetLastError(),(unsigned long)wrote,(unsigned long)n);VirtualFreeEx(p,arg,0,MEM_RELEASE);CloseHandle(p);return 4;}
 HANDLE t=CreateRemoteThread(p,0,0,(LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"LoadLibraryW"),arg,0,0);
 if(!t){printf("thread-error %lu\n",GetLastError());return 5;}
 if(WaitForSingleObject(t,10000)!=WAIT_OBJECT_0){printf("load-timeout\n");return 6;}
 DWORD result=0;GetExitCodeThread(t,&result);CloseHandle(t);VirtualFreeEx(p,arg,0,MEM_RELEASE);CloseHandle(p);
 printf("module %08lx\n",result);return result?0:7;
}
