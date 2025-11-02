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

// Alarm ramp speed enumeration
typedef enum {
    ALARM_RAMP_FAST = 0,       // 10 seconds
    ALARM_RAMP_MODERATE = 1,   // 30 seconds
    ALARM_RAMP_SLOW = 2        // 60 seconds
} AlarmRampSpeed;

// Alarm state structure
typedef struct {
    BOOL isActive;
    WORD hour;
    WORD minute;
    BOOL repeatDaily;
    AlarmRampSpeed rampSpeed;
    BOOL isRinging;
    DWORD ringStartTime;
    DWORD lastBeepTime;
} AlarmState;

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
static HANDLE g_hInput = INVALID_HANDLE_VALUE;
static AlarmState g_alarmState = { 0 };

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
static void ParseCommandLineArgs(_In_ int argc, _In_ wchar_t* argv[]);
static void CheckKeyboardInput(void);
static void PromptForAlarm(void);
static void PrintAlarmStatusLine(void);
static void CheckAlarmTime(_In_ const SYSTEMTIME* st);
static void TriggerAlarm(void);
static void UpdateAlarmBeep(void);
static DWORD GetRampDurationMs(_In_ AlarmRampSpeed speed);

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
    const WORD titleTextAttribute = FOREGROUND_BLUE;
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

// Optimized screen clear - only clears content area, not title or alarm status
static void ClearScreenSafe(void) {
    if (g_hConsole == INVALID_HANDLE_VALUE) {
        return;
    }

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(g_hConsole, &csbi)) {
        return;
    }

    // Clear from row 1 up to (but not including) the alarm status row
    // Alarm status is at row 21, so we clear rows 1-20
    const SHORT dateStartY = 12;
    const SHORT alarmStatusRow = dateStartY + ASCII_CHAR_HEIGHT + 2; // row 21
    
    // Only clear if alarm status row is within console bounds
    if (alarmStatusRow < csbi.dwSize.Y) {
        SHORT rowsToClear = alarmStatusRow - 1; // Clear rows 1 to 20
        if (rowsToClear > 0) {
            DWORD dwConSize = csbi.dwSize.X * rowsToClear;
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
    } else {
        // Fallback: clear all except title and bottom row
        DWORD dwConSize = csbi.dwSize.X * (csbi.dwSize.Y - 2);
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
}

// Get ramp duration in milliseconds
static DWORD GetRampDurationMs(_In_ AlarmRampSpeed speed) {
    switch (speed) {
        case ALARM_RAMP_FAST:
            return 10000;      // 10 seconds
        case ALARM_RAMP_MODERATE:
            return 30000;      // 30 seconds
        case ALARM_RAMP_SLOW:
            return 60000;      // 60 seconds
        default:
            return 30000;
    }
}

// Print alarm status line 2 lines below date display
static void PrintAlarmStatusLine(void) {
    if (g_hConsole == INVALID_HANDLE_VALUE) {
        return;
    }
    
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(g_hConsole, &csbi)) {
        return;
    }
    
    // Date display starts at y=12, has height 7, so ends at y=18
    // Alarm status goes 2 lines below that: 12 + 7 + 2 = 21
    const SHORT dateStartY = 12;
    const SHORT statusRow = dateStartY + ASCII_CHAR_HEIGHT + 2;
    
    // Ensure status row is within console bounds
    if (statusRow >= csbi.dwSize.Y) {
        return;
    }
    
    COORD coord = { 0, statusRow };
    DWORD cCharsWritten;
    
    // Clear ONLY this specific line without affecting anything above
    FillConsoleOutputCharacterW(
        g_hConsole, L' ', csbi.dwSize.X, coord, &cCharsWritten
    );
    
    // Set normal attribute for clearing attributes
    WORD normalAttribute = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    FillConsoleOutputAttribute(
        g_hConsole, normalAttribute, csbi.dwSize.X, coord, &cCharsWritten
    );
    
    if (g_alarmState.isActive || g_alarmState.isRinging) {
        // Set yellow foreground for alarm status
        WORD alarmAttribute = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        SetConsoleTextAttribute(g_hConsole, alarmAttribute);
        
        SetCursorPosition(0, statusRow);
        
        if (g_alarmState.isRinging) {
            wprintf(L"ALARM RINGING - Press Alt+X to stop");
        } else {
            wchar_t rampStr[16] = L"";
            switch (g_alarmState.rampSpeed) {
                case ALARM_RAMP_FAST:
                    wcscpy_s(rampStr, 16, L"fast");
                    break;
                case ALARM_RAMP_MODERATE:
                    wcscpy_s(rampStr, 16, L"moderate");
                    break;
                case ALARM_RAMP_SLOW:
                    wcscpy_s(rampStr, 16, L"slow");
                    break;
            }
            
            wprintf(L"ALARM SET: %02d:%02d", g_alarmState.hour, g_alarmState.minute);
            if (g_alarmState.repeatDaily) {
                wprintf(L" [REPEAT]");
            }
            wprintf(L" [RAMP: %ls]", rampStr);
        }
        fflush(stdout);
        
        SetConsoleTextAttribute(g_hConsole, normalAttribute);
    }
}

