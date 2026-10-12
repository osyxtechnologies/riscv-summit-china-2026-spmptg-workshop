cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr)
project(dual-task)

if(WORKSHOP_CONFIG)
  # Milestone 0 supplies the participant's memory-domain configuration.
  target_include_directories(app PRIVATE src)
  target_sources(app PRIVATE src/isolation.c ${WORKSHOP_CONFIG})
else()
  target_sources(app PRIVATE src/main.c)
endif()
