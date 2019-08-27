#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <cab202_graphics.h>
#include <cab202_timers.h>


#include <stdio.h>
#include <stdbool.h>
#include <time.h>

#define MAX_ITEMS (100)
#define BUFFER 50;
#define SPEED 1; // Default speed (directly affects BOTH Tom and Jerrys movement speed as well as Toms wall bounce)
#define PIXELCOUNT 2500 // Maximum number of pixels for walls to render. CAREFUL this value directly affects Tom AI speed

#define MIN(x,y) (((x) < (y)) ? (x) : (y))
#define MAX(x,y) (((x) > (y)) ? (x) : (y))
#define ABS(x)	 (((x) >= 0) ? (x) : -(x))
#define TPOSCALC(c,s,t) (t == 0 ? (c + (s / (PIXELCOUNT)) ) : (c - (s / (PIXELCOUNT)) ) ) // C = current position, S = speed, T = Type (Type refers to either positive or negative of Jerrys coords)
#define SCALEX(x) (x * game.W) // Scale wall point to screen resolution
#define SCALEY(y) ((y * (game.H - game.ob_y)) + game.ob_y) // Scale wall point to resolution on Y (includes automatic scaling from the status bar)
#define VSPAWN(x,y,wx,wy) (y == wy ? true : (x == wx ? true : false) )
#define CRENDER(x, y, i) (x != 0 && y != 0 ? draw_char(x, y, i) : 0)



/*
    Contains the vast majority of game mechanics and logic.
*/
struct game_logic
{
    bool g_over; // Game over / reset
    bool pause; // Game pause mode
    int score; // Current game score
    int loop_delay; // Game rendering delay

    int c_active; // Cheese active elements
    int c_interval; // Cheese interval for placing cheese
    int c_advance; // Cheese max before next room loads 

    int mt_active;
    int mt_max;
    int mt_interval;

    int fw_active;
    int fw_max;
    int fw_interval;

    int lives; // Lives remaining
    int min; // Timer min
    int sec; // Timer sec
    int cpsec; // Previous second for cheese
    int mtpsec;
    int milsec; // Timer milliseconds
    int level;

    int W;
    int H;

    // Status bar out of bounds collision
    int ob_x;
    int ob_y;

    bool wall;
} game;

struct gObj
{
    int x;
    int y;
} c1, c2, c3, c4 ,c5, t1, t2 ,t3 ,t4, t5;

/*
    Contains struct for both jerry and toms x,y speed and global vars that need
    ready access.
*/
struct character
{
    double speed;
    double x,y;
    char img;
    int wall;
} jerry, tom;




// 
double walls[50][4]; // Walls initial position
double walls_scaled[50][4]; // Walls scaled up to screen resolution
double wall_pixels[PIXELCOUNT][2]; // Array holding x,y coordinate of each wall pixel. 0 = x, 1 = y

/*
    Generic function that scales the walls up to the screen resoultion
*/
void scale_walls()
{
    for(int i = 0; i != 50; i++)
    {
        if(walls[i][0] * game.W != 0)
        {
            walls_scaled[i][0] = SCALEX(walls[i][0]);
            walls_scaled[i][1] = SCALEY(walls[i][1]);
            walls_scaled[i][2] = SCALEX(walls[i][2]);
            walls_scaled[i][3] = SCALEY(walls[i][3]);
        }
    }
}

void status_bar()
{
    // * Surely there's a cleaner way to do this?
    double margin = (game.W - 9.5) / 4;
    draw_formatted(0,0,"Student Number: 10407642");
    draw_formatted(margin,0,"Score: %d", game.score);
    draw_formatted(margin*2,0,"Lives: %d", game.lives);
    draw_formatted(margin*3,0,"Player: %c", jerry.img);
    draw_formatted(margin*4,0,"Time %02d:%02d", game.min, game.sec);
    draw_formatted(0,1,"Cheese: %d", game.c_active);
    draw_formatted(margin,1,"Traps: %d", game.mt_active);
    draw_formatted(margin*2,1,"Fireworks: %d", game.fw_active);
    draw_formatted(margin*3,1,"Level: %d", game.level);
    draw_line(0, 2, game.W, 2, '-');
}

