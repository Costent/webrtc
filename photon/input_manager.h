#ifndef PHOTON_INPUT_MANAGER_H_
#define PHOTON_INPUT_MANAGER_H_

#include <Windows.h>

#define KEY_SHIFT 0x10

namespace photon {

class InputManager {
 public:
  InputManager(void);
  ~InputManager(void);
  void KeyDown(char key);
  void KeyUp(char key);
  void MouseMove(int x, int y);
  void MouseButtonEvent(char button, char state);
  void MouseWheelEvent(int x);

 private:
  void KeyEvent(char key, unsigned long flags);

  bool shift_;
};

}  // namespace photon

#endif  // PHOTON_INPUT_MANAGER_H_
