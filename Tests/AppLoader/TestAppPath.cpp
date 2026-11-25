#include <gtest/gtest.h>

#include <AppLoader/AppScene.hpp>

#include <string>

/*
    Путь к игре должен сохраняться целиком, пока влезает в AppPath: прежний
    размер (127 символов) обрезал реальные пути в Apps/ и терял игру.
*/
TEST(TestAppPath, KeepsPathUpToCapacity)
{
    for (size_t length : {size_t{1}, size_t{127}, size_t{128}, AppPath.Capacity()})
    {
        const std::string path(length, 'p');

        SetAppPath(path.c_str());

        EXPECT_EQ(AppPath.TextLength(), length);
        EXPECT_STREQ(AppPath.ConstChar(), path.c_str());
    }
}

/*
    Путь длиннее ёмкости обрезается — остаётся ровно Capacity() символов,
    последний байт остаётся нулевым терминалом.
*/
TEST(TestAppPath, TruncatesLongerPath)
{
    const std::string path(AppPath.Capacity() + 10, 'p');

    SetAppPath(path.c_str());

    EXPECT_EQ(AppPath.TextLength(), AppPath.Capacity());
    EXPECT_EQ(AppPath[AppPath.Capacity()], '\0');
}

TEST(TestAppPath, EmptyPathIsCleared)
{
    SetAppPath("Apps/linux-debug/Game/libGame.so");

    SetAppPath("");

    EXPECT_EQ(AppPath.TextLength(), 0u);
}

TEST(TestAppPath, NullPathIsCleared)
{
    SetAppPath("Apps/linux-debug/Game/libGame.so");

    SetAppPath(nullptr);

    EXPECT_EQ(AppPath.TextLength(), 0u);
}