/*
    Calculates where all wall pixels are
    - Could not use a check to see which pixels match the wall
*/
void calc_wall_pixels()
{
    int pixel_count = 0;
    for(int l = 0; l != 50; l++)
    {
        // Initializing the array with 0
        // We parse out points from x0 + y0 later
        wall_pixels[pixel_count][0] = 0;
        wall_pixels[pixel_count][1] = 0;

        int x1 = walls_scaled[l][0];
        int y1 = walls_scaled[l][1];
        int x2 = walls_scaled[l][2];
        int y2 = walls_scaled[l][3];

        // Basically the same code from the Wall drawer

        if(x1 == x2)
        {
            // Count all the pixels on the Y coordinate between both points
            int y_min = MIN(y1, y2),y_max = MAX(y1, y2);
            for(int i = y_min; i <= y_max; i++)
            {
                wall_pixels[pixel_count][0] = x1;
                wall_pixels[pixel_count][1] = i;
                pixel_count++;
            }
        }
        else if(y1 == y2)
        {
            int x_min = MIN(x1, x2),x_max = MAX(x1, x2);
            for(int i = x_min; i <= x_max; i++)
            {
                wall_pixels[pixel_count][0] = i;
                wall_pixels[pixel_count][1] = y1;
                pixel_count++;
            }
        }
        else
        {
            if (x1 > x2) {
                int t = x1;
                x1 = x2;
                x2 = t;
                t = y1;
                y1 = y2;
                y2 = t;
            }
            double dx = x2 - x1;
            double dy = y2 - y1;
            double err = 0.0;
            double derr = ABS(dy / dx);

            for (int x = x1, y = y1; (dx > 0) ? x <= x2 : x >= x2; (dx > 0) ? x++ : x--) {
                wall_pixels[pixel_count][0] = x;
                wall_pixels[pixel_count][1] = y;
                err += derr;
                pixel_count++;
                while (err >= 0.5 && ((dy > 0) ? y <= y2 : y >= y2)) {
                    wall_pixels[pixel_count][0] = x;
                    wall_pixels[pixel_count][1] = y;
                    y += (dy > 0) - (dy < 0);
                    err -= 1.0;
                    pixel_count++;
                }
            }
        }
    }
}

/*
    Checks to see if a wall is 1 pixel away from tom in all directions
    returns: 1 (Wall detected)
    returns: 0 (No Wall detected)
*/
int tom_wall ( double wall_x, double wall_y )
{
    int t = 0;
    if(round(tom.x - 1) == wall_x && round(tom.y) == wall_y)          tom.x = round(tom.x + 1), t = 1; // left 
    if(round(tom.x + 1) == wall_x && round(tom.y) == wall_y)          tom.x = round(tom.x - 1), t = 1; // right
    if(round(tom.y - 1) == (wall_y) && round(tom.x) == wall_x)        tom.y = round(tom.y + 1), t = 1; // above
    if(round(tom.y + 1) == (wall_y) && round(tom.x) == wall_x)        tom.y = round(tom.y - 1), t = 1; // below
    return t;
}

