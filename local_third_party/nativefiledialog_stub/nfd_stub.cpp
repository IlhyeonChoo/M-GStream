#include "nfd.h"

namespace {

constexpr const char* kNativeFileDialogStubError =
    "nativefiledialog stub is active in the split working copy; file dialogs are unavailable.";

}  // namespace

extern "C" {

nfdresult_t NFD_OpenDialog(const nfdchar_t*, const nfdchar_t*, nfdchar_t** outPath) {
    if (outPath) {
        *outPath = nullptr;
    }
    return NFD_CANCEL;
}

nfdresult_t NFD_SaveDialog(const nfdchar_t*, const nfdchar_t*, nfdchar_t** outPath) {
    if (outPath) {
        *outPath = nullptr;
    }
    return NFD_CANCEL;
}

nfdresult_t NFD_PickFolder(const nfdchar_t*, nfdchar_t** outPath) {
    if (outPath) {
        *outPath = nullptr;
    }
    return NFD_CANCEL;
}

const nfdchar_t* NFD_GetError(void) {
    return kNativeFileDialogStubError;
}

}  // extern "C"
