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

		void Init(Window* window, Camera* camera);

		void processInput();

	private:

		void processKeyboardInput();

		void processMouseInput();

		bool getKeyPress(Key key);

		int getMouseButtonPress(MouseButton button);

		int getMouseButtonRelease(MouseButton button);

	private:

		Window* mWindow = nullptr;
		Camera* mCamera = nullptr;

		bool firstMouse;

		f32 lastX;
		f32 lastY;

		f32 mMouseSensitivityX = 0.1f;
		f32 mMouseSensitivityY = 0.1f;
	};
}