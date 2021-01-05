#include <vector>
#include <deque>
#include <string>
#include <cstdio>
#include <cinttypes>

#include "includewindows.h"
#include "log.h"

// Attempt to place window under cursor instead of the same relative coordinates.
const bool positionUnderCursor = true;

// Margin from the edges of monitor. Only used if 'positionUnderCursor' is also true.
const int32_t edgeMarginX = 20;
const int32_t edgeMarginY = 20;

// Margin used if cursor is positioned in taskbar area when an explorer window opens (like in a case where new window is opened by middle clicking)
const bool useDifferentMarginIfInTaskbarArea = true;
const int32_t edgeMarginInTaskbarX = 120;
const int32_t edgeMarginInTaskbarY = 20;

////////////////////////////////////////////////

std::string getLastErrorAsString()
{
	DWORD errorMessageID = ::GetLastError();
	if (errorMessageID == 0)
		return std::string();

	LPSTR messageBuffer = nullptr;
	size_t size = FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, nullptr);

	std::string message(messageBuffer, size);

	LocalFree(messageBuffer);

	return message;
}

std::string getProcessName(HWND hWnd)
{
	char buffer[MAX_PATH] = { 0 };
	DWORD dwProcId = 0;

	GetWindowThreadProcessId(hWnd, &dwProcId);

	HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwProcId);
	if (hProc != nullptr)
	{
		GetModuleBaseNameA(hProc, nullptr, buffer, MAX_PATH);
		CloseHandle(hProc);
	}
	else
	{
		dprintf("OpenProcess error: %s\n", getLastErrorAsString().c_str());
	}

	return std::string(buffer);
}

bool iequals(const std::string& a, const std::string& b)
{
	return std::equal(a.begin(), a.end(),
		b.begin(), b.end(),
		[](char a, char b) {
		return tolower(a) == tolower(b);
	});
}

bool iequals(const std::wstring& a, const std::wstring& b)
{
	return std::equal(a.begin(), a.end(),
		b.begin(), b.end(),
		[](wchar_t a, wchar_t b) {
		return towlower(a) == towlower(b);
	});
}

bool pointWithinRect(POINT point, RECT rect)
{
	return point.x >= rect.left &&
		   point.x <= rect.right &&
		   point.y >= rect.top &&
		   point.y <= rect.bottom;
}

bool cursorInTaskbarArea(POINT cursorPosition, RECT workArea, RECT monitorArea)
{
	return pointWithinRect(cursorPosition, monitorArea) && !pointWithinRect(cursorPosition, workArea);
}

BOOL CALLBACK monitorEnumProc(HMONITOR monitor, HDC dc, LPRECT rekt, LPARAM userdata)
{
	std::vector<HMONITOR> &monitors = *(std::vector<HMONITOR>*)userdata;
	monitors.push_back(monitor);
	return true;
}

#undef max
#undef min

inline int32_t min(int32_t a, int32_t b)
{
	return (a <= b) ? a : b;
}
inline int32_t max(int32_t a, int32_t b)
{
	return (a >= b) ? a : b;
}
inline int32_t clamp(int32_t value, int32_t minValue, int32_t maxValue)
{
	return min(maxValue, max(minValue, value));
}

