# Read file
$content = Get-Content ascii_time.c -Raw

# Define old and new code for the screen clear fix
$old = @'
    // Initialize console size tracking
    CheckConsoleResize();

    // Initial screen setup
    ClearScreenSafe();
    PrintTitleLine();
'@

$new = @'
    // Initialize console size tracking
    CheckConsoleResize();

    // Initial screen setup - clear entire screen first
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(g_hConsole, &csbi)) {
        DWORD dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
        COORD coordScreen = { 0, 0 };
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
    
    PrintTitleLine();
'@

# Replace
$content = $content.Replace($old, $new)

# Write back
Set-Content ascii_time.c -Value $content -NoNewline

Write-Host "Screen clear fix applied successfully!"
