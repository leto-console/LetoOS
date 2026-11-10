add_compile_definitions(${PRESET_DEFINE_MACRO})

cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

if(MSVC)
    add_compile_options(/wd4251)
endif()

# Функция добавления импортированной библиотеки Leto
# Использование:
#   add_leto_library(LetoAPI)
#   add_leto_library(LetoCore COMPILE_DEFINITIONS <...>)
#   add_leto_library(LetoAPI LIB_DIR <...> BIN_DIR <...>)
#   add_leto_library(LetoCore LIB_DIR <...> BIN_DIR <...> COMPILE_DEFINITIONS <...>)
function(add_leto_library LIB_NAME)

    # Разбор именованных аргументов
    set(options "")
    set(oneValueArgs LIB_DIR BIN_DIR)
    set(multiValueArgs COMPILE_DEFINITIONS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	# Если не переданы, используем глобальные переменные
    if(NOT ARG_LIB_DIR)
        set(ARG_LIB_DIR "${LIB_DIR}")
    endif()
    if(NOT ARG_BIN_DIR)
        set(ARG_BIN_DIR "${BIN_DIR}")
    endif()

    # Платформо-зависимые настройки
    if(WIN32)
        set(_type SHARED)
        set(_prefix "")
        set(_ext "lib")
        set(_implib "${ARG_LIB_DIR}/${LIB_NAME}.${_ext}")
        set(_location "${ARG_BIN_DIR}/${LIB_NAME}.dll")
    else()
        set(_type STATIC)
        set(_prefix "lib")
        set(_ext "a")
        set(_implib "")
        set(_location "${ARG_LIB_DIR}/${_prefix}${LIB_NAME}.${_ext}")
    endif()

    # Создание импортированной цели
    add_library(${LIB_NAME} ${_type} IMPORTED GLOBAL)

    # Базовые свойства (общие для всех библиотек)
    set_target_properties(${LIB_NAME} PROPERTIES
        IMPORTED_LOCATION "${_location}"
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{LETO_PATH}/${LIB_NAME}/include"
    )

    # IMPORTED_IMPLIB нужен только для Windows (SHARED)
    if(_implib)
        set_target_properties(${LIB_NAME} PROPERTIES
            IMPORTED_IMPLIB "${_implib}"
        )
    endif()

    # Дополнительные compile definitions (если переданы)
    if(ARG_COMPILE_DEFINITIONS)
        set_target_properties(${LIB_NAME} PROPERTIES
            INTERFACE_COMPILE_DEFINITIONS "${ARG_COMPILE_DEFINITIONS}"
        )
    endif()
	
endfunction()

if("${PRESET_DEFINE_MACRO}" STREQUAL "__STM32__")
	project("LetoOS" C CXX ASM)
	file(GLOB HW_COURCES "src/*.cpp" "src/*.c" )
	add_library(${PROJECT_NAME} STATIC "${C_CPP_COURCES}" "${HW_COURCES}")
else()
	project("LetoOS" C CXX)
	if(DEFINED ENV{LETO_PATH})
		add_library("${PROJECT_NAME}" SHARED "${C_CPP_COURCES}")
		target_compile_definitions(${PROJECT_NAME} PRIVATE LETO_OS_SHARED)
	else()
		add_library("${PROJECT_NAME}" STATIC "${C_CPP_COURCES}")
	endif()
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

	#find_package(Qt6 COMPONENTS Widgets REQUIRED Core)
	#qt_standard_project_setup()
	#add_executable ("${PROJECT_NAME}" "Main/App.cpp" "${C_CPP_COURCES}")
	#target_link_libraries("${PROJECT_NAME}" PRIVATE Qt6::Core Qt6::Widgets)

elseif (${PRESET_DEFINE_MACRO} STREQUAL "__TERMUX__")

	include(FetchContent)
	FetchContent_Declare(
		cpp-httplib
		GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
		GIT_TAG        v0.19.0
	)
	FetchContent_MakeAvailable(cpp-httplib)

	set_target_properties("${PROJECT_NAME}" PROPERTIES POSITION_INDEPENDENT_CODE ON)

	target_link_libraries(${PROJECT_NAME} PRIVATE httplib::httplib)
	target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

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

	set(LIB_DIR "$ENV{LETO_PATH}/Common/${CMAKE_PRESET_NAME}/lib")
	set(BIN_DIR "$ENV{LETO_PATH}/Common/${CMAKE_PRESET_NAME}/bin")

	add_leto_library(LetoAPI)
	add_leto_library(LetoCore)

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
# if (NOT ${PRESET_DEFINE_MACRO} STREQUAL "__STM32__")
# 	enable_testing()
#     add_subdirectory(Tests)
# endif()
