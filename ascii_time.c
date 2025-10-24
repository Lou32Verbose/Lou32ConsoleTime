#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <time.h>
#include <locale.h>
#include <sal.h>
#include <string.h>

// Layout constants
#define ASCII_CHAR_WIDTH 5
#define ASCII_CHAR_HEIGHT 7
#define ASCII_CHAR_SPACING 6
#define TIME_COLON_OFFSET 12
#define TIME_AMPM_OFFSET 36
#define DATE_DASH1_OFFSET 24
#define DATE_DASH2_OFFSET 42
#define CONSOLE_FALLBACK_WIDTH 80
#define CONSOLE_FALLBACK_HEIGHT 25
#define UPDATE_INTERVAL_MS 50
#define RESIZE_EVENT_BUFFER_SIZE 128

// ASCII art definitions
static const wchar_t* g_asciiDigits[10] = {
    L" ███ \n██ ██\n██ ██\n██ ██\n██ ██\n ███ \n     ",
    L"  ██ \n ███ \n  ██ \n  ██ \n  ██ \n█████\n     ",
    L"████ \n   ██\n  ██ \n ██  \n██   \n█████\n     ",
    L"████ \n   ██\n ███ \n   ██\n   ██\n████ \n     ",
    L"   ██\n  ███\n ██ ██\n██████\n   ██\n   ██\n     ",
    L"█████\n██   \n████ \n   ██\n   ██\n████ \n     ",
    L" ███ \n██   \n████ \n██ ██\n██ ██\n ███ \n     ",
    L"█████\n   ██\n  ██ \n ██  \n ██  \n ██  \n     ",
    L" ███ \n██ ██\n ███ \n██ ██\n██ ██\n ███ \n     ",
    L" ███ \n██ ██\n██ ██\n ████\n   ██\n ███ \n     "
};

static const wchar_t* g_asciiColon = 
    L"     \n     \n  ██ \n     \n  ██ \n     \n     ";
static const wchar_t* g_asciiSpace = 
    L"     \n     \n     \n     \n     \n     \n     ";
static const wchar_t* g_asciiDash = 
    L"     \n     \n     \n█████\n     \n     \n     ";
static const wchar_t* g_asciiA = 
    L"  ██ \n ████\n██ ██\n█████\n██ ██\n██ ██\n     ";
static const wchar_t* g_asciiM = 
    L"█   █\n██ ██\n█████\n██ ██\n██ ██\n██ ██\n     ";
static const wchar_t* g_asciiP = 
    L"████ \n██ ██\n██ ██\n████ \n██   \n██   \n     ";

// State tracking structure
typedef struct {
    int hourTens;
    int hourOnes;
    int minuteTens;
    int minuteOnes;
    BOOL isPM;
    int yearThousands;
    int yearHundreds;
    int yearTens;
    int yearOnes;
    int monthTens;
    int monthOnes;
    int dayTens;
    int dayOnes;
    BOOL initialized;
} DisplayState;

// Global variables
static SHORT g_lastConsoleWidth = 0;
static SHORT g_lastConsoleHeight = 0;
static DisplayState g_displayState = { 0 };
static HANDLE g_hConsole = INVALID_HANDLE_VALUE;

// Function declarations
_Success_(return != NULL)
_Ret_notnull_
static const wchar_t* GetAsciiDigit(_In_ int digit);
static void SetCursorPosition(_In_ SHORT x, _In_ SHORT y);
static void UpdateCharPosition(
    _In_ SHORT x, 
    _In_ SHORT y, 
    _In_ const wchar_t* asciiChar
);
static void UpdateCharPositionIfChanged(
    _In_ SHORT x,
    _In_ SHORT y,
    _In_ const wchar_t* asciiChar,
    _In_ const wchar_t* oldAsciiChar
);
static void PrintTimeAscii(
    _In_ const SYSTEMTIME* st, 
    _In_ SHORT startX, 
    _In_ SHORT startY,
    _In_ BOOL forceRedraw
);
static void PrintDateAscii(
    _In_ const SYSTEMTIME* st, 
    _In_ SHORT startX, 
    _In_ SHORT startY,
    _In_ BOOL forceRedraw
);
static void ClearScreenSafe(void);
static void HideCursor(_In_ BOOL hide);
static void PrintTitleLine(void);
static BOOL CheckConsoleResize(void);
static void GetConsoleSize(_Out_ SHORT* width, _Out_ SHORT* height);
static BOOL CheckForResizeEvent(void);
static void RedrawAll(_In_ const SYSTEMTIME* st);