/*
    Calculates Tom seeking path and wall collision randomization
    Called from Wall Collision loop
    * Mostly complete pair some minor game improvements
*/
void tom_ai( double wall_x, double wall_y )
{
    int near_wall = tom_wall(wall_x, wall_y);
    if( game.pause ) return;
    if(!near_wall)
    {
        if(!tom.wall)
        {
            int dx,dy;
            dx = jerry.x - tom.x;
            dy = jerry.y - tom.y;

            if( round(jerry.x) == round(tom.x) && round(tom.y) <= round(jerry.y) )           tom.y = TPOSCALC(tom.y, tom.speed, 0),near_wall = 0;
            else if( round(jerry.x) == round(tom.x) && round(tom.y) > round(jerry.y)  )      tom.y = TPOSCALC(tom.y, tom.speed, 1),near_wall = 0;
            else if( round(jerry.y) == round(tom.y) && round(tom.x) <= round(jerry.x) )      tom.x = TPOSCALC(tom.x, tom.speed, 0),near_wall = 0;
            else if( round(jerry.y) == round(tom.y) && round(tom.x) > round(jerry.x)  )      tom.x = TPOSCALC(tom.x, tom.speed, 1),near_wall = 0;
            else // Line algroithim appeared to exibit the same behaviours as the code below
            {
                if ( dx < 0 ) tom.x = TPOSCALC(tom.x, tom.speed, 1),near_wall = 0;
                if ( dx > 0 ) tom.x = TPOSCALC(tom.x, tom.speed, 0),near_wall = 0;
                if ( dy < 0 ) tom.y = TPOSCALC(tom.y, tom.speed, 1),near_wall = 0;
                if ( dy > 0 ) tom.y = TPOSCALC(tom.y, tom.speed, 0),near_wall = 0;
            }
        }
    }
    else
    {
            int js,ts,x,y;
            js = jerry.speed;
            // Tom speed needs to be rounded up to 1 so we can use rand()
            ts = tom.speed <= 1 ? 1 : tom.speed;
            // Fly window interaction
            // x and y check to see if Jerry is left/right above/below the players current coordinate
            // TODO: Add minmum distance based on Toms position and the nearest wall behind the teleport
            // * Could also animate this process very easily
            x = tom.x > jerry.x ? tom.x + rand() % 2 : tom.x - rand() % 2;
            y = tom.y > jerry.y ? tom.y + rand() % 2 : tom.y - rand() % 2;

            tom.x = TPOSCALC(x, rand() % (ts + js), (tom.x < jerry.x ? 0 : 1));
            tom.y = TPOSCALC(y, rand() % (ts + js), (tom.y < jerry.y ? 0 : 1));
            tom.wall = 0;
    }
}

void mt_place( int wx, int wy )
{
    if(game.sec % game.mt_interval == 0 && game.mtpsec != game.sec)
    {
        if(game.mt_active == 5) return;
        game.mtpsec = game.sec;
        if(t1.x == 0 && t1.y == 0) t1.x = tom.x, t1.y = tom.y;
        else if(t1.x == 0 && t1.y == 0) t1.x = tom.x, t1.y = tom.y;
        else if(t2.x == 0 && t2.y == 0) t2.x = tom.x, t2.y = tom.y;
        else if(t3.x == 0 && t3.y == 0) t3.x = tom.x, t3.y = tom.y;
        else if(t4.x == 0 && t4.y == 0) t4.x = tom.x, t4.y = tom.y;
        game.mt_active++;
    }
}

void mt_render( void )
{
    CRENDER(t1.x, t1.y, 'M');
    CRENDER(t2.x, t2.y, 'M');
    CRENDER(t3.x, t3.y, 'M');
    CRENDER(t4.x, t4.y, 'M');
    CRENDER(t5.x, t5.y, 'M');


    if(jerry.x == t1.x && jerry.y == t1.y) game.score++, t1.x = 0, t1.y = 0, game.mt_active--;
    if(jerry.x == t2.x && jerry.y == t2.y) game.score++, t2.x = 0, t2.y = 0, game.mt_active--;
    if(jerry.x == t3.x && jerry.y == t3.y) game.score++, t3.x = 0, t3.y = 0, game.mt_active--;
    if(jerry.x == t4.x && jerry.y == t4.y) game.score++, t4.x = 0, t4.y = 0, game.mt_active--;
    if(jerry.x == t5.x && jerry.y == t5.y) game.score++, t5.x = 0, t5.y = 0, game.mt_active--;
}

