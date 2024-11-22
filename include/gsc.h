#pragma once
#include <stdint.h>
#include <stddef.h>

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

	enum
	{
		GSC_OK,
		GSC_ERROR,
		GSC_YIELD,
		GSC_NOT_FOUND,
		GSC_OUT_OF_MEMORY
	};

#define GSC_TYPES(X)   \
	X(UNDEFINED)       \
	X(STRING)          \
	X(INTERNED_STRING) \
	X(INTEGER)         \
	X(BOOLEAN)         \
	X(FLOAT)           \
	X(VECTOR)          \
	X(FUNCTION)        \
	X(OBJECT)          \
	X(REFERENCE)       \
	X(THREAD)

#define GSC_TYPES_STRINGS(TYPE) #TYPE,
	static const char *gsc_type_names[] = { GSC_TYPES(GSC_TYPES_STRINGS) NULL };
#define GSC_TYPES_ENUM(TYPE) GSC_TYPE_##TYPE,
	enum
	{
		GSC_TYPES(GSC_TYPES_ENUM) GSC_TYPE_MAX
	};

	typedef struct gsc_Context gsc_Context;
	typedef struct
	{
		void *(*allocate_memory)(void *ctx, int size);							// Allocate memory of specified size
		void (*free_memory)(void *ctx, void *ptr);								// Free previously allocated memory
		const char *(*read_file)(void *ctx, const char *filename, int *status); // Read file and return status
		void *userdata;															// User-defined data pointer
		int verbose;
		int main_memory_size;
		int temp_memory_size;
		int string_table_memory_size;
		const char *default_self;
		int max_threads;
	} gsc_CreateOptions;

	GSC_API gsc_Context *gsc_create(gsc_CreateOptions options);
	GSC_API void gsc_destroy(gsc_Context *ctx);
	GSC_API void gsc_error(gsc_Context *ctx, const char *fmt, ...);

	GSC_API int gsc_link(gsc_Context *ctx);

	#define GSC_COMPILE_FLAG_NONE (0)
	#define GSC_COMPILE_FLAG_PRINT_EXPRESSION (1)

	GSC_API int gsc_compile(gsc_Context *ctx, const char *filename, int flags);
	GSC_API const char *gsc_next_compile_dependency(gsc_Context *ctx);
	GSC_API void *gsc_temp_alloc(gsc_Context *ctx, int size);
	GSC_API int gsc_update(gsc_Context *ctx, float dt);
	GSC_API int gsc_call(gsc_Context *ctx, const char *file, const char *function, int nargs);
	GSC_API int gsc_call_method(gsc_Context *ctx, const char *file, const char *function, int nargs);
	GSC_API void gsc_object_set_debug_info(gsc_Context *ctx,
										   void *object,
										   const char *file,
										   const char *function,
										   int line);
	typedef int (*gsc_Function)(gsc_Context *);

	typedef struct
	{
		const char *name;
		gsc_Function function;
	} gsc_FunctionEntry;

	typedef struct
	{
		const char *name;
		gsc_Function getter;
		gsc_Function setter;
	} gsc_FieldEntry;

	typedef struct gsc_Object gsc_Object;
	GSC_API int gsc_register_string(gsc_Context *ctx, const char *s);
	GSC_API const char *gsc_string(gsc_Context *ctx, int index);

	GSC_API void gsc_register_function(gsc_Context *ctx, const char *file, const char *name, gsc_Function);

	GSC_API void gsc_object_set_field(gsc_Context *ctx, int obj_index, const char *name);
	GSC_API void gsc_object_get_field(gsc_Context *ctx, int obj_index, const char *name);
	GSC_API const char *gsc_object_get_tag(gsc_Context *ctx, int obj_index);

	GSC_API int gsc_top(gsc_Context *ctx);
	GSC_API int gsc_type(gsc_Context *ctx, int index);
	GSC_API void gsc_push(gsc_Context *ctx, void *value);
	GSC_API int gsc_push_object(gsc_Context *ctx, void *value);
	GSC_API void gsc_pop(gsc_Context *ctx, int count);

	GSC_API void *gsc_allocate_object(gsc_Context *ctx);

	// Push specific types onto the stack
	GSC_API int gsc_add_object(gsc_Context *ctx); // Push an new object
	GSC_API int gsc_add_tagged_object(gsc_Context *ctx, const char *tag);
	GSC_API int gsc_object_get_proxy(gsc_Context *ctx, int obj_index);
	GSC_API void gsc_object_set_proxy(gsc_Context *ctx, int obj_index, int proxy_index);

	GSC_API void *gsc_object_get_userdata(gsc_Context *ctx, int obj_index);
	GSC_API void gsc_object_set_userdata(gsc_Context *ctx, int obj_index, void *userdata);

	GSC_API void gsc_add_int(gsc_Context *ctx, int64_t value);			  // Push an integer
	GSC_API void gsc_add_float(gsc_Context *ctx, float value);		  // Push a float
	GSC_API void gsc_add_string(gsc_Context *ctx, const char *value); // Push a string
	GSC_API void gsc_add_vec3(gsc_Context *ctx, /*const*/ float *value); // Push a vec3
	GSC_API void gsc_add_function(gsc_Context *ctx, gsc_Function value);
	GSC_API void gsc_add_bool(gsc_Context *state, int cond);

	// Conversion functions to retrieve values from the stack
	GSC_API int64_t gsc_to_int(gsc_Context *ctx, int index);
	GSC_API float gsc_to_float(gsc_Context *ctx, int index);
	GSC_API const char *gsc_to_string(gsc_Context *ctx, int index);

	// Get arguments for a function call, index relative to stack frame for function
	// index -1: self
	// index 0: first argument
	// index 1: second argument
	// and so on
	GSC_API int64_t gsc_get_int(gsc_Context *ctx, int index);
	GSC_API int gsc_get_bool(gsc_Context *ctx, int index);
	GSC_API float gsc_get_float(gsc_Context *ctx, int index);
	GSC_API const char *gsc_get_string(gsc_Context *ctx, int index);
	GSC_API void gsc_get_vec3(gsc_Context *ctx, int index, float *v);
	GSC_API int gsc_get_object(gsc_Context *ctx, int index);
	GSC_API int gsc_get_type(gsc_Context *ctx, int index);
	GSC_API int gsc_numargs(gsc_Context *ctx);
	GSC_API int gsc_arg(gsc_Context *ctx, int index);

	GSC_API void* gsc_get_ptr(gsc_Context *ctx, int index);

	GSC_API int gsc_get_global(gsc_Context *ctx, const char *name);
	GSC_API void gsc_set_global(gsc_Context *ctx, const char *name);

	// This function may break
	GSC_API void *gsc_get_internal_pointer(gsc_Context *ctx, const char *tag);

#ifdef __cplusplus
}
#endif
