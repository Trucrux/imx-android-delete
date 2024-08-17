cmake_minimum_required(VERSION 2.6)

set(WAYLAND_XML_PRIFIX ${SDKTARGETSYSROOT}/usr/share/wayland-protocols)
set(GEN_INSTALL_PREFIX ${PROJECT_SOURCE_DIR}/display/wayland-client)

function(gen_wayland_file module type version)
    if(${type} STREQUAL "unstable")
        set(xml_file ${WAYLAND_XML_PRIFIX}/${type}/${module}/${module}-${type}-${version}.xml)
        set(output_name ${GEN_INSTALL_PREFIX}/${module}-${type}-${version})
    elseif(${type} STREQUAL "staging")
        set(xml_file ${WAYLAND_XML_PRIFIX}/${type}/${module}/${module}-${version}.xml)
        set(output_name ${GEN_INSTALL_PREFIX}/${module}-${version})
    else()
        set(xml_file ${WAYLAND_XML_PRIFIX}/${type}/${module}/${module}.xml)
        set(output_name ${GEN_INSTALL_PREFIX}/${module})
    endif()
    
    if (EXISTS xml_file)
        message("${xml_file} Not exist")
    endif()

    execute_process(COMMAND wayland-scanner client-header ${xml_file} ${output_name}-client-protocol.h)
    execute_process(COMMAND wayland-scanner private-code ${xml_file} ${output_name}-protocol.c)
endfunction()

gen_wayland_file("xdg-shell" "stable" "")
gen_wayland_file("viewporter" "stable" "")
gen_wayland_file("linux-dmabuf" "unstable" "v1")