// Get current console size
static void GetConsoleSize(_Out_ SHORT* width, _Out_ SHORT* height) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    
    if (GetConsoleScreenBufferInfo(g_hConsole, &csbi)) {
        *width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        *height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    } else {
        *width = CONSOLE_FALLBACK_WIDTH;
        *height = CONSOLE_FALLBACK_HEIGHT;
    }
}

// Check if console has been resized
static BOOL CheckConsoleResize(void) {
    SHORT currentWidth, currentHeight;
    GetConsoleSize(&currentWidth, &currentHeight);
    
    if (g_lastConsoleWidth == 0 && g_lastConsoleHeight == 0) {
        g_lastConsoleWidth = currentWidth;
        g_lastConsoleHeight = currentHeight;
        return FALSE;
    }
    
    if (currentWidth != g_lastConsoleWidth || 
        currentHeight != g_lastConsoleHeight) {
        g_lastConsoleWidth = currentWidth;
        g_lastConsoleHeight = currentHeight;
        return TRUE;
    }
    
    return FALSE;
}

// Fast check for resize events
static BOOL CheckForResizeEvent(void) {
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    if (hInput == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    
    INPUT_RECORD irInBuf[RESIZE_EVENT_BUFFER_SIZE];
    DWORD cNumRead;
    
    if (!PeekConsoleInput(hInput, irInBuf, RESIZE_EVENT_BUFFER_SIZE, &cNumRead)) {
        return FALSE;
    }
    
    for (DWORD i = 0; i < cNumRead; i++) {
        if (irInBuf[i].EventType == WINDOW_BUFFER_SIZE_EVENT) {
            ReadConsoleInput(hInput, irInBuf, cNumRead, &cNumRead);
            return TRUE;
        }
    }
    
    return FALSE;
}

// Get ASCII art representation of a digit
_Success_(return != NULL)
_Ret_notnull_
static const wchar_t* GetAsciiDigit(_In_ int digit) {
    if (digit < 0 || digit > 9) {
        return g_asciiSpace;
    }
    return g_asciiDigits[digit];
}

// Position cursor at specific coordinates
static void SetCursorPosition(_In_ SHORT x, _In_ SHORT y) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    
    if (!GetConsoleScreenBufferInfo(g_hConsole, &csbi)) {
        return;
    }
    
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= csbi.dwSize.X) x = csbi.dwSize.X - 1;
    if (y >= csbi.dwSize.Y) y = csbi.dwSize.Y - 1;
    
    COORD cursorPos = { x, y };
    SetConsoleCursorPosition(g_hConsole, cursorPos);
}

// Hide or show the cursor
static void HideCursor(_In_ BOOL hide) {
    CONSOLE_CURSOR_INFO cursorInfo;
    
    if (!GetConsoleCursorInfo(g_hConsole, &cursorInfo)) {
        return;
    }
    
    cursorInfo.bVisible = !hide;
    SetConsoleCursorInfo(g_hConsole, &cursorInfo);
}

// Update a single character position (direct overwrite, no clearing)
static void UpdateCharPosition(
    _In_ SHORT x, 
    _In_ SHORT y, 
    _In_ const wchar_t* asciiChar
) {
    if (!asciiChar) return;
    
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    
    if (!GetConsoleScreenBufferInfo(g_hConsole, &csbi)) {
        return;
    }
    
    if (x < 0 || y < 0 || x >= csbi.dwSize.X || y >= csbi.dwSize.Y) {
        return;
    }
    
    for (int line = 0; line < ASCII_CHAR_HEIGHT; line++) {
        SHORT currentY = (SHORT)(y + line);
        
        if (currentY >= csbi.dwSize.Y) {
            break;
        }
        
        SetCursorPosition(x, currentY);
        
        const wchar_t* current = asciiChar;
        for (int i = 0; i < line && *current; i++) {
            while (*current && *current != L'\n') {
                current++;
            }
            if (*current == L'\n') {
                current++;
            }
        }
        
        if (*current) {
            const wchar_t* end = current;
            while (*end && *end != L'\n') {
                end++;
            }
            
            int charsToWrite = (int)(end - current);
            if (x + charsToWrite > csbi.dwSize.X) {
                charsToWrite = csbi.dwSize.X - x;
            }
            
            for (int i = 0; i < charsToWrite; i++) {
                fputwc(current[i], stdout);
            }
        }
    }
    fflush(stdout);
}