bool repositionWindow(const HWND handle)
{
	std::vector<HMONITOR> monitors;
	monitors.clear();
	if (EnumDisplayMonitors(nullptr, nullptr, &monitorEnumProc, (LPARAM)&monitors) == 0)
	{
		dprintf("EnumDisplayMonitors error: %s\n", getLastErrorAsString().c_str());
		return false;
	}

	POINT cursorPosition;
	if (!GetCursorPos(&cursorPosition))
	{
		dprintf("GetCursorPos error: %s\n", getLastErrorAsString().c_str());
		return false;
	}

	int32_t cursorMonitorIndex = -1;

	for (int32_t i = 0; i < monitors.size(); ++i)
	{
		MONITORINFO monitorInfo;
		monitorInfo.cbSize = sizeof(monitorInfo);

		if (!GetMonitorInfoA(monitors[i], &monitorInfo))
		{
			dprintf("GetMonitorInfoA error [monitor %d]: %s\n", i, getLastErrorAsString().c_str());
			return false;
		}

		if (pointWithinRect(cursorPosition, monitorInfo.rcMonitor))
		{
			dprintf("Cursor is on monitor %d\n", i);
			cursorMonitorIndex = i;
			break;
		}
	}

	if (cursorMonitorIndex == -1)
	{
		dprintf("Could not determine which monitor has the cursor.\n");
		return false;
	}

	HMONITOR targetMonitor = monitors[cursorMonitorIndex];

	RECT windowRect;
	if (!GetWindowRect(handle, &windowRect))
	{
		dprintf("GetWindowRect error: %s\n", getLastErrorAsString().c_str());
		return false;
	}

	HMONITOR currentMonitor = MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST);
	MONITORINFO currentMonitorInfo;
	currentMonitorInfo.cbSize = sizeof(MONITORINFO);
	if (!GetMonitorInfoA(currentMonitor, &currentMonitorInfo))
		return false;

	MONITORINFO targetMonitorInfo;
	targetMonitorInfo.cbSize = sizeof(targetMonitorInfo);
	if (!GetMonitorInfoA(targetMonitor, &targetMonitorInfo))
		return false;

	dprintf("  Current monitor : (%d, %d) - (%d, %d)\n", currentMonitorInfo.rcWork.left, currentMonitorInfo.rcWork.top, currentMonitorInfo.rcWork.right, currentMonitorInfo.rcWork.bottom);
	dprintf("  Target monitor  : (%d, %d) - (%d, %d)\n", targetMonitorInfo.rcWork.left, targetMonitorInfo.rcWork.top, targetMonitorInfo.rcWork.right, targetMonitorInfo.rcWork.bottom);

	dprintf("  Window Rekt (%d, %d) (%d, %d)\n", windowRect.left, windowRect.top, windowRect.right, windowRect.bottom);

	int32_t left = windowRect.left - currentMonitorInfo.rcWork.left;
	int32_t top = windowRect.top - currentMonitorInfo.rcWork.top;
	
	int32_t screenWidth = targetMonitorInfo.rcWork.right - targetMonitorInfo.rcWork.left;
	int32_t screenHeight = targetMonitorInfo.rcWork.bottom - targetMonitorInfo.rcWork.top;

	int32_t width = windowRect.right - windowRect.left;
	width = min(width, screenWidth);

	int32_t height = windowRect.bottom - windowRect.top;
	height = min(height, screenHeight);

	if (positionUnderCursor)
	{
		const bool inTaskbar = useDifferentMarginIfInTaskbarArea && cursorInTaskbarArea(cursorPosition, targetMonitorInfo.rcWork, targetMonitorInfo.rcMonitor);
		int32_t MarginX = inTaskbar ? edgeMarginInTaskbarX : edgeMarginX;
		int32_t MarginY = inTaskbar ? edgeMarginInTaskbarY : edgeMarginY;

		left = cursorPosition.x - targetMonitorInfo.rcMonitor.left - width / 2;

		int32_t minLeft = 0 + MarginX;
		int32_t maxLeft = screenWidth - width - MarginX;
		left = clamp(left, minLeft, maxLeft);

		top = cursorPosition.y - targetMonitorInfo.rcMonitor.top - height / 3;

		int32_t minTop = 0 + MarginY;
		int32_t maxTop = screenHeight - height - MarginY;
		top = clamp(top, minTop, maxTop);
	}

	POINT targetPos;
	targetPos.x = targetMonitorInfo.rcWork.left + left;
	targetPos.y = targetMonitorInfo.rcWork.top + top;

	if (!SetWindowPos(handle, nullptr, targetPos.x, targetPos.y, width, height, SWP_NOZORDER | SWP_NOOWNERZORDER))
	{
		dprintf("SetWindowPos error: %s\n", getLastErrorAsString().c_str());
		return false;
	}

	dprintf("The window is positioned on the target monitor, yay!\n");

	return true;
}