// Interactive prompt for alarm settings
static void PromptForAlarm(void) {
    if (g_hConsole == INVALID_HANDLE_VALUE || g_hInput == INVALID_HANDLE_VALUE) {
        return;
    }
    
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(g_hConsole, &csbi)) {
        return;
    }
    
    // Use bottom row for prompts, ensuring it doesn't interfere with display
    // Make sure it's below the alarm status row (row 21)
    const SHORT dateStartY = 12;
    const SHORT alarmStatusRow = dateStartY + ASCII_CHAR_HEIGHT + 2; // row 21
    SHORT bottomRow = csbi.dwSize.Y - 1;
    
    // If console is too small and bottom row would overlap with alarm status,
    // use the alarm status row itself (it will be redrawn after prompt)
    if (bottomRow <= alarmStatusRow) {
        // Console too small - use alarm status row, will be restored after
        bottomRow = alarmStatusRow;
    }
    
    WORD normalAttribute = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    
    // Show cursor for input
    HideCursor(FALSE);
    
    // Clear ONLY the bottom line and move cursor there
    // This will not affect anything above
    COORD coord = { 0, bottomRow };
    DWORD cCharsWritten;
    FillConsoleOutputCharacterW(
        g_hConsole, L' ', csbi.dwSize.X, coord, &cCharsWritten
    );
    FillConsoleOutputAttribute(
        g_hConsole, normalAttribute, csbi.dwSize.X, coord, &cCharsWritten
    );
    
    SetCursorPosition(0, bottomRow);
    SetConsoleTextAttribute(g_hConsole, normalAttribute);
    wprintf(L"Enter alarm time (HH:MM): ");
    fflush(stdout);
    
    // Read time input
    wchar_t timeInput[32] = L"";
    DWORD charsRead = 0;
    if (ReadConsoleW(g_hInput, timeInput, 31, &charsRead, NULL) && charsRead > 1) {
        // Clear the prompt line immediately after reading
        FillConsoleOutputCharacterW(
            g_hConsole, L' ', csbi.dwSize.X, coord, &cCharsWritten
        );
        FillConsoleOutputAttribute(
            g_hConsole, normalAttribute, csbi.dwSize.X, coord, &cCharsWritten
        );
        
        // Remove newline
        if (charsRead > 0 && timeInput[charsRead - 1] == L'\n') {
            timeInput[charsRead - 1] = L'\0';
            charsRead--;
        }
        if (charsRead > 0 && timeInput[charsRead - 1] == L'\r') {
            timeInput[charsRead - 1] = L'\0';
            charsRead--;
        }
        
        // Parse time
        int hour = 0, minute = 0;
        if (swscanf_s(timeInput, L"%d:%d", &hour, &minute) == 2) {
            if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
                g_alarmState.hour = (WORD)hour;
                g_alarmState.minute = (WORD)minute;
                
                // Prompt for repeat
                SetCursorPosition(0, bottomRow);
                wprintf(L"Repeat daily? (Y/N): ");
                fflush(stdout);
                
                wchar_t repeatInput[8] = L"";
                charsRead = 0;
                if (ReadConsoleW(g_hInput, repeatInput, 7, &charsRead, NULL) && charsRead > 0) {
                    // Clear the prompt line immediately after reading
                    FillConsoleOutputCharacterW(
                        g_hConsole, L' ', csbi.dwSize.X, coord, &cCharsWritten
                    );
                    FillConsoleOutputAttribute(
                        g_hConsole, normalAttribute, csbi.dwSize.X, coord, &cCharsWritten
                    );
                    
                    wchar_t firstChar = towupper(repeatInput[0]);
                    g_alarmState.repeatDaily = (firstChar == L'Y');
                }
                
                // Prompt for ramp speed
                SetCursorPosition(0, bottomRow);
                wprintf(L"Ramp speed (fast/moderate/slow) [moderate]: ");
                fflush(stdout);
                
                wchar_t rampInput[16] = L"";
                charsRead = 0;
                if (ReadConsoleW(g_hInput, rampInput, 15, &charsRead, NULL) && charsRead > 1) {
                    // Clear the prompt line immediately after reading
                    FillConsoleOutputCharacterW(
                        g_hConsole, L' ', csbi.dwSize.X, coord, &cCharsWritten
                    );
                    FillConsoleOutputAttribute(
                        g_hConsole, normalAttribute, csbi.dwSize.X, coord, &cCharsWritten
                    );
                    
                    // Remove newline
                    if (charsRead > 0 && rampInput[charsRead - 1] == L'\n') {
                        rampInput[charsRead - 1] = L'\0';
                    }
                    
                    // Convert to lowercase for comparison
                    for (int i = 0; rampInput[i] != L'\0'; i++) {
                        rampInput[i] = towlower(rampInput[i]);
                    }
                    
                    if (wcsncmp(rampInput, L"fast", 4) == 0) {
                        g_alarmState.rampSpeed = ALARM_RAMP_FAST;
                    } else if (wcsncmp(rampInput, L"slow", 4) == 0) {
                        g_alarmState.rampSpeed = ALARM_RAMP_SLOW;
                    } else {
                        g_alarmState.rampSpeed = ALARM_RAMP_MODERATE;
                    }
                } else {
                    // Clear the prompt line if no input
                    FillConsoleOutputCharacterW(
                        g_hConsole, L' ', csbi.dwSize.X, coord, &cCharsWritten
                    );
                    FillConsoleOutputAttribute(
                        g_hConsole, normalAttribute, csbi.dwSize.X, coord, &cCharsWritten
                    );
                    g_alarmState.rampSpeed = ALARM_RAMP_MODERATE;
                }
                
                g_alarmState.isActive = TRUE;
            }
        }
    } else {
        // Clear the prompt line if read failed
        FillConsoleOutputCharacterW(
            g_hConsole, L' ', csbi.dwSize.X, coord, &cCharsWritten
        );
        FillConsoleOutputAttribute(
            g_hConsole, normalAttribute, csbi.dwSize.X, coord, &cCharsWritten
        );
    }
    
    // Hide cursor again
    HideCursor(TRUE);
    
    // Update alarm status display
    PrintAlarmStatusLine();
}

