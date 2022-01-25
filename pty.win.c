#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include <winpty/winpty.h>

#include "minivt.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define BUF_SIZE 2048

// because the proper function isn't defined by microsoft duh
uintptr_t _beginthreadex( // NATIVE CODE
    void *security,
    unsigned stack_size,
    unsigned ( __stdcall *start_address )( void * ),
    void *arglist,
    unsigned initflag,
    unsigned *thrdaddr
);

typedef struct {
    winpty_t *pty;
    HANDLE read_side, write_side;
} thread_t;

static void write_callback(int type, vt_answer_t *ans, void *p) {
    uintptr_t *args = (uintptr_t *) p;
    thread_t *th    = (thread_t *) args[0];
    BOOL     *w     = (BOOL *)     args[1];
    DWORD    *write = (DWORD *)    args[2];
    switch (type) {
    // TODO: don't ignore VT_MSG_CONTENT
    case VT_MSG_PASS:
        *w = WriteFile(th->write_side, ans->buffer.b, ans->buffer.len, write, NULL);
        break;
    case VT_MSG_RESIZE:
        winpty_set_size(th->pty, ans->point.c, ans->point.r, NULL);
        break;
    }
}

static BOOL try_read(HANDLE h, LPVOID data, DWORD max_size, DWORD *r) {
    DWORD read = 0;
    PeekNamedPipe(h, NULL, 0, NULL, &read, NULL);
    // if there's more data to read, we'll read that much to prevent block
    // if there's 1 or no data, we read 1 so it blocks until data is available
    read = read > 1 ? MIN(max_size, read) : 1;
    return ReadFile(h, data, read, r, NULL);
}

__stdcall unsigned int stdin_thread(void *ptr) {
    thread_t *th = (thread_t *) ptr;
    BYTE buf[BUF_SIZE];
    DWORD read = 0, write = 0;
    BOOL r, w;

    intptr_t args[3] = { (intptr_t) ptr, (intptr_t) &w, (intptr_t) &write };
    vt_parser_t *vt = vtnew(write_callback, args);
    do {
        r = try_read(th->read_side, buf, BUF_SIZE, &read);
        if (r && read > 0)
            vtparse(vt, (const char *)buf, read);
    } while (r && w);
    vtfree(vt);
    return 0;
}

__stdcall unsigned int stdout_thread(void *ptr) {
    thread_t *th = (thread_t *) ptr;
    BYTE buf[BUF_SIZE];
    DWORD read = 0, write = 0;
    BOOL r, w;
    do {
        r = try_read(th->read_side, buf, BUF_SIZE, &read);
        w = WriteFile(th->write_side, buf, read, &write, NULL);
    } while (r && w);
    return 0;
}

static void winpty_error(winpty_error_ptr_t err) {
    LPCWSTR msg = winpty_error_msg(err);
    fwprintf(stderr, L"winpty: %ls\n", msg);
    winpty_error_free(err);
}

static void win32_error(DWORD err) {
    LPWSTR msg = NULL;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER
        | FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR) &msg,
        0,
        NULL
    );

    if (msg != NULL) {
        fwprintf(stderr, L"error: %ls\n", msg);
        LocalFree(msg);
    }
}

int main() {
    int argc = 0;
    LPWSTR cmdline = GetCommandLineW();
    LPWSTR *argv = CommandLineToArgvW(cmdline, &argc);

    if (argc < 2) {
        fprintf(stderr, "error: insufficient arguments\n");
        return 1;
    }

    winpty_error_ptr_t err = NULL;
    winpty_config_t *config = NULL;
    winpty_t *pty = NULL;
    winpty_spawn_config_t *spawn_config = NULL;

    HANDLE stdin_handle, stdout_handle, pty_in, pty_out;
    HANDLE threads[2];

    stdin_handle = stdout_handle = pty_in = pty_out = INVALID_HANDLE_VALUE;
    threads[0] = threads[1] = INVALID_HANDLE_VALUE;

    config = winpty_config_new(0, &err);
    if (config == NULL) {
        winpty_error(err);
        goto cleanup;
    }
    winpty_error_free(err);

    winpty_config_set_initial_size(config, 80, 24);
    
    pty = winpty_open(config, &err);
    if (pty == NULL) {
        winpty_error(err);
        goto cleanup;
    }
    winpty_error_free(err);

    LPCWSTR pty_in_name = winpty_conin_name(pty);
    LPCWSTR pty_out_name = winpty_conout_name(pty);

    stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
    stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);

    DWORD cm = 0;
    GetConsoleMode(stdin_handle, &cm);
    SetConsoleMode(stdin_handle, cm & ~(ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|ENABLE_PROCESSED_INPUT));

    pty_in = CreateFileW(pty_in_name, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    pty_out = CreateFileW(pty_out_name, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (pty_in == INVALID_HANDLE_VALUE || pty_out == INVALID_HANDLE_VALUE) {
        win32_error(GetLastError());
        goto cleanup;
    }

    thread_t in_thread = { pty, stdin_handle, pty_in };

    thread_t out_thread = { pty, pty_out, stdout_handle };

    unsigned int thread_id;
    threads[0] = (HANDLE) _beginthreadex(NULL, 0, stdin_thread, &in_thread, 0, &thread_id);
    threads[1] = (HANDLE) _beginthreadex(NULL, 0, stdout_thread, &out_thread, 0, &thread_id);

    spawn_config = winpty_spawn_config_new(WINPTY_SPAWN_FLAG_MASK, argv[1], NULL, NULL, NULL, &err);
    if (spawn_config == NULL) {
        winpty_error(err);
        goto cleanup;
    }
    winpty_error_free(err);

    HANDLE proc = INVALID_HANDLE_VALUE;
    if (!winpty_spawn(pty, spawn_config, &proc, NULL, NULL, &err)) {
        winpty_error(err);
        goto cleanup;
    }
    winpty_error_free(err);

    WaitForSingleObject(proc, INFINITE);

    // this will cause pending ReadFile on stdin to stop so the thread will detect
    // that pty_out has closed
    CancelIoEx(stdin_handle, NULL);
    
    WaitForMultipleObjects(2, threads, TRUE, INFINITE);

cleanup:
    winpty_config_free(config);
    winpty_free(pty);
    winpty_spawn_config_free(spawn_config);
    CloseHandle(threads[0]);
    CloseHandle(threads[1]);
    CloseHandle(pty_in);
    CloseHandle(pty_out);

    return 0;
}
