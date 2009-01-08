/*
This is a part of the LiteStep Shell Source code.

Copyright (C) 1997-2002 The LiteStep Development Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/ 
/****************************************************************************
11/20/01 - Bobby G. Vinyard (Message)
  - Removed WH_SHELL code, now using RegisterShellHook in LiteStep.exe only
  - HookMgr and Hook only need to be loaded if a module uses their hooks
    (mmsupport for instance) by settings "LSUseHooks" to true in the step.rc
11/17/01 - Bobby G. Vinyard (Message)
    - Removed the test code defines and tchar defines
01/10/01 - Bobby G. Vinyard (Message)
    - Added code to exclude certain programs from having their messages
      hooked. Add the executable exe name to HKCU\software\litestep\hookexclude
      as a string.
01/09/01 - Bobby G. Vinyard (Message)
    - Removed some external dependencies
    - GetMsgProc ignores messages that have the PM_NOREMOVE flag (they are just
      being peek'd and not removed from the que)
    - Removed CRT dependencies from the release build and made _DLLMainCRTStartup
      a stub to call DLLMain
11/25/00 - Bobby G. Vinyard (Message)
    - Readded code to hookmgr and hook to handle CallWndProc, evidently the cause
      for the lockups was the hooking of the debugger (in my case deb.exe), so code
      has been added to keep the deb.exe proccess from being hooked. Anyone using hooks
      should replace "deb.exe" in hook.cpp with the name of their debugger. (The code to
      enable CallWndProc is enabled by defining _TEST_CODE)   
06/03/00 - Bobby G. Vinyard (Message)
    - Aww... yet another rewrite of hook (Back to using WM_COPYDATA, silly me
      you can't pass the pointer to the MSG struct from GetMsgProc)
    - Probably should be using WM_CALLWNDPROC or WM_CALLWNDPROCRET but these
      lock my _tsystem up everytime i try set one... =\ ( so the implementation
      is commented out for now)
05/19/00 - Bobby G. Vinyard (Message)
    - Moved hInst into shared memory section
    - Does not return HSHELL_WINDOWCREATED/DESTROYED for cmd.exe under w2k
      (may have something to do with threading)
    - Skip catching HSHELL_TASKMAN, HSHELL_LANGUAGE, HSHELL_ACTIVATESHELLWINDOW
      they were causing lockups in win9x
05/18/00 - Bobby G. Vinyard (Message)
    - Removed the following libs from being linked (thet weren't needed):
      uuid.lib odbc32.lib odbccp32.lib gdi32.lib winspool.lib comdlg32.lib 
      ole32.lib oleaut32.lib advapi32.lib
    - Removed headers files that were not needed
    - Revised shell message handling, the shell messages are now sent only to 
      litestep to be handled by msgmgr and not hookmgr
    - Removed InstallShellFilter, shell hook is now install automatically
    - GetMsgProc no longer uses WM_COPYDATA, instead the LM_SHELLMESSAGE is sent
      to hookmgr with:
      wParam: Specifies whether the message has been removed from the queue
      lParam: Pointer to an MSG structure that contains details about the message
****************************************************************************/
#define HOOK_DLL
#include "hook.h"

#pragma data_seg("SHAREDATA")
static HHOOK g_hHookMessage = 0; // Hook for WH_GETMESSAGE
static HHOOK g_hHookCallWnd = 0;

#pragma data_seg()
HWND hwndHookMgr = NULL;
HWND hwndLiteStep = NULL;
HINSTANCE hInst = NULL;
bool bFilter = false;

MSG msgd;
COPYDATASTRUCT gcds = { 0, sizeof(MSG), &msgd };

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	UNREFERENCED_PARAMETER(lpvReserved);
	if (fdwReason == DLL_PROCESS_ATTACH)
	{

		// We don't need thread notifications for what we're doing.  Thus, get
		// rid of them, thereby eliminating some of the overhead of this DLL
		DisableThreadLibraryCalls(hInst);

		hInst = hinstDLL;
		hwndHookMgr = FindWindow(HOOKMGRWINDOWCLASS, HOOKMGRWINDOWNAME);
		hwndLiteStep = FindWindow("TApplication", "LiteStep");

		HKEY hKey;
		if (RegOpenKeyEx(HKEY_CURRENT_USER,
		                 "Software\\Litestep\\HookExclude",
		                 0,
		                 KEY_READ,
		                 &hKey) == ERROR_SUCCESS)
		{
			char szExeName[MAX_PATH+1] = { 0 };
			char *szFileName = szExeName;
			char *tcp;

			GetModuleFileName(NULL, szExeName, sizeof(szExeName)-1);

			for (tcp = szExeName; *tcp; tcp++)
			{
				if (*tcp == '/' || *tcp == '\\')
					szFileName = tcp + 1;
			}

			if (RegQueryValueEx(hKey, szFileName, NULL, NULL,
			                    NULL, NULL) == ERROR_SUCCESS)
			{
				bFilter = true;
			}
			else
			{
				bFilter = false;
			}
		}
		RegCloseKey(hKey);
	}
	return TRUE;
}