// Update character position only if it has changed
static void UpdateCharPositionIfChanged(
    _In_ SHORT x,
    _In_ SHORT y,
    _In_ const wchar_t* asciiChar,
    _In_ const wchar_t* oldAsciiChar
) {
    if (asciiChar != oldAsciiChar) {
        UpdateCharPosition(x, y, asciiChar);
    }
}

// Print the title line
static void PrintTitleLine(void) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(g_hConsole, &csbi)) {
        return;
    }
    
    const WORD titleBackgroundAttribute = BACKGROUND_RED | BACKGROUND_GREEN | 
                                          BACKGROUND_BLUE | BACKGROUND_INTENSITY;
    const WORD titleTextAttribute = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    const WORD normalAttribute = FOREGROUND_RED | FOREGROUND_GREEN | 
                                 FOREGROUND_BLUE;
    
    SetConsoleTextAttribute(g_hConsole, titleBackgroundAttribute);
    
    DWORD cCharsWritten;
    COORD coord = { 0, 0 };
    SHORT lineWidth = csbi.dwSize.X;
    
    FillConsoleOutputCharacterW(
        g_hConsole, L' ', lineWidth, coord, &cCharsWritten
    );
    FillConsoleOutputAttribute(
        g_hConsole, titleBackgroundAttribute, lineWidth, coord, &cCharsWritten
    );
    
    SetCursorPosition(0, 0);
    SetConsoleTextAttribute(g_hConsole, titleBackgroundAttribute | titleTextAttribute);
    wprintf(L"Lou32 Visual Time & Date System Display Utility Apparatus");
    fflush(stdout);
    
    SetConsoleTextAttribute(g_hConsole, normalAttribute);
}

// Print time with smart updates
static void PrintTimeAscii(
    _In_ const SYSTEMTIME* st, 
    _In_ SHORT startX, 
    _In_ SHORT startY,
    _In_ BOOL forceRedraw
) {
    int hour12 = st->wHour % 12;
    if (hour12 == 0) {
        hour12 = 12;
    }
    
    BOOL isPM = (st->wHour >= 12);
    
    int hourTens = hour12 / 10;
    int hourOnes = hour12 % 10;
    int minuteTens = st->wMinute / 10;
    int minuteOnes = st->wMinute % 10;

    if (forceRedraw || !g_displayState.initialized) {
        // Full redraw
        UpdateCharPosition(startX, startY, GetAsciiDigit(hourTens));
        UpdateCharPosition(
            (SHORT)(startX + ASCII_CHAR_SPACING), startY, GetAsciiDigit(hourOnes)
        );
        UpdateCharPosition(
            (SHORT)(startX + TIME_COLON_OFFSET), startY, g_asciiColon
        );
        UpdateCharPosition(
            (SHORT)(startX + TIME_COLON_OFFSET + ASCII_CHAR_SPACING), 
            startY, 
            GetAsciiDigit(minuteTens)
        );
        UpdateCharPosition(
            (SHORT)(startX + TIME_COLON_OFFSET + ASCII_CHAR_SPACING * 2), 
            startY, 
            GetAsciiDigit(minuteOnes)
        );
        
        SHORT ampmX = (SHORT)(startX + TIME_AMPM_OFFSET);
        const wchar_t* ampmChar = isPM ? g_asciiP : g_asciiA;
        UpdateCharPosition(ampmX, startY, ampmChar);
        UpdateCharPosition((SHORT)(ampmX + ASCII_CHAR_SPACING), startY, g_asciiM);
    } else {
        // Smart update - only changed digits
        UpdateCharPositionIfChanged(
            startX, startY, 
            GetAsciiDigit(hourTens), 
            GetAsciiDigit(g_displayState.hourTens)
        );
        UpdateCharPositionIfChanged(
            (SHORT)(startX + ASCII_CHAR_SPACING), startY, 
            GetAsciiDigit(hourOnes), 
            GetAsciiDigit(g_displayState.hourOnes)
        );
        UpdateCharPositionIfChanged(
            (SHORT)(startX + TIME_COLON_OFFSET + ASCII_CHAR_SPACING), startY, 
            GetAsciiDigit(minuteTens), 
            GetAsciiDigit(g_displayState.minuteTens)
        );
        UpdateCharPositionIfChanged(
            (SHORT)(startX + TIME_COLON_OFFSET + ASCII_CHAR_SPACING * 2), startY, 
            GetAsciiDigit(minuteOnes), 
            GetAsciiDigit(g_displayState.minuteOnes)
        );
        
        if (isPM != g_displayState.isPM) {
            SHORT ampmX = (SHORT)(startX + TIME_AMPM_OFFSET);
            const wchar_t* ampmChar = isPM ? g_asciiP : g_asciiA;
            UpdateCharPosition(ampmX, startY, ampmChar);
        }
    }
    
    // Update state
    g_displayState.hourTens = hourTens;
    g_displayState.hourOnes = hourOnes;
    g_displayState.minuteTens = minuteTens;
    g_displayState.minuteOnes = minuteOnes;
    g_displayState.isPM = isPM;
}

