#pragma once

#include <stdint.h>

// Pure C++11 animation scheduling. The caller owns drawing and persistence.
// All deadlines are relative and well below 2^31 ms, including around millis()
// wrap. One tick advances at most one visible pose; delayed callers never catch up.
namespace pet_animation {

enum class Mode : uint8_t { Idle, Walk, Jump, Roll, Bark, Eat, Rest };

class Engine {
public:
  static constexpr uint32_t intervalMs = 300U;

  void begin(uint32_t now, uint32_t seed) {
    random_ = seed ? seed : 0x9e3779b9U;
    nextAt_ = now + intervalMs;
    lastEatAt_ = 0;
    x_ = 104;
    direction_ = 1;
    mode_ = Mode::Idle;
    lastAction_ = Mode::Idle;
    step_ = 0;
    walkSteps_ = 6;
    introNext_ = 0;
    visible_ = false;
    paused_ = false;
    ate_ = false;
    begun_ = true;
  }

  void setVisible(bool value, uint32_t now) {
    if (visible_ == value) return;
    visible_ = value;
    // Preserve the current action/pose and intro queue; do not advance on entry.
    if (value) nextAt_ = now + intervalMs;
  }

  void setPaused(bool value, uint32_t now) {
    if (paused_ == value) return;
    paused_ = value;
    if (!value) nextAt_ = now + intervalMs;
  }

  bool tick(uint32_t now) {
    if (!begun_ || !visible_ || paused_ || !due(now, nextAt_)) return false;
    if (mode_ == Mode::Idle || mode_ == Mode::Rest) {
      const Mode previous = mode_;
      const int16_t previousX = x_;
      const uint8_t previousFrame = frame();
      startNext(now);
      return mode_ != previous || x_ != previousX || frame() != previousFrame;
    }

    if (static_cast<uint8_t>(step_ + 1U) < stepCount()) {
      ++step_;
      if (mode_ == Mode::Walk) walkOneStep();
      nextAt_ = now + intervalMs;
    } else {
      mode_ = Mode::Rest;
      step_ = 0;
      nextAt_ = now + restMs();
    }
    return true;
  }

  bool perform(Mode action, uint32_t now) {
    if (!begun_ || (action != Mode::Jump && action != Mode::Roll &&
                    action != Mode::Bark && action != Mode::Eat)) return false;
    // An explicit interaction ends the demonstration queue, then normal
    // autonomous selection resumes after this action and its rest period.
    introNext_ = kIntroCount;
    start(action, now, false);
    return true;
  }

  void pat(uint32_t now) { (void)perform(Mode::Jump, now); }

  Mode mode() const { return mode_; }
  uint8_t pose() const {
    static const uint8_t bark[] = {0, 1, 2, 1, 2, 3};
    static const uint8_t eat[] = {0, 1, 2, 1, 2, 1, 2, 3};
    if (mode_ == Mode::Bark) return bark[step_];
    if (mode_ == Mode::Eat) return eat[step_];
    if (mode_ == Mode::Walk) return step_ % 4U;
    if (mode_ == Mode::Jump || mode_ == Mode::Roll) return step_;
    return 0;
  }
  uint8_t frame() const {
    switch (mode_) {
      case Mode::Jump: return pose();
      case Mode::Walk: return static_cast<uint8_t>(6U + pose());
      case Mode::Roll: return static_cast<uint8_t>(10U + pose());
      case Mode::Bark: return static_cast<uint8_t>(16U + pose());
      case Mode::Eat: return static_cast<uint8_t>(20U + pose());
      default: return 0;
    }
  }
  int16_t x() const { return x_; }
  int8_t direction() const { return direction_; }
  bool paused() const { return paused_; }
  bool visible() const { return visible_; }
  uint32_t nextAt() const { return nextAt_; }

private:
  static constexpr uint8_t kIntroCount = 5;
  uint32_t random_ = 0x9e3779b9U;
  uint32_t nextAt_ = 0, lastEatAt_ = 0;
  int16_t x_ = 104;
  int8_t direction_ = 1;
  Mode mode_ = Mode::Idle, lastAction_ = Mode::Idle;
  uint8_t step_ = 0, walkSteps_ = 6, introNext_ = 0;
  bool visible_ = false, paused_ = false, ate_ = false, begun_ = false;

  static bool due(uint32_t now, uint32_t deadline) {
    // Unsigned arithmetic has defined wrap semantics, unlike signed overflow.
    return static_cast<uint32_t>(now - deadline) < 0x80000000U;
  }
  uint32_t randomValue() {
    random_ ^= random_ << 13;
    random_ ^= random_ >> 17;
    random_ ^= random_ << 5;
    return random_;
  }
  uint32_t restMs() { return 1800U + randomValue() % 1201U; }
  uint8_t stepCount() const {
    if (mode_ == Mode::Walk) return walkSteps_;
    if (mode_ == Mode::Jump || mode_ == Mode::Roll || mode_ == Mode::Bark) return 6;
    if (mode_ == Mode::Eat) return 8;
    return 1;
  }
  void walkOneStep() {
    const int16_t proposed = static_cast<int16_t>(x_ + 6 * direction_);
    if (proposed >= 152) { x_ = 152; direction_ = -1; }
    else if (proposed <= 56) { x_ = 56; direction_ = 1; }
    else x_ = proposed;
  }
  void start(Mode action, uint32_t now, bool firstWalk) {
    mode_ = action;
    lastAction_ = action;
    step_ = 0;
    // Match the approved preview: stationary actions share the centered stage.
    if (action == Mode::Roll || action == Mode::Bark || action == Mode::Eat) x_ = 104;
    if (action == Mode::Rest) {
      nextAt_ = now + restMs();
      return;
    }
    if (action == Mode::Walk) {
      walkSteps_ = firstWalk ? 6U : static_cast<uint8_t>(4U + randomValue() % 4U);
      walkOneStep();
    }
    if (action == Mode::Eat) { ate_ = true; lastEatAt_ = now; }
    nextAt_ = now + intervalMs;
  }
  void startNext(uint32_t now) {
    static const Mode intro[] = {Mode::Walk, Mode::Jump, Mode::Roll, Mode::Bark, Mode::Eat};
    if (introNext_ < kIntroCount) {
      const bool firstWalk = introNext_ == 0;
      const Mode action = intro[introNext_++];
      start(action, now, firstWalk);
      return;
    }

    // 9/16 walk, 3/16 quiet rest; brief playful actions are less frequent.
    // Eat is eligible immediately on a never-fed pet, otherwise after 90 s.
    for (uint8_t attempt = 0; attempt < 24; ++attempt) {
      const uint32_t choice = randomValue() % 16U;
      Mode action = choice < 9U ? Mode::Walk : choice < 12U ? Mode::Rest :
                    choice == 12U ? Mode::Jump : choice == 13U ? Mode::Roll :
                    choice == 14U ? Mode::Bark : Mode::Eat;
      if (action == lastAction_) continue;
      if (action == Mode::Eat && ate_ && static_cast<uint32_t>(now - lastEatAt_) < 90000U) continue;
      start(action, now, false);
      return;
    }
    // Bounded retry loop, with a non-repeating, always-valid fallback.
    start(lastAction_ == Mode::Walk ? Mode::Rest : Mode::Walk, now, false);
  }
};

} // namespace pet_animation
