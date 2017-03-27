cmake_minimum_required(VERSION 2.8)
Project(libosip2)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_definitions(-DOSIP_MT)

file(GLOB osipparser2_src ${CMAKE_CURRENT_SOURCE_DIR}/src/osipparser2/*.c)
file(GLOB osipparser2_inc ${CMAKE_CURRENT_SOURCE_DIR}/include/osipparser2/*.h)
file(GLOB osipheaders2_inc ${CMAKE_CURRENT_SOURCE_DIR}/include/osipparser2/headers/*.h)

add_library(osipparser2 STATIC ${osipparser2_src} ${osipparser2_inc})

file(GLOB osip2_src ${CMAKE_CURRENT_SOURCE_DIR}/src/osip2/*.c)
file(GLOB osip2_inc ${CMAKE_CURRENT_SOURCE_DIR}/include/osip2/*.h)
add_library(osip2 STATIC ${osip2_src} ${osip2_inc})
add_dependencies(osip2 osipparser2)

set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/../..)
install(FILES ${osipheaders2_inc} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/osipparser2/headers)
install(FILES ${osipparser2_inc} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/osipparser2)
install(FILES ${osip2_inc} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/osip2)
install(TARGETS osip2 osipparser2 DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