// Print date with smart updates
static void PrintDateAscii(
    _In_ const SYSTEMTIME* st, 
    _In_ SHORT startX, 
    _In_ SHORT startY,
    _In_ BOOL forceRedraw
) {
    int yearThousands = st->wYear / 1000;
    int yearHundreds = (st->wYear % 1000) / 100;
    int yearTens = (st->wYear % 100) / 10;
    int yearOnes = st->wYear % 10;
    int monthTens = st->wMonth / 10;
    int monthOnes = st->wMonth % 10;
    int dayTens = st->wDay / 10;
    int dayOnes = st->wDay % 10;

    if (forceRedraw || !g_displayState.initialized) {
        // Full redraw
        SHORT currentX = startX;
        UpdateCharPosition(currentX, startY, GetAsciiDigit(yearThousands));
        currentX = (SHORT)(currentX + ASCII_CHAR_SPACING);
        UpdateCharPosition(currentX, startY, GetAsciiDigit(yearHundreds));
        currentX = (SHORT)(currentX + ASCII_CHAR_SPACING);
        UpdateCharPosition(currentX, startY, GetAsciiDigit(yearTens));
        currentX = (SHORT)(currentX + ASCII_CHAR_SPACING);
        UpdateCharPosition(currentX, startY, GetAsciiDigit(yearOnes));
        currentX = (SHORT)(currentX + ASCII_CHAR_SPACING);
        UpdateCharPosition(currentX, startY, g_asciiDash);
        currentX = (SHORT)(currentX + ASCII_CHAR_SPACING);
        UpdateCharPosition(currentX, startY, GetAsciiDigit(monthTens));
        currentX = (SHORT)(currentX + ASCII_CHAR_SPACING);
        UpdateCharPosition(currentX, startY, GetAsciiDigit(monthOnes));
        currentX = (SHORT)(currentX + ASCII_CHAR_SPACING);
        UpdateCharPosition(currentX, startY, g_asciiDash);
        currentX = (SHORT)(currentX + ASCII_CHAR_SPACING);
        UpdateCharPosition(currentX, startY, GetAsciiDigit(dayTens));
        currentX = (SHORT)(currentX + ASCII_CHAR_SPACING);
        UpdateCharPosition(currentX, startY, GetAsciiDigit(dayOnes));
    } else {
        // Smart update - only changed digits
        SHORT currentX = startX;
        
        UpdateCharPositionIfChanged(
            currentX, startY, 
            GetAsciiDigit(yearThousands), 
            GetAsciiDigit(g_displayState.yearThousands)
        );
        currentX = (SHORT)(currentX + ASCII_CHAR_SPACING);
        
        UpdateCharPositionIfChanged(
            currentX, startY, 
            GetAsciiDigit(yearHundreds), 
            GetAsciiDigit(g_displayState.yearHundreds)
        );
        currentX = (SHORT)(currentX + ASCII_CHAR_SPACING);
        
        UpdateCharPositionIfChanged(
            currentX, startY, 
            GetAsciiDigit(yearTens), 
            GetAsciiDigit(g_displayState.yearTens)
        );
        currentX = (SHORT)(currentX + ASCII_CHAR_SPACING);
        
        UpdateCharPositionIfChanged(
            currentX, startY, 
            GetAsciiDigit(yearOnes), 
            GetAsciiDigit(g_displayState.yearOnes)
        );
        currentX = (SHORT)(currentX + ASCII_CHAR_SPACING * 2); // Skip dash
        
        UpdateCharPositionIfChanged(
            currentX, startY, 
            GetAsciiDigit(monthTens), 
            GetAsciiDigit(g_displayState.monthTens)
        );
        currentX = (SHORT)(currentX + ASCII_CHAR_SPACING);
        
        UpdateCharPositionIfChanged(
            currentX, startY, 
            GetAsciiDigit(monthOnes), 
            GetAsciiDigit(g_displayState.monthOnes)
        );
        currentX = (SHORT)(currentX + ASCII_CHAR_SPACING * 2); // Skip dash
        
        UpdateCharPositionIfChanged(
            currentX, startY, 
            GetAsciiDigit(dayTens), 
            GetAsciiDigit(g_displayState.dayTens)
        );
        currentX = (SHORT)(currentX + ASCII_CHAR_SPACING);
        
        UpdateCharPositionIfChanged(
            currentX, startY, 
            GetAsciiDigit(dayOnes), 
            GetAsciiDigit(g_displayState.dayOnes)
        );
    }
    
    // Update state
    g_displayState.yearThousands = yearThousands;
    g_displayState.yearHundreds = yearHundreds;
    g_displayState.yearTens = yearTens;
    g_displayState.yearOnes = yearOnes;
    g_displayState.monthTens = monthTens;
    g_displayState.monthOnes = monthOnes;
    g_displayState.dayTens = dayTens;
    g_displayState.dayOnes = dayOnes;
}

