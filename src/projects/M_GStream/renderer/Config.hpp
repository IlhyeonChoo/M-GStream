#pragma once

#include <core/system/Config.hpp>
#include <core/system/CommandLineArgs.hpp>

#ifdef SIBR_OS_WINDOWS
#ifdef SIBR_STATIC_DEFINE
#define SIBR_EXPORT
#define SIBR_NO_EXPORT
#else
#ifndef SIBR_MGSTREAM_EXPORT
#ifdef SIBR_MGSTREAM_EXPORTS
#define SIBR_MGSTREAM_EXPORT __declspec(dllexport)
#else
#define SIBR_MGSTREAM_EXPORT __declspec(dllimport)
#endif
#endif
#ifndef SIBR_NO_EXPORT
#define SIBR_NO_EXPORT
#endif
#endif
#else
#define SIBR_MGSTREAM_EXPORT
#endif