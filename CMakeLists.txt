cmake_minimum_required(VERSION 3.10)
project("LetoOS" C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(AddLetoLibrary)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIB_DIR "$ENV{LETO_PATH}/Common/${CMAKE_PRESET_NAME}/lib")
set(BIN_DIR "$ENV{LETO_PATH}/Common/${CMAKE_PRESET_NAME}/bin")

file(
	GLOB C_CPP_COURCES 
	"src/*/*.cpp" "src/*/*.c"
	"src/*/*/*.cpp" "src/*/*/*.c"
	"src/*/*/*/*.cpp" "src/*/*/*/*.c"
	"src/*/*/*/*/*.cpp" "src/*/*/*/*/*.c"
	"include/*/*.cpp" "include/*/*.c"
	"include/*/*/*.cpp" "include/*/*/*.c"
	"include/*/*/*/*.cpp" "include/*/*/*/*.c"
	"include/*/*/*/*/*.cpp" "include/*/*/*/*/*.c"
)

add_compile_definitions(${PRESET_DEFINE_MACRO})

if(MSVC)
    add_compile_options(/wd4251)
endif()

if(DEFINED ENV{LETO_PATH} AND WIN32)
    set(LIBRARY_TYPE SHARED)
else()
	set(LIBRARY_TYPE STATIC)
endif()

add_library(${PROJECT_NAME} ${LIBRARY_TYPE} ${C_CPP_COURCES})

if(${LIBRARY_TYPE} STREQUAL "SHARED")
	target_compile_definitions(${PROJECT_NAME} PRIVATE LETO_OS_SHARED)
endif()

if("${PRESET_DEFINE_MACRO}" STREQUAL "__STM32__")
	file(GLOB HW_COURCES "src/*.cpp" "src/*.c" )
	target_sources(${PROJECT_NAME} PRIVATE "${HW_COURCES}")
endif()

target_include_directories(${PROJECT_NAME} PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin     # Для исполняемых файлов
    ARCHIVE DESTINATION lib     # Для статических библиотек
    LIBRARY DESTINATION lib     # Для динамических библиотек
)
target_compile_definitions(${PROJECT_NAME} PRIVATE LETO_OS_EXPORT_LIBRARY)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (${PRESET_DEFINE_MACRO} STREQUAL "__STM32__")
	enable_language(ASM)

	target_compile_definitions(${PROJECT_NAME} PRIVATE
		USE_HAL_DRIVER
		${MCU_CHIP}
	)
	
	# ------------------------------------------------------------------------------
	# Настройки архитектуры из .cproject (STM32F401CCUx)
	# ------------------------------------------------------------------------------
	set(OBJECT_GEN_FLAGS "-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb")
	set(COMPILER_FLAGS "-Os -ffunction-sections -fdata-sections -Wall -fstack-usage")
	# -fcyclomatic-complexity - Используется только в STM32CubeIDE

	# Флаги компиляции для C / C++ / ASM
	set(CMAKE_C_FLAGS   "${OBJECT_GEN_FLAGS} ${COMPILER_FLAGS}" CACHE INTERNAL "C Compiler Flags")
	set(CMAKE_CXX_FLAGS "${OBJECT_GEN_FLAGS} ${COMPILER_FLAGS} -fno-exceptions -fno-rtti -fno-use-cxa-atexit" CACHE INTERNAL "C++ Compiler Flags")
	set(CMAKE_ASM_FLAGS "${OBJECT_GEN_FLAGS} -x assembler-with-cpp" CACHE INTERNAL "ASM Compiler Flags")

	#set(CMAKE_EXE_LINKER_FLAGS "-Wl,--gc-sections --specs=nano.specs --specs=nosys.specs -mthumb -mabi=aapcs -Wl,-Map=${CMAKE_PROJECT_NAME}.map" CACHE INTERNAL "Linker options")

	# ------------------------------------------------------------------------------
	# Пути к заголовочным файлам (-I)
	# ------------------------------------------------------------------------------
	target_include_directories(${PROJECT_NAME} PUBLIC
		Drivers/CMSIS/Device/ST/STM32F4xx/Include
		Drivers/CMSIS/Include
		Drivers/STM32F4xx_HAL_Driver/Inc
		Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
	)

elseif (${PRESET_DEFINE_MACRO} STREQUAL "__WIN__")

	# Включаем предупреждения только для вашей цели
	target_compile_options("${PROJECT_NAME}" PRIVATE
		$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wunused>
		$<$<CXX_COMPILER_ID:MSVC>:/W4 /wd4251>
	)

	if(DEFINED ENV{LETO_PATH})
		install(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}> 
				DESTINATION bin
				OPTIONAL)
	endif()

elseif (${PRESET_DEFINE_MACRO} STREQUAL "__LINUX__")

    find_package(SDL2 REQUIRED CONFIG)
    target_link_libraries(${PROJECT_NAME} PUBLIC SDL2::SDL2)

endif ()

target_compile_definitions("${PROJECT_NAME}" PRIVATE LETO_OS_EXPORT_LIBRARY)

# ============================================= LetoAPI dependency ============================================

if(DEFINED ENV{LETO_PATH})
    message(STATUS "[${PROJECT_NAME}] Найдена переменная окружения LETO_PATH. Подключаем готовые артефакты...")

	if (${PRESET_DEFINE_MACRO} STREQUAL "__WIN__")
		add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_directory
			"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
			"$ENV{LETO_PATH}/Common/${CMAKE_PRESET_NAME}/bin/"
		)
	endif()

	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
		"$ENV{LETO_PATH}/Common/${CMAKE_PRESET_NAME}/lib/"
	)

	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}"
		"$ENV{LETO_PATH}/Common/${CMAKE_PRESET_NAME}/lib/"
	)

	add_leto_library(LetoAPI COMPILE_DEFINITIONS LETOCORE_SHARED)
	add_leto_library(LetoCore COMPILE_DEFINITIONS LETOCORE_SHARED)

	if (${PRESET_DEFINE_MACRO} STREQUAL "__WIN__")
		add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"$<IF:$<TARGET_EXISTS:LetoAPI>,$<TARGET_FILE:LetoAPI>,$ENV{LETO_PATH}/LetoAPI/bin/${CMAKE_PRESET_NAME}/LetoAPI.dll>"
			$<TARGET_FILE_DIR:${PROJECT_NAME}>
			COMMENT "[${PROJECT_NAME}] Копирование LetoAPI.dll в папку тестов..."
		)
	endif()
else()
    message(STATUS "[${PROJECT_NAME}] Переменная окружения LETO_PATH не найдена. Серверная сборка через FetchContent...")

	include(FetchContent)
    FetchContent_Declare(
        LetoCore
		# SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../LetoAPI"
        GIT_REPOSITORY https://github.com/leto-console/LetoCore
        GIT_TAG        main
    )
    FetchContent_MakeAvailable(LetoCore)
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC LetoCore LetoAPI)
# ==============================================================================================================

# === Подключение подсистемы тестов ===
# Тесты собираются только если это не целевая платформа STM32
if (NOT ${PRESET_DEFINE_MACRO} STREQUAL "__STM32__")
	enable_testing()
    add_subdirectory(Tests)
endif()
