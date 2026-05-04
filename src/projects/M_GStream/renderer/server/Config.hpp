#pragma once

#include <projects/M_GStream/renderer/Config.hpp>

#ifdef SIBR_OS_WINDOWS
#ifndef SIBR_MGSTREAM_SERVER_EXPORT
#if defined(SIBR_MGSTREAM_SERVER_STATIC_DEFINE)
#define SIBR_MGSTREAM_SERVER_EXPORT
#elif defined(SIBR_MGSTREAM_SERVER_EXPORTS)
#define SIBR_MGSTREAM_SERVER_EXPORT __declspec(dllexport)
#else
#define SIBR_MGSTREAM_SERVER_EXPORT __declspec(dllimport)
#endif
#endif
#ifndef SIBR_MGSTREAM_SERVER_NO_EXPORT
#define SIBR_MGSTREAM_SERVER_NO_EXPORT
#endif
#else
#define SIBR_MGSTREAM_SERVER_EXPORT
#define SIBR_MGSTREAM_SERVER_NO_EXPORT
#endif