void cheese_place( int wx, int wy )
{
    if(game.sec % game.c_interval == 0 && game.cpsec != game.sec)
    {
        if(game.c_active == 5) return;
        game.cpsec = game.sec;
        int x,y;
        x = rand() % screen_width();
        y = rand() % screen_height();

        while (VSPAWN(x, y, wx, wy))
        {
            x = rand() % screen_width();
            y = rand() % screen_height();
        }
        if(c1.x == 0 && c1.y == 0) c1.x = x, c1.y = y;
        else if(c2.x == 0 && c2.y == 0) c2.x = x, c2.y = y;
        else if(c3.x == 0 && c3.y == 0) c3.x = x, c3.y = y;
        else if(c4.x == 0 && c4.y == 0) c4.x = x, c4.y = y;
        else if(c5.x == 0 && c5.y == 0) c5.x = x, c5.y = y;
        game.c_active++;
    }
}

void cheese_render()
{
    CRENDER(c1.x, c1.y, 'C');
    CRENDER(c2.x, c2.y, 'C');
    CRENDER(c3.x, c3.y, 'C');
    CRENDER(c4.x, c4.y, 'C');
    CRENDER(c5.x, c5.y, 'C');
    // Cheese interactions
    if(jerry.x == c1.x && jerry.y == c1.y) game.score++, c1.x = 0, c1.y = 0, game.c_active--;
    if(jerry.x == c2.x && jerry.y == c2.y) game.score++, c2.x = 0, c2.y = 0, game.c_active--;
    if(jerry.x == c3.x && jerry.y == c3.y) game.score++, c3.x = 0, c3.y = 0, game.c_active--;
    if(jerry.x == c4.x && jerry.y == c4.y) game.score++, c4.x = 0, c4.y = 0, game.c_active--;
    if(jerry.x == c5.x && jerry.y == c5.y) game.score++, c5.x = 0, c5.y = 0, game.c_active--;

}


void collision_wall( char key )
{
    calc_wall_pixels();
    for(int i = 0; i != PIXELCOUNT; i++)
    {
        if(wall_pixels[i][0] != 0)
        {
            // Using jerry.speed to directly affect movement speed
            if(key == 'a' && jerry.x - 1 == round(wall_pixels[i][0]) && wall_pixels[i][1] == jerry.y) jerry.x = jerry.x + jerry.speed;
            if(key == 'd' && jerry.x + 1 == round(wall_pixels[i][0]) && wall_pixels[i][1] == jerry.y) jerry.x = jerry.x - jerry.speed;
            if(key == 'w' && jerry.y - 1 == round(wall_pixels[i][1]) && wall_pixels[i][0] == jerry.x) jerry.y = jerry.y + jerry.speed;
            if(key == 's' && jerry.y + 1 == round(wall_pixels[i][1]) && wall_pixels[i][0] == jerry.x) jerry.y = jerry.y - jerry.speed;
            tom_ai( wall_pixels[i][0], wall_pixels[i][1]); // Calling TOMS ai
            cheese_place( wall_pixels[i][0], wall_pixels[i][1]); // Cheese location
            mt_place( wall_pixels[i][0], wall_pixels[i][1]);
        }
    }
}

void draw_wall(void)
{
    scale_walls();
    for(int i = 0; i != 50; i++)
    {
        if(walls_scaled[i][0] != 0)
        {
            draw_line(walls_scaled[i][0], walls_scaled[i][1], walls_scaled[i][2], walls_scaled[i][3], '*');
        }
    }
}


void read_file(FILE*stream)
{
    int wall_num=0;
    while(!feof(stream))
    {
        char command;
        double a1, a2, a3, a4;
        double num = fscanf(stream, " %c %lf %lf %lf %lf", &command, &a1, &a2, &a3, &a4);
        if(num == 3) // Number
        {
            if(command == 'J')
            {
                jerry.img = command;
                jerry.x = a1;
                jerry.y = a2;
            }
            if(command == 'T')
            {
                tom.img = command;
                tom.x = a1;
                tom.y = a2;
            }
        }
        else if(num == 5)
        {
            if(command == 'W')
            {
                walls[wall_num][0] = a1;
                walls[wall_num][1] = a2;
                walls[wall_num][2] = a3;
                walls[wall_num][3] = a4;
                wall_num++;
            }
        }
    }
}

