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

		void Init(Window* window, Camera* camera);

		void processInput();

	private:

		void processKeyboardInput();

		void processMouseInput();

		bool getKeyPress(Key key);

	private:

		Window* mWindow = nullptr;
		Camera* mCamera = nullptr;

		bool firstMouse;
		float yaw;	// yaw is initialized to -90.0 degrees since a yaw of 0.0 results in a direction vector pointing to the right so we initially rotate a bit to the left.
		float pitch;
		float lastX;
		float lastY;
	};
}