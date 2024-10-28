#pragma once

#if defined(_WIN32) || defined(_WIN64)
	#if defined(GSC_EXPORTS)
		#define GSC_API __declspec(dllexport)
	#else
		#define GSC_API __declspec(dllimport)
	#endif
#else
	#define GSC_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C"
{
#endif
	
	#define GSC_OK (0)
	#define GSC_ERROR (1)
	#define GSC_NOT_FOUND (2)
	#define GSC_DONE (3)
	#define GSC_YIELD (4)

	typedef struct gsc_State gsc_State;
	typedef struct
	{
		void *(*allocate_memory)(void *ctx, int size);							// Allocate memory of specified size
		void (*free_memory)(void *ctx, void *ptr);								// Free previously allocated memory
		const char *(*read_file)(void *ctx, const char *filename, int *status); // Read file and return status
		void *userdata;															// User-defined data pointer
	} gsc_CreateOptions;

	GSC_API gsc_State *gsc_create(gsc_CreateOptions options);
	GSC_API void gsc_destroy(gsc_State *state);
	
	GSC_API int gsc_compile(gsc_State *state, const char *filename);
	GSC_API const char *gsc_next_compile_dependency(gsc_State *state);

	GSC_API int gsc_update(gsc_State *state, int delta_time);
	GSC_API int gsc_call(gsc_State *state, const char *namespace, const char *function, int nargs);

	typedef int (*gsc_Function)(gsc_State *);
	typedef struct gsc_Object gsc_Object;
	typedef int (*gsc_Method)(gsc_State *, gsc_Object *);
	GSC_API void gsc_register_function(gsc_State *state, const char *namespace, const char *name, gsc_Function);
	GSC_API void gsc_register_method(gsc_State *state, const char *namespace, const char *name, gsc_Method);

	GSC_API void gsc_object_set_field(gsc_State *state, gsc_Object *, const char *name);

	GSC_API void gsc_push(gsc_State *state, void *value);
	GSC_API void *gsc_pop(gsc_State *state);

	// Push specific types onto the stack
	GSC_API void gsc_push_int(gsc_State *state, int value);			   // Push an integer
	GSC_API void gsc_push_float(gsc_State *state, float value);		   // Push a float
	GSC_API void gsc_push_string(gsc_State *state, const char *value); // Push a string

	// Conversion functions to retrieve values from the stack
	GSC_API int gsc_to_int(gsc_State *state, int index);
	GSC_API float gsc_to_float(gsc_State *state, int index);
	GSC_API const char *gsc_to_string(gsc_State *state, int index);

#ifdef __cplusplus
}
#endif
