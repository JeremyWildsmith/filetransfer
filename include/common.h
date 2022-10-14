#pragma once


static const int READ_BUFFER_SIZE = 8192;
static const int PORT_DEFAULT = 8888;

static const int EXIT_INVALID_ARGUMENT = 2;

/// @brief Resolves a filepath to a valid and existing path
/// @param src Path to be resolved
/// @param buffer Buffer where the resolved path is stored. Must be PATH_MAX size at minimum.
/// @return A pointer to buffer upon success, or null on failure
char* resolve_filepath(const char* src, char* buffer);


/// @brief Resolves a filepath to a valid and existing path. Will create directories as necessary.
/// @param src Path to be resolved
/// @param buffer Buffer where the resolved path is stored. Must be PATH_MAX size at minimum.
/// @return A pointer to buffer upon success, or null on failure
char* resolve_dirpath(const char* src, char* buffer);
