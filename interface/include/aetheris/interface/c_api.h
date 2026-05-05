#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && defined(AETHERIS_IUI_SHARED)
#if defined(AETHERIS_IUI_BUILDING_DLL)
#define AETHERIS_API __declspec(dllexport)
#else
#define AETHERIS_API __declspec(dllimport)
#endif
#else
#define AETHERIS_API
#endif

/**
 * Opaque runtime context used to keep the C ABI stable as internals evolve.
 */
typedef struct aetheris_context aetheris_context;

/**
 * Status codes returned by C ABI functions.
 */
typedef enum aetheris_status_code {
  AETHERIS_STATUS_OK = 0,
  AETHERIS_STATUS_INVALID_ARGUMENT = 1,
  AETHERIS_STATUS_INTERNAL_ERROR = 2
} aetheris_status_code;

/**
 * C ABI status value. The message pointer is static and owned by the library.
 */
typedef struct aetheris_status {
  aetheris_status_code code;
  const char* message;
} aetheris_status;

/**
 * Returns the semantic version of the linked Aetheris-IUI core.
 */
AETHERIS_API const char* aetheris_version(void);

/**
 * Returns the stable ABI version implemented by this library.
 */
AETHERIS_API uint32_t aetheris_abi_version(void);

/**
 * Creates a new runtime context.
 */
AETHERIS_API aetheris_status aetheris_create_context(aetheris_context** out_context);

/**
 * Destroys a runtime context created with aetheris_create_context.
 */
AETHERIS_API void aetheris_destroy_context(aetheris_context* context);

/**
 * Returns a JSON string snapshot of a session for UI consumption.
 *
 * session_json    - JSON-serialized IntentSession produced by the server.
 * out_json        - set to a heap-allocated JSON string on success;
 *                   caller must free with aetheris_free_string().
 *
 * On error, *out_json is set to NULL and the returned status describes the
 * failure. Passing NULL for session_json or out_json returns
 * AETHERIS_STATUS_INVALID_ARGUMENT.
 */
AETHERIS_API aetheris_status aetheris_session_snapshot_json(const aetheris_context* context,
                                                            const char* session_json,
                                                            char** out_json);

/**
 * Frees a string returned by aetheris_session_snapshot_json.
 * Passing NULL is a no-op.
 */
AETHERIS_API void aetheris_free_string(char* str);

#ifdef __cplusplus
}
#endif
