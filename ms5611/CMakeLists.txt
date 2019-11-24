cmake_minimum_required(VERSION 2.8)

project(ms5611)
set(SRCS main.cpp font.cpp
"${ENV_SCRIPTS_DIR}/stm32++/tprintf.cpp" 
"${ENV_SCRIPTS_DIR}/stm32++/semihosting.cpp"
"${ENV_SCRIPTS_DIR}/stm32++/timeutl.cpp"
#"${ENV_SCRIPTS_DIR}/stm32++/fonts.c"
)
set(imgname "${CMAKE_PROJECT_NAME}.elf")
add_executable(${imgname} ${SRCS})
add_custom_target(flash bash -c "${ENV_SCRIPTS_DIR}/flash.sh ./${imgname}" DEPENDS ${imgname})
