
# Helper function to add preprocesor definition of FILE_BASENAME
# to pass the filename without directory path for debugging use.
#
# Note that in header files this is not consistent with
# __FILE__ and __LINE__ since FILE_BASENAME will be the
# compilation unit source file name (.c/.cpp).
#
# Example:
#
#   define_file_basename_for_sources(my_target)
#
# Will add -DFILE_BASENAME="filename" for each source file depended on
# by my_target, where filename is the name of the file.
#
function(define_file_basename_for_sources targetname)
    get_target_property(source_files "${targetname}" SOURCES)
    foreach(sourcefile ${source_files})
        # Add the FILE_BASENAME=filename compile definition to the list.
        get_filename_component(basename "${sourcefile}" NAME)
        # Set the updated compile definitions on the source file.
        set_property(
            SOURCE "${sourcefile}" APPEND
            PROPERTY COMPILE_DEFINITIONS "FILE_BASENAME=\"${basename}\"")
    endforeach()
endfunction()