extern "C" BOOL _stdcall _DllMainCRTStartup(HINSTANCE hinstDLL,
	        DWORD fdwReason,
	        LPVOID lpvReserved)
{
	return DllMain(hinstDLL, fdwReason, lpvReserved);
}

void setMsgHook(HHOOK hMSG)
{
	g_hHookMessage = hMSG;
}

HHOOK getMsgHook()
{
	return g_hHookMessage;
}

void setCallWndHook(HHOOK hCallWnd)
{
	g_hHookCallWnd = hCallWnd;
}

HHOOK getCallWndHook()
{
	return g_hHookCallWnd;
}


void ProcessGetMsg(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if ((hwndHookMgr == NULL) || (!IsWindow(hwndHookMgr)))
	{
		hwndHookMgr = FindWindow(HOOKMGRWINDOWCLASS, HOOKMGRWINDOWNAME);
	}
	if (hwndHookMgr != NULL)
	{
		gcds.dwData = msg;
		msgd.hwnd = hwnd;
		msgd.message = msg;
		msgd.lParam = lParam;
		msgd.wParam = wParam;
		SendMessage(hwndHookMgr, WM_COPYDATA, (WPARAM)msg, (LPARAM) & gcds);
	}
}


#ifdef _DEBUG 
// *** DO NOT PUT ANY BREAKPOINTS IN THIS CODE!!!
BOOL IsCurProcDebugger()
{
	typedef BOOL (WINAPI* IsDebuggerPresentProc)();

	IsDebuggerPresentProc fnIsDebuggerPresent = (IsDebuggerPresentProc)
		GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsDebuggerPresent");

	// If a debugger is attached use the current directory as base path
	return fnIsDebuggerPresent && fnIsDebuggerPresent();
}
#endif  // _DEBUG


/*
Call back for WH_GETMESSAGE
*/
LRESULT CALLBACK GetMsgProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	PMSG pmsg;

#ifdef _DEBUG
	if (!IsCurProcDebugger()) // If you are not in the debugger...
#endif // _DEBUG
	{
		if ((nCode < 0) || (bFilter) /*|| (wParam == PM_REMOVE)*/)
		{
			return (CallNextHookEx(g_hHookMessage, nCode, wParam, lParam));
		}

		pmsg = (PMSG)lParam;
		UINT msg = pmsg->message;
		if (pmsg && pmsg->hwnd)
		{
			if ((msg < WM_USER) && (msg > WM_NULL))
			{
				switch (msg)
				{
					case WM_TIMER:
					break;
					case WM_PAINT:
					break;
					case WM_SYSCOMMAND:
					default:
					{
						//_LSDEBUGPRINTMSG("GetMsgProc Hook Called for Message", (int)msg);
						ProcessGetMsg(pmsg->hwnd, pmsg->message, pmsg->wParam, pmsg->lParam);
					}
					break;
				}
			}
		}
	}

	return (CallNextHookEx(g_hHookMessage, nCode, wParam, lParam));
}


/*
Call back for WH_CALLWNDPROC
*/
LRESULT CALLBACK CallWndProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	PCWPSTRUCT pmsg;

#ifdef _DEBUG
	if (!IsCurProcDebugger()) // If you are not in the debugger...
#endif // _DEBUG
	{
		if ((nCode < 0) || (bFilter))
		{
			return (CallNextHookEx(g_hHookCallWnd, nCode, wParam, lParam));
		}

		pmsg = (PCWPSTRUCT)lParam;
		UINT msg = pmsg->message;
		if (pmsg && pmsg->hwnd)
		{
			if ((msg < WM_USER) && (msg > WM_NULL))
			{
				switch (msg)
				{
					case WM_ERASEBKGND:
					break;
					case WM_NCPAINT:
					break;
					case WM_COPYDATA:
					break;
					case WM_SYSCOMMAND:
					default:
					{
						//_LSDEBUGPRINTMSG("CallWndProc Hook Called for Message", (int)msg);
						ProcessGetMsg(pmsg->hwnd, pmsg->message, pmsg->wParam, pmsg->lParam);
					}
					break;
				}
			}
		}
	}

	return (CallNextHookEx(g_hHookCallWnd, nCode, wParam, lParam));
}