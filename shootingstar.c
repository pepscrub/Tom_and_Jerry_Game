#include <stdlib.h>
#include <math.h>
#include <cab202_graphics.h>
#include <cab202_timers.h>

bool game_over = false;
double bx,by,x,y,dx,dy;
//  Declare global variables.

void setup(void) {

    // bomb
    x = 52;
    y = 9;

    // star
    bx = 10;
    by = 12;


    double t1,t2,d;

    t1 = x - bx;
    t2 = y - by;

    d = sqrt(t1*t1 + t2*t2);

    dx = t1 * .15 / d;
    dy = t2 * .15 / d;

    draw_char(bx, by, '!');
    draw_char(x, y, '*');
}

void loop(void) {
    bx = dx + bx;
    by = dy + by;
    clear_screen();

    draw_char(52, 9, '*');
    draw_char(round(bx), round(by), '!');
    if(round(bx) == 52 && round(by) == 9)
    {
        game_over = true;
    }
    else
    {
        game_over = false;
    }
    show_screen();
}

int main() {
    // milliseconds sleep between calls to loop.
    const int DELAY = 10;

    setup_screen();
    setup();
    show_screen();

    while (!game_over) {
        loop();
        timer_pause(DELAY);
    }

    return 0;
}