cmake_minimum_required(VERSION 3.16)

set(EXCLUDE_COMPONENTS embed_claw embed_claw2)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(EmbedClaw)

spiffs_create_partition_image(spiffs spiffs_data FLASH_IN_PROJECT)
