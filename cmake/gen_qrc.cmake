# Generates a qrc embedding the compiled translation files.
# The translation file list is read from ${QM_LIST} (a cmake script that
# sets TRANSLATION_FILES) to avoid quoting issues in generated build rules.
# Usage:
#   cmake -DQM_LIST=/path/qm_list.cmake -DOUT=/path/translations_generated.qrc -P gen_qrc.cmake
include("${QM_LIST}")
set(content "<RCC>\n    <qresource prefix=\"/translations\">\n")
foreach(qm IN LISTS TRANSLATION_FILES)
    get_filename_component(qm_name "${qm}" NAME)
    # alias as the bare language code ("tr"), matching the reference layout
    string(REGEX REPLACE "^.*_([^_.]+)\\.qm$" "\\1" qm_alias "${qm_name}")
    if(qm_alias STREQUAL qm_name)
        set(qm_alias "${qm_name}")
    endif()
    string(APPEND content "        <file alias=\"${qm_alias}\">${qm}</file>\n")
endforeach()
string(APPEND content "    </qresource>\n</RCC>\n")
file(WRITE "${OUT}" "${content}")
