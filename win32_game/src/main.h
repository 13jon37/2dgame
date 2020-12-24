#pragma once

#define GAME_NAME "win32_game"

#define GAME_RES_WIDTH  512
#define GAME_RES_HEIGHT 288
#define GAME_BPP        32
#define GAME_DRAWING_AREA_MEMORY_SIZE ((GAME_RES_WIDTH * GAME_RES_HEIGHT) * (GAME_BPP / 8))

// 16.67 milliseconds is 60 frames per second.
#define TARGET_MICROSECONDS_PER_FRAME 16667ULL // Not used but will keep 

#define SIMD

// Disable warning about inline function information 
// because its really not a warning just unneeded info
#pragma warning(disable: 4711)

// Disable 4 byte padding warning for now
#pragma warning(disable: 4820)

// Disable PULONG warning
#pragma warning(disable: 4057)

// Disable pixel32_t* cast warning
#pragma warning(disable: 4133)

#define SUIT_0				0
#define SUIT_1				1
#define SUIT_2				2
#define FACING_DOWN_0		0
#define FACING_DOWN_1		1
#define FACING_DOWN_2		2
#define FACING_LEFT_0		3
#define FACING_LEFT_1		4
#define FACING_LEFT_2		5
#define FACING_RIGHT_0		6
#define FACING_RIGHT_1		7
#define FACING_RIGHT_2		8
#define FACING_UP_0			9
#define FACING_UP_1			10
#define FACING_UP_2			11
#define DIRECTION_DOWN		0
#define DIRECTION_LEFT		1
#define DIRECTION_RIGHT		2
#define DIRECTION_UP		3

typedef LONG(NTAPI* _NtTimerResolution) (OUT PULONG MinimalResolution, OUT PULONG MaximumResolution, OUT PULONG CurrentResolution);
typedef	NTSYSAPI NTSTATUS (NTAPI* _NtSetTimerResolution) (IN ULONG DesiredResolution, IN BOOLEAN SetResolution, OUT PULONG CurrentResolution);

_NtTimerResolution NtQueryTimerResolution;
_NtSetTimerResolution NtSetTimerResolution;

typedef struct GAME_BITMAP_STRUCT {
	BITMAPINFO bitmap_info;
	void* memory;
} game_bitmap_t;

typedef struct PIXEL32_STRUCT {
	u8 blue;
	u8 green;
	u8 red;
	u8 alpha;
} pixel32_t;

typedef struct PERFORMANCE_DATA_STRUCT {
	u64 total_frames_rendered;
	i32 monitor_width;
	i32 monitor_height;
	i32 mcpf;
	i32 fps;
	MONITORINFO monitor_info;
	LONG minimum_timer_resolution;
	LONG maximum_timer_resolution;
	LONG current_timer_resolution;
	DWORD handle_count;
	PROCESS_MEMORY_COUNTERS_EX mem_info;
	SYSTEM_INFO system_info;
	i64 current_system_time;
	i64 previous_system_time;
	FILETIME process_creation_time;
	FILETIME process_exit_time;
	i64 current_user_cpu_time;
	i64 current_kernel_cpu_time;
	i64 previous_user_cpu_time;
	i64 previous_kernel_cpu_time;
	double cpu_percent;
} performance_data_t;

typedef struct PLAYER_STRUCT {
	char name[12];
	game_bitmap_t sprite[3][12];
	i32 x, y;
	i32 speed;
	u8 movement_remaining;
	u8 current_suit;
	u8 sprite_index;
	u8 direction;
	i32 health;
} player_t;

internal LRESULT CALLBACK main_window_proc(_In_ HWND window_handle, _In_ UINT message, _In_ WPARAM w_param, _In_ LPARAM l_param);
internal DWORD create_main_game_window(void);
internal BOOL game_is_running(void);
internal void process_player_input(void);
internal void render_frame_graphics(void);
internal DWORD load_32_bitpp_bitmap_from_file(_In_ char* file_name, _Inout_ game_bitmap_t* game_bit_map);
internal DWORD initalize_player(void);
internal void blit_32bpp_bitmap_to_buffer(_In_ game_bitmap_t* game_bit_map, _In_ u16 x, _In_ u16 y);

#ifdef SIMD
__forceinline
void clear_screen(_In_ __m128i* color);
#else
__forceinline
void clear_screen(_In_ pixel32_t* pixel);
#endif
