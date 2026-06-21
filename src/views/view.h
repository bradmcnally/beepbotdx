#pragma once

#include "core/canvas.h"
#include "platform/input.h"

class View {
public:
    virtual ~View() {}
    virtual void enter() = 0;
    virtual void update(InputEvent event) = 0;
    virtual void draw(Canvas& canvas) = 0;
    virtual void exit() = 0;
};
