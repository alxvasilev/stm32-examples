cmake_minimum_required(VERSION 2.8)

project(photodiode)
set(SRCS main.cpp ${STM32PP_SRCS} ${STM32PP_SRCPATH}/stdfonts.cpp)
set(imgname "${CMAKE_PROJECT_NAME}.elf")
add_executable(${imgname} ${SRCS})
stm32_create_utility_targets(${imgname})