// Check keyboard input for hotkeys
static void CheckKeyboardInput(void) {
    if (g_hInput == INVALID_HANDLE_VALUE) {
        return;
    }
    
    INPUT_RECORD irInBuf[32];
    DWORD cNumRead;
    
    if (!PeekConsoleInput(g_hInput, irInBuf, 32, &cNumRead)) {
        return;
    }
    
    if (cNumRead == 0) {
        return;
    }
    
    BOOL shouldRead = FALSE;
    
    for (DWORD i = 0; i < cNumRead; i++) {
        if (irInBuf[i].EventType == KEY_EVENT && irInBuf[i].Event.KeyEvent.bKeyDown) {
            KEY_EVENT_RECORD* ker = &irInBuf[i].Event.KeyEvent;
            
            // Check for Alt+A (set alarm)
            if ((ker->dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) &&
                (ker->wVirtualKeyCode == 'A' || ker->wVirtualKeyCode == 'a')) {
                shouldRead = TRUE;
                // Read all input to clear buffer
                ReadConsoleInput(g_hInput, irInBuf, cNumRead, &cNumRead);
                PromptForAlarm();
                break;
            }
            
            // Check for Alt+X (abort alarm)
            if ((ker->dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) &&
                (ker->wVirtualKeyCode == 'X' || ker->wVirtualKeyCode == 'x')) {
                shouldRead = TRUE;
                // Read all input to clear buffer
                ReadConsoleInput(g_hInput, irInBuf, cNumRead, &cNumRead);
                
                if (g_alarmState.isRinging) {
                    g_alarmState.isRinging = FALSE;
                    // If not repeating, disable alarm after user stops it
                    if (!g_alarmState.repeatDaily) {
                        g_alarmState.isActive = FALSE;
                    }
                    PrintAlarmStatusLine();
                }
                break;
            }
        }
    }
    
    if (shouldRead && cNumRead > 0) {
        // Already read above
        return;
    }
}

