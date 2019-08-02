#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <strsafe.h>
#include <windows.h>
#include "cJSON.h"

#ifdef  UNICODE
#define _char(c) L##c
#define _fopen_s _wfopen_s
#define _fprintf fwprintf
#define _splitpath _wsplitpath_s
#else
#define _char(c) c
#define _fopen_s fopen_s
#define _fprintf fprintf
#define _splitpath _splitpath_s
#endif

#define CONFIG_FILE_DATA_MAX_LEN 10000
#define CONFIG_FILE_DATA_BUF_LEN CONFIG_FILE_DATA_MAX_LEN + 1
#define CMD_LINE_LEN_INCR_STEP 1000


int main(int argc, char *argv[])
{
    FILE *configFile = NULL;

    TCHAR exeFilePath[MAX_PATH];

    TCHAR configFilePath[MAX_PATH];

    GetModuleFileName(NULL, exeFilePath, _MAX_PATH);

    TCHAR exeFileDrive[_MAX_DRIVE];
    TCHAR exeFileParentPath[_MAX_DIR];
    TCHAR exeFileName[_MAX_FNAME];
    TCHAR exeFileExt[_MAX_EXT];

    _splitpath(
        exeFilePath,
        exeFileDrive,
        _MAX_DRIVE,
        exeFileParentPath,
        _MAX_DIR,
        exeFileName,
        _MAX_FNAME,
        exeFileExt,
        _MAX_EXT
    );

    // Now `exeFilePath` is the parent directory path.
    PathCombine(exeFilePath, exeFileDrive, exeFileParentPath);

    TCHAR configFileName[_MAX_FNAME];

    configFileName[0] = _char('\0');

    StringCchCat(configFileName, _MAX_FNAME, exeFileName);

    StringCchCat(configFileName, _MAX_FNAME, TEXT("_config.json"));

    PathCombine(configFilePath, exeFilePath, configFileName);

    errno_t err_num = _fopen_s(&configFile, configFilePath, TEXT("r"));

    if (err_num != 0 || configFile == NULL) {
        _fprintf(stderr, TEXT("Error: Failed opening config file: `%s`.\n"), configFilePath);

        return 1;
    }

    char configFileData[CONFIG_FILE_DATA_BUF_LEN];

    size_t readLen = fread(configFileData, sizeof(char), CONFIG_FILE_DATA_BUF_LEN, configFile);

    if (ferror(configFile) != 0) {
        _fprintf(stderr, TEXT("Error: Failed reading config file: `%s`.\n"), configFilePath);

        return 1;
    }

    if (readLen == CONFIG_FILE_DATA_BUF_LEN) {
        _fprintf(
            stderr,
            TEXT("Error: Config file can not be larger than %d bytes.\n"),
            CONFIG_FILE_DATA_MAX_LEN
            );

        return 1;
    }

    configFileData[readLen++] = _char('\0');

    cJSON *configObj = cJSON_Parse(configFileData);

    fclose(configFile);

    if (configObj == NULL)
    {
        _fprintf(stderr, TEXT("Error: Failed parsing config file: `%s`.\n"), configFilePath);

        const char *errorPtr = cJSON_GetErrorPtr();

        if (errorPtr != NULL)
        {
            fprintf(stderr, "Parsing failed at:\n---\n%s\n---\n", errorPtr);
        }

        return 1;
    }

    cJSON* cmdObj = cJSON_GetObjectItemCaseSensitive(configObj, "CMD");

    if (!cJSON_IsArray(cmdObj)) {
        fprintf(stderr, "Error: Config `CMD` is not array.\n");

        return 1;
    }

    cJSON* showConsoleObj = cJSON_GetObjectItemCaseSensitive(configObj, "SHOW_CONSOLE");

    if (!cJSON_IsNumber(showConsoleObj)) {
        fprintf(stderr, "Error: Config `SHOW_CONSOLE` is not number.\n");

        return 1;
    }

    int cmdLineMaxLen = CMD_LINE_LEN_INCR_STEP;

    TCHAR* cmdLine = malloc(sizeof(TCHAR) * cmdLineMaxLen);

    int cmdLineIndex = 0;

    int cmdPartIndex = 0;

#define ensureCmdLineNotFull(cmdLine, cmdLineMaxLen, cmdLineIndex) \
if (cmdLineIndex == cmdLineMaxLen) {\
    cmdLineMaxLen += CMD_LINE_LEN_INCR_STEP;\
    cmdLine = realloc(cmdLine, sizeof(TCHAR) * cmdLineMaxLen);\
    if (cmdLine == NULL) {\
        _fprintf(stderr, TEXT("Error: Failed allocating memory.\n"));\
        return 1;\
    }\
}

    cJSON* cmdPartObj;

    cJSON_ArrayForEach(cmdPartObj, cmdObj)
    {
        if (!cJSON_IsString(cmdPartObj))
        {
            _fprintf(stderr, TEXT("Error: Config `CMD`'s item %d is not string.\n"), cmdPartIndex + 1);

            return 1;
        }

#ifdef  UNICODE
        size_t cmdPartLen = MultiByteToWideChar(CP_UTF8, 0, cmdPartObj->valuestring, -1, NULL, 0);

        TCHAR* cmdPart2 = malloc(sizeof(TCHAR) * cmdPartLen + 1);

        MultiByteToWideChar(CP_UTF8, 0, cmdPartObj->valuestring, -1, cmdPart2, cmdPartLen);

        cmdPart2[cmdPartLen] = _char('\0');
#else
        TCHAR* cmdPart2 = cmdPartObj->valuestring;
#endif
        ensureCmdLineNotFull(cmdLine, cmdLineMaxLen, cmdLineIndex);

        cmdLine[cmdLineIndex++] = _char('"');

        TCHAR cmdChar;

        for (int cmdPartCharIndex = 0; cmdChar = cmdPart2[cmdPartCharIndex]; cmdPartCharIndex++) {
            if (cmdChar == _char('"')) {
                ensureCmdLineNotFull(cmdLine, cmdLineMaxLen, cmdLineIndex);

                cmdLine[cmdLineIndex++] = _char('\\');

                ensureCmdLineNotFull(cmdLine, cmdLineMaxLen, cmdLineIndex);

                cmdLine[cmdLineIndex++] = _char('"');
            }
            else {
                ensureCmdLineNotFull(cmdLine, cmdLineMaxLen, cmdLineIndex);

                cmdLine[cmdLineIndex++] = cmdChar;
            }
        }

        ensureCmdLineNotFull(cmdLine, cmdLineMaxLen, cmdLineIndex);

        cmdLine[cmdLineIndex++] = _char('"');

        if (cmdPartObj->next != NULL) {
            ensureCmdLineNotFull(cmdLine, cmdLineMaxLen, cmdLineIndex);

            cmdLine[cmdLineIndex++] = _char(' ');
        }

        cmdPartIndex++;
    }

    ensureCmdLineNotFull(cmdLine, cmdLineMaxLen, cmdLineIndex);

    cmdLine[cmdLineIndex] = _char('\0');

    int creationFlags = showConsoleObj->valueint == 1 ? 0 : DETACHED_PROCESS;

    STARTUPINFO startupInfo = { sizeof(startupInfo) };

    PROCESS_INFORMATION processInfo;

    BOOL createProcessIsSuccess = CreateProcess(
        // Use command line's first part as module name.
        NULL,
        cmdLine,
        // No process handle inheritance.
        NULL,
        // No thread handle inheritance.
        NULL,
        // No handle inheritance.
        FALSE,
        creationFlags,
        // Use parent's environment block.
        NULL,
        // Use parent's starting directory.
        NULL,
        &startupInfo,
        &processInfo
    );

    if (!createProcessIsSuccess) {
        _fprintf(stderr, TEXT("Error: Failed running command:\n---\n%s\n---\n"), cmdLine);

        return 1;
    }

    return 0;
}