// Optimized screen clear - only clears content area, not title
static void ClearScreenSafe(void) {
    if (g_hConsole == INVALID_HANDLE_VALUE) {
        return;
    }

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(g_hConsole, &csbi)) {
        return;
    }

    DWORD dwConSize = csbi.dwSize.X * (csbi.dwSize.Y - 1);
    COORD coordScreen = { 0, 1 };
    DWORD cCharsWritten;

    FillConsoleOutputCharacterW(
        g_hConsole, 
        L' ', 
        dwConSize, 
        coordScreen, 
        &cCharsWritten
    );
    FillConsoleOutputAttribute(
        g_hConsole, 
        csbi.wAttributes, 
        dwConSize, 
        coordScreen, 
        &cCharsWritten
    );
}

// Redraw all content
static void RedrawAll(_In_ const SYSTEMTIME* st) {
    HideCursor(TRUE);
    PrintTitleLine();
    g_displayState.initialized = FALSE;
    PrintTimeAscii(st, 0, 3, TRUE);
    PrintDateAscii(st, 0, 12, TRUE);
    g_displayState.initialized = TRUE;
}

int wmain(void) {
    if (!setlocale(LC_ALL, ".UTF8")) {
        fwprintf(stderr, L"Warning: Could not set UTF-8 locale\n");
    }

    if (!SetConsoleOutputCP(CP_UTF8)) {
        fwprintf(stderr, L"Warning: Could not set console code page\n");
    }

    g_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (g_hConsole == INVALID_HANDLE_VALUE) {
        fwprintf(
            stderr, 
            L"Error: Could not get console handle (error %lu)\n", 
            GetLastError()
        );
        return 1;
    }

    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    if (hInput != INVALID_HANDLE_VALUE) {
        DWORD mode;
        if (GetConsoleMode(hInput, &mode)) {
            SetConsoleMode(hInput, mode | ENABLE_WINDOW_INPUT);
        }
    }

    HideCursor(TRUE);

    // Initialize console size tracking
    CheckConsoleResize();

    // Initial screen setup
    ClearScreenSafe();
    PrintTitleLine();

    SYSTEMTIME st;
    GetLocalTime(&st);
    
    // Initial draw
    g_displayState.initialized = FALSE;
    PrintTimeAscii(&st, 0, 3, TRUE);
    PrintDateAscii(&st, 0, 12, TRUE);
    g_displayState.initialized = TRUE;

    // Flush console input buffer
    if (hInput != INVALID_HANDLE_VALUE) {
        FlushConsoleInputBuffer(hInput);
    }

    // Main loop
    while (TRUE) {
        // Check for resize event
        if (CheckForResizeEvent()) {
            CheckConsoleResize();
            GetLocalTime(&st);
            RedrawAll(&st);
        } else {
            // Normal update
            GetLocalTime(&st);
            PrintTimeAscii(&st, 0, 3, FALSE);
            PrintDateAscii(&st, 0, 12, FALSE);
        }

        Sleep(UPDATE_INTERVAL_MS);
    }

    HideCursor(FALSE);
    return 0;
}