// Check if alarm time matches and trigger if needed
static void CheckAlarmTime(_In_ const SYSTEMTIME* st) {
    if (!g_alarmState.isActive || g_alarmState.isRinging) {
        return;
    }
    
    // Check if current time matches alarm time
    if (st->wHour == g_alarmState.hour && st->wMinute == g_alarmState.minute) {
        TriggerAlarm();
    }
}

// Trigger the alarm
static void TriggerAlarm(void) {
    g_alarmState.isRinging = TRUE;
    g_alarmState.ringStartTime = GetTickCount();
    g_alarmState.lastBeepTime = 0;  // Reset beep timer
    PrintAlarmStatusLine();
}

// Update alarm beep with volume ramping
static void UpdateAlarmBeep(void) {
    if (!g_alarmState.isRinging) {
        return;
    }
    
    DWORD currentTime = GetTickCount();
    
    // Only beep at intervals (every 500ms) to avoid constant beeping
    const DWORD beepIntervalMs = 500;
    if (currentTime - g_alarmState.lastBeepTime < beepIntervalMs) {
        return;
    }
    
    g_alarmState.lastBeepTime = currentTime;
    
    DWORD elapsedMs = currentTime - g_alarmState.ringStartTime;
    DWORD rampDurationMs = GetRampDurationMs(g_alarmState.rampSpeed);
    
    // Calculate intensity (0.0 to 1.0) based on elapsed time
    float intensity = (float)elapsedMs / (float)rampDurationMs;
    if (intensity > 1.0f) {
        intensity = 1.0f;
    }
    
    // Beep frequency and duration constants
    const DWORD baseFrequency = 800;      // Base frequency in Hz
    const DWORD maxFrequency = 2000;      // Max frequency in Hz
    const DWORD beepDuration = 200;       // Duration per beep in ms
    
    // Calculate current frequency based on intensity
    DWORD currentFrequency = (DWORD)(baseFrequency + (maxFrequency - baseFrequency) * intensity);
    
    // Calculate number of beeps to simulate volume (more beeps = louder)
    // At max intensity, we'll do multiple rapid beeps
    int beepCount = 1 + (int)(intensity * 4);  // 1 to 5 beeps
    
    // Beep with increasing frequency/intensity
    for (int i = 0; i < beepCount; i++) {
        Beep(currentFrequency, beepDuration);
        if (i < beepCount - 1) {
            Sleep(50);  // Small gap between beeps
        }
    }
}

