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

/**
 * @brief Записать путь к текущему приложению
 */
extern LETO_OS_EXPORT void SetAppPath(const char* path);

/**
 * @brief Получить путь к текущему приложению
 */
extern LETO_OS_EXPORT StaticTextView GetAppPath();

// Сцены выбора приложения + обработки текущего запущенного приложения
class LETO_OS_EXPORT AppScene : public IScene
{
protected:
	/// Проверить, что событие пользовательского ввода нужно отправлять в приложение
	bool IsInputForApp(const AppEvent& event);

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
