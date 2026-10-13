#pragma once

namespace ah {
// Main-thread intent survives interruptions, but never resumes a hidden game.
struct AudioState {
    bool wanted=false,interrupted=false,userRequired=false;
    void resume() { wanted=true; }
    void suspend() { wanted=false; }
    void interruptionBegan() { interrupted=true; }
    void interruptionEnded(bool shouldResume) {
        interrupted=false;
        if (!shouldResume) userRequired=true;
    }
    void mediaReset() { interrupted=false;userRequired=true; }
    // An end notification is not guaranteed after a phone-call suspension.
    void userAction() { interrupted=false;userRequired=false; }
    bool canStart(bool appActive) const { return wanted && appActive && !interrupted && !userRequired; }
};
}
