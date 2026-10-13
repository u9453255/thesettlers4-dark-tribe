// Standalone offline loader fixture. It is never linked to game code.
#include <windows.h>
#include <stdio.h>
static void eventName(wchar_t*name){swprintf(name,100,L"Local\\DarkTribeLoaderFixture%lu",GetCurrentProcessId());}
#ifdef BUILD_LOADER_FIXTURE_DLL
BOOL WINAPI DllMain(HINSTANCE,DWORD reason,void*){
 if(reason==DLL_PROCESS_ATTACH){wchar_t name[100];eventName(name);HANDLE event=OpenEventW(EVENT_MODIFY_STATE,FALSE,name);if(!event)return FALSE;SetEvent(event);CloseHandle(event);}return TRUE;
}
#else
int wmain(){wchar_t name[100];eventName(name);HANDLE event=CreateEventW(nullptr,TRUE,FALSE,name);if(!event)return 1;puts("fixture-ready");fflush(stdout);DWORD result=WaitForSingleObject(event,10000);Sleep(200);CloseHandle(event);return result==WAIT_OBJECT_0?0:2;}
#endif
