cmake_minimum_required(VERSION 2.8)

project(nvstore)
add_definitions(-fno-exceptions)
set(SRCS main.cpp ${STM32PP_SRCS})
set(imgname "${CMAKE_PROJECT_NAME}.elf")
add_executable(${imgname} ${SRCS})
stm32_create_utility_targets(${imgname})
