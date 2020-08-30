#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

typedef struct CMainWindow CMainWindow; // NOLINT
void mainwindow_open_file(CMainWindow* w, const char* path);

#ifdef __cplusplus
}
#endif // __cplusplus
