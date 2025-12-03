# Read file
$content = Get-Content ascii_time.c -Raw

# Define old and new code
$old = @'
            for (int i = 0; i < charsToWrite; i++) {
                fputwc(current[i], stdout);
            }
        }
    }
    fflush(stdout);
}
'@

$new = @'
            if (charsToWrite > 0) {
                // Use WriteConsoleOutputCharacterW for direct, unbuffered output
                COORD writePos = { x, currentY };
                DWORD charsWritten;
                WriteConsoleOutputCharacterW(
                    g_hConsole,
                    current,
                    charsToWrite,
                    writePos,
                    &charsWritten
                );
            }
        }
    }
}
'@

# Replace
$content = $content.Replace($old, $new)

# Write back
Set-Content ascii_time.c -Value $content -NoNewline

Write-Host "File updated successfully!"