// Parse command-line arguments
static void ParseCommandLineArgs(_In_ int argc, _In_ wchar_t* argv[]) {
    BOOL repeatSet = FALSE;
    BOOL rampSet = FALSE;
    
    for (int i = 1; i < argc; i++) {
        wchar_t* arg = argv[i];
        
        // Check for /alarm or /a flag
        if ((_wcsicmp(arg, L"/alarm") == 0 || _wcsicmp(arg, L"/a") == 0) && i + 1 < argc) {
            wchar_t* timeStr = argv[++i];
            int hour = 0, minute = 0;
            
            // Parse HH:MM format
            if (swscanf_s(timeStr, L"%d:%d", &hour, &minute) == 2) {
                if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
                    g_alarmState.hour = (WORD)hour;
                    g_alarmState.minute = (WORD)minute;
                    g_alarmState.isActive = TRUE;
                }
            }
        }
        // Check for /repeat or /r flag
        else if (_wcsicmp(arg, L"/repeat") == 0 || _wcsicmp(arg, L"/r") == 0) {
            if (!repeatSet) {
                g_alarmState.repeatDaily = TRUE;
                repeatSet = TRUE;
            }
        }
        // Check for /ramp flag
        else if ((_wcsicmp(arg, L"/ramp") == 0) && i + 1 < argc && !rampSet) {
            wchar_t* rampStr = argv[++i];
            if (_wcsicmp(rampStr, L"fast") == 0) {
                g_alarmState.rampSpeed = ALARM_RAMP_FAST;
                rampSet = TRUE;
            } else if (_wcsicmp(rampStr, L"moderate") == 0) {
                g_alarmState.rampSpeed = ALARM_RAMP_MODERATE;
                rampSet = TRUE;
            } else if (_wcsicmp(rampStr, L"slow") == 0) {
                g_alarmState.rampSpeed = ALARM_RAMP_SLOW;
                rampSet = TRUE;
            }
        }
    }
    
    // Set default ramp speed if not specified
    if (!rampSet) {
        g_alarmState.rampSpeed = ALARM_RAMP_MODERATE;
    }
}

// Redraw all content
static void RedrawAll(_In_ const SYSTEMTIME* st) {
    HideCursor(TRUE);
    PrintTitleLine();
    g_displayState.initialized = FALSE;
    PrintTimeAscii(st, 0, 3, TRUE);
    PrintDateAscii(st, 0, 12, TRUE);
    g_displayState.initialized = TRUE;
    PrintAlarmStatusLine();
}

int wmain(int argc, wchar_t* argv[]) {
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

    g_hInput = GetStdHandle(STD_INPUT_HANDLE);
    if (g_hInput != INVALID_HANDLE_VALUE) {
        DWORD mode;
        if (GetConsoleMode(g_hInput, &mode)) {
            SetConsoleMode(g_hInput, mode | ENABLE_WINDOW_INPUT | ENABLE_PROCESSED_INPUT);
        }
    }

    // Parse command-line arguments
    ParseCommandLineArgs(argc, argv);

    HideCursor(TRUE);

    // Initialize console size tracking
    CheckConsoleResize();

    // Initial screen setup
    ClearScreenSafe();
    PrintTitleLine();

    SYSTEMTIME st;
    GetLocalTime(&st);
    
    // Track previous minute to detect when alarm minute passes
    WORD lastCheckedMinute = 0xFF;
    
    // Initial draw
    g_displayState.initialized = FALSE;
    PrintTimeAscii(&st, 0, 3, TRUE);
    PrintDateAscii(&st, 0, 12, TRUE);
    g_displayState.initialized = TRUE;
    PrintAlarmStatusLine();

    // Flush console input buffer
    if (g_hInput != INVALID_HANDLE_VALUE) {
        FlushConsoleInputBuffer(g_hInput);
    }

    // Main loop
    while (TRUE) {
        // Check keyboard input for hotkeys
        CheckKeyboardInput();
        
        // Check for resize event
        if (CheckForResizeEvent()) {
            CheckConsoleResize();
            GetLocalTime(&st);
            RedrawAll(&st);
            PrintAlarmStatusLine();
        } else {
            // Normal update
            GetLocalTime(&st);
            
            // Check alarm time (only once per minute to avoid repeated triggers)
            if (st.wMinute != lastCheckedMinute) {
                lastCheckedMinute = st.wMinute;
                CheckAlarmTime(&st);
            }
            
            PrintTimeAscii(&st, 0, 3, FALSE);
            PrintDateAscii(&st, 0, 12, FALSE);
        }
        
        // Update alarm beep if ringing
        if (g_alarmState.isRinging) {
            UpdateAlarmBeep();
            // Update status line to show it's still ringing
            PrintAlarmStatusLine();
        }

        Sleep(UPDATE_INTERVAL_MS);
    }

    HideCursor(FALSE);
    return 0;
}