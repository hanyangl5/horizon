#include "utils.h"
#include "Window.h"
#include "Camera.h"

namespace Horizon {

	class InputManager {
	public:
		enum class Key {
			ESCAPE,
			SPACE,
			KEY_W,
			KEY_S,
			KEY_A,
			KEY_D,
			KEY_LCTRL,
			KEY_LSHIFT
		};

		enum class MouseButton {
			LEFT_BUTTON,
			RIGHT_BUTTON
		};

		enum class INPUT_STATE {
			PRESS,
			RELEASE
		};
	public:
		InputManager(std::shared_ptr<Window> window, std::shared_ptr<Camera> camera);

		~InputManager();

		void processInput();

	private:

		void processKeyboardInput();

		void processMouseInput();

		bool getKeyPress(Key key);

		int getMouseButtonPress(MouseButton button);

		int getMouseButtonRelease(MouseButton button);

	private:

		std::shared_ptr<Window> mWindow = nullptr;
		std::shared_ptr<Camera> mCamera = nullptr;

		f32 lastX;
		f32 lastY;
		f32 mMouseSensitivityX = 0.1f;
		f32 mMouseSensitivityY = 0.1f;
		bool firstMouse;

	};
}