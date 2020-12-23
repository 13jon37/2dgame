// Push warning level to 3 
#pragma warning(push, 3)
#include <stdio.h>
#include <windows.h>
#include <Psapi.h>
#include <emmintrin.h>
#pragma warning(pop)

#include <stdint.h>

#include "language_layer.h"
#include "main.h"

global_variable HWND g_game_window;
global_variable BOOL g_game_is_running;

global_variable game_bitmap_t g_back_buffer;
global_variable performance_data_t g_performance_data;

global_variable player_t g_player;

int CALLBACK WinMain(
	_In_	 HINSTANCE instance,
	_In_opt_ HINSTANCE previous_instance,
	_In_     PSTR      command_line,
	_In_     INT       cmd_show)
{
	UNREFERENCED_PARAMETER(instance);
	UNREFERENCED_PARAMETER(previous_instance);
	UNREFERENCED_PARAMETER(command_line);
	UNREFERENCED_PARAMETER(cmd_show);

	LARGE_INTEGER perf_counter_frequency_result = { 0 };
	QueryPerformanceFrequency(&perf_counter_frequency_result);
	i64 perf_counter_frequency = perf_counter_frequency_result.QuadPart;

	HMODULE nt_dll_handle = 0;

	if ((nt_dll_handle = GetModuleHandleA("ntdll.dll")) == NULL)
	{
		MessageBoxA(NULL, "Could not find ntdll.dll!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);

		goto Exit;
	}

	if ((NtQueryTimerResolution = 
		(_NtTimerResolution)GetProcAddress(nt_dll_handle, "NtQueryTimerResolution")) == NULL)
	{
		MessageBoxA(NULL, "Could not find ntdll.dll function!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);

		goto Exit;
	}

	NtQueryTimerResolution(&g_performance_data.minimum_timer_resolution, 
						   &g_performance_data.maximum_timer_resolution, 
		                   &g_performance_data.current_timer_resolution);

	if ((NtSetTimerResolution =
		(_NtSetTimerResolution)GetProcAddress(nt_dll_handle, "NtSetTimerResolution")) == NULL)
	{
		MessageBoxA(NULL, "Could not find ntdll.dll function!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);

		goto Exit;
	}

	// Set Timer Res to 1.00
	NtSetTimerResolution(10000, TRUE, &g_performance_data.current_timer_resolution);

	GetSystemInfo(&g_performance_data.system_info);

	if (game_is_running() == TRUE)
	{
		MessageBoxA(NULL, "Another instance of this program is already running!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);

		goto Exit;
	}

	if (create_main_game_window() != ERROR_SUCCESS)
	{
		goto Exit;
	}

	// If there is already memory allocated already, free it
	if (g_back_buffer.memory)
		VirtualFree(g_back_buffer.memory, 0, MEM_RELEASE);

	g_back_buffer.bitmap_info.bmiHeader.biSize = sizeof(g_back_buffer.bitmap_info.bmiHeader);
	g_back_buffer.bitmap_info.bmiHeader.biWidth = GAME_RES_WIDTH;
	g_back_buffer.bitmap_info.bmiHeader.biHeight = GAME_RES_HEIGHT;
	g_back_buffer.bitmap_info.bmiHeader.biBitCount = GAME_BPP;
	g_back_buffer.bitmap_info.bmiHeader.biCompression = BI_RGB;
	g_back_buffer.bitmap_info.bmiHeader.biPlanes = 1;

	g_back_buffer.memory = VirtualAlloc(NULL, GAME_DRAWING_AREA_MEMORY_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	if (g_back_buffer.memory == NULL)
	{
		MessageBoxA(NULL, "Failed to allocate memory for drawing surface!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);

		goto Exit;
	}

	g_player.x = 25;
	g_player.y = 25;

	g_game_is_running = TRUE;

	LARGE_INTEGER last_counter;
	QueryPerformanceCounter(&last_counter);

	i64 last_cycle_count = __rdtsc();

	while (g_game_is_running)
	{
		MSG message = { 0 };
		// Message loop
		if (PeekMessageA(&message, g_game_window, 0, 0, PM_REMOVE))
		{
			DispatchMessageA(&message);
		}

		/* Main Game Loop */

		process_player_input();

		render_frame_graphics();

		/* Calculate fps -- THANKS HANDMADE HERO, I WATCHED DONT REALLY UNDERSTAND THO */

		LARGE_INTEGER end_counter;
		QueryPerformanceCounter(&end_counter);

		i64 end_cycle_count = __rdtsc();

		i64 cycles_elapsed = end_cycle_count - last_cycle_count;
		i64 counter_elapsed = end_counter.QuadPart - last_counter.QuadPart;
		i32 ms_per_frame = (i32)((1000 * counter_elapsed) / perf_counter_frequency);
		g_performance_data.fps = (i32)(perf_counter_frequency / counter_elapsed);
		g_performance_data.mcpf = (i32)(cycles_elapsed / (1000 * 1000));

		// Every Two Frames Do these things
		if ((g_performance_data.total_frames_rendered % 120) == 0)
		{
			// Get Handle Count
			GetProcessHandleCount(GetCurrentProcess(), &g_performance_data.handle_count);

			// Get Memory Usage
			K32GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&g_performance_data.mem_info, sizeof(g_performance_data.mem_info));

			GetSystemTimeAsFileTime(&g_performance_data.current_system_time);

			// Get CPU Usage
			GetProcessTimes(GetCurrentProcess(),
				&g_performance_data.process_creation_time,
				&g_performance_data.process_exit_time,
				(FILETIME*)&g_performance_data.current_kernel_cpu_time,
				(FILETIME*)&g_performance_data.current_user_cpu_time);

			g_performance_data.cpu_percent =
				(double)((g_performance_data.current_kernel_cpu_time - g_performance_data.previous_kernel_cpu_time)
					+ (g_performance_data.current_user_cpu_time - g_performance_data.previous_user_cpu_time));

			g_performance_data.cpu_percent /= (g_performance_data.current_system_time - g_performance_data.previous_system_time);

			g_performance_data.cpu_percent /= g_performance_data.system_info.dwNumberOfProcessors;

			g_performance_data.cpu_percent *= 100;
		}

			// Jank way to limit my fps, idk tbh
		if (g_performance_data.mcpf <= TARGET_MICROSECONDS_PER_FRAME)
		{
			if (g_performance_data.mcpf <= (TARGET_MICROSECONDS_PER_FRAME - (g_performance_data.current_timer_resolution * .001f)))
				Sleep(1);

			g_performance_data.mcpf = TARGET_MICROSECONDS_PER_FRAME;
		}
		
		g_performance_data.total_frames_rendered++;

		char buffer[256] = { 0 };
		
		wsprintfA(buffer, "%dms/f  %df/s %dmc/f\n", ms_per_frame, g_performance_data.fps, g_performance_data.mcpf);
		OutputDebugStringA(buffer);

		g_performance_data.previous_kernel_cpu_time = g_performance_data.current_kernel_cpu_time;
		g_performance_data.previous_user_cpu_time = g_performance_data.current_user_cpu_time;
		g_performance_data.previous_system_time = g_performance_data.current_system_time;

		last_counter = end_counter;
		last_cycle_count = end_cycle_count;	
	}

Exit:

	return 0;
}

// Window Procedure
internal LRESULT 
CALLBACK main_window_proc(
	_In_ HWND window_handle,         // handle to window
	_In_ UINT message,				 // message identifier
	_In_ WPARAM w_param,			 // first message parameter
	_In_ LPARAM l_param)			 // second message parameter
{
	LRESULT result = 0;

	switch (message)
	{
		case WM_CREATE: {
			// Initialize the window. 
		} break;

		case WM_PAINT: {
			// Paint the window's client area. 
		} break;

		case WM_SIZE: {
			// Set the size and position of the window. 
		} break;

		case WM_DESTROY: {
			// Clean up window-specific data objects. 
			g_game_is_running = FALSE;
			PostQuitMessage(0);
		} break;

		case WM_CLOSE: {
			g_game_is_running = FALSE;
			PostQuitMessage(0);
		} break;

		// 
		// Process other messages. 
		// 

		default: {
			result = DefWindowProcA(window_handle, message, w_param, l_param);
		} break;
	}
	return result;
}

internal DWORD 
create_main_game_window(void)
{
	DWORD result = ERROR_SUCCESS;

	WNDCLASSEXA window_class = { 0 };

	// Register the Window Class
	window_class.cbSize = sizeof(WNDCLASSEXA);
	window_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	window_class.lpfnWndProc = main_window_proc;
	window_class.cbClsExtra = 0;
	window_class.cbWndExtra = 0;
	window_class.hInstance = GetModuleHandleA(NULL);
	window_class.hIcon = LoadIconA(NULL, IDI_APPLICATION);
	window_class.hIconSm = LoadIconA(NULL, IDI_APPLICATION);
	window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
	window_class.hbrBackground = CreateSolidBrush(RGB(255, 0, 255));
	window_class.lpszMenuName = NULL;
	window_class.lpszClassName = GAME_NAME "_window_class";

	//SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	if (RegisterClassExA(&window_class) == 0)
	{
		result = GetLastError();

		MessageBoxA(NULL, "Window Registration Failed!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);

		goto Exit;
	}

	// Create the Window
	g_game_window = CreateWindowExA(0,
									window_class.lpszClassName,
									GAME_NAME,
									WS_OVERLAPPEDWINDOW | WS_VISIBLE,
									CW_USEDEFAULT, CW_USEDEFAULT,
									640, 480,
									NULL, NULL,
									window_class.hInstance, NULL);

	if (g_game_window == NULL)
	{
		result = GetLastError();

		MessageBoxA(NULL, "Window Creation Failed!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);

		goto Exit;
	}

	g_performance_data.monitor_info.cbSize = sizeof(MONITORINFO);

	if (GetMonitorInfoA(MonitorFromWindow(g_game_window, MONITOR_DEFAULTTOPRIMARY), &g_performance_data.monitor_info) == 0)
	{
		result = ERROR_MONITOR_NO_DESCRIPTOR;

		goto Exit;
	}

	g_performance_data.monitor_width = 
		g_performance_data.monitor_info.rcMonitor.right - g_performance_data.monitor_info.rcMonitor.left;

	g_performance_data.monitor_height =
		g_performance_data.monitor_info.rcMonitor.bottom - g_performance_data.monitor_info.rcMonitor.top;

	if (SetWindowLongPtrA(g_game_window, GWL_STYLE, WS_VISIBLE) == 0)
	{
		result = GetLastError();

		goto Exit;
	}

	if (SetWindowPos(g_game_window, 
		HWND_TOPMOST,
		g_performance_data.monitor_info.rcMonitor.left,
		g_performance_data.monitor_info.rcMonitor.top, 
		g_performance_data.monitor_width,
		g_performance_data.monitor_height, 
		SWP_FRAMECHANGED) == 0)
	{
		result = GetLastError();

		goto Exit;
	}

Exit:

	return result;
}

internal BOOL 
game_is_running(void)
{
	HANDLE mutex = NULL;

	// Create a global mutex
	mutex = CreateMutexA(NULL, FALSE, "_GameMutex");

	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

internal void 
process_player_input(void)
{
	i16 escape_key_is_down = GetAsyncKeyState(VK_ESCAPE);

	i16 left_key_is_down = GetAsyncKeyState(VK_LEFT) | GetAsyncKeyState('A');
	i16 right_key_is_down = GetAsyncKeyState(VK_RIGHT) | GetAsyncKeyState('D');
	i16 up_key_is_down = GetAsyncKeyState(VK_UP) | GetAsyncKeyState('W');
	i16 down_key_is_down = GetAsyncKeyState(VK_DOWN) | GetAsyncKeyState('S');

	static i16 debug_key_was_down;

	static i16 left_key_was_down;
	static i16 right_key_was_down;
	static i16 up_key_was_down;
	static i16 down_key_was_down;

	if (escape_key_is_down)
	{
		SendMessageA(g_game_window, WM_CLOSE, 0, 0);
	}

	if (left_key_is_down)
	{
		if (g_player.x > 0)
			g_player.x--;
	}

	if (right_key_is_down)
	{
		if (g_player.x < GAME_RES_WIDTH - 16)
			g_player.x++;
	}

	if (up_key_is_down)
	{
		if (g_player.y > 0)
			g_player.y--;
	}

	if (down_key_is_down)
	{
		if (g_player.y < GAME_RES_HEIGHT - 16)
			g_player.y++;
	}

	left_key_was_down = left_key_is_down;
	right_key_was_down = right_key_is_down;
	up_key_was_down = up_key_is_down;
	down_key_was_down = down_key_is_down;
}

internal void 
render_frame_graphics(void)
{
#ifdef SIMD
	__m128i quad_pixel = { 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff };

	clear_screen(&quad_pixel);
#else
	pixel32_t pixel = { 0x7f, 0x00, 0x00, 0xff };

	clear_screen(&pixel);
#endif
	i32 screen_x  = g_player.x;
	i32 screen_y = g_player.y;

	i32 start_pixel = 
		((GAME_RES_WIDTH * GAME_RES_HEIGHT) - GAME_RES_WIDTH) - (GAME_RES_WIDTH * screen_y) + screen_x;

	for (i32 y = 0; y < 16; ++y)
	{
		for (i32 x = 0; x < 16; ++x)
		{
			memset((pixel32_t*)g_back_buffer.memory + (uintptr_t)start_pixel 
				+ x - ((uintptr_t)GAME_RES_WIDTH * y), 0xFF, sizeof(pixel32_t));
		}
	}

	HDC device_context = GetDC(g_game_window);

	StretchDIBits(device_context, 
		0, 
		0, 
		g_performance_data.monitor_width,
		g_performance_data.monitor_height,
		0, 
		0, 
		GAME_RES_WIDTH, 
		GAME_RES_HEIGHT,
		g_back_buffer.memory, 
		&g_back_buffer.bitmap_info, 
		DIB_RGB_COLORS, 
		SRCCOPY);

	// Text may flicker on different systems, kinda sucks but oh well

	SelectObject(device_context, (HFONT)GetStockObject(ANSI_FIXED_FONT));

	char debug_text_buffer[64] = { 0 };

	SetTextColor(device_context, RGB(255, 0, 0));

	SetBkMode(device_context, TRANSPARENT);

	sprintf_s(debug_text_buffer, sizeof(debug_text_buffer), "FPS: %d", g_performance_data.fps);
	TextOutA(device_context, 0, 0, debug_text_buffer, (int)strlen(debug_text_buffer));

	sprintf_s(debug_text_buffer, sizeof(debug_text_buffer), "MCPF: %d", g_performance_data.mcpf);
	TextOutA(device_context, 0, 16, debug_text_buffer, (int)strlen(debug_text_buffer));
	
	sprintf_s(debug_text_buffer, sizeof(debug_text_buffer), "Min timer res: %.02f", 
		g_performance_data.minimum_timer_resolution / 10000.f);
	TextOutA(device_context, 0, 32, debug_text_buffer, (int)strlen(debug_text_buffer));

	sprintf_s(debug_text_buffer, sizeof(debug_text_buffer), "Max timer res: %.02f", 
		g_performance_data.maximum_timer_resolution / 10000.f);
	TextOutA(device_context, 0, 48, debug_text_buffer, (int)strlen(debug_text_buffer));

	sprintf_s(debug_text_buffer, sizeof(debug_text_buffer), "Current timer res: %.02f", 
		g_performance_data.current_timer_resolution / 10000.f);
	TextOutA(device_context, 0, 64, debug_text_buffer, (int)strlen(debug_text_buffer));

	sprintf_s(debug_text_buffer, sizeof(debug_text_buffer), "Handles: %lu",
		g_performance_data.handle_count);
	TextOutA(device_context, 0, 80, debug_text_buffer, (int)strlen(debug_text_buffer));

	sprintf_s(debug_text_buffer, sizeof(debug_text_buffer), "Memory: %lu KB",
		g_performance_data.mem_info.PrivateUsage / 1024);
	TextOutA(device_context, 0, 96, debug_text_buffer, (int)strlen(debug_text_buffer));

	sprintf_s(debug_text_buffer, sizeof(debug_text_buffer), "CPU: %0.02f %%",
		g_performance_data.cpu_percent);
	TextOutA(device_context, 0, 112, debug_text_buffer, (int)strlen(debug_text_buffer));

	ReleaseDC(g_game_window, device_context);
}

#ifdef SIMD
__forceinline 
void clear_screen(_In_ __m128i* color)
{
	// 4 pixels at a time
	for (i32 i = 0; i < GAME_RES_WIDTH * GAME_RES_HEIGHT; i += 4)
		_mm_store_si128((pixel32_t*)g_back_buffer.memory + i, *color);
}
#else
__forceinline
void clear_screen(_In_ pixel32_t* pixel)
{
	for (i32 i = 0; i < GAME_RES_WIDTH * GAME_RES_HEIGHT; ++i)
		memcpy_s((pixel32_t*)g_back_buffer.memory + i, sizeof(pixel32_t), &pixel, sizeof(pixel32_t));
}
#endif