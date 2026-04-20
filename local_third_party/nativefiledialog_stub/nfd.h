#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef char nfdchar_t;

typedef enum nfdresult_t {
    NFD_ERROR = 0,
    NFD_OKAY = 1,
    NFD_CANCEL = 2,
} nfdresult_t;

nfdresult_t NFD_OpenDialog(const nfdchar_t* filterList, const nfdchar_t* defaultPath, nfdchar_t** outPath);
nfdresult_t NFD_SaveDialog(const nfdchar_t* filterList, const nfdchar_t* defaultPath, nfdchar_t** outPath);
nfdresult_t NFD_PickFolder(const nfdchar_t* defaultPath, nfdchar_t** outPath);
const nfdchar_t* NFD_GetError(void);

#ifdef __cplusplus
}
#endif
