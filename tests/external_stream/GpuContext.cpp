#include <raylib.h>
/** @brief Isolates raylib C names from the native Windows/D3D11 test translation unit. */
void OpenTestContext()
{
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 32, "External stream integration test");
}
void CloseTestContext() { CloseWindow(); }