void WinEventProc(
	HWINEVENTHOOK hWinEventHook,
	DWORD event,
	HWND hwnd,
	LONG idObject,
	LONG idChild,
	DWORD idEventThread,
	DWORD dwmsEventTime
)
{
	if (event != EVENT_OBJECT_SHOW)
		return;

	if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF)
		return;

	// Might be unnecessary considering the event type
	if (!IsWindowVisible(hwnd))
		return;

	// Require window to be standalone (no parent)
	if (hwnd != GetAncestor(hwnd, GA_ROOT))
		return;

	// Check process name, looking for explorer.exe only
	const std::string currentProcessName = getProcessName(hwnd);
	if (!iequals(currentProcessName, "explorer.exe"))
		return;

	// Don't allow empty titles
	wchar_t titleBuffer[1024] = { 0 };
	if (GetWindowTextW(hwnd, titleBuffer, 1024) == 0)
		return;

	std::wstring windowTitle(titleBuffer);

	char classNameBuffer[1024] = { 0 };
	if (GetClassName(hwnd, classNameBuffer, 1024) == 0)
		return;

	std::string className(classNameBuffer);
	
	// Windows explorer instances have this class name
	if (className != "CabinetWClass")
		return;

	dprintf("EVENT FOR 0x%016" PRIX64 " %s\n", (ptrdiff_t)hwnd, currentProcessName.c_str());
	dwprintf(L"   Title    : %s\n", windowTitle.c_str());
	dprintf(  "   Class    : %s\n", className.c_str());
	repositionWindow(hwnd);
	dprintf("\n");
}

bool IsDuplicateProcessRunning(const std::string processName, DWORD pid)
{
	bool exists = false;

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);

	if (Process32First(snapshot, &entry))
	{
		while (Process32Next(snapshot, &entry))
		{
// 			dprintf("Process : %s (pid %d)\n", entry.szExeFile, entry.th32ProcessID);
			if (iequals(entry.szExeFile, processName) && entry.th32ProcessID != pid)
			{
				exists = true;
				break;
			}
		}
	}

	CloseHandle(snapshot);
	return exists;
}

bool checkHasDuplicateProcess()
{
	char currentProcessNameFull[MAX_PATH];
	GetModuleFileName(nullptr, currentProcessNameFull, MAX_PATH);

	const char *last = strrchr(currentProcessNameFull, '\\');
	std::string currentProcessName(last != NULL ? last + 1 : currentProcessNameFull);

// 	dprintf("This exe:  %s (pid %d)\n", currentProcessName.c_str(), GetCurrentProcessId());
	return IsDuplicateProcessRunning(currentProcessName, GetCurrentProcessId());
}

int __stdcall wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR cmdArgs, int windowShowCmd)
{
	// Prevent duplicate instances of this software
	if (checkHasDuplicateProcess())
	{
		MessageBoxA(nullptr, "This program is already running.\n\nExisting process can be closed from Task Manager (open by pressing Ctrl+Shift+Esc).", "Duplicate process", MB_OK | MB_ICONERROR);
		return 0;
	}

	HWINEVENTHOOK hook = SetWinEventHook(
		EVENT_OBJECT_SHOW,
		EVENT_OBJECT_SHOW,
		nullptr,
		&WinEventProc,
		0,
		0,
		WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

	std::vector<HMONITOR> monitors;
	if (EnumDisplayMonitors(nullptr, nullptr, &monitorEnumProc, (LPARAM)&monitors) == 0)
	{
		dprintf("EnumDisplayMonitors error: %s\n", getLastErrorAsString().c_str());
		return -1;
	}

	for (int32_t i = 0; i < monitors.size(); ++i)
	{
		MONITORINFO monitorInfo;
		monitorInfo.cbSize = sizeof(monitorInfo);

		if (GetMonitorInfoA(monitors[i], &monitorInfo))
		{
			int screenWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
			int screenHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;

			dprintf("Monitor %d:\n", i);
			dprintf(  "  Size   : %d x %d\n", screenWidth, screenHeight);
			dprintf(  "  Area   : (%d, %d) - (%d, %d)\n", monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top, monitorInfo.rcMonitor.right, monitorInfo.rcMonitor.bottom);
			dprintf("\n");
		}
	}

	bool running = true;
	while (running)
	{
		MSG msg = { };
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) // Needed for hook events
		{
			// NOP
		}

		Sleep(25);
	}

	return 0;
}