void jerry_setup( void ) 
{
    jerry.speed = 1;

    jerry.x = jerry.x * game.W;
    jerry.y = (jerry.y * game.H) + game.ob_y;

}

void tom_setup( void )
{
    tom.speed = jerry.speed / 2;

    tom.x = tom.x * game.W;
    tom.y = (tom.y * game.H) + game.ob_y;
    tom.wall = 0;
}

void tom_ob_check( void )
{
    if(tom.x < game.ob_x){tom.x++;}
    if(tom.y < game.ob_y){tom.y++;}
    if(tom.x > game.W - 1) {tom.x = game.W - 1;} 
    if(tom.y > game.H - 1) {tom.y = game.H - 1;}
}


void controller( void )
{
    int key = get_char();
    if(key == 'p') game.pause = (game.pause) ? false : true;

    if(game.wall) return;

    collision_wall( key );

    if(key == 'a' && jerry.x - 1 >= game.ob_x) jerry.x--;
    else if (key == 'd' && jerry.x + 1 < game.W) jerry.x++;
    else if (key == 'w' && jerry.y - 1 >= game.ob_y) jerry.y--;
    else if (key == 's' && jerry.y + 1 < game.H) jerry.y++;
}

void game_logic_setup ( void )
{   
    // Game conditions
    game.g_over = false;
    game.score = 0;
    game.loop_delay = 10;

    game.pause = false;

    game.ob_x = 0;
    game.ob_y = 3;


    // Cheese
    game.c_active = 0;
    game.c_advance = 5;
    game.c_interval = 2;

    // Mouse traps
    game.mt_active = 0;
    game.mt_interval = 3;
    game.mt_max = 5;

    game.fw_active = 0;
    game.fw_interval = 0;
    game.fw_max = 0;

    game.lives = 5;
    game.min = 0;
    game.sec = 0;
    game.milsec = 0;

    game.level = 0;

    // Screen
    game.W = screen_width();
    game.H = screen_height();
    game.wall = false; // Collision boolean

    c1.x = 0, c1.y = 0;
    c2.x = 0, c2.y = 0;
    c3.x = 0, c3.y = 0;
    c4.x = 0, c4.y = 0;
    c5.x = 0, c5.y = 0;

    t1.x = 0, t1.y = 0;
    t2.x = 0, t2.y = 0;
    t3.x = 0, c3.y = 0;
    t4.x = 0, t4.y = 0;
    t5.x = 0, t5.y = 0;
}
/*
    Initial game setup
*/
void setup ( void ) 
{
    game_logic_setup();
    jerry_setup();
    tom_setup();
    status_bar();
}


/*
    Timer calculation
    Conditions:
    - Game pause: false
*/
void timer( void )
{
    if ( game.pause ) return;
    if ( game.milsec == 60 ) game.sec++, game.milsec = 0;
    if ( game.sec == 60 ) game.min++, game.sec = 0;
    game.milsec++;
}


void loop( void ) 
{
    clear_screen();
    game.W = screen_width();
    game.H = screen_height();


    status_bar();
    draw_wall();
    controller();
    tom_ob_check();
    timer();
    cheese_render();
    mt_render();

    draw_char(jerry.x, jerry.y, jerry.img);
    draw_char(tom.x, tom.y, tom.img);

    show_screen();
}

int main(int argc, char *argv[]) 
{
    for (int i = 1; i < argc; i++) 
    {
        FILE * stream = fopen(argv[i], "r");
        if (stream != NULL)
        {
            read_file(stream);
            fclose(stream);
        }
    }
    srand( time(NULL) );
    setup_screen();
    setup();

    draw_wall();
    show_screen();

    while ( !game.g_over ) {
        loop();
        timer_pause(10);
    }
    return 0;
}