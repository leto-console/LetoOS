/**
 * @brief AppScene.hpp
 * @date Aug 25, 2025
 * @author Rakhimov T.
 */

#ifndef INC_APP_LOADER_APP_SCENE_HPP_
#define INC_APP_LOADER_APP_SCENE_HPP_

#include "LetoOSExport.h"

#include <SceneManager/IScene.hpp>
#include <SceneManager/ISceneBuilder.hpp>

#include <GamesSupport/BaseGame.hpp>
#include <AppLoader/AppInfo.hpp>

// Путь до папки с игрой. Совпадает с AppInfo::path: путь длиннее 127 символов
// раньше обрезался молча.
extern LETO_OS_EXPORT StaticText<260> AppPath;

/**
 * @brief Записать путь к игре в AppPath
 *
 * Путь, не влезающий в AppPath целиком, обрезается — сообщаем об этом в лог,
 * иначе игра потом не найдётся по усечённому пути и причина будет не видна.
 */
extern LETO_OS_EXPORT void SetAppPath(const char* path);

// Сцены выбора игры + обработки текущей запущенной игры
class LETO_OS_EXPORT AppScene : public IScene
{
protected:
	/// Проверить, что событие пользовательского ввода нужно отправлять в игру
	bool IsInputForGame(const AppEvent& event);

public:
	AppScene(ISceneManager* scene_manager);

	void OnShow() override;
	void OnHide() override;

	void Draw(IScreen& screen) override;
	void Loop() override;
	bool ProcessInput(const AppEvent& event) override;

	SCENE_NO_ARGS_BUILDER(AppScene)
};

#endif
