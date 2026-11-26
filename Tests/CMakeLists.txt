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

# Fixtures/ собирается отдельными целями (см. ниже) и в исполняемый файл тестов
# попадать не должен: его исходники — это тела разделяемых библиотек.
list(FILTER TEST_SOURCES EXCLUDE REGEX "/Fixtures/")

# Создаём исполняемый файл с тестами
add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES})

target_compile_definitions(${PROJECT_NAME}_tests PRIVATE LETO_API_V1_USE=0)
target_compile_features(${PROJECT_NAME}_tests PRIVATE cxx_std_17)
install(TARGETS ${PROJECT_NAME}_tests
    RUNTIME DESTINATION bin     # Для исполняемых файлов
)

# Подключаем заголовочные файлы основного проекта
target_include_directories(${PROJECT_NAME}_tests PRIVATE 
    "${CMAKE_SOURCE_DIR}/Inc"
    "${CMAKE_SOURCE_DIR}/Src"
    "${CMAKE_CURRENT_SOURCE_DIR}"   # TestSupport.hpp
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

# ==============================================================================
#                          Фикстуры загрузчика
# ==============================================================================

# Сканирование открывает реальный файл через dlopen, поэтому «хорошую» игру
# нельзя подделать без сборки разделяемой библиотеки. Фикстуры собираются
# только там, где есть dlopen: у Windows своя ветка загрузчика (LoadLibrary),
# и её эти тесты не покрывают.
if (NOT "${PRESET_DEFINE_MACRO}" STREQUAL "__STM32__" AND
        NOT "${PRESET_DEFINE_MACRO}" STREQUAL "__WIN__")
    set(LETO_TEST_FIXTURE_DIR "${CMAKE_BINARY_DIR}/fixtures")
    file(MAKE_DIRECTORY "${LETO_TEST_FIXTURE_DIR}")

    # Игра с полным циклом жизни и журналом вызовов. Имя и id задаются снаружи:
    # один исходник даёт разные игры, и тест отличает их по заголовку.
    function(add_game_fixture target output_name id en_name ru_name)
        add_library(${target} SHARED Fixtures/GameFixture.cpp)
        target_link_libraries(${target} PRIVATE LetoAPI)
        target_compile_definitions(${target} PRIVATE
            "LETO_TEST_FIXTURE_ID=${id}"
            "LETO_TEST_FIXTURE_NAME=\"${en_name}\""
            "LETO_TEST_FIXTURE_RU_NAME=\"${ru_name}\""
        )
        set_target_properties(${target} PROPERTIES
            PREFIX "lib"
            OUTPUT_NAME "${output_name}"
            LIBRARY_OUTPUT_DIRECTORY "${LETO_TEST_FIXTURE_DIR}"
        )
        add_dependencies(${PROJECT_NAME}_tests ${target})
    endfunction()

    add_game_fixture(TestGameFixture valid_game 0x0101 "ValidGame" "Тестовая игра")

    # Библиотека без заголовка игры: загрузчик обязан пропустить её по имени
    # символа, а не упасть.
    add_library(TestNoSymbolFixture SHARED Fixtures/NoSymbol.cpp)
    set_target_properties(TestNoSymbolFixture PROPERTIES
        PREFIX "lib"
        OUTPUT_NAME "no_symbol"
        LIBRARY_OUTPUT_DIRECTORY "${LETO_TEST_FIXTURE_DIR}"
    )
    add_dependencies(${PROJECT_NAME}_tests TestNoSymbolFixture)

    # Заголовок есть и экспортирован, но type не LETO_EXEC_GAME.
    add_library(TestWrongMagicFixture SHARED Fixtures/WrongMagic.cpp)
    target_link_libraries(TestWrongMagicFixture PRIVATE LetoAPI)
    set_target_properties(TestWrongMagicFixture PROPERTIES
        PREFIX "lib"
        OUTPUT_NAME "wrong_magic"
        LIBRARY_OUTPUT_DIRECTORY "${LETO_TEST_FIXTURE_DIR}"
    )
    add_dependencies(${PROJECT_NAME}_tests TestWrongMagicFixture)

    # Контракт #92-2: библиотека целиком скрыта, точку входа делает видимой
    # только экспортный макрос LetoAPI. Флаг понимают GCC и Clang.
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_game_fixture(TestHiddenGameFixture hidden_visibility 0x0103 "HiddenGame" "Скрытая игра")
        target_compile_options(TestHiddenGameFixture PRIVATE -fvisibility=hidden)
        target_compile_definitions(TestHiddenGameFixture PRIVATE LETO_TEST_FIXTURE_HIDDEN)
    endif()

    # Битый и пустой файл — тоже кандидаты в каталоге игры: сканирование идёт
    # раз в секунду, и один такой файл не должен ломать список. Это цели, а не
    # file(WRITE) на этапе конфигурации, чтобы «сборка фикстур» была одной
    # командой и файл пересоздавался, если его испортил тест.
    function(add_invalid_fixture target output_name content)
        add_custom_target(${target} ALL
            COMMAND ${CMAKE_COMMAND}
                -DOUTPUT=${LETO_TEST_FIXTURE_DIR}/${output_name}
                "-DCONTENT=${content}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/Fixtures/MakeInvalidFixture.cmake"
            COMMENT "Фикстура загрузчика: ${output_name}"
            VERBATIM
        )
        add_dependencies(${PROJECT_NAME}_tests ${target})
    endfunction()

    add_invalid_fixture(TestBrokenFileFixture libbroken_file.so "not an ELF at all\n")
    add_invalid_fixture(TestEmptyFileFixture libempty_file.so "")

    target_compile_definitions(${PROJECT_NAME}_tests PRIVATE
        "LETO_TEST_FIXTURE_DIR=\"${LETO_TEST_FIXTURE_DIR}\""
        "LETO_TEST_PRESET_NAME=\"${CMAKE_PRESET_NAME}\""
    )
endif()

# === Интеграция с CTest ===
include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}_tests
    TEST_PREFIX "${PROJECT_NAME}."
    NO_PRETTY_TYPES
    NO_PRETTY_VALUES
    DISCOVERY_TIMEOUT 30
)
