cmake_minimum_required(VERSION 3.10)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2 # Replace with the latest or preferred version
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Собираем файлы тестов
file(GLOB TEST_SOURCES 
    "*/*.cpp"
)

# Fixtures/ собирается отдельной целью (см. ниже) и в исполняемый файл тестов
# попадать не должен: его исходник задаёт видимость символа через GCC-атрибут.
list(FILTER TEST_SOURCES EXCLUDE REGEX "/Fixtures/")

# Создаём исполняемый файл с тестами
add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES})

# Фикстура загрузчика: настоящая разделяемая библиотека с заголовком игры.
# Отдельная цель, потому что тестам нужен реальный файл: сканирование открывает
# его через dlopen, и подделать это без сборки нельзя.
if (NOT ${PRESET_DEFINE_MACRO} STREQUAL "__STM32__" AND NOT ${PRESET_DEFINE_MACRO} STREQUAL "__WIN__")
    add_library(TestGameFixture SHARED Fixtures/TestGameFixture.cpp)
    target_link_libraries(TestGameFixture PRIVATE LetoAPI)
    set_target_properties(TestGameFixture PROPERTIES
        PREFIX "lib"
        OUTPUT_NAME "TestGameFixture"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/fixtures"
    )
    add_dependencies(${PROJECT_NAME}_tests TestGameFixture)
    target_compile_definitions(${PROJECT_NAME}_tests PRIVATE
        "LETO_TEST_FIXTURE_DIR=\"${CMAKE_BINARY_DIR}/fixtures\"")
endif()

target_compile_definitions(${PROJECT_NAME}_tests PRIVATE LETO_API_V1_USE=0)
target_compile_features(${PROJECT_NAME}_tests PRIVATE cxx_std_17)
install(TARGETS ${PROJECT_NAME}_tests
    RUNTIME DESTINATION bin     # Для исполняемых файлов
)

# Подключаем заголовочные файлы основного проекта
target_include_directories(${PROJECT_NAME}_tests PRIVATE 
    "${CMAKE_SOURCE_DIR}/Inc"
    "${CMAKE_SOURCE_DIR}/Src"
)

# Линкуем с основной библиотекой и gtest
target_link_libraries(${PROJECT_NAME}_tests PRIVATE 
    ${PROJECT_NAME}  # Твоя основная библиотека
    GTest::gtest_main
)

## Для Windows: настройки линковщика (если нужно)
#if (${PRESET_DEFINE_MACRO} STREQUAL "__WIN__")
#    target_compile_definitions(${PROJECT_NAME}_tests PRIVATE __WIN__)
#    set_target_properties(${PROJECT_NAME}_tests PROPERTIES 
#        LINK_FLAGS "/ENTRY:mainCRTStartup"
#    )
#elseif (${PRESET_DEFINE_MACRO} STREQUAL "__LINUX__")
#    target_compile_definitions(${PROJECT_NAME}_tests PRIVATE __LINUX__)
#    # Если используешь Qt в тестах:
#    # target_link_libraries(${PROJECT_NAME}_tests PRIVATE Qt6::Core Qt6::Widgets)
#endif()

# === Интеграция с CTest ===
include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}_tests
    TEST_PREFIX "${PROJECT_NAME}."
    NO_PRETTY_TYPES
    NO_PRETTY_VALUES
    DISCOVERY_TIMEOUT 30
)
