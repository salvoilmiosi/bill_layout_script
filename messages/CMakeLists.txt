find_package(Gettext)

set(translation_languages
    it
    en
)

foreach(lang ${translation_languages})
    set(basename "bls_${lang}")
    set(translation_po_file "${basename}.po")

    GETTEXT_PROCESS_PO_FILES("${lang}" PO_FILES "${translation_po_file}")
    
    set(translation_mo_file "${CMAKE_CURRENT_BINARY_DIR}/${basename}.gmo")
    set(translation_c_file "${CMAKE_CURRENT_BINARY_DIR}/${basename}.c")

    list(APPEND translation_c_files "${translation_c_file}")

    string(MAKE_C_IDENTIFIER "${basename}" basename)

    add_custom_command(
        OUTPUT "${translation_c_file}"
        COMMAND ${CMAKE_COMMAND}
            -D "resource_file_name=${translation_mo_file}"
            -D "source_file_name=${translation_c_file}"
            -D "variable_name=__translation_${basename}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/EmbedResource.cmake"
        DEPENDS pofiles "${translation_po_file}"
        VERBATIM
    )
endforeach()

add_library(translation_obj OBJECT ${translation_c